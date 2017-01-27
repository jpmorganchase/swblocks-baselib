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

#ifndef __BL_ASYNC_ASYNCEXECUTORWRAPPERCALLBACK_H_
#define __BL_ASYNC_ASYNCEXECUTORWRAPPERCALLBACK_H_

#include <baselib/async/AsyncExecutorWrapperBase.h>

namespace bl
{
    /**
     * @brief class AsyncExecutorWrapperCallback - an implementation of async call on
     * top of a simple callback
     *
     * For more information see the description of AsyncExecutorWrapperBase class
     */

    template
    <
        typename E = void
    >
    class AsyncExecutorWrapperCallbackT : public AsyncExecutorWrapperBase< AsyncSharedStateBaseDefaultImpl >
    {
    public:

        typedef AsyncSharedStateBaseDefaultImpl                                             shared_state_t;
        typedef AsyncExecutorWrapperBase< shared_state_t >                                  base_type;
        typedef typename base_type::AsyncOperationStateBase                                 AsyncOperationStateBase;
        typedef AsyncOperationState::create_task_callback_t                                 create_task_callback_t;

        /**
         * @brief class AsyncOperationState - this object holds the state for an async operation that
         * is already in progress
         */

        template
        <
            typename E2 = void
        >
        class AsyncOperationStateImplT : public AsyncOperationStateBase
        {
            BL_CTR_DEFAULT( AsyncOperationStateImplT, protected )
            BL_DECLARE_OBJECT_IMPL( AsyncOperationStateImplT )

        private:

            typedef AsyncOperationStateBase                                                 base_type;

            cpp::void_callback_t                                                            m_callback;
            create_task_callback_t                                                          m_createTaskCallback;

        public:

            void reset() NOEXCEPT
            {
                cpp::void_callback_t().swap( m_callback );
            }

            auto callback() const NOEXCEPT -> const cpp::void_callback_t&
            {
                return m_callback;
            }

            void callback( SAA_in cpp::void_callback_t&& callback ) NOEXCEPT
            {
                callback.swap( m_callback );
            }

            auto createTaskCallback() const NOEXCEPT -> const create_task_callback_t&
            {
                return m_createTaskCallback;
            }

            void createTaskCallback( SAA_in create_task_callback_t&& createTaskCallback ) NOEXCEPT
            {
                createTaskCallback.swap( m_createTaskCallback );
            }

            /**************************************************************************************
             * AsyncOperationState implementation
             */

            virtual void execute() OVERRIDE
            {
                BL_ASSERT( m_callback );

                m_callback();
            }

            virtual om::ObjPtr< tasks::Task > createTask() OVERRIDE
            {
                if( m_createTaskCallback )
                {
                    return m_createTaskCallback();
                }

                return nullptr;
            }
        };

        typedef om::ObjectImpl< AsyncOperationStateImplT<> > AsyncOperationStateImpl;

    protected:

        AsyncExecutorWrapperCallbackT(
            SAA_in_opt              const std::size_t                                       threadsCount = 0U,
            SAA_in_opt              const om::ObjPtr< tasks::TaskControlToken >&            controlToken = nullptr,
            SAA_in_opt              const std::size_t                                       maxConcurrentTasks = 0U
            )
            :
            base_type(
                AsyncSharedStateBaseDefaultImpl::createInstance(
                    om::copy( controlToken )
                    ),
                threadsCount,
                maxConcurrentTasks
                )
        {
        }

    public:

        template
        <
            typename T = AsyncOperationState
        >
        om::ObjPtr< T > createOperationState(
            SAA_in              cpp::void_callback_t&&                                      callback,
            SAA_in_opt          create_task_callback_t&&                                    createTaskCallback
            )
        {
            auto operationState = base_type::impl() -> template tryGetOperationState< AsyncOperationStateImpl >();

            if( ! operationState )
            {
                operationState =
                    AsyncOperationStateImpl::template createInstance< AsyncOperationStateImpl >();
            }

            operationState -> impl( om::copy( base_type::impl() ) );
            operationState -> callback( BL_PARAM_FWD( callback ) );
            operationState -> createTaskCallback( BL_PARAM_FWD( createTaskCallback ) );

            BL_ASSERT( operationState -> callback() );

            return om::qi< T >( operationState );
        }
    };

    typedef om::ObjectImpl< AsyncExecutorWrapperCallbackT<> > AsyncExecutorWrapperCallbackImpl;

} // bl

#endif /* __BL_ASYNC_ASYNCEXECUTORWRAPPERCALLBACK_H_ */
