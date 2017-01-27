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

#ifndef __BL_ASYNC_ASYNCEXECUTORIMPL_H_
#define __BL_ASYNC_ASYNCEXECUTORIMPL_H_

#include <baselib/async/AsyncExecutor.h>
#include <baselib/async/AsyncOperationState.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/Task.h>
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
    namespace detail
    {
        /**
         * @brief class AsyncExecutorImpl - an implementation of async executor interface
         * on top of a sync processing interface
         *
         * The implementation of the async executor has the following goals:
         *
         * -- provide async interface abstraction for sync processing interface which is
         *    implemented on top of a dedicated thread pool and an execution queue for
         *    controlling resource usage (see details below)
         *
         * -- limit the # of async tasks executing concurrently at any point in time to ensure
         *    that there will not be thread trashing or clogging the queue with too many
         *    requests - this is guaranteed by the size of the dedicated thread pool
         *
         * -- limit the # of outstanding async tasks which have already started executing to
         *    ensure that there will be limit on memory consumption by the server; since the
         *    async model is flexible and does not block the # of incoming outstanding async
         *    calls can be unbounded we need to make sure all async requests are "buffered"
         *    based on the amount of resources we're willing to dedicate for this; all
         *    async requests which do not fit this limit will be queued up in the pending
         *    queue for execution when resources are available; the execution queue guarantees
         *    fairness and ensures that the async requests will be processed in FIFO order
         */

        template
        <
            typename E = void
        >
        class AsyncExecutorImplT : public AsyncExecutor
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( AsyncExecutorImplT  )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( AsyncExecutor )
                BL_QITBL_ENTRY( om::Disposable )
            BL_QITBL_END( AsyncExecutor )

        protected:

            typedef AsyncExecutorImplT< E >                                             async_executor_t;

            enum
            {
                /*
                 * The multiplier of maximum concurrent tasks relative to the # of threads
                 * in the thread pool
                 *
                 * This usually determines some sort of upper boundary for the amount of
                 * resources that can be used by the async operations as this number
                 * determines the maximum number of async operations which have started
                 * executing and are considered 'expensive'
                 */

                MAX_TASKS_MULTIPLIER = 8,
            };

            /**
             * @brief class ExecutorTask - this is an internal task object which handles the execution
             * of the async operations
             *
             * The task object is in the ready queue if it is available to be picked up by a new async
             * operation and it stays in the pending queue or the execution queue if it is assigned to
             * an async operation
             *
             * Once the task object is assigned to an async operation and pushed on the queue it will
             * be eventually executed even if the async operation has to be canceled because even in
             * the case of cancellation of the async operation we still need to invoke the callback
             *
             * Here is how this task object works:
             *
             * -- the first time the async operation is scheduled on the queue m_callsExpected is set
             *    to two - one for the requested async operation and one for the task termination /
             *    exit event which can be scheduled out of order
             *
             * -- the initial state of m_stopped is false
             *
             * -- new async calls can be scheduled on an async operation only after the previous one
             *    has completed and the callback was called, but they can also be scheduled from within
             *    the callback which is very important because it allows us to schedule async calls
             *    continually until the top level async task is done (see examples in the test code)
             *
             * -- every time we get into onExecute and acquire the lock we decrement m_callsExpected
             *    and check it is zero; if it is zero that means this was the last call for a task
             *    which is ready to be freed; in this case only we call notifyReady() - in all other
             *    cases the task remains executing to keep its place in the execution queue and ensure
             *    we don't allocate more memory for "buffering" than originally requested
             *
             * -- if a new call is scheduled from within the callback then we simply increment
             *    m_callsExpected and post a new event on the thread pool; this way the async task
             *    which is already executing will continue executing the new async call ahead of the
             *    other async operations that are already pending; this can be allowed only if
             *    m_callsExpected is 1 because this means we don't have an outstanding async call;
             *    if m_callsExpected is 2 then this means we have an outstanding async call and we
             *    can't allow this, so the code throws; the caller has to first cancel the current
             *    async operation if he/she wants to schedule a new one (or wait until it is executed)
             *
             * -- when onExecute is called with m_callsExpected > 1 we have a real call; this real call
             *    is made only if m_stopped is false, otherwise it means the task is part of a canceled
             *    async operation in which case we simply ignore the call and call the callback with
             *    Result object which has asio::error::operation_aborted
             *
             * -- we also track the expected executed calls in separate member which is called
             *    m_remainingToExecute and is defined as std::atomic; we need that to be able to check
             *    that new async calls cannot be scheduled while others are currently pending; the var
             *    is decremented right before we call the callback, so scheduling a new async operation
             *    can be done from with the callback (which is normal as this is just continuation of
             *    the async operation in progress doing another call)
             */

            template
            <
                typename E2 = void
            >
            class ExecutorTaskT :
                public tasks::TaskBase
            {
                BL_DECLARE_OBJECT_IMPL( ExecutorTaskT )

            private:

                typedef ExecutorTaskT< E2 >                                             this_type;
                typedef tasks::TaskBase                                                 base_type;

                async_executor_t&                                                       m_asyncExecutor;
                om::ObjPtr< AsyncOperationState >                                       m_operationState;
                cpp::ScalarTypeIniter< std::uint16_t >                                  m_callsExpected;
                std::atomic< std::uint32_t >                                            m_remainingToExecute;
                cpp::ScalarTypeIniter< bool >                                           m_stopped;
                AsyncOperation::callback_t                                              m_callback;
                om::ObjPtr< tasks::Task >                                               m_operationStateTaskInProgress;
                os::mutex                                                               m_executeLock;

                void onExecute() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    /*
                     * m_executeLock will guard and ensure the serialization of the async
                     * calls being scheduled
                     *
                     * This lock is needed to ensure the following:
                     *
                     * 1) multiple async calls scheduled on the same async operation object
                     * are always done in sequence and never concurrently
                     *
                     * 2) the last call for onExecute which is just meant to terminate the
                     * task will be done only *after* the last async call is done and the
                     * callback has returned
                     *
                     * m_executeLock will also be acquired always first and always before
                     * the task lock, so deadlock cannot happen (because we always acquire
                     * the locks in the same order)
                     */

                    BL_MUTEX_GUARD( m_executeLock );

                    bool taskReady = false;
                    om::ObjPtr< tasks::Task > operationStateTaskInProgress;
                    AsyncOperation::callback_t callback;
                    bool stopped = false;

                    BL_TASKS_HANDLER_BEGIN()

                    BL_ASSERT( m_operationState );

                    --m_callsExpected.lvalue();

                    if( m_callsExpected )
                    {
                        /*
                         * At this point we capture the callback and reset it
                         * before we release the task lock as after we release the
                         * task lock a new async call may be scheduled on the same
                         * async operation object which will change m_callback
                         *
                         * We also want to release the callback as soon as we call
                         * it as the caller does not expect us to keep reference to
                         * the callback after the call has been completed
                         *
                         * While holding the task lock we also save the state of
                         * m_stopped which will be subsequently checked outside of
                         * the lock
                         */

                        callback.swap( m_callback );
                        BL_ASSERT( callback );

                        if( m_operationStateTaskInProgress )
                        {
                            /*
                             * If there was an operation state task executed then we save this in
                             * operationStateTaskInProgress to complete the call correctly below
                             */

                            BL_ASSERT(
                                tasks::Task::Completed == m_operationStateTaskInProgress -> getState()
                                );

                            operationStateTaskInProgress.swap( m_operationStateTaskInProgress );
                        }

                        stopped = m_stopped;
                    }
                    else
                    {
                        /*
                         * This is the last call which is simply meant to
                         * terminate the task
                         */

                        BL_ASSERT( ! m_callback );
                        BL_ASSERT( ! m_operationStateTaskInProgress );

                        taskReady = true;
                    }

                    BL_TASKS_HANDLER_END_NOTREADY()

                    /*
                     * Both callback( ... ) and notifyReady() are expected to be NOEXCEPT
                     */

                    if( taskReady )
                    {
                        /*
                         * This async operation has completed and it is time to release the resources
                         */

                        m_operationState -> releaseResources();
                        m_operationState.reset();

                        notifyReady();
                    }
                    else
                    {
                        std::exception_ptr eptr = nullptr;
                        eh::error_code code;

                        /*
                         * Now we can execute the async call outside of the task lock and
                         * only after we do this then we decrement m_remainingToExecute
                         *
                         * Note also that executing of the async call will be done in try
                         * catch block to ensure we handle failures and pass them to the
                         * callback
                         */

                        if( stopped )
                        {
                            code = asio::error::operation_aborted;
                        }
                        else
                        {
                            /*
                             * This is not the last call - let's execute whatever the
                             * requested operation is
                             *
                             * However we will only attempt to call executeOperation
                             * if operationStateTaskInProgress is nullptr
                             *
                             * If operationStateTaskInProgress is non-nullptr that means
                             * the operation state has modeled its execution as an abstract
                             * task and we don't need to call execute() method
                             */

                            if( operationStateTaskInProgress )
                            {
                                eptr = operationStateTaskInProgress -> exception();
                            }
                            else
                            {
                                try
                                {
                                    code = executeOperation( m_operationState );
                                }
                                catch( std::exception& )
                                {
                                    eptr = std::current_exception();
                                }
                            }
                        }

                        --m_remainingToExecute;

                        callback(
                            AsyncOperation::Result(
                                eptr /* exception */,
                                eptr ? eh::error_code() : code,
                                std::move( operationStateTaskInProgress )
                                )
                            );
                    }

                    BL_NOEXCEPT_END()
                }

                static eh::error_code executeOperation( SAA_in const om::ObjPtr< AsyncOperationState >& operationState )
                {
                    const auto& controlToken = operationState -> controlToken();

                    if( controlToken && controlToken -> isCanceled() )
                    {
                        return asio::error::operation_aborted;
                    }

                    operationState -> execute();

                    return eh::error_code();
                }

                void scheduleAsyncCall( SAA_in const bool increment ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    /*
                     * m_callsExpected cannot be zero here (it can only be 2U or 1U)
                     *
                     * It will be 2U if the task has not been canceled but not yet
                     * executing and it could be 1U in the somewhat rare case when
                     * the task was canceled before it got executed in which case
                     * onExecute could be called even before the task has officially
                     * started executing (to simply call the callback with
                     * asio::error::operation_aborted)
                     */

                    BL_ASSERT( m_callsExpected.value() );

                    if( increment )
                    {
                        ++m_callsExpected.lvalue();
                        ++m_remainingToExecute;
                    }

                    m_asyncExecutor.postCompletionCallback(
                        cpp::bind(
                            &this_type::onExecute,
                            om::ObjPtrCopyable< this_type >::acquireRef( this )
                            )
                        );

                    BL_NOEXCEPT_END()
                }

            protected:

                ExecutorTaskT( SAA_inout async_executor_t& asyncExecutor )
                    :
                    m_asyncExecutor( asyncExecutor )
                {
                    resetState();
                }

                void onOperationTaskReady( SAA_in const bool newCall ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( base_type::m_lock );

                    /*
                     * Since this task was 'manually' scheduled (but not by an execution
                     * queue) we need to also manually handle the continuation logic here
                     * to maintain the same invariants as if the task was executed by an
                     * execution queue
                     */

                    BL_ASSERT( m_operationStateTaskInProgress );

                    std::exception_ptr continuationException;

                    try
                    {
                        auto continuationTask = m_operationStateTaskInProgress -> continuationTask();

                        if( continuationTask )
                        {
                            /*
                             * This task has a continuation task, just schedule the
                             * continuations until there are no more such
                             */

                            continuationTask -> scheduleNothrow(
                                m_asyncExecutor.externalTasksQueue(),
                                cpp::bind(
                                    &this_type::onOperationTaskReady,
                                    this,
                                    newCall
                                    )
                                );

                            m_operationStateTaskInProgress = std::move( continuationTask );

                            return;
                        }
                    }
                    catch( std::exception& )
                    {
                        continuationException = std::current_exception();
                    }

                    if( continuationException )
                    {
                        /*
                         * Something with creating and scheduling the continuation task has
                         * failed
                         *
                         * We treat this as a regular task failure as creating and scheduling
                         * the continuation is considered as part of the task itself
                         */

                        m_operationStateTaskInProgress -> exception( continuationException );
                    }

                    m_operationStateTaskInProgress -> setCompletedState();

                    scheduleAsyncCall( newCall /* increment */ ) /* NOEXCEPT */;

                    BL_NOEXCEPT_END()
                }

                void scheduleCall( SAA_in const bool newCall ) NOEXCEPT
                {
                    BL_ASSERT( m_operationState );
                    BL_ASSERT( ! m_operationStateTaskInProgress );

                    /*
                     * We only attempt to create and schedule the operation state task if
                     * the executor task has not been stopped / cancelled preemptively
                     */

                    if( ! m_stopped )
                    {
                        try
                        {
                            m_operationStateTaskInProgress = m_operationState -> createTask();

                            if( m_operationStateTaskInProgress )
                            {
                                /*
                                 * The operation state has modeled its execution as an abstract task
                                 *
                                 * In this case we need to execute the task instead of calling the
                                 * operation state execute() method
                                 */

                                m_operationStateTaskInProgress -> scheduleNothrow(
                                    m_asyncExecutor.externalTasksQueue(),
                                    cpp::bind(
                                        &this_type::onOperationTaskReady,
                                        this,
                                        newCall
                                        )
                                    );

                                return;
                            }
                        }
                        catch( std::exception& )
                        {
                            /*
                             * Note that the section below can't be truly NOEXCEPT as the createInstance
                             * call can throw potentially (and even if the constructor is NOEXCEPT still
                             * it can throw std::bad_alloc)
                             *
                             * However we can't really propagate the exception safely from this situation
                             * so our best option (in that rare case) would be to terminate the process
                             */

                            BL_NOEXCEPT_BEGIN()

                            /*
                             * An exception occurred while trying to create the task or while
                             * scheduling it for execution
                             *
                             * In this case just capture the exception / error and if necessary
                             * create a simple task to carry the exception if the task creation
                             * has failed (which will keep the code and error handling uniform
                             * and error clean)
                             */

                            const auto eptr = std::current_exception();

                            if( m_operationStateTaskInProgress )
                            {
                                m_operationStateTaskInProgress -> exception( eptr );
                            }
                            else
                            {
                                m_operationStateTaskInProgress =
                                    tasks::SimpleCompletedTask::createInstance< tasks::Task >( eptr );
                            }

                            BL_NOEXCEPT_END()
                        }
                    }

                    scheduleAsyncCall( newCall /* increment */ ) /* NOEXCEPT */;
                }

                virtual void scheduleTask( SAA_in const std::shared_ptr< tasks::ExecutionQueue >& eq ) OVERRIDE
                {
                    BL_UNUSED( eq );

                    scheduleCall( false /* newCall */ );
                }

            public:

                virtual void requestCancel() NOEXCEPT OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    om::ObjPtr< tasks::Task > task2Cancel;

                    {
                        BL_MUTEX_GUARD( base_type::m_lock );

                        if( m_stopped )
                        {
                            return;
                        }

                        /*
                         * We only schedule a cancellation call preemptively if there is no
                         * operation state task in progress
                         *
                         * Otherwise the best we can do is to simply attempt to cancel the
                         * task and ignore the call
                         *
                         * Once the async operation completes another cancellation call will
                         * be made to stop the task, but this second call will always be with
                         * m_operationStateTaskInProgress equal to nullptr
                         */

                        if( m_operationStateTaskInProgress )
                        {
                            task2Cancel = om::copy( m_operationStateTaskInProgress );
                        }
                        else
                        {
                            m_stopped = true;

                            scheduleAsyncCall( false /* increment */ );
                        }
                    }

                    if( task2Cancel )
                    {
                        task2Cancel -> requestCancel();
                    }
                    else
                    {
                        /*
                         * Since this is a request exit call we try to prioritize it ahead of the queue
                         * so it completes as quickly as possible and frees the task object
                         *
                         * Note that it is very important to make this call outside of holding the task
                         * lock as trying to acquire the queue lock while holding the task lock can cause
                         * a deadlock problem (as the execution queue will attempt to acquire the task
                         * lock while holding the queue lock and attempting to schedule tasks)
                         */

                        m_asyncExecutor.tryPrioritize( om::qi< tasks::Task >( this ) );
                    }

                    BL_NOEXCEPT_END()
                }

                void ensureNoCallsPending()
                {
                    /*
                     * Before we can schedule a new async call the current one must have
                     * executed already
                     *
                     * I.e. new async call can be scheduled either after the async
                     * operation is canceled successfully or after the callback has
                     * been called
                     *
                     * Note also that the new operation can be scheduled from within the
                     * callback too
                     */

                    BL_CHK(
                        false,
                        0U == m_remainingToExecute,
                        BL_MSG()
                            << "Another async call is already in progress"
                        );
                }

                void requestNewAsyncCall( SAA_in AsyncOperation::callback_t&& callback ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( base_type::m_lock );

                    ensureNoCallsPending();

                    /*
                     * Request the new operation with the new callback
                     */

                    m_callback.swap( callback );

                    scheduleCall( true /* newCall */ ) /* NOEXCEPT */;

                    BL_NOEXCEPT_END()
                }

                void resetState() NOEXCEPT
                {
                    m_operationState.reset();
                    m_stopped = false;
                    m_callsExpected = 2U;
                    m_remainingToExecute = 1U;
                    m_operationStateTaskInProgress = nullptr;
                    AsyncOperation::callback_t().swap( m_callback );
                }

                void assign( SAA_in om::ObjPtr< AsyncOperationState >&& operationState ) NOEXCEPT
                {
                    m_operationState = BL_PARAM_FWD( operationState );
                }

                auto operationState() const NOEXCEPT -> const om::ObjPtr< AsyncOperationState >&
                {
                    return m_operationState;
                }

                bool stopped() const NOEXCEPT
                {
                    return m_stopped;
                }

                auto callback() const NOEXCEPT -> const AsyncOperation::callback_t&
                {
                    return m_callback;
                }

                void callback( SAA_in AsyncOperation::callback_t&& callback ) NOEXCEPT
                {
                    callback.swap( m_callback );
                }
            };

            typedef om::ObjectImpl< ExecutorTaskT<> > ExecutorTaskImpl;

            /**
             * @brief class ExecutorAsyncOperation - this is the implementation of the AsyncOperation
             *
             * It aggregates the task object and the async operation state from above
             *
             * This object is also pooled (for performance reasons) and this it implements the required
             * pool interface - the freed() getter and setter
             */

            template
            <
                typename E2 = void
            >
            class ExecutorAsyncOperationT :
                public AsyncOperation
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( ExecutorAsyncOperationT, AsyncOperation )

            private:

                typedef ExecutorAsyncOperationT< E2 >                                   this_type;

                async_executor_t&                                                       m_asyncExecutor;
                om::ObjPtr< AsyncOperationState >                                       m_operationState;
                om::ObjPtr< ExecutorTaskImpl >                                          m_task;
                os::mutex                                                               m_lock;
                cpp::ScalarTypeIniter< bool >                                           m_freed;
                cpp::ScalarTypeIniter< bool >                                           m_active;

            protected:

                ExecutorAsyncOperationT( SAA_inout async_executor_t& asyncExecutor )
                    :
                    m_asyncExecutor( asyncExecutor )
                {
                }

                ~ExecutorAsyncOperationT() NOEXCEPT
                {
                    resetState();
                }

            public:

                void resetState() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( m_lock );

                    if( m_task )
                    {
                        m_task -> requestCancel();
                        m_task.reset();
                    }

                    if( m_active )
                    {
                        BL_ASSERT( m_operationState );
                        --m_asyncExecutor.m_outstandingCalls;
                        m_active = false;
                    }

                    m_operationState.reset();

                    BL_NOEXCEPT_END()
                }

                bool active() const NOEXCEPT
                {
                    return m_active;
                }

                void active( SAA_in const bool active ) NOEXCEPT
                {
                    m_active = active;
                }

                auto operationState() const NOEXCEPT -> const om::ObjPtr< AsyncOperationState >&
                {
                    return m_operationState;
                }

                void operationState( SAA_in om::ObjPtr< AsyncOperationState >&& operationState ) NOEXCEPT
                {
                    m_operationState = BL_PARAM_FWD( operationState );
                }

                auto task() const NOEXCEPT -> const om::ObjPtr< ExecutorTaskImpl >&
                {
                    return m_task;
                }

                void task( SAA_in om::ObjPtr< ExecutorTaskImpl >&& task ) NOEXCEPT
                {
                    BL_MUTEX_GUARD( m_lock );

                    m_task = std::move( task );
                }

                bool freed() const NOEXCEPT
                {
                    return m_freed;
                }

                void freed( SAA_in const bool freed ) NOEXCEPT
                {
                    m_freed = freed;
                }

                virtual void cancel() NOEXCEPT OVERRIDE
                {
                    resetState();
                }
            };

            typedef om::ObjectImpl< ExecutorAsyncOperationT<> > ExecutorAsyncOperationImpl;

            /******************************************************************************************
             * The implementation of the AsyncExecutor interface follows here
             */

            typedef om::ObjectImpl
            <
                SimplePool< om::ObjPtr< ExecutorAsyncOperationImpl > >
            >
            pool_operations_t;

            const om::ObjPtr< pool_operations_t >                                           m_operationsPool;
            om::ObjPtrDisposable< ThreadPool >                                              m_threadPool;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                                   m_workers;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                                   m_completionTasksQueue;
            os::mutex                                                                       m_completionTasksLock;
            std::atomic< std::uint32_t >                                                    m_outstandingCalls;
            os::mutex                                                                       m_lock;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                                   m_externalTasksQueueLocal;
            std::shared_ptr< tasks::ExecutionQueue >                                        m_externalTasksQueue;

            AsyncExecutorImplT(
                SAA_in_opt              const std::size_t                                   threadsCount = 0U,
                SAA_in_opt              const std::size_t                                   maxConcurrentTasks = 0U,
                SAA_in_opt              const std::shared_ptr< tasks::ExecutionQueue >&     externalTasksQueue = nullptr
                )
                :
                m_operationsPool( pool_operations_t::template createInstance() ),
                m_outstandingCalls( 0U ),
                m_externalTasksQueue( externalTasksQueue )
            {
                const std::size_t threadsCountActual =
                    threadsCount ? threadsCount : ThreadPoolImpl::THREADS_COUNT_DEFAULT;

                auto threadPool = om::lockDisposable(
                    ThreadPoolImpl::createInstance< ThreadPool >(
                        os::getAbstractPriorityDefault(),
                        threadsCountActual
                        )
                    );

                const std::size_t maxConcurrentTasksActual =
                    maxConcurrentTasks ? maxConcurrentTasks : MAX_TASKS_MULTIPLIER * threadsCountActual;

                auto workers = om::lockDisposable(
                    tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                        tasks::ExecutionQueue::OptionKeepAll,
                        maxConcurrentTasksActual /* maxExecuting */
                        )
                    );

                workers -> setLocalThreadPool( threadPool.get() );

                /*
                 * The completion tasks queue throttle limit should be set to the # of threads
                 *
                 * There is no performance advantage of setting this limit to a large value
                 * while if it is larger it decreases the fairness and increases the chance
                 * of small temporary starvation
                 */

                const auto completionTasksQueueThrottleLimit = threadsCountActual;

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Async executor completion tasks queue throttle limit was set to "
                        << completionTasksQueueThrottleLimit
                    );

                auto completionTasksQueue = om::lockDisposable(
                    tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                        tasks::ExecutionQueue::OptionKeepAll,
                        completionTasksQueueThrottleLimit /* maxExecuting */
                        )
                    );

                completionTasksQueue -> setLocalThreadPool( threadPool.get() );

                om::ObjPtrDisposable< tasks::ExecutionQueue > externalTasksQueueLocal;
                std::shared_ptr< tasks::ExecutionQueue > externalTasksQueueLocalSharedPtr;

                if( ! m_externalTasksQueue )
                {
                    /*
                     * If the external tasks queue is not provided we create one locally
                     * (which would actually normally be the case)
                     */

                    externalTasksQueueLocal = om::lockDisposable(
                        tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                            tasks::ExecutionQueue::OptionKeepNone
                            )
                        );

                    externalTasksQueueLocal -> setLocalThreadPool( threadPool.get() );

                    externalTasksQueueLocalSharedPtr =
                        om::getSharedPtr< tasks::ExecutionQueue >( externalTasksQueueLocal );
                }

                /*
                 * NOEXCEPT block from now on - just move the objects that were constructed
                 * into the member variables
                 *
                 * Note that once this object is constructed dispose() must be called on it
                 * before the destructor is called
                 */

                m_threadPool = std::move( threadPool );
                m_workers = std::move( workers );
                m_completionTasksQueue = std::move( completionTasksQueue );

                if( ! m_externalTasksQueue )
                {
                    externalTasksQueueLocal.swap( m_externalTasksQueueLocal );
                    externalTasksQueueLocalSharedPtr.swap( m_externalTasksQueue );
                }
            }

            ~AsyncExecutorImplT() NOEXCEPT
            {
                BL_RT_ASSERT(
                    ! ( m_threadPool || m_workers || m_completionTasksQueue || 0U != m_outstandingCalls ),
                    "AsyncExecutorImpl::~AsyncExecutorImpl: object has not been disposed properly"
                    );
            }

            /**
             * @brief This is a small helper which is only going to be used in DEBUG builds
             * with BL_ASSERT
             *
             * This function will always return true; it returns bool for convenience, so it
             * can be called in BL_ASSERT directly. If something is not ok the function will
             * assert itself
             */

            static bool verifyQueues( SAA_in const om::ObjPtrDisposable< tasks::ExecutionQueue >& queue )
            {
                /*
                 * At this point we should not have pending or executing tasks which are
                 * not stopped as this will imply we have outstanding async calls
                 */

                const auto cbVerifyStopped = []( SAA_in const om::ObjPtr< tasks::Task >& task )
                {
                    const auto taskImpl = om::qi< ExecutorTaskImpl >( task );
                    BL_UNUSED( taskImpl );

                    BL_ASSERT( taskImpl -> stopped() );
                };

                queue -> scanQueue( tasks::ExecutionQueue::QueueId::Pending, cbVerifyStopped );
                queue -> scanQueue( tasks::ExecutionQueue::QueueId::Executing, cbVerifyStopped );

                /*
                 * This function always returns true - see comment above in the function header
                 */

                return true;
            }

            void asyncBeginImpl(
                SAA_in                  const om::ObjPtr< AsyncOperation >&             operation,
                SAA_in                  AsyncOperation::callback_t&&                    callback
                )
            {
                BL_MUTEX_GUARD( m_lock );

                const auto operationImpl = om::qi< ExecutorAsyncOperationImpl >( operation );

                if( operationImpl -> freed() )
                {
                    BL_THROW(
                        UnexpectedException(),
                        BL_MSG()
                            << "Attempting to execute async operation on released object"
                        );
                }

                if( operationImpl -> task() )
                {
                    /*
                     * The valid scenario here is that this is a continuation of an
                     * existing call
                     *
                     * The operation is currently in progress or called from within
                     * the callback itself (we can't differentiate both cases) --
                     * in this case we just have to schedule the new operation
                     * on the task directly
                     */

                    operationImpl -> task() -> ensureNoCallsPending();

                    operationImpl -> task() -> requestNewAsyncCall( BL_PARAM_FWD( callback ) );

                    return;
                }

                /*
                 * We either get what's available from the head of the execution queue
                 * or we just create a new task via the tasks pool
                 */

                auto topTask = m_workers -> top( false /* wait */ );

                om::ObjPtr< ExecutorTaskImpl > task;

                if( topTask )
                {
                    task = om::qi< ExecutorTaskImpl >( topTask );
                    task -> resetState();
                }
                else
                {
                    task = ExecutorTaskImpl::template createInstance( *this );
                    topTask = om::qi< tasks::Task >( task );
                }

                BL_ASSERT( ! task -> callback() );
                BL_ASSERT( tasks::Task::Running != task -> getState() );
                BL_ASSERT( ! task -> operationState() );

                task -> callback( BL_PARAM_FWD( callback ) );
                task -> assign( om::copy( operationImpl -> operationState() ) );
                operationImpl -> task( om::copy( task ) );

                try
                {
                    BL_ASSERT( topTask && om::areEqual( topTask, task ) );
                    m_workers -> push_back( topTask );
                }
                catch( std::exception& )
                {
                    /*
                     * Revert the state if something goes wrong with starting the async call
                     */

                    operationImpl -> task( nullptr );
                    task -> resetState();

                    throw;
                }
            }

        public:

            bool tryPrioritize( SAA_in const om::ObjPtr< tasks::Task >& task )
            {
                return m_workers -> prioritize( task, false /* wait */ );
            }

            auto externalTasksQueue() const NOEXCEPT -> const std::shared_ptr< tasks::ExecutionQueue >&
            {
                return m_externalTasksQueue;
            }

            void postCompletionCallback( SAA_in cpp::void_callback_t&& callback )
            {
                BL_MUTEX_GUARD( m_completionTasksLock );

                /*
                 * The completion queue doesn't discard the ready tasks, so they
                 * can be reused to avoid unnecessary heap operations
                 */

                auto task = m_completionTasksQueue -> top( false /* wait */ );

                om::ObjPtr< tasks::SimpleTaskImpl > taskImpl;

                if( task )
                {
                    taskImpl = om::qi< tasks::SimpleTaskImpl >( task );
                    taskImpl -> setCB( BL_PARAM_FWD( callback ) );
                }
                else
                {
                    taskImpl = tasks::SimpleTaskImpl::createInstance( BL_PARAM_FWD( callback ) );
                    task = om::qi< tasks::Task >( taskImpl );
                }

                m_completionTasksQueue -> push_back( task );
            }

            virtual void dispose() OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                if( m_outstandingCalls )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Outstanding calls "
                            << m_outstandingCalls
                        );

                    BL_RT_ASSERT(
                        0U == m_outstandingCalls,
                        "AsyncExecutorImpl::dispose(): async executor is being disposed while there "
                        "are still outstanding calls to it"
                        );
                }

                tasks::ExecutionQueue::disposeQueue( m_workers );
                tasks::ExecutionQueue::disposeQueue( m_externalTasksQueueLocal );

                /*
                 * Note that all scheduled completion tasks should always be executed fully and
                 * never canceled as cancellation of these may result in the respective worker
                 * task to never complete
                 */

                if( m_completionTasksQueue )
                {
                    m_completionTasksQueue -> flushAndDiscardReady();
                    tasks::ExecutionQueue::disposeQueue( m_completionTasksQueue );
                }

                if( m_threadPool )
                {
                    m_threadPool -> dispose();
                    m_threadPool.reset();
                }

                BL_NOEXCEPT_END()
            }

            virtual auto createOperation( SAA_in om::ObjPtr< AsyncOperationState >&& operationState )
                NOEXCEPT -> om::ObjPtr< AsyncOperation > OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                auto operation = m_operationsPool -> tryGet();

                if( ! operation )
                {
                    operation = ExecutorAsyncOperationImpl::template createInstance( *this );
                }

                BL_ASSERT( ! operation -> operationState() );
                operation -> operationState( std::move( operationState ) );

                ++m_outstandingCalls;
                operation -> active( true );

                return om::qi< AsyncOperation >( operation );
            }

            virtual void releaseOperation( SAA_in om::ObjPtr< AsyncOperation >& operation ) NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                auto operationImpl = om::qi< ExecutorAsyncOperationImpl >( operation );

                if( operationImpl -> task() )
                {
                    /*
                     * When operation is being released it must not have any outstanding
                     * async calls
                     *
                     * To ensure that there is no outstanding async call you must either call
                     * releaseOperation after the callback has been executed or from within
                     * the callback itself or you simply have to call cancel before you call
                     * releaseOperation
                     *
                     * Note also that AsyncOperation::cancel() currently only works async --
                     * i.e. after it returns there is no guarantee that the callback was
                     * called and that the operation is truly canceled / out of the queue
                     *
                     * Once you schedule an async operation the only guarantee is that the
                     * callback will eventually be called either normally or because of
                     * cancellation (in which case you get asio::error::operation_aborted)
                     */

                    operationImpl -> task() -> ensureNoCallsPending();
                }

                operationImpl -> resetState();

                m_operationsPool -> put( std::move( operationImpl ) );

                operation.reset();

                BL_NOEXCEPT_END()
            }

            virtual void asyncBegin(
                SAA_in                  const om::ObjPtr< AsyncOperation >&    operation,
                SAA_in                  AsyncOperation::callback_t&&           callback
                ) OVERRIDE
            {
                asyncBeginImpl( operation, BL_PARAM_FWD( callback ) );
            }
        };

        typedef om::ObjectImpl< AsyncExecutorImplT<> > AsyncExecutorImpl;

    } // detail

} // bl



#endif /* __BL_ASYNC_ASYNCEXECUTORIMPL_H_ */
