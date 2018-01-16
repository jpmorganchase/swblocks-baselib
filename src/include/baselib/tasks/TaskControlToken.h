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

#ifndef __BL_TASKS_TASKCONTROLTOKEN_H_
#define __BL_TASKS_TASKCONTROLTOKEN_H_

#include <baselib/tasks/Task.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( TaskControlToken, "ab0d9f16-f176-422f-8134-d8e0f36ff050" )
BL_IID_DECLARE( TaskControlTokenRW, "2a29ca56-1565-4206-a4aa-26f91a868f41" )

namespace bl
{
    namespace tasks
    {
        /**
         * @brief interface TaskControlToken
         */

        class TaskControlToken : public om::Object
        {
            BL_DECLARE_INTERFACE( TaskControlToken )

        public:

            virtual bool isCanceled() const NOEXCEPT = 0;
        };

        /**
         * @brief interface TaskControlTokenRW
         */

        class TaskControlTokenRW : public TaskControlToken
        {
            BL_DECLARE_INTERFACE( TaskControlTokenRW )

        public:

            virtual void requestCancel() = 0;

            virtual void registerCancelableTask( SAA_in om::ObjPtrCopyable< Task >&& task ) = 0;

            virtual void unregisterCancelableTask( SAA_in om::ObjPtrCopyable< Task >&& task ) = 0;
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_TASKCONTROLTOKEN_H_ */
