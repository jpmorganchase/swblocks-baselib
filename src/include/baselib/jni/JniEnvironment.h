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

#include <baselib/jni/JavaVirtualMachine.h>
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

            static cpp::ScalarTypeIniter< bool >                g_exceptionHandlingBootstrapped;

            JNIEnv*                                             m_jniEnv;

            static cpp::ScalarTypeIniter< jmethodID >           g_classGetName;
            static cpp::ScalarTypeIniter< jmethodID >           g_objectToString;

            static GlobalReference< jclass >                    g_throwableClass;
            static cpp::ScalarTypeIniter< jmethodID >           g_throwableGetCause;
            static cpp::ScalarTypeIniter< jmethodID >           g_throwableGetStackTrace;

            static GlobalReference< jclass >                    g_threadClass;
            static cpp::ScalarTypeIniter< jmethodID >           g_threadCurrentThread;
            static cpp::ScalarTypeIniter< jmethodID >           g_threadGetName;

            mutable cpp::ScalarTypeIniter< bool >               m_processingException;

            JniEnvironmentT()
            {
                JNIEnv* jniEnv;

                const jint jniErrorCode = JavaVirtualMachine::instance().getJavaVM() -> AttachCurrentThread(
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

                    const auto buffer = MessageBuffer();

                    buffer << cbMessage();

                    if( g_exceptionHandlingBootstrapped )
                    {
                        buffer
                            << '\n'
                            << getExceptionStackTrace( exception );
                    }

                    BL_THROW( JavaException(), buffer );
                }
            }

            std::string getExceptionStackTrace( SAA_in const LocalReference< jthrowable >& exception ) const
            {
                const auto currentThread = callStaticObjectMethod< jobjectArray >(
                    g_threadClass.get(),
                    g_threadCurrentThread
                    );

                const auto threadName = callObjectMethod< jstring >(
                    currentThread.get(),
                    g_threadGetName
                    );

                cpp::SafeOutputStringStream oss;

                oss
                    << "Exception in thread \""
                    << javaStringToCString( threadName )
                    << "\" "
                    ;

                appendStackTraceElements( oss, exception );

                return oss.str();
            }

            void appendStackTraceElements(
                SAA_in  cpp::SafeOutputStringStream&                oss,
                SAA_in  const LocalReference< jthrowable >&         exception,
                SAA_in  const int                                   level = 0
                ) const
            {
                if ( level > 0 )
                {
                    oss << "\nCaused by: ";
                }

                const auto exceptionAsString = callObjectMethod< jstring >(
                    exception.get(),
                    g_objectToString
                    );

                oss << javaStringToCString( exceptionAsString );

                const auto stackTraceElements = callObjectMethod< jobjectArray >(
                    exception.get(),
                    g_throwableGetStackTrace
                    );

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

                if( stackTraceElements.get() )
                {
                    const auto cause = callObjectMethod< jthrowable >( exception.get(), g_throwableGetCause );

                    if ( cause.get() )
                    {
                        appendStackTraceElements( oss, cause, level + 1 );
                    }
                }
            }

        public:

            ~JniEnvironmentT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                const jint jniErrorCode = JavaVirtualMachine::instance().getJavaVM() -> DetachCurrentThread();

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

            jsize getArrayLength( SAA_in const jarray array ) const
            {
                const auto size = m_jniEnv -> GetArrayLength( array );

                CHECK_JAVA_EXCEPTION( "GetArrayLength failed" );

                return size;
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

            void initializeStaticData()
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

                g_throwableClass = createGlobalReference< jclass >( findJavaClass( "java/lang/Throwable" ) );

                g_throwableGetCause = getMethodID(
                    g_throwableClass.get(),
                    "getCause",
                    "()Ljava/lang/Throwable;"
                    );

                g_throwableGetStackTrace = getMethodID(
                    g_throwableClass.get(),
                    "getStackTrace",
                    "()[Ljava/lang/StackTraceElement;"
                    );

                g_threadClass = createGlobalReference< jclass >( findJavaClass( "java/lang/Thread" ) );

                g_threadCurrentThread = getStaticMethodID(
                    g_threadClass.get(),
                    "currentThread",
                    "()Ljava/lang/Thread;"
                    );

                g_threadGetName = getMethodID(
                    g_threadClass.get(),
                    "getName",
                    "()Ljava/lang/String;"
                    );

                g_exceptionHandlingBootstrapped = true;
            }

            void clearStaticData()
            {
                g_throwableClass.reset();
                g_threadClass.reset();
            }
        };

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT,
            os::thread_specific_ptr< typename JniEnvironmentT< TCLASS >::this_type >,           g_tlsDataJni );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, std::atomic< int64_t >,                       g_jniThreadCount );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< bool >,                g_exceptionHandlingBootstrapped );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< jmethodID >,           g_classGetName );
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< jmethodID >,           g_objectToString );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, GlobalReference< jclass >,                    g_throwableClass );
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< jmethodID >,           g_throwableGetCause );
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< jmethodID >,           g_throwableGetStackTrace );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, GlobalReference< jclass >,                    g_threadClass );
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< jmethodID >,           g_threadCurrentThread );
        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, cpp::ScalarTypeIniter< jmethodID >,           g_threadGetName );

        typedef JniEnvironmentT<> JniEnvironment;

    } // jni

} // bl

#undef CHECK_JAVA_EXCEPTION

#endif /* __BL_JNI_JNIENVIRONMENT_H_ */
