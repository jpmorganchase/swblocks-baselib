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

#ifndef __BL_JNI_JAVABRIDGERESTHELPER_H_
#define __BL_JNI_JAVABRIDGERESTHELPER_H_

#include <baselib/data/models/Functions.h>

#include <baselib/jni/DirectByteBuffer.h>
#include <baselib/jni/JavaBridge.h>

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
        class JavaBridgeRestHelperT : public om::ObjectDefaultBase
        {
        private:

            static const std::string                                                    g_shutdownJson;

            jni::JavaBridge                                                             m_bridge;

        protected:

            JavaBridgeRestHelperT(
                SAA_in          const jni::JavaBridge::callback_t&                      callback,
                SAA_in          const std::string&                                      javaClass,
                SAA_in          const std::string&                                      javaCallbackName
                )
                :
                m_bridge( javaClass, javaCallbackName, callback )
            {
            }

        public:

            void execute(
                SAA_in      const om::ObjPtr< dm::FunctionContext >&                    context,
                SAA_in      const std::string&                                          request,
                SAA_out     const om::ObjPtr< data::DataBlock >&                        resultBlock
                )
            {
                const auto contextAsString =
                    dm::DataModelUtils::getDocAsPackedJsonString( context );

                const jni::DirectByteBuffer input(
                    2 * sizeof( std::int32_t ) + request.size() + contextAsString.size()
                    );

                input.prepareForWrite();

                input.getBuffer() -> write( request );
                input.getBuffer() -> write( contextAsString );

                jni::DirectByteBuffer output( om::copy( resultBlock ) );

                m_bridge.dispatch( input, output );
            }

            void shutdown()
            {
                const jni::DirectByteBuffer input( sizeof( std::int32_t ) + g_shutdownJson.size() );
                input.prepareForWrite();

                input.getBuffer() -> write( g_shutdownJson );

                jni::DirectByteBuffer output( 0L );

                m_bridge.dispatch( input, output );
            }
        };

        BL_DEFINE_STATIC_CONST_STRING( JavaBridgeRestHelperT, g_shutdownJson ) = "{\"shutdown\": true}";

        typedef om::ObjectImpl< JavaBridgeRestHelperT<> > JavaBridgeRestHelper;

    } // jni

} // bl

#endif /* __BL_JNI_JAVABRIDGERESTHELPER_H_ */
