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

#ifndef __BL_ASYNC_ASYNCEXECUTORWRAPPERBASE_H_
#define __BL_ASYNC_ASYNCEXECUTORWRAPPERBASE_H_

#include <baselib/async/AsyncBaseIncludes.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TaskControlToken.h>

#include <baselib/core/OS.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    /**
     * @brief class AsyncOperationStatePoolable - the base class for pool-able operation state objects
     */

    template
    <
        typename E = void
    >
    class AsyncOperationStatePoolableT : public AsyncOperationState
    {
        BL_CTR_DEFAULT( AsyncOperationStatePoolableT, protected )
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( AsyncOperationStatePoolableT, AsyncOperationState )

    protected:

        cpp::ScalarTypeIniter< bool >                                           m_freed;

    public:

        bool freed() const NOEXCEPT
        {
            return m_freed;
        }

        void freed( SAA_in const bool freed ) NOEXCEPT
        {
            m_freed = freed;
        }

        /*
         * Implement part of the public interface, so the derived classes don't have
         * to implement the smart defaults
         */

        virtual void execute() OVERRIDE
        {
            /*
             * NOOP
             */
        }

        virtual om::ObjPtr< tasks::Task > createTask() OVERRIDE
        {
            return nullptr;
        }

        virtual void* get() NOEXCEPT OVERRIDE
        {
            return reinterpret_cast< void* >( this );
        }
    };

    typedef AsyncOperationStatePoolableT<> AsyncOperationStatePoolable;

    /**
     * @brief class AsyncSharedStateBase - the base class for state objects that will be used to maintain
     * the shared state between the AsyncExecutorWrapper object and the operation states it manages
     */

    template
    <
        typename E = void
    >
    class AsyncSharedStateBaseT : public om::ObjectDefaultBase
    {
    protected:

        typedef om::ObjectImpl
        <
            SimplePool< om::ObjPtr< AsyncOperationStatePoolable > >
        >
        pool_operation_states_t;

        const om::ObjPtr< tasks::TaskControlToken >                                     m_controlToken;
        const om::ObjPtr< pool_operation_states_t >                                     m_operationStatesPool;

        AsyncSharedStateBaseT( SAA_in_opt om::ObjPtr< tasks::TaskControlToken >&& controlToken = nullptr )
            :
            m_controlToken( BL_PARAM_FWD( controlToken ) ),
            m_operationStatesPool( pool_operation_states_t::template createInstance< pool_operation_states_t >() )
        {
        }

    public:

        auto controlToken() const NOEXCEPT -> const om::ObjPtr< tasks::TaskControlToken >&
        {
            return m_controlToken;
        }

        void returnOperationState( SAA_in om::ObjPtr< AsyncOperationStatePoolable >&& operationState )
        {
            m_operationStatesPool -> put( BL_PARAM_FWD( operationState ) );
        }

        template
        <
            typename T
        >
        om::ObjPtr< T > tryGetOperationState()
        {
            auto operationState = m_operationStatesPool -> tryGet();

            if( operationState )
            {
                return om::qi< T >( operationState );
            }

            return nullptr;
        }
    };

    typedef AsyncSharedStateBaseT<> AsyncSharedStateBase;
    typedef om::ObjectImpl< AsyncSharedStateBase > AsyncSharedStateBaseDefaultImpl;

    /**
     * @brief class AsyncExecutorWrapperBase - a base class for implementing async
     * wrappers on top of a sync interface
     *
     * The implementation of the derived async wrappers usually has the following goals:
     *
     * -- provide async interface abstraction for the wrapped sync interface, dedicated
     *    thread pool and an execution queue for controlling resource usage (see below)
     *
     * -- limit the # of async tasks executing concurrently at any point in time to ensure
     *    that there will be no thread thrashing or clogging the sync interface queue with
     *    too many requests - this is guaranteed by the size of the dedicated thread pool
     *
     * -- limit the # of async tasks scheduled for execution concurrently in general to
     *    ensure that there will be limit on memory consumption by the server; since the
     *    async model is flexible and does not block and the # of incoming connections
     *    can be very large we need to make sure all async requests are "buffered" based
     *    on the amount of resources we're willing to dedicate for this; all outstanding
     *    async requests which do not fit this limit will be queued up in the pending
     *    queue for execution when resources are available; the execution queue guarantees
     *    fairness and ensures that the async requests will be processed in FIFO order
     */

    template
    <
        typename IMPL = AsyncSharedStateBaseDefaultImpl
    >
    class AsyncExecutorWrapperBase : public om::DisposableObjectBase
    {
    public:

        /**
         * @brief class AsyncOperationStateBase - The base class to derive an operation state from
         */

        template
        <
            typename E = void
        >
        class AsyncOperationStateBaseT : public AsyncOperationStatePoolable
        {
            BL_CTR_DEFAULT( AsyncOperationStateBaseT, protected )
            BL_DECLARE_OBJECT_IMPL( AsyncOperationStateBaseT )

        protected:

            /*
             * Note: that m_impl should only be set for the duration of when the operation state
             * is active and right before it is returned / made inactive in releaseResources() it
             * should be reset back to nullptr
             *
             * If it is not reset back to nullptr in releaseResources() then you will create a
             * circular reference if the shared state object (IMPL) is pooling the operation
             * states as it will keep reference to itself while in the pool and the objects will
             * never be released
             */

            om::ObjPtr< IMPL >                                                      m_impl;

        public:

            auto impl() const NOEXCEPT -> const om::ObjPtr< IMPL >&
            {
                return m_impl;
            }

            void impl( SAA_in_opt om::ObjPtr< IMPL >&& impl ) NOEXCEPT
            {
                m_impl = BL_PARAM_FWD( impl );
            }

            virtual auto controlToken() NOEXCEPT -> const om::ObjPtr< tasks::TaskControlToken >& OVERRIDE
            {
                BL_ASSERT( m_impl );

                return static_cast< AsyncSharedStateBase* >( m_impl.get() ) -> controlToken();
            }

            virtual void releaseResources() NOEXCEPT OVERRIDE
            {
                BL_ASSERT( m_impl );

                auto thisRef = om::ObjPtrCopyable< AsyncOperationStatePoolable >::acquireRef(
                    static_cast< AsyncOperationStatePoolable* >( this )
                    ).detachAsUnique();

                om::ObjPtr< IMPL > impl( std::move( m_impl ) );

                static_cast< AsyncSharedStateBase* >( impl.get() ) -> returnOperationState( std::move( thisRef ) );

                BL_ASSERT( ! thisRef );
            }
        };

        typedef AsyncOperationStateBaseT<>                                              AsyncOperationStateBase;

    protected:

        typedef detail::AsyncExecutorImpl                                               AsyncExecutorImpl;

        const om::ObjPtr< IMPL >                                                        m_impl;
        const om::ObjPtrDisposable< AsyncExecutor >                                     m_asyncExecutor;

        AsyncExecutorWrapperBase(
            SAA_in                  om::ObjPtr< IMPL >&&                                impl,
            SAA_in_opt              const std::size_t                                   threadsCount = 0U,
            SAA_in_opt              const std::size_t                                   maxConcurrentTasks = 0U
            )
            :
            m_impl( BL_PARAM_FWD( impl ) ),
            m_asyncExecutor( AsyncExecutorImpl::createInstance< AsyncExecutor >( threadsCount, maxConcurrentTasks ) )
        {
        }

    public:

        auto impl() const NOEXCEPT -> const om::ObjPtr< IMPL >&
        {
            return m_impl;
        }

        auto asyncExecutor() const NOEXCEPT -> const om::ObjPtrDisposable< AsyncExecutor >&
        {
            return m_asyncExecutor;
        }

        /*
         * Returning true is the default behavior as this is what is normally logically expected by default
         * from a server and also that this is the current (and original) behavior of the blob server
         */

        virtual bool stopServerOnUnexpectedBackendError() const NOEXCEPT
        {
            return true;
        }

        virtual void dispose() OVERRIDE
        {
            BL_NOEXCEPT_BEGIN()

            m_asyncExecutor -> dispose();

            BL_NOEXCEPT_END()
        }
    };

} // bl

#endif /* __BL_ASYNC_ASYNCEXECUTORWRAPPERBASE_H_ */
