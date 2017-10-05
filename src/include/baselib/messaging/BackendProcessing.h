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

#ifndef __BL_MESSAGING_BACKENDPROCESSING_H_
#define __BL_MESSAGING_BACKENDPROCESSING_H_

#include <baselib/tasks/Task.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( BackendProcessing, "35d46382-2d66-4e2f-908f-62b8830c5750" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The core messaging backend processing interface
         */

        class BackendProcessing : public om::Disposable
        {
            BL_DECLARE_INTERFACE( BackendProcessing )

        public:

            enum class CommandId : std::uint16_t
            {
                None = 0,
                FlushPeerSessions,
                Remove,
            };

            enum class OperationId : std::uint16_t
            {
                None = 0,

                Alloc,
                SecureAlloc,
                SecureDiscard,
                AuthenticateClient,
                GetServerState,

                Get,
                Put,
                Command,
            };

            virtual bool autoBlockDispatching() const NOEXCEPT = 0;

            virtual void setHostServices( SAA_in om::ObjPtr< om::Proxy >&& hostServices ) NOEXCEPT = 0;

            virtual auto createBackendProcessingTask(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const CommandId                                 commandId,
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in_opt              const uuid_t&                                   sourcePeerId,
                SAA_in_opt              const uuid_t&                                   targetPeerId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                )
                -> om::ObjPtr< tasks::Task > = 0;

            virtual bool isConnected() const NOEXCEPT = 0;
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BACKENDPROCESSING_H_ */
