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

            static os::mutex                                    g_lock;
            static cpp::ScalarTypeIniter< bool >                g_staticDataInitialized;
            static jclass                                       g_javaClass;
            static jmethodID                                    g_newInstance;
            static jmethodID                                    g_dispatch;

            LocalReference< jobject >                           m_instance;

            static void initializeStaticData( SAA_in const std::string& javaClassName )
            {
                BL_MUTEX_GUARD( g_lock );

                if( ! g_staticDataInitialized )
                {
                    const auto& environment = JniEnvironment::instance();

                    auto javaClass = environment.createGlobalReference< jclass >(
                        environment.findJavaClass( javaClassName )
                        );

                    g_javaClass = javaClass.get();
                    javaClass.release();

                    g_newInstance = environment.getStaticMethodID(
                        g_javaClass,
                        "newInstance",
                        "()L" + javaClassName + ";"
                        );

                    g_dispatch = environment.getMethodID(
                        g_javaClass,
                        "dispatch",
                        "(Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V"
                        );

                    g_staticDataInitialized = true;
                }
            }

        public:

            JavaBridgeT( SAA_in const std::string& javaClassName )
            {
                initializeStaticData( javaClassName );

                m_instance = JniEnvironment::instance().callStaticObjectMethod< jobject >(
                    g_javaClass,
                    g_newInstance
                    );
            }

            void dispatch(
                SAA_in  const DirectByteBuffer&                 inBuffer,
                SAA_out DirectByteBuffer&                       outBuffer
                ) const
            {
                const auto& environment = JniEnvironment::instance();

                inBuffer.prepareForJavaRead();
                outBuffer.prepareForJavaWrite();

                environment.callVoidMethod(
                    m_instance.get(),
                    g_dispatch,
                    inBuffer.getJavaBuffer().get(),
                    outBuffer.getJavaBuffer().get()
                    );

                outBuffer.prepareForRead();
            }
        };

        BL_DEFINE_STATIC_MEMBER( JavaBridgeT, os::mutex,                        g_lock );
        BL_DEFINE_STATIC_MEMBER( JavaBridgeT, cpp::ScalarTypeIniter< bool >,    g_staticDataInitialized );
        BL_DEFINE_STATIC_MEMBER( JavaBridgeT, jclass,                           g_javaClass ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JavaBridgeT, jmethodID,                        g_newInstance ) = nullptr;
        BL_DEFINE_STATIC_MEMBER( JavaBridgeT, jmethodID,                        g_dispatch ) = nullptr;

        typedef JavaBridgeT<> JavaBridge;

    } // jni

} // bl

#endif /* __BL_JNI_JAVABRIDGE_H_ */
