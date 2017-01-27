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

#ifndef __BL_TASKS_EXECUTIONQUEUENOTIFYBASE_H_
#define __BL_TASKS_EXECUTIONQUEUENOTIFYBASE_H_

#include <baselib/tasks/ExecutionQueueNotify.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class ExecutionQueueNotifyBase
         */
        template
        <
            typename E = void
        >
        class ExecutionQueueNotifyBaseT :
            public ExecutionQueueNotify
        {
            BL_CTR_DEFAULT( ExecutionQueueNotifyBaseT, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( ExecutionQueueNotifyBaseT, ExecutionQueueNotify )

        public:

            virtual std::size_t maxReadyOrExecuting() const NOEXCEPT OVERRIDE
            {
                return 0U;
            }

            virtual void onEvent(
                SAA_in                      const EventId                           eventId,
                SAA_in_opt                  const om::ObjPtrCopyable< Task >&       task
                ) NOEXCEPT OVERRIDE
            {
                BL_UNUSED( eventId );
                BL_UNUSED( task );
            }
        };

        typedef ExecutionQueueNotifyBaseT<> ExecutionQueueNotifyBase;

    } // tasks

} // bl

#endif /* __BL_EXECUTIONQUEUENOTIFYBASE_H_ */
