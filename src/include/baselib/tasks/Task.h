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

#ifndef __BL_TASKS_TASK_H_
#define __BL_TASKS_TASK_H_

#include <baselib/tasks/TasksIncludes.h>

BL_IID_DECLARE( Task, "f1cc17ad-ad75-44e3-95ad-439ce9587472" )

namespace bl
{
    namespace tasks
    {
        /*
         * Forward declarations
         */

        class ExecutionQueue;

        /**
         * @brief The task interface
         */
        class Task : public om::Object
        {
            BL_DECLARE_INTERFACE( Task )

        public:

            enum State
            {
                Created,
                Running,
                PendingCompletion,
                Completed,
            };

            /**
             * @brief Returns true if the task has failed and also not executing
             */

            virtual bool isFailed() const NOEXCEPT = 0;

            /**
             * @brief Returns true if the task has failed, but maybe still executing
             */

            virtual bool isFailedOrFailing() const NOEXCEPT = 0;

            /**
             * @brief Returns the task current state
             */

            virtual Task::State getState() const NOEXCEPT = 0;

            /**
             * @brief Sets the task current state to Task::Completed (e.g. usually it should only be
             * called by the execution queue, or other executors)
             */

            virtual void setCompletedState() NOEXCEPT = 0;

            /**
             * @brief Returns the exception (if the task failed to execute)
             */

            virtual std::exception_ptr exception() const NOEXCEPT = 0;

            /**
             * @brief Sets the exception (e.g. if the task has to be forced to fail externally due
             * to a continuation task failing to be created or scheduled)
             */

            virtual void exception( SAA_in_opt const std::exception_ptr& exception ) NOEXCEPT = 0;

            /**
             * @brief Returns the task name
             *
             * Note tasks may or may not have a meaningful name and if the
             * task doesn't have a name this value can be an empty string
             */

            virtual const std::string& name() const NOEXCEPT = 0;

            /**
             * @brief Schedules the task to commence execution
             *
             * Note that this method is expected to be NOEXCEPT as the task should always be able
             * to start and finish execution via invoking the provided callback - even in the case
             * where it fails to properly start it is still the tasks responsibility to capture
             * the exception and invoke the callback to declare itself ready
             */

            virtual void scheduleNothrow(
                SAA_in                  const std::shared_ptr< ExecutionQueue >&    eq,
                SAA_in                  cpp::void_callback_noexcept_t&&             callbackReady
                ) NOEXCEPT = 0;

            /**
             * @brief Requests the task to be canceled gracefully
             */

            virtual void requestCancel() NOEXCEPT = 0;

            /**
             * @brief This is to support task continuations
             */

            virtual om::ObjPtr< Task > continuationTask() = 0;
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_TASK_H_ */
