/*
 * This file is part of the swblocks-baselib library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BL_JNI_JNIENVIRONMENT_H_
#define __BL_JNI_JNIENVIRONMENT_H_

#include <baselib/jni/JniResourceWrappers.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#define CHECK_JAVA_EXCEPTION( message ) \
    checkJavaException( \
        [ & ]() -> std::string \
        { \
            return resolveMessage( message ); \
        }); \

namespace bl
{
    namespace jni
    {
        inline std::string jniErrorMessage( SAA_in const jint jniErrorCode )
        {
            /*
             * Possible return values for JNI functions from jni.h
             */

            switch( jniErrorCode )
            {
                case JNI_OK:        return "success";
                case JNI_ERR:       return "unknown error";
                case JNI_EDETACHED: return "thread detached from the VM";
                case JNI_EVERSION:  return "JNI version error";
                case JNI_ENOMEM:    return "not enough memory";
                case JNI_EEXIST:    return "VM already created";
                case JNI_EINVAL:    return "invalid arguments";
                default:            return "invalid JNI error code";
            }
        }

        /**
         * @brief class JniEnvironment - a wrapper class for JNIEnv* raw pointer
         */

        template
        <
            typename E = void
        >
        class JniEnvironmentT
        {
            BL_NO_COPY_OR_MOVE( JniEnvironmentT )

        private:

            typedef JniEnvironmentT< E >                        this_type;

            static os::thread_specific_ptr< this_type >         g_tlsDataJni;

            static std::atomic< int64_t >                       g_jniThreadCount;

            static JavaVM*                                      g_javaVM;
            static bool                                         g_exceptionHandlingBootstrapped;

            static jmethodID                                    g_classGetName;
            static jmethodID                                    g_objectToString;
            static jmethodID                                    g_objectGetClass;

            static jclass                                       g_throwableClass;
            static jmethodID                                    g_throwableGetMessage;
            static jmethodID                                    g_throwableGetCause;
            static jmethodID                                    g_throwableGetStackTrace;

            static jclass                                       g_threadClass;
            static jmethodID                                    g_threadCurrentThread;
            static jmethodID                                    g_threadGetName;

            static jmethodID                                    g_byteBufferGetCapacity;
            static jmethodID                                    g_byteBufferGetPosition;
            static jmethodID                                    g_byteBufferSetPosition;
            static jmethodID                                    g_byteBufferGetLimit;
            static jmethodID                                    g_byteBufferSetLimit;
            static jmethodID                                    g_byteBufferClear;
            static jmethodID                                    g_byteBufferFlip;
            static jmethodID                                    g_byteBufferOrder;
            static jmethodID                                    g_byteBufferIsDirect;
            static jmethodID                                    g_byteBufferArray;
            static jobject                                      g_nativeByteOrder;

            JNIEnv*                                             m_jniEnv;

            mutable bool                                        m_processingException;

            JniEnvironmentT()
                :
                m_jniEnv( nullptr ),
                m_processingException( false )
            {
                JNIEnv* jniEnv;

                const jint jniErrorCode = g_javaVM -> AttachCurrentThread(
                    reinterpret_cast< void** >( &jniEnv ),
                    nullptr /* thread_args */
                    );

                BL_CHK_T(
                    false,
                    jniErrorCode == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to attach current thread to JVM. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );

                m_jniEnv = jniEnv;

                ++g_jniThreadCount;
            }

            void checkJavaException( SAA_in const cpp::function< std::string () >& cbMessage ) const
            {
                const auto javaException = m_jniEnv -> ExceptionOccurred();

                if( javaException )
                {
                    const auto exception = LocalReference< jthrowable >::attach( javaException );

                    m_jniEnv -> ExceptionClear();

                    if( m_processingException )
                    {
                        BL_RIP_MSG( "Another Java exception thrown while processing details of the original exception" );
                    }

                    m_processingException = true;

                    BL_SCOPE_EXIT(
                        {
                            m_processingException = false;
                        }
                        );

                    throwJavaException( cbMessage, exception );
                }
            }

            void throwJavaException(
                SAA_in      const cpp::function< std::string () >&          cbMessage,
                SAA_in      const LocalReference< jthrowable >&             throwable
                ) const
            {
                auto exception = JavaException();

                if( g_exceptionHandlingBootstrapped )
                {
                    const auto currentThread = callStaticObjectMethod< jobjectArray >(
                        g_threadClass,
                        g_threadCurrentThread
                        );

                    const auto threadName = callObjectMethod< jstring >(
                        currentThread.get(),
                        g_threadGetName
                        );

                    const auto message = callObjectMethod< jstring >(
                        throwable.get(),
                        g_throwableGetMessage
                        );

                    const auto toString = callObjectMethod< jstring >(
                        throwable.get(),
                        g_objectToString
                        );

                    const auto javaClass = callObjectMethod< jclass >(
                        throwable.get(),
                        g_objectGetClass
                        );

                    cpp::SafeOutputStringStream stackTrace;

                    appendStackTraceElements( stackTrace, throwable );

                    exception
                        << eh::errinfo_original_message( javaStringToCString( message ) )
                        << eh::errinfo_original_type( getClassName( javaClass.get() ) )
                        << eh::errinfo_original_thread_name( javaStringToCString( threadName ) )
                        << eh::errinfo_original_stack_trace( stackTrace.str() )
                        << eh::errinfo_string_value( javaStringToCString( toString ) );
                }

                BL_THROW( std::move( exception ), cbMessage() );
            }

            void appendStackTraceElements(
                SAA_in  cpp::SafeOutputStringStream&                oss,
                SAA_in  const LocalReference< jthrowable >&         exception,
                SAA_in  const int                                   level = 0
                ) const
            {
                if( level > 0 )
                {
                    oss << "\nCaused by: ";

                    const auto exceptionAsString = callObjectMethod< jstring >(
                        exception.get(),
                        g_objectToString
                        );

                    oss << javaStringToCString( exceptionAsString );
                }

                const auto stackTraceElements = callObjectMethod< jobjectArray >(
                    exception.get(),
                    g_throwableGetStackTrace
                    );

                if( ! stackTraceElements.get() )
                {
                    return;
                }

                for( jsize index = 0; index < getArrayLength( stackTraceElements.get() ); ++index )
                {
                    const auto stackTraceElement = getObjectArrayElement< jobject >(
                        stackTraceElements.get(),
                        index
                        );

                    const auto frameString = callObjectMethod< jstring >(
                        stackTraceElement.get(),
                        g_objectToString
                        );

                    oss
                        << "\n    at "
                        << javaStringToCString( frameString );
                }

                const auto cause = callObjectMethod< jthrowable >( exception.get(), g_throwableGetCause );

                if ( cause.get() )
                {
                    appendStackTraceElements( oss, cause, level + 1 );
                }
            }

            void bootstrapExceptionHandling()
            {
                const auto classClass = findJavaClass( "java/lang/Class" );

                g_classGetName = getMethodID(
                    classClass.get(),
                    "getName",
                    "()Ljava/lang/String;"
                    );

                const auto objectClass = findJavaClass( "java/lang/Object" );

                g_objectToString = getMethodID(
                    objectClass.get(),
                    "toString",
                    "()Ljava/lang/String;"
                    );

                g_objectGetClass = getMethodID(
                    objectClass.get(),
                    "getClass",
                    "()Ljava/lang/Class;"
                    );

                auto throwableClass = createGlobalReference< jclass >( findJavaClass( "java/lang/Throwable" ) );

                g_throwableGetMessage = getMethodID(
                    throwableClass.get(),
                    "getMessage",
                    "()Ljava/lang/String;"
                    );

                g_throwableGetCause = getMethodID(
                    throwableClass.get(),
                    "getCause",
                    "()Ljava/lang/Throwable;"
                    );

                g_throwableGetStackTrace = getMethodID(
                    throwableClass.get(),
                    "getStackTrace",
                    "()[Ljava/lang/StackTraceElement;"
                    );

                g_throwableClass = throwableClass.get();
                throwableClass.release();

                auto threadClass = createGlobalReference< jclass >( findJavaClass( "java/lang/Thread" ) );

                g_threadCurrentThread = getStaticMethodID(
                    threadClass.get(),
                    "currentThread",
                    "()Ljava/lang/Thread;"
                    );

                g_threadGetName = getMethodID(
                    threadClass.get(),
                    "getName",
                    "()Ljava/lang/String;"
                    );

                g_threadClass = threadClass.get();
                threadClass.release();

                g_exceptionHandlingBootstrapped = true;
            }

            void initializeStaticData()
            {
                bootstrapExceptionHandling();

                const auto byteBufferClass = findJavaClass( "java/nio/ByteBuffer" );

                g_byteBufferGetCapacity = getMethodID( byteBufferClass.get(), "capacity", "()I" );
                g_byteBufferGetPosition = getMethodID( byteBufferClass.get(), "position", "()I" );
                g_byteBufferSetPosition = getMethodID( byteBufferClass.get(), "position", "(I)Ljava/nio/Buffer;" );
                g_byteBufferGetLimit = getMethodID( byteBufferClass.get(), "limit", "()I" );
                g_byteBufferSetLimit = getMethodID( byteBufferClass.get(), "limit", "(I)Ljava/nio/Buffer;" );
                g_byteBufferClear = getMethodID( byteBufferClass.get(), "clear", "()Ljava/nio/Buffer;" );
                g_byteBufferFlip = getMethodID( byteBufferClass.get(), "flip", "()Ljava/nio/Buffer;" );
                g_byteBufferOrder = getMethodID( byteBufferClass.get(), "order", "(Ljava/nio/ByteOrder;)Ljava/nio/ByteBuffer;" );
                g_byteBufferIsDirect = getMethodID( byteBufferClass.get(), "isDirect", "()Z" );
                g_byteBufferArray = getMethodID( byteBufferClass.get(), "array", "()[B" );

                const auto byteOrderClass = findJavaClass( "java/nio/ByteOrder" );
                const auto byteOrderNativeOrder = getStaticMethodID( byteOrderClass.get(), "nativeOrder", "()Ljava/nio/ByteOrder;" );

                auto nativeByteOrder = createGlobalReference< jobject >(
                    callStaticObjectMethod< jobject >( byteOrderClass.get(), byteOrderNativeOrder )
                    );

                g_nativeByteOrder = nativeByteOrder.get();
                nativeByteOrder.release();
            }

        public:

            ~JniEnvironmentT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                const jint jniErrorCode = g_javaVM -> DetachCurrentThread();

                BL_CHK_T(
                    false,
                    jniErrorCode == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to detach current thread from JVM. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );

                --g_jniThreadCount;

                BL_NOEXCEPT_END()
            }

            static void setJavaVM( JavaVM* javaVM )
            {
                BL_ASSERT( g_javaVM == nullptr );

                g_javaVM = javaVM;

                BL_SCOPE_EXIT(
                {
                    detach();
                }
                );

                instance().initializeStaticData();
            }

            static void detach() NOEXCEPT
            {
                if( g_tlsDataJni.get() )
                {
                    g_tlsDataJni.reset();
                }
            }

            static auto instance() -> this_type&
            {
                if( ! g_tlsDataJni.get() )
                {
                    g_tlsDataJni.reset( new this_type() );
                }

                BL_ASSERT( g_tlsDataJni.get() );

                return *g_tlsDataJni.get();
            }

            static auto instanceNoThrow() NOEXCEPT -> this_type&
            {
                BL_ASSERT( g_tlsDataJni.get() );

                return *g_tlsDataJni.get();
            }

            static int64_t getJniThreadCount() NOEXCEPT
            {
                return g_jniThreadCount;
            }

            jint getVersion() const NOEXCEPT
            {
                return m_jniEnv -> GetVersion();
            }

            JNIEnv* getRawPtr() const NOEXCEPT
            {
                return m_jniEnv;
            }

            void deleteLocalRef( SAA_in const jobject object ) const NOEXCEPT
            {
                m_jniEnv -> DeleteLocalRef( object );
            }

            template
            <
                typename T
            >
            GlobalReference< T > createGlobalReference( SAA_in T javaObject ) const
            {
                auto globalReference = GlobalReference< T >::attach(
                    reinterpret_cast< T >( m_jniEnv -> NewGlobalRef( javaObject ) )
                    );

                BL_CHK_T(
                    nullptr,
                    globalReference.get(),
                    JavaException(),
                    BL_MSG()
                        << "Failed to create new global reference"
                    );

                return globalReference;
            }

            template
            <
                typename T
            >
            GlobalReference< T > createGlobalReference( SAA_in const LocalReference< T >& localReference ) const
            {
                auto globalReference = GlobalReference< T >::attach(
                    reinterpret_cast< T >( m_jniEnv -> NewGlobalRef( localReference.get() ) )
                    );

                BL_CHK_T(
                    nullptr,
                    globalReference.get(),
                    JavaException(),
                    BL_MSG()
                        << "Failed to create new global reference"
                    );

                return globalReference;
            }

            void deleteGlobalRef( SAA_in const jobject object ) const NOEXCEPT
            {
                m_jniEnv -> DeleteGlobalRef( object );
            }

            LocalReference< jclass > findJavaClass( SAA_in const std::string& className ) const
            {
                auto javaClass = LocalReference< jclass >::attach(
                    m_jniEnv -> FindClass( className.c_str() )
                    );

                CHECK_JAVA_EXCEPTION(
                    BL_MSG()
                        << "Java class '"
                        << className
                        << "' not found"
                    );

                return javaClass;
            }

            std::string javaStringToCString( SAA_in const LocalReference< jstring >& javaString ) const
            {
                const auto utfChars = m_jniEnv -> GetStringUTFChars(
                    javaString.get(),
                    nullptr /* isCopy */
                    );

                BL_SCOPE_EXIT(
                    {
                        m_jniEnv -> ReleaseStringUTFChars( javaString.get(), utfChars );
                    }
                    );

                return std::string( utfChars );
            }

            LocalReference< jclass > getObjectClass( SAA_in const jobject object ) const
            {
                auto javaClass = LocalReference< jclass >::attach(
                    m_jniEnv -> GetObjectClass( object )
                    );

                if( javaClass.get() == nullptr )
                {
                    BL_RIP_MSG( "Failed to get the object class from jobject reference" );
                }

                return javaClass;
            }

            std::string getClassName( SAA_in const jclass javaClass ) const
            {
                const auto className = LocalReference< jstring >::attach(
                    static_cast< jstring >( m_jniEnv -> CallObjectMethod( javaClass, g_classGetName ) )
                    );

                if( className.get() == nullptr )
                {
                    BL_RIP_MSG( "Failed to get the class name from jclass reference" );
                }

                return javaStringToCString( className );
            }

            void throwNew(
                SAA_in  const jclass                        javaClass,
                SAA_in  const std::string&                  message )
                const
            {
                if( m_jniEnv -> ThrowNew( javaClass, message.c_str() ) != 0 )
                {
                    BL_RIP_MSG( "Failed to throw Java exception from JNI code" );
                }
            }

            jmethodID getMethodID(
                SAA_in  const jclass                        javaClass,
                SAA_in  const std::string&                  methodName,
                SAA_in  const std::string&                  methodSignature
                ) const
            {
                const auto methodID = m_jniEnv -> GetMethodID(
                    javaClass,
                    methodName.c_str(),
                    methodSignature.c_str()
                    );

                CHECK_JAVA_EXCEPTION(
                    BL_MSG()
                        << "Method '"
                        << methodName
                        << "' with signature '"
                        << methodSignature
                        << "' not found in class '"
                        << getClassName( javaClass )
                        << "'"
                    );

                return methodID;
            }

            jmethodID getStaticMethodID(
                SAA_in  const jclass                        javaClass,
                SAA_in  const std::string&                  methodName,
                SAA_in  const std::string&                  methodSignature
                ) const
            {
                const auto methodID = m_jniEnv -> GetStaticMethodID(
                    javaClass,
                    methodName.c_str(),
                    methodSignature.c_str()
                    );

                CHECK_JAVA_EXCEPTION(
                    BL_MSG()
                        << "Static method '"
                        << methodName
                        << "' with signature '"
                        << methodSignature
                        << "' not found in class '"
                        << getClassName( javaClass )
                        << "'"
                    );

                return methodID;
            }

            void callVoidMethod(
                SAA_in  const jobject                       object,
                SAA_in  const jmethodID                     methodID,
                ...
                ) const
            {
                va_list args;
                va_start( args, methodID );

                m_jniEnv -> CallVoidMethodV( object, methodID, args );

                va_end( args );

                CHECK_JAVA_EXCEPTION( "CallVoidMethodV failed" );
            }

            void callStaticVoidMethod(
                SAA_in  const jclass                        javaClass,
                SAA_in  const jmethodID                     methodID,
                ...
                ) const
            {
                va_list args;
                va_start( args, methodID );

                m_jniEnv -> CallStaticVoidMethodV( javaClass, methodID, args );

                va_end( args );

                CHECK_JAVA_EXCEPTION( "CallStaticVoidMethodV failed" );
            }

            template
            <
                typename T
            >
            LocalReference< T > callObjectMethod(
                SAA_in  const jobject                       obj,
                SAA_in  const jmethodID                     methodID,
                ...
                ) const
            {
                va_list args;
                va_start( args, methodID );

                auto localReference = LocalReference< T >::attach(
                    static_cast< T >( m_jniEnv -> CallObjectMethodV( obj, methodID, args ) )
                    );

                va_end( args );

                CHECK_JAVA_EXCEPTION( "CallObjectMethod failed" );

                return localReference;
            }

            template
            <
                typename T
            >
            LocalReference< T > callStaticObjectMethod(
                SAA_in  const jclass                        javaClass,
                SAA_in  const jmethodID                     methodID,
                ...
                ) const
            {
                va_list args;
                va_start( args, methodID );

                auto localReference = LocalReference< T >::attach(
                    static_cast< T >( m_jniEnv -> CallStaticObjectMethodV( javaClass, methodID, args ) )
                    );

                va_end( args );

                CHECK_JAVA_EXCEPTION( "CallStaticObjectMethod failed" );

                return localReference;
            }

            jboolean callBooleanMethod(
                SAA_in  const jobject                       object,
                SAA_in  const jmethodID                     methodID,
                ...
                ) const
            {
                va_list args;
                va_start( args, methodID );

                const jboolean result = m_jniEnv -> CallBooleanMethodV( object, methodID, args );

                va_end( args );

                CHECK_JAVA_EXCEPTION( "Boolean failed" );

                return result;
            }

            jint callIntMethod(
                SAA_in  const jobject                       object,
                SAA_in  const jmethodID                     methodID,
                ...
                ) const
            {
                va_list args;
                va_start( args, methodID );

                const jint result = m_jniEnv -> CallIntMethodV( object, methodID, args );

                va_end( args );

                CHECK_JAVA_EXCEPTION( "CallIntMethodV failed" );

                return result;
            }

            LocalReference< jobject > createDirectByteBuffer(
                SAA_in  const void*                         address,
                SAA_in  const jlong                         capacity
                ) const
            {
                auto localReference = LocalReference< jobject >::attach(
                    m_jniEnv -> NewDirectByteBuffer( const_cast< void* >( address ), capacity )
                    );

                CHECK_JAVA_EXCEPTION( "NewDirectByteBuffer failed" );

                ( void ) callObjectMethod< jobject >(
                    localReference.get(),
                    g_byteBufferOrder,
                    g_nativeByteOrder
                    );

                return localReference;
            }

            jint getByteBufferCapacity( SAA_in const jobject byteBuffer ) const
            {
                return callIntMethod(
                    byteBuffer,
                    g_byteBufferGetCapacity
                    );
            }

            jint getByteBufferPosition( SAA_in const jobject byteBuffer ) const
            {
                return callIntMethod(
                    byteBuffer,
                    g_byteBufferGetPosition
                    );
            }

            void setByteBufferPosition(
                SAA_in  const jobject                       byteBuffer,
                SAA_in  const jint                          position
                ) const
            {
                ( void ) callObjectMethod< jobject >(
                    byteBuffer,
                    g_byteBufferSetPosition,
                    position
                    );
            }

            void setByteBufferLimit(
                SAA_in  const jobject                       byteBuffer,
                SAA_in  const jint                          limit
                ) const
            {
                ( void ) callObjectMethod< jobject >(
                    byteBuffer,
                    g_byteBufferSetLimit,
                    limit
                    );
            }

            jint getByteBufferLimit( SAA_in const jobject byteBuffer ) const
            {
                return callIntMethod(
                    byteBuffer,
                    g_byteBufferGetLimit
                    );
            }

            void clearByteBuffer( SAA_in const jobject byteBuffer ) const
            {
                ( void ) callObjectMethod< jobject >(
                    byteBuffer,
                    g_byteBufferClear
                    );
            }

            void flipByteBuffer( SAA_in const jobject byteBuffer ) const
            {
                ( void ) callObjectMethod< jobject >(
                    byteBuffer,
                    g_byteBufferFlip
                    );
            }

            bool isDirectByteBuffer( SAA_in const jobject byteBuffer ) const
            {
                const jboolean result = callBooleanMethod(
                    byteBuffer,
                    g_byteBufferIsDirect
                    );

                return result != 0;
            }

            LocalReference< jbyteArray > getByteBufferArray( SAA_in const jobject byteBuffer ) const
            {
                return callObjectMethod< jbyteArray >(
                    byteBuffer,
                    g_byteBufferArray
                    );
            }

            jsize getArrayLength( SAA_in const jarray array ) const
            {
                const auto size = m_jniEnv -> GetArrayLength( array );

                CHECK_JAVA_EXCEPTION( "GetArrayLength failed" );

                return size;
            }

            jbyte* getByteArrayElements( SAA_in const jbyteArray array ) const
            {
                jboolean isCopy;

                jbyte *address = m_jniEnv -> GetByteArrayElements( array, &isCopy );

                CHECK_JAVA_EXCEPTION( "GetByteArrayElements failed" );

                return address;
            }

            /*
             * The mode flag:
             * 0           Update the data on the Java heap. Free the space used by the copy.
             * JNI_COMMIT  Update the data on the Java heap. Do not free the space used by the copy.
             * JNI_ABORT   Do not update the data on the Java heap. Free the space used by the copy.
             *
             * https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Release_PrimitiveType_ArrayElements_routines
             * The mode flag is used to avoid unnecessary copying to the Java heap when working with a copied array.
             * The mode flag is ignored if you are working with an array that has been pinned.
             *
             * https://www.ibm.com/support/knowledgecenter/en/SSB23S_1.1.0.14/com.ibm.java.lnx.80.doc/diag/understanding/jni_mode_flag.html
             *
             */

            void releaseByteArrayElements(
                SAA_in  const jbyteArray                    array,
                SAA_in  jbyte*                              elems,
                SAA_in  const jint                          mode = 0
                ) const
            {
                m_jniEnv -> ReleaseByteArrayElements( array, elems, mode );

                CHECK_JAVA_EXCEPTION( "ReleaseByteArrayElements failed" );
            }

            template
            <
                typename T
            >
            LocalReference< T > getObjectArrayElement(
                SAA_in  const jobjectArray                  array,
                SAA_in  const jsize                         index
                ) const
            {
                auto localReference = LocalReference< T >::attach(
                    static_cast< T >( m_jniEnv -> GetObjectArrayElement( array, index ) )
                    );

                CHECK_JAVA_EXCEPTION( "GetObjectArrayElement failed" );

                return localReference;
            }
        };

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT,
            os::thread_specific_ptr< typename JniEnvironmentT< TCLASS >::this_type >,           g_tlsDataJni );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, std::atomic< int64_t >,                       g_jniThreadCount );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, JavaVM*,                                      g_javaVM );
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, bool,                                         g_exceptionHandlingBootstrapped ) = false;

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_classGetName ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_objectToString ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_objectGetClass ) = nullptr;

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jclass,                                       g_throwableClass ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_throwableGetMessage ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_throwableGetCause ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_throwableGetStackTrace ) = nullptr;

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jclass,                                       g_threadClass ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_threadCurrentThread ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_threadGetName ) = nullptr;

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferGetCapacity ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferGetPosition ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferSetPosition ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferGetLimit ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferSetLimit ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferClear ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferFlip ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferOrder ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferIsDirect ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jmethodID,                                    g_byteBufferArray ) = nullptr;

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, jobject,                                      g_nativeByteOrder ) = nullptr;

        typedef JniEnvironmentT<> JniEnvironment;

    } // jni

} // bl

#undef CHECK_JAVA_EXCEPTION

#endif /* __BL_JNI_JNIENVIRONMENT_H_ */
