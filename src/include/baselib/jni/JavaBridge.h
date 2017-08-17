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

            GlobalReference< jclass >                           m_javaClass;
            jmethodID                                           m_getInstance;
            jmethodID                                           m_dispatch;

            LocalReference< jobject >                           m_instance;

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

                m_instance = JniEnvironment::instance().callStaticObjectMethod< jobject >(
                    m_javaClass.get(),
                    m_getInstance
                    );
            }

            void dispatch(
                SAA_in  const DirectByteBuffer&                 inBuffer,
                SAA_in  const DirectByteBuffer&                 outBuffer
                ) const
            {
                inBuffer.prepareForJavaRead();
                outBuffer.prepareForJavaWrite();

                JniEnvironment::instance().callVoidMethod(
                    m_instance.get(),
                    m_dispatch,
                    inBuffer.getJavaBuffer().get(),
                    outBuffer.getJavaBuffer().get()
                    );

                outBuffer.prepareForRead();
            }
        };

        typedef JavaBridgeT<> JavaBridge;

    } // jni

} // bl

#endif /* __BL_JNI_JAVABRIDGE_H_ */
