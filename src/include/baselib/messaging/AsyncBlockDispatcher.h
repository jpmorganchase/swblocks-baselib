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

#ifndef __BL_MESSAGING_ASYNCBLOCKDISPATCHER_H_
#define __BL_MESSAGING_ASYNCBLOCKDISPATCHER_H_

#include <baselib/messaging/MessageBlockCompletionQueue.h>

#include <baselib/tasks/Task.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( AsyncBlockDispatcher, "b1c999b1-649c-49d2-8ae3-96085a6da922" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief class AsyncBlockDispatcher - an async block dispatcher interface
         */

        class AsyncBlockDispatcher : public om::Object
        {
            BL_DECLARE_INTERFACE( AsyncBlockDispatcher )

        public:

            virtual auto getAllActiveQueuesIds() -> std::unordered_set< uuid_t > = 0;

            virtual auto tryGetMessageBlockCompletionQueue( SAA_in const uuid_t& targetPeerId )
                -> om::ObjPtr< MessageBlockCompletionQueue > = 0;

            virtual auto createDispatchTask(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&                data
                )
                -> om::ObjPtr< tasks::Task > = 0;
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_ASYNCBLOCKDISPATCHER_H_ */
