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

#ifndef __BL_ASYNC_ASYNCOPERATIONSTATE_H_
#define __BL_ASYNC_ASYNCOPERATIONSTATE_H_

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskControlToken.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( AsyncOperationState, "f7682235-2e4f-431c-847c-6001593b6179" )

namespace bl
{
    /**
     * @brief interface AsyncOperationState - The async operation state
     * base interface
     *
     * This interface is to be implemented by the async executor client
     * code which will create async operation objects
     *
     * The createTask() API allows for the async operation to be modeled
     * as a task
     *
     * If createTask() returns nullptr that means the execution will
     * be sync and will happen happen inside the execute() call, but if
     * it returns non-nullptr then the returned task will be executed
     * instead of the execute() method being called
     *
     * The task returned by createTask() can be a 'composite task'
     * (usually derived from tasks::WrapperTaskBase) which can
     * execute a complex task tree which e.g. consists of both
     * blocking and non-blocking individual tasks which will then
     * of course be executed on the respective thread pools, etc
     */

    class AsyncOperationState : public om::Object
    {
        BL_DECLARE_INTERFACE( AsyncOperationState )

    public:

        typedef cpp::function< om::ObjPtr< tasks::Task > () > create_task_callback_t;

        template
        <
            typename T
        >
        const T* getAsConst() NOEXCEPT
        {
            return reinterpret_cast< const T* >( get() );
        }

        template
        <
            typename T
        >
        T* getAs() NOEXCEPT
        {
            return reinterpret_cast< T* >( get() );
        }

        virtual void execute() = 0;
        virtual om::ObjPtr< tasks::Task > createTask() = 0;
        virtual auto controlToken() NOEXCEPT -> const om::ObjPtr< tasks::TaskControlToken >& = 0;
        virtual void releaseResources() NOEXCEPT = 0;
        virtual void* get() NOEXCEPT = 0;
    };

} // bl

#endif /* __BL_ASYNC_ASYNCOPERATIONSTATE_H_ */
