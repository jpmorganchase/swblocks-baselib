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

#ifndef __BL_ASYNC_ASYNCEXECUTOR_H_
#define __BL_ASYNC_ASYNCEXECUTOR_H_

#include <baselib/async/AsyncOperationState.h>
#include <baselib/async/AsyncOperation.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( AsyncExecutor, "25259cc0-cba2-4bb4-b192-d71b461c28d8" )

namespace bl
{
    /**
     * @brief interface AsyncExecutor - Async interface for sync data processing
     *
     * It provides an async interface for one or more  operations which need to be
     * executed synchronously
     *
     * It provides an interface which allows for controlling resource usage in a server
     * application which needs to ensure an upper bound of operations that are in the
     * process of execution
     *
     * Async operations are assumed to be 'inexpensive' (i.e. they have not allocated scarce
     * resources) until they start executing, then they transition to 'expensive' as they
     * begin executing and once they're released they again transition into 'inexpensive'
     *
     * The above allows the implementation to ensure that the # of executing operations which
     * have not been released on a server is always bounded (otherwise the server will OOM or
     * hit some other resource exhaustion problem as the number of clients keep increasing)
     *
     * Note also that when we say executing operations above we mean 'logically executing'
     * (i.e. these are the operation which are 'in-progress'), but not the 'physically'
     * executing operations which is limited by the size of the thread pool and the # of
     * cores on the machine (whichever is smaller)
     *
     * You can see the semantics of releaseOperation below, but the main idea to provide it
     * is to enable object pooling (where possible) as a performance enhancement and its use
     * is generally not required (although recommended) -- i.e. you can of course release the
     * created operation objects by simply releasing the reference to the object as you would
     * do with any other object (which of course normally happens automatically in 99% of
     * the cases via om::ObjPtr)
     */

    class AsyncExecutor : public om::Disposable
    {
        BL_DECLARE_INTERFACE( AsyncExecutor )

    public:

        auto createOperation( SAA_in const om::ObjPtr< AsyncOperationState >& operationState )
            NOEXCEPT -> om::ObjPtr< AsyncOperation >
        {
            return createOperation( om::copy( operationState ) );
        }

        virtual auto createOperation( SAA_in om::ObjPtr< AsyncOperationState >&& operationState )
            NOEXCEPT -> om::ObjPtr< AsyncOperation > = 0;

        /**
         * @brief Returns an async operation for reuse
         *
         * This API can only be called on async operations which are already canceled or the
         * callback was executed or currently in the process of been executed
         *
         * Note that it can be called from within the callback itself and in general from any
         * point from which the async* APIs below can be called
         *
         * Note also that of course after this API is called the async operation should never
         * be used anymore, so do not make copies of it which can cause this kind of situation
         * (this is also the reason the API takes reference to the object, so it can be cleared)
         *
         * If you need to make copies and release the async operation independently or from
         * multiple locations / threads then don't call this API, but simply release the object
         * reference as you would do on any object
         */

        virtual void releaseOperation( SAA_in om::ObjPtr< AsyncOperation >& operation ) NOEXCEPT = 0;

        virtual void asyncBegin(
            SAA_in                  const om::ObjPtr< AsyncOperation >&    operation,
            SAA_in                  AsyncOperation::callback_t&&           callback
            ) = 0;
    };

} // bl

#endif /* __BL_ASYNC_ASYNCEXECUTOR_H_ */
