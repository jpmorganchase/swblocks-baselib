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

#ifndef __BL_TASKS_EXECUTIONQUEUENOTIFY_H_
#define __BL_TASKS_EXECUTIONQUEUENOTIFY_H_

#include <baselib/tasks/Task.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( ExecutionQueueNotify, "1eb49d31-8278-425f-a8ae-11755aac9a71" )

namespace bl
{
    namespace tasks
    {
        /**
         * @brief ExecutionQueueNotify interface
         */

        class ExecutionQueueNotify : public om::Object
        {
            BL_DECLARE_INTERFACE( ExecutionQueueNotify )

        public:

            enum EventId
            {
                TaskReady           =   0x0001,
                TaskDiscarded       =   0x0002,
                AllTasksCompleted   =   0x0004,
            };

            enum
            {
                AllEvents = TaskReady | TaskDiscarded | AllTasksCompleted,
            };

            /**
             * @brief returns the maximum number of ready and executing tasks
             *
             * If zero is returned that means there is no limit
             *
             * We're not going to schedule more tasks until some of these are
             * processed and removed from the queue.
             */

            virtual std::size_t maxReadyOrExecuting() const NOEXCEPT = 0;

            /**
             * @brief Notify the callback that an event have occurred in the execution queue
             *
             * Note: The task parameter can be nullptr depending on the eventId (e.g. if eventId=AllTasksCompleted)
             */

            virtual void onEvent(
                SAA_in                      const EventId                           eventId,
                SAA_in_opt                  const om::ObjPtrCopyable< Task >&       task
                ) NOEXCEPT = 0;
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_EXECUTIONQUEUENOTIFY_H_ */
