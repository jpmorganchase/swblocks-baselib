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

        private:

            typedef cpp::function< void( SAA_in const DirectByteBuffer& ) NOEXCEPT >     callback_t;

            GlobalReference< jclass >                           m_javaClass;
            jmethodID                                           m_getInstance;
            jmethodID                                           m_dispatch;

            GlobalReference< jobject >                          m_instance;

            void prepareJavaClassData( SAA_in const std::string& javaClassName )
            {
                const auto& environment = JniEnvironment::instance();

                m_javaClass = environment.createGlobalReference< jclass >(
                    environment.findJavaClass( javaClassName )
                    );

                m_getInstance = environment.getStaticMethodID(
                    m_javaClass.get(),
                    "getInstance",
                    "()L" + javaClassName + ";"
                    );

                m_dispatch = environment.getMethodID(
                    m_javaClass.get(),
                    "dispatch",
                    "(Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V"
                    );
            }

        public:

            JavaBridgeT( SAA_in const std::string& javaClassName )
            {
                prepareJavaClassData( javaClassName );

                const auto& environment = JniEnvironment::instance();

                m_instance = environment.createGlobalReference< jobject >(
                    environment.callStaticObjectMethod< jobject >(
                        m_javaClass.get(),
                        m_getInstance
                        )
                    );
            }

            JavaBridgeT(
                SAA_in  const std::string&                      javaClassName,
                SAA_in  const std::string&                      javaClassNativeCallbackName
                )
                : JavaBridgeT( javaClassName )
            {
                registerCallback( javaClassNativeCallbackName );
            }

            void dispatch(
                SAA_in  const DirectByteBuffer&                 inBuffer,
                SAA_in  const DirectByteBuffer&                 outBuffer
                ) const
            {
                inBuffer.prepareForJavaRead();

                outBuffer.prepareForWrite();
                outBuffer.prepareForJavaWrite();

                JniEnvironment::instance().callVoidMethod(
                    m_instance.get(),
                    m_dispatch,
                    inBuffer.getJavaBuffer().get(),
                    outBuffer.getJavaBuffer().get()
                    );

                outBuffer.prepareForRead();
            }

            void dispatch(
                SAA_in  const DirectByteBuffer&                 inBuffer,
                SAA_in  const DirectByteBuffer&                 outBuffer,
                SAA_in  const callback_t&                       callback
                ) const
            {
                inBuffer.prepareForJavaRead();

                outBuffer.prepareForWrite();

                const auto& outRawBuffer = outBuffer.getBuffer();

                std::uint64_t outBufferAddress = reinterpret_cast< std::uint64_t >( &outBuffer );
                outRawBuffer -> write( outBufferAddress );

                std::uint64_t callbackAddress = reinterpret_cast< std::uint64_t >( &callback );
                outRawBuffer -> write( callbackAddress );

                outBuffer.prepareForJavaWrite();

                const auto callbackPrefixSize = outRawBuffer -> size();

                JniEnvironment::instance().callVoidMethod(
                    m_instance.get(),
                    m_dispatch,
                    inBuffer.getJavaBuffer().get(),
                    outBuffer.getJavaBuffer().get()
                    );

                outBuffer.prepareForRead( callbackPrefixSize );
            }

            static void javaCallback(
                SAA_in  JNIEnv*                                 jniEnv,
                SAA_in  jclass                                  javaClass,
                SAA_in  jobject                                 byteBuffer
                ) NOEXCEPT
            {
                BL_UNUSED( javaClass );

                BL_NOEXCEPT_BEGIN()

                const char* dataAddress = static_cast< const char* >( jniEnv -> GetDirectBufferAddress( byteBuffer ) );

                if( dataAddress == nullptr )
                {
                    BL_RIP_MSG( "GetDirectBufferAddress returned nullptr in the JNI callback" );
                }

                std::uint64_t outBufferAddress;
                std::memcpy( &outBufferAddress, dataAddress, sizeof( outBufferAddress ) );

                const DirectByteBuffer& outBuffer = *reinterpret_cast< DirectByteBuffer* >( outBufferAddress );

                outBuffer.prepareForRead();

                const auto& outRawBuffer = outBuffer.getBuffer();

                /*
                 * Skip output buffer address which just read above
                 */

                outRawBuffer -> read( &outBufferAddress );

                std::uint64_t callbackAddress;
                outRawBuffer -> read( &callbackAddress );

                const auto callbackPrefixSize = outRawBuffer -> offset1();

                const callback_t& callback = *reinterpret_cast< callback_t* >( callbackAddress );

                callback( outBuffer );

                outBuffer.prepareForWrite( callbackPrefixSize );

                outBuffer.prepareForJavaWrite();

                BL_NOEXCEPT_END()
            }

            void registerCallback( SAA_in const std::string& javaClassNativeCallbackName )
            {
                JNIEnv* jniEnv = JniEnvironment::instance().getRawPtr();

                JNINativeMethod nativeCb =
                {
                     const_cast< char* >( javaClassNativeCallbackName.c_str() ),
                     const_cast< char* >( "(Ljava/nio/ByteBuffer;)V" ),
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
