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

#ifndef __BL_JNI_JAVABRIDGE_H_
#define __BL_JNI_JAVABRIDGE_H_

#include <baselib/jni/JniEnvironment.h>
#include <baselib/jni/DirectByteBuffer.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        template
        <
            typename E = void
        >
        class JavaBridgeT
        {
            BL_NO_COPY_OR_MOVE( JavaBridgeT )

        public:

             typedef cpp::function< void(
                SAA_in  const DirectByteBuffer&,
                SAA_out DirectByteBuffer&
                ) >                                             callback_t;

        private:

            GlobalReference< jclass >                           m_javaClass;
            jmethodID                                           m_getInstance;
            jmethodID                                           m_dispatch;

            std::string                                         m_javaClassName;
            std::string                                         m_javaCallbackName;

            GlobalReference< jobject >                          m_instance;

            callback_t                                          m_callback;

            void prepareJavaClassData()
            {
                const auto& environment = JniEnvironment::instance();

                m_javaClass = environment.createGlobalReference< jclass >(
                    environment.findJavaClass( m_javaClassName )
                    );

                m_getInstance = environment.getStaticMethodID(
                    m_javaClass.get(),
                    "getInstance",
                    "()L" + m_javaClassName + ";"
                    );

                m_dispatch = environment.getMethodID(
                    m_javaClass.get(),
                    "dispatch",
                    m_javaCallbackName.empty()
                        ? "(Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V"
                        : "(Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;J)V"
                    );

                m_instance = environment.createGlobalReference< jobject >(
                    environment.callStaticObjectMethod< jobject >(
                        m_javaClass.get(),
                        m_getInstance
                        )
                    );
            }

            static auto dataBlockFromDirectByteBuffer(
                SAA_in  JNIEnv*                                 jniEnv,
                SAA_in  jobject                                 byteBuffer
                ) -> bl::om::ObjPtr< data::DataBlock >
            {
                char* byteBufferAddress = reinterpret_cast< char* >( jniEnv -> GetDirectBufferAddress( byteBuffer ) );

                if( byteBufferAddress == nullptr )
                {
                    BL_RIP_MSG( "GetDirectBufferAddress returned nullptr in the JNI callback" );
                }

                const jlong byteBufferCapacity = jniEnv -> GetDirectBufferCapacity( byteBuffer );

                if( byteBufferCapacity < 0 )
                {
                    BL_RIP_MSG( "GetDirectBufferCapacity failed" );
                }

                bl::om::ObjPtr< data::DataBlock > inDataBlock = bl::data::DataBlock::createInstance(
                    byteBufferAddress,
                    byteBufferCapacity
                );

                return inDataBlock;
            }

            static ByteArrayPtr byteArrayPtrFromIndirectByteBuffer(
                SAA_in  const JniEnvironment&                   environment,
                SAA_in  jobject                                 byteBuffer,
                SAA_in  jint                                    mode
                )
            {
                LocalReference< jbyteArray > byteArray = environment.getByteBufferArray( byteBuffer );

                jbyte* elems = environment.getByteArrayElements( byteArray.get() );

                return ByteArrayPtr::attach(
                    elems,
                    ByteArrayPtrDeleter( std::move( byteArray ), mode )
                    );
            }

        public:

            JavaBridgeT( SAA_in const std::string& javaClassName )
                :
                m_javaClassName( javaClassName )
            {
                prepareJavaClassData();
            }

            JavaBridgeT(
                SAA_in  const std::string&                      javaClassName,
                SAA_in  const std::string&                      javaCallbackName,
                SAA_in  const callback_t&                       callback = callback_t()
                )
                :
                m_javaClassName( javaClassName ),
                m_javaCallbackName( javaCallbackName )
            {
                prepareJavaClassData();

                registerCallback();

                if( callback )
                {
                    m_callback = callback;
                }
            }

            void dispatch(
                SAA_in  const DirectByteBuffer&                 inBuffer,
                SAA_in  const DirectByteBuffer&                 outBuffer
                ) const
            {
                dispatch( inBuffer, outBuffer, m_callback );
            }

            void dispatch(
                SAA_in  const DirectByteBuffer&                 inBuffer,
                SAA_in  const DirectByteBuffer&                 outBuffer,
                SAA_in  const callback_t&                       callback
                ) const
            {
                inBuffer.prepareForJavaRead();

                outBuffer.prepareForWrite();
                outBuffer.prepareForJavaWrite();

                if( ! m_javaCallbackName.empty() )
                {
                    JniEnvironment::instance().callVoidMethod(
                        m_instance.get(),
                        m_dispatch,
                        inBuffer.getJavaBuffer().get(),
                        outBuffer.getJavaBuffer().get(),
                        reinterpret_cast< jlong >( &callback )
                        );
                }
                else
                {
                    JniEnvironment::instance().callVoidMethod(
                        m_instance.get(),
                        m_dispatch,
                        inBuffer.getJavaBuffer().get(),
                        outBuffer.getJavaBuffer().get()
                        );
                }

                outBuffer.prepareForRead();
            }

            static void javaCallback(
                SAA_in  JNIEnv*                                 jniEnv,
                SAA_in  jobject                                 javaObject,
                SAA_in  jobject                                 inJavaBuffer,
                SAA_in  jobject                                 outJavaBuffer,
                SAA_in  jlong                                   callbackAddress
                ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                const auto& environment = JniEnvironment::instance();

                ByteArrayPtr inByteArrayPtr = nullptr;
                ByteArrayPtr outByteArrayPtr = nullptr;

                bl::om::ObjPtr< data::DataBlock > inDataBlock;
                bl::om::ObjPtr< data::DataBlock > outDataBlock;

                if( environment.isDirectByteBuffer( inJavaBuffer ) )
                {
                    inDataBlock = dataBlockFromDirectByteBuffer( jniEnv, inJavaBuffer );
                }
                else
                {
                    inByteArrayPtr = byteArrayPtrFromIndirectByteBuffer( environment, inJavaBuffer, JNI_ABORT );

                    inDataBlock = bl::data::DataBlock::createInstance(
                        reinterpret_cast< char* >( inByteArrayPtr.get() ),
                        bl::numbers::safeCoerceTo< std::size_t >( environment.getByteBufferCapacity( inJavaBuffer ) )
                        );
                }

                const DirectByteBuffer inBuffer( std::move( inDataBlock ), inJavaBuffer );
                inBuffer.prepareForRead();

                if( environment.isDirectByteBuffer( outJavaBuffer ) )
                {
                    outDataBlock = dataBlockFromDirectByteBuffer( jniEnv, outJavaBuffer );
                }
                else
                {
                    outByteArrayPtr = byteArrayPtrFromIndirectByteBuffer( environment, outJavaBuffer, 0 /* mode */ );

                    outDataBlock = bl::data::DataBlock::createInstance(
                        reinterpret_cast< char* >( outByteArrayPtr.get() ),
                        bl::numbers::safeCoerceTo< std::size_t >( environment.getByteBufferCapacity( outJavaBuffer ) )
                        );
                }

                DirectByteBuffer outBuffer( std::move( outDataBlock ), outJavaBuffer );
                outBuffer.prepareForWrite();

                const callback_t& callback = *reinterpret_cast< callback_t* >( callbackAddress );


                std::string exceptionText;
                try
                {
                    callback( inBuffer, outBuffer );
                }
                catch( std::exception& e )
                {
                    exceptionText = e.what();
                }

                outBuffer.prepareForJavaRead();

                if( ! exceptionText.empty() )
                {
                    const auto objectClass = environment.getObjectClass( javaObject );

                    auto javaClassName = environment.getClassName( objectClass.get() );
                    str::replace_all( javaClassName, ".", "/" );

                    const auto exception = environment.findJavaClass( javaClassName + "$JniException" );
                    environment.throwNew( exception.get(), exceptionText.c_str() );
                }

                BL_NOEXCEPT_END()
            }

            void registerCallback()
            {
                JNIEnv* jniEnv = JniEnvironment::instance().getRawPtr();

                JNINativeMethod nativeCb =
                {
                     const_cast< char* >( m_javaCallbackName.c_str() ),
                     const_cast< char* >( "(Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;J)V" ),
                     reinterpret_cast< void* >( javaCallback )
                };

                const jint jniErrorCode = jniEnv -> RegisterNatives(
                    m_javaClass.get(),
                    &nativeCb,
                    1 /* nMethods */ );

                BL_CHK_T(
                    false,
                    jniErrorCode == 0,
                    JavaException(),
                    BL_MSG()
                        << "Call to RegisterNatives failed. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );
            }
        };

        typedef JavaBridgeT<> JavaBridge;

    } // jni

} // bl

#endif /* __BL_JNI_JAVABRIDGE_H_ */
