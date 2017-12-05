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

#ifndef __BL_MESSAGING_MESSAGEBLOCKCOMPLETIONQUEUE_H_
#define __BL_MESSAGING_MESSAGEBLOCKCOMPLETIONQUEUE_H_

#include <baselib/messaging/BrokerErrorCodes.h>

#include <baselib/tasks/TaskBase.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( MessageBlockCompletionQueue, "4c2aa12b-1ab9-434c-a6f0-aacff41f1d8b" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief class MessageBlockCompletionQueue - a message block completion queue interface
         */

        class MessageBlockCompletionQueue : public om::Object
        {
            BL_DECLARE_INTERFACE( MessageBlockCompletionQueue )

        public:

            typedef tasks::CompletionCallback                                               CompletionCallback;

            virtual void requestHeartbeat() = 0;

            virtual bool tryScheduleBlock(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  om::ObjPtr< data::DataBlock >&&                     dataBlock,
                SAA_in                  CompletionCallback&&                                callback
                ) = 0;

            void scheduleBlock(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  om::ObjPtr< data::DataBlock >&&                     dataBlock,
                SAA_in                  CompletionCallback&&                                callback
                )
            {
                BL_CHK_SERVER_ERROR(
                    false,
                    tryScheduleBlock(
                        targetPeerId,
                        BL_PARAM_FWD( dataBlock ),
                        BL_PARAM_FWD( callback )
                        ),
                    BrokerErrorCodes::TargetPeerQueueFull,
                    BL_MSG()
                        << "Messaging client ring buffer queue is full"
                    );
            }
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGEBLOCKCOMPLETIONQUEUE_H_ */
