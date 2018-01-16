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

#ifndef __BL_TASKS_EXECUTIONQUEUEIMPL_H_
#define __BL_TASKS_EXECUTIONQUEUEIMPL_H_

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotify.h>
#include <baselib/tasks/TasksIncludes.h>
#include <baselib/tasks/TaskBase.h>

#include <baselib/core/Logging.h>
#include <baselib/core/TlsState.h>
#include <baselib/core/Intrusive.h>
#include <baselib/core/Pool.h>
#include <baselib/core/BaseIncludes.h>

#include <unordered_map>
#include <type_traits>

namespace bl
{
    namespace tasks
    {
        namespace detail
        {
            template
            <
                bool isBack
            >
            class InsertSelector;

            template
            <
            >
            class InsertSelector< true /* isBack */ >
            {
            public:

                template
                <
                    typename Container,
                    typename T
                >
                static void insert( Container& c, T&& x ) NOEXCEPT
                {
                    /*
                     * Note: the Container must support nothrow insert (see NOEXCEPT specifier)
                     */

                    c.push_back( std::forward< T >( x ) );
                }
            };

            template
            <
            >
            class InsertSelector< false /* isBack */ >
            {
            public:

                template
                <
                    typename Container,
                    typename T
                >
                static void insert( Container& c, T&& x ) NOEXCEPT
                {
                    /*
                     * Note: the Container must support nothrow insert (see NOEXCEPT specifier)
                     */

                    c.push_front( std::forward< T >( x ) );
                }
            };

        } // detail

        /**
         * @brief class ExecutionQueueImpl - forward declaration
         */

        template
        <
            typename E = void
        >
        class ExecutionQueueImplT;

        typedef om::ObjectImpl< ExecutionQueueImplT<>, true /* enableSharedPtr */ > ExecutionQueueImpl;

        /**
         * @brief class ExecutionQueueImpl - the execution queue implementation
         */

        template
        <
            typename E
        >
        class ExecutionQueueImplT :
            public ExecutionQueue
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( ExecutionQueueImplT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( Disposable )
                BL_QITBL_ENTRY( ExecutionQueue )
            BL_QITBL_END( ExecutionQueue )

        private:

            class TaskInfo :
                public intrusive::list_base_hook< intrusive::link_mode< intrusive::auto_unlink > >
            {
                BL_NO_COPY_OR_MOVE( TaskInfo )

            public:

                enum OwnerQueue
                {
                    None,
                    Pending,
                    Executing,
                    Ready,
                };

            private:

                om::ObjPtr< Task >              m_task;
                OwnerQueue                      m_ownerQueue;
                cpp::ScalarTypeIniter< bool >   m_freed;

            public:

                TaskInfo()
                    :
                    m_ownerQueue( None )
                {
                }

                ~TaskInfo() NOEXCEPT
                {
                    /*
                     * We can only destroy task info if it doesn't belong to
                     * any queue
                     */

                    BL_ASSERT( None == m_ownerQueue );
                }

                const om::ObjPtr< Task >& getTask() const NOEXCEPT
                {
                    return m_task;
                }

                void attachTask( SAA_in om::ObjPtr< Task >&& task ) NOEXCEPT
                {
                    m_task = std::move( task );
                }

                auto detachTask() NOEXCEPT -> om::ObjPtr< Task >
                {
                    return std::move( m_task );
                }

                auto getOwnerQueue() const NOEXCEPT -> OwnerQueue
                {
                    return m_ownerQueue;
                }

                void setOwnerQueue( SAA_in const OwnerQueue ownerQueue ) NOEXCEPT
                {
                    m_ownerQueue = ownerQueue;
                }

                bool freed() const NOEXCEPT
                {
                    return m_freed;
                }

                void freed( SAA_in const bool freed ) NOEXCEPT
                {
                    m_freed = freed;
                }
            };

            typedef intrusive::list <
                TaskInfo,
                intrusive::constant_time_size< false >
                >
                tasks_queue_t;

            typedef cpp::SafeUniquePtr< TaskInfo >                                  task_info_ptr_t;
            typedef om::ObjectImpl< SimplePool< task_info_ptr_t > >                 taskinfo_pool_t;

            typedef ExecutionQueueImplT< E >                                        this_type;
            typedef std::unordered_map< Task*, TaskInfo* >                          tasks_map_t;

            ThreadPool*                                                             m_localThreadPool;

            std::size_t                                                             m_maxExecuting;
            unsigned                                                                m_options;
            bool                                                                    m_shutdown;

            std::size_t                                                             m_executingCount;
            std::size_t                                                             m_readyCount;
            tasks_queue_t                                                           m_pending;
            tasks_queue_t                                                           m_executing;
            tasks_queue_t                                                           m_ready;
            tasks_map_t                                                             m_allTasks;

            mutable os::mutex                                                       m_lock;
            os::mutex                                                               m_lockEvents;
            os::condition_variable                                                  m_cvReady;

            om::ObjPtr< om::Proxy >                                                 m_notifyCB;
            unsigned                                                                m_eventsMask;
            om::ObjPtr< om::Proxy >                                                 m_observerThis;
            om::ObjPtr< taskinfo_pool_t >                                           m_taskInfoPool;

        protected:

            ExecutionQueueImplT(
                SAA_in                  const unsigned                              options = OptionKeepFailed,
                SAA_in                  const std::size_t                           maxExecuting = 0
                )
                :
                m_localThreadPool( nullptr ),
                m_maxExecuting( maxExecuting ),
                m_options( options ),
                m_shutdown( false ),
                m_executingCount( 0U ),
                m_readyCount( 0U ),
                m_eventsMask( 0 ),
                m_taskInfoPool( taskinfo_pool_t::template createInstance< taskinfo_pool_t >() )
            {
                m_observerThis = om::ProxyImpl::createInstance< om::Proxy >();
                m_observerThis -> connect( this );
            }

            ~ExecutionQueueImplT() NOEXCEPT
            {
                BL_MUTEX_GUARD( m_lock );

                /*
                 * Must be empty and disposed
                 */

                BL_ASSERT( ! m_observerThis );
                BL_ASSERT( isEmptyInternal() );
            }

            bool keepTask( SAA_in const om::ObjPtrCopyable< Task >& task ) const NOEXCEPT
            {
                const bool keep = task -> isFailed() ?
                    ( 0U != ( m_options & OptionKeepFailed ) ) :
                    ( 0U != ( m_options & OptionKeepSuccessful ) );

                return keep;
            }

            std::size_t getReadyOrExecuting() const NOEXCEPT
            {
                return m_readyCount + m_executingCount;
            }

            std::size_t getMaxReadyOrExecuting() const NOEXCEPT
            {
                if( m_notifyCB )
                {
                    const auto notifyCB = m_notifyCB -> tryAcquireRef< ExecutionQueueNotify >();

                    if( notifyCB )
                    {
                        return notifyCB -> maxReadyOrExecuting();
                    }
                }

                return 0U;
            }

            cpp::void_callback_noexcept_t getEventNotifyCB(
                SAA_in                  const ExecutionQueueNotify::EventId         eventId,
                SAA_in_opt              const om::ObjPtrCopyable< Task >&           task
                ) const
            {
                cpp::void_callback_noexcept_t onNotify;

                if( m_notifyCB && ( m_eventsMask & eventId ) )
                {
                    om::ObjPtrCopyable< ExecutionQueueNotify > notifyCB =
                        m_notifyCB -> tryAcquireRef< ExecutionQueueNotify >();

                    if( notifyCB )
                    {
                        onNotify = cpp::bind(
                            &ExecutionQueueNotify::onEvent,
                            notifyCB,
                            eventId,
                            task
                            );
                    }
                }

                return onNotify;
            }

            static void onReadyObserver(
                SAA_in                  const om::ObjPtrCopyable< om::Proxy >&      observerThis,
                SAA_in                  const om::ObjPtrCopyable< Task >&           task
                ) NOEXCEPT
            {
                /*
                 * We of course only notify the execution queue if it has not
                 * been released already
                 *
                 * Note that tasks which run on an execution queue usually are
                 * flushed before the queue is released, however the ready
                 * notifications are queued up (and they're *not* holding the queue
                 * lock) so it may happen that some tasks are attempting to call
                 * onReady() on an execution queue which has been released
                 * already
                 */

                const om::ObjPtrCopyable< ExecutionQueue > eq =
                    observerThis -> tryAcquireRef< ExecutionQueue >();

                if( eq )
                {
                    const auto eqImpl = om::qi< ExecutionQueueImpl >( eq );
                    eqImpl -> onReady( task );
                }
            }

            void moveTaskToReadyQueue( SAA_inout TaskInfo* taskInfo )
            {
                taskInfo -> unlink();
                taskInfo -> setOwnerQueue( TaskInfo::Ready );
                m_ready.push_back( *taskInfo );
                ++m_readyCount;
            }

            void moveExecutingTaskToPendingQueue( SAA_inout TaskInfo* taskInfo )
            {
                BL_ASSERT( TaskInfo::Executing == taskInfo -> getOwnerQueue() );
                taskInfo -> unlink();
                taskInfo -> setOwnerQueue( TaskInfo::Pending );
                m_pending.push_back( *taskInfo );
                --m_executingCount;
            }

            void onReady( SAA_in const om::ObjPtrCopyable< Task >& task ) NOEXCEPT
            {
                cpp::void_callback_noexcept_t onNotify;
                cpp::void_callback_noexcept_t onNotifyAllCompleted;

                BL_MUTEX_GUARD( m_lockEvents );

                {
                    BL_MUTEX_GUARD( m_lock );

                    if( ! m_observerThis )
                    {
                        /*
                         * The object has already been disposed -
                         * don't try to do anything
                         */

                        return;
                    }

                    std::exception_ptr continuationException;

                    bool continuationIsSelf = false;
                    auto taskInfo = getTaskInfoPtrFromTask( task.get() );

                    try
                    {
                        const auto continuationTask = task -> continuationTask();

                        if( continuationTask )
                        {
                            /*
                             * Push the continuation task to the front of the execution queue and attempt
                             * to schedule it immediately
                             */

                            if( om::areEqual( continuationTask, task ) )
                            {
                                moveExecutingTaskToPendingQueue( taskInfo );
                                padExecutingQueueNothrow();

                                continuationIsSelf = true;
                            }
                            else
                            {
                                pushInternalNoLock< false /* isBack */ >( continuationTask, false /* dontSchedule */ );
                            }
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

                        task -> exception( continuationException );
                    }

                    if( ! continuationIsSelf )
                    {
                        --m_executingCount;

                        task -> setCompletedState();

                        if( keepTask( task ) )
                        {
                            moveTaskToReadyQueue( taskInfo );

                            onNotify = getEventNotifyCB( ExecutionQueueNotify::TaskReady, task );
                        }
                        else
                        {
                            unlinkAndDestroy( taskInfo );

                            onNotify = getEventNotifyCB( ExecutionQueueNotify::TaskDiscarded, task );
                        }

                        padExecutingQueueNothrow();

                        m_cvReady.notify_all();
                    }
                }

                if( onNotify )
                {
                    onNotify();
                }

                {
                    BL_MUTEX_GUARD( m_lock );

                    if( m_pending.empty() && m_executing.empty() )
                    {
                        BL_ASSERT( 0U == m_executingCount );

                        onNotifyAllCompleted = getEventNotifyCB(
                            ExecutionQueueNotify::AllTasksCompleted,
                            om::ObjPtrCopyable< Task >::acquireRef( nullptr )
                            );
                    }
                }

                if( onNotifyAllCompleted )
                {
                    onNotifyAllCompleted();
                }
            }

            void padExecutingQueueNothrow() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                while( ! m_pending.empty() )
                {
                    const auto maxReadyOrExecuting = getMaxReadyOrExecuting();
                    const auto readyOrExecuting = getReadyOrExecuting();

                    if(
                        ( m_maxExecuting && m_executingCount >= m_maxExecuting ) ||
                        ( maxReadyOrExecuting && readyOrExecuting >= maxReadyOrExecuting )
                        )
                    {
                        /*
                         * If we have any throttle limit let's enforce it
                         */

                        break;
                    }

                    auto& taskInfo = m_pending.front();

                    /*
                     * The execution queue must support SharedPtr
                     */

                    const auto eqThis = om::getSharedPtr< ExecutionQueue >( this );

                    taskInfo.getTask() -> scheduleNothrow(
                        eqThis,
                        cpp::bind(
                            &this_type::onReadyObserver,
                            om::ObjPtrCopyable< om::Proxy >( m_observerThis ),
                            om::ObjPtrCopyable< Task >( taskInfo.getTask() )
                            )
                        );

                    taskInfo.unlink();
                    taskInfo.setOwnerQueue( TaskInfo::Executing );
                    m_executing.push_back( taskInfo );

                    ++m_executingCount;
                }

                BL_NOEXCEPT_END()
            }

            template
            <
                bool isBack
            >
            void pushInternalNoLock(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  dontSchedule = false
                )
            {
                typedef detail::InsertSelector< isBack > inserter_t;

                const auto pos = m_allTasks.find( task.get() );

                if( pos != m_allTasks.end() )
                {
                    /*
                     * The task is already in the queue; it must be
                     * in the ready queue otherwise we assert; if it
                     * is in the ready queue we just re-schedule it
                     */

                    TaskInfo* taskInfo = pos -> second;
                    BL_ASSERT( TaskInfo::Ready == taskInfo -> getOwnerQueue() );

                    if( false == dontSchedule )
                    {
                        taskInfo -> unlink();
                        taskInfo -> setOwnerQueue( TaskInfo::Pending );
                        inserter_t::insert( m_pending, *taskInfo );

                        --m_readyCount;
                    }
                }
                else
                {
                    auto taskInfo = m_taskInfoPool -> tryGet();

                    if( ! taskInfo )
                    {
                        taskInfo = cpp::SafeUniquePtr< TaskInfo >::attach( new TaskInfo() );
                    }

                    taskInfo -> attachTask( om::copy( task ) );

                    m_allTasks[ task.get() ] = taskInfo.get();

                    /*
                     * Note: the operations below must be nothrow!
                     */

                    taskInfo -> setOwnerQueue( dontSchedule ? TaskInfo::Ready : TaskInfo::Pending );
                    inserter_t::insert( dontSchedule ? m_ready : m_pending, *taskInfo );
                    taskInfo.release();

                    if( dontSchedule )
                    {
                        ++m_readyCount;
                    }
                }

                padExecutingQueueNothrow();
            }

            template
            <
                bool isBack
            >
            void pushInternal(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  dontSchedule = false
                )
            {
                /*
                 * Each task can only belong to a single execution queue
                 * and when initially assigned in the queue it can only
                 * be in non-running state.
                 */

                BL_ASSERT( Task::Running != task -> getState() );

                BL_MUTEX_GUARD( m_lock );

                pushInternalNoLock< isBack >( task, dontSchedule );
            }

            /**
             * @brief Unlinks an element and returns it in the pool
             *
             * Note: This is expected to be a nothrow operation
             */

            void unlinkAndDestroy( SAA_inout TaskInfo* taskInfo ) NOEXCEPT
            {
                /*
                 * std::unordered_map::erase() only throws if the hasher or key_equal throw
                 * the rest of the operations are nothrow
                 */

                BL_VERIFY( 1 == m_allTasks.erase( taskInfo -> getTask().get() ) );
                taskInfo -> unlink();
                taskInfo -> setOwnerQueue( TaskInfo::None );
                taskInfo -> detachTask();

                m_taskInfoPool -> put( task_info_ptr_t::attach( taskInfo ) );
            }

            /**
             * @brief Clears the queue safely by maintaining the state of m_allTasks
             *
             * Note: This is expected to be a nothrow operation
             */

            void clearQueue( SAA_inout tasks_queue_t& queue, SAA_inout_opt std::size_t* counter = nullptr ) NOEXCEPT
            {
                while( ! queue.empty() )
                {
                    unlinkAndDestroy( &queue.front() );

                    if( counter )
                    {
                        --( *counter );
                    }
                }

                BL_ASSERT( nullptr == counter || 0U == *counter );
            }

            /**
             * @brief Returns a task info from task
             */

            TaskInfo* getTaskInfoPtrFromTask( SAA_inout Task* task, SAA_in bool allowMissing = false )
            {
                BL_ASSERT( task );

                const auto pos = m_allTasks.find( task );

                const bool found = pos != m_allTasks.end();

                if( ! allowMissing )
                {
                    BL_CHK(
                        false,
                        found,
                        BL_MSG()
                            << "Attempt to wait on a task that is not in the execution queue"
                        );
                }

                return found ? pos -> second : nullptr;
            }

            bool hasPendingOrExecuting() const NOEXCEPT
            {
                return ( false == m_pending.empty() || false == m_executing.empty() );
            }

            /**
             * @brief Flushes the queue; this is an internal function and is expected to be called
             * only when holding the lock
             */

            void flushInternal(
                SAA_inout               os::mutex_unique_lock&                      guard,
                SAA_in                  const bool                                  wait,
                SAA_in_opt              const bool                                  discardPending,
                SAA_in_opt              const bool                                  nothrowIfFailed,
                SAA_in_opt              const bool                                  discardReady,
                SAA_in_opt              const bool                                  cancelExecuting
                )
            {
                if( ! discardPending )
                {
                    padExecutingQueueNothrow();
                }

                if( isEmptyInternal() )
                {
                    return;
                }

                if( discardPending )
                {
                    clearQueue( m_pending );
                }

                if( cancelExecuting )
                {
                    for( const auto& taskInfo : m_executing )
                    {
                        taskInfo.getTask() -> requestCancel();
                    }
                }

                if( wait && hasPendingOrExecuting() )
                {
                    /*
                     * Wait until there are no more pending or executing items
                     */

                    const auto cb = [ this ]() -> bool
                    {
                        if( ! hasPendingOrExecuting() )
                        {
                            return true;
                        }

                        return false;
                    };

                    m_cvReady.wait( guard, cb );
                }

                if( false == nothrowIfFailed )
                {
                    /*
                     * Must check the ready list for failed items and throw
                     * if necessary (we only throw the first error)
                     */

                    std::exception_ptr exception = nullptr;

                    for( const auto& taskInfo : m_ready )
                    {
                        if( taskInfo.getTask() -> isFailed() )
                        {
                            exception = taskInfo.getTask() -> exception();
                            BL_ASSERT( nullptr != exception );
                            break;
                        }
                    }

                    if( exception )
                    {
                        /*
                         * If an exception is to be thrown due to nothrowIfFailed=false
                         * then we must first clear the ready list
                         */

                        clearQueue( m_ready, &m_readyCount );
                        cpp::safeRethrowException( exception );
                    }
                }

                if( discardReady )
                {
                    clearQueue( m_ready, &m_readyCount );
                }
            }

            bool isEmptyInternal() const NOEXCEPT
            {
                const auto allEmpty = ( m_pending.empty() && m_executing.empty() && m_ready.empty() );

                BL_ASSERT( allEmpty == m_allTasks.empty() );

                if( allEmpty )
                {
                    BL_ASSERT( 0U == m_readyCount );
                    BL_ASSERT( 0U == m_executingCount );
                }

                return allEmpty;
            }

            om::ObjPtr< Task > getFirstReady( SAA_in const bool wait = true, SAA_in const bool remove = true )
            {
                os::mutex_unique_lock g( m_lock );

                /*
                 * Check to pad the execution queue if we have been throttled
                 */

                padExecutingQueueNothrow();

                /*
                 * We only want to wait if wait=true, there are no ready
                 * tasks *and* there are other tasks in the queue (either
                 * pending or executing)
                 */

                if( wait && m_ready.empty() && false == isEmptyInternal() )
                {
                    BL_ASSERT( 0U == m_readyCount );

                    const auto cb = [ this ]() -> bool
                    {
                        if( isEmptyInternal() )
                        {
                            return true;
                        }

                        if( ! m_ready.empty() )
                        {
                            return true;
                        }

                        return false;
                    };

                    m_cvReady.wait( g, cb );
                }

                if( m_ready.empty() )
                {
                    BL_ASSERT( 0U == m_readyCount );

                    return nullptr;
                }

                auto* taskInfo = &m_ready.front();
                auto ptr = om::copy( taskInfo -> getTask() );

                if( remove )
                {
                    unlinkAndDestroy( taskInfo );
                    --m_readyCount;
                }

                return ptr;
            }

            tasks_queue_t& getQueue( SAA_in const QueueId queueId ) NOEXCEPT
            {
                switch( queueId )
                {
                    default:
                        BL_ASSERT( false && "Invalid queue id" );
                        return m_ready;

                    case Ready:
                        return m_ready;

                    case Executing:
                        return m_executing;

                    case Pending:
                        return m_pending;
                }
            }

            void waitInternal(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  prioritize,
                SAA_in                  const bool                                  cancel
                )
            {
                os::mutex_unique_lock g( m_lock );

                /*
                 * Check to pad the execution queue if we have been throttled
                 */

                padExecutingQueueNothrow();

                /*
                 * First we check if the task is in already completed state and return
                 * if this is the case. If the task is not in 'Completed' state then
                 * we assume it is scheduled in this execution queue and will throw,
                 * from getTaskInfoPtrFromTask( ... ), if it isn't.
                 */

                {
                    auto taskInfo = getTaskInfoPtrFromTask( task.get(), true /* allowMissing */ );

                    if( ! taskInfo )
                    {
                        BL_CHK(
                            false,
                            Task::Completed == task -> getState(),
                            BL_MSG()
                                << "Attempt to wait on a task that is not in the execution queue"
                            );

                        return;
                    }

                    const auto ownerQueue = taskInfo -> getOwnerQueue();

                    if( TaskInfo::Ready == ownerQueue || ( TaskInfo::Pending == ownerQueue && cancel ) )
                    {
                        /*
                         * If the task is either in the ready queue or in the pending queue and cancel was
                         * requested then there is nothing to do except to unlink it and return
                         */

                        unlinkAndDestroy( taskInfo );

                        if( TaskInfo::Ready == ownerQueue )
                        {
                            --m_readyCount;
                        }

                        return;
                    }

                    /*
                     * If we're here then the task is either already executing or pending, but
                     * cancel is false. In both cases we need to wait for it to complete.
                     */

                    if( prioritize && TaskInfo::Pending == ownerQueue )
                    {
                        /*
                         * If the task is in the pending queue we must prioritize it at the head
                         * of the queue, so it will complete as soon as possible
                         */

                        taskInfo -> unlink();
                        m_pending.push_front( *taskInfo );
                    }
                }

                if( cancel )
                {
                    task -> requestCancel();
                }

                /*
                 * The callback that will be used to wait for the task to complete
                 */

                const auto cb = [ this, &task ]() -> bool
                {
                    /*
                     * Check to pad the execution queue if we have been throttled
                     */

                    padExecutingQueueNothrow();

                    const auto i = m_allTasks.find( task.get() );

                    if( i == m_allTasks.end() )
                    {
                        /*
                         * The task has completed and was discarded (since keepSuccessful is false)
                         */

                        return true;
                    }

                    if( TaskInfo::Ready == i -> second -> getOwnerQueue() )
                    {
                        /*
                         * The task completed normally and it is in the ready queue
                         */

                        return true;
                    }

                    return false;
                };

                /*
                 * Let's wait for the task to complete
                 */

                m_cvReady.wait( g, cb );

                /*
                 * Check if the task is in the ready queue to remove it from there
                 */

                const auto pos = m_allTasks.find( task.get() );

                if( pos != m_allTasks.end() )
                {
                    auto* taskInfo = pos -> second;
                    BL_ASSERT( TaskInfo::Ready == taskInfo -> getOwnerQueue() );

                    unlinkAndDestroy( taskInfo );
                    --m_readyCount;
                }
            }

        public:

            virtual bool isEmpty() const NOEXCEPT OVERRIDE
            {
                BL_MUTEX_GUARD( const_cast< this_type* >( this ) -> m_lock );
                return isEmptyInternal();
            }

            virtual bool hasReady() const NOEXCEPT OVERRIDE
            {
                return ( ! m_ready.empty() );
            }

            virtual bool hasExecuting() const NOEXCEPT OVERRIDE
            {
                return ( ! m_executing.empty() );
            }

            virtual bool hasPending() const NOEXCEPT OVERRIDE
            {
                return ( ! m_pending.empty() );
            }

            virtual std::size_t size() const NOEXCEPT OVERRIDE
            {
                return m_allTasks.size();
            }

            virtual void setThrottleLimit( SAA_in const std::size_t maxExecuting = 0 ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                m_maxExecuting = maxExecuting;
            }

            virtual void setOptions( SAA_in const unsigned options = OptionKeepFailed ) OVERRIDE
            {
                m_options = options;
            }

            virtual void setNotifyCallback(
                SAA_in                  om::ObjPtr< om::Proxy >&&                   notifyCB,
                SAA_in                  const unsigned                              eventsMask = ExecutionQueueNotify::AllEvents
                ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                m_notifyCB = std::move( notifyCB );
                m_eventsMask = eventsMask;
            }

            virtual ThreadPool* getLocalThreadPool() const NOEXCEPT OVERRIDE
            {
                return m_localThreadPool;
            }

            virtual void setLocalThreadPool( SAA_inout ThreadPool* threadPool ) NOEXCEPT OVERRIDE
            {
                m_localThreadPool = threadPool;
            }

            virtual om::ObjPtr< Task > push_back(
                SAA_in                  cpp::void_callback_t&&                      cbTask,
                SAA_in                  const bool                                  dontSchedule = false
                ) OVERRIDE
            {
                auto task = SimpleTaskImpl::createInstance< Task >( std::forward< cpp::void_callback_t >( cbTask ) );
                push_back( task, dontSchedule );
                return task;
            }

            virtual void push_back(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  dontSchedule = false
                ) OVERRIDE
            {
                pushInternal< true /* isBack */ >( task, dontSchedule );
            }

            virtual om::ObjPtr< Task > push_front(
                SAA_in                  cpp::void_callback_t&&                      cbTask,
                SAA_in                  const bool                                  dontSchedule = false
                ) OVERRIDE
            {
                auto task = SimpleTaskImpl::createInstance< Task >( std::forward< cpp::void_callback_t >( cbTask ) );
                push_front( task, dontSchedule );
                return task;
            }

            virtual void push_front(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  dontSchedule = false
                ) OVERRIDE
            {
                pushInternal< false /* isBack */ >( task, dontSchedule );
            }

            virtual om::ObjPtr< Task > pop( SAA_in const bool wait = true ) OVERRIDE
            {
                return getFirstReady( wait, true /* remove */ );
            }

            virtual om::ObjPtr< Task > top( SAA_in const bool wait = true ) OVERRIDE
            {
                return getFirstReady( wait, false /* remove */ );
            }

            virtual void scanQueue(
                SAA_in                  const QueueId                               queueId,
                SAA_in                  const task_callback_type&                   cbTasks
                ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                for( const auto& taskInfo : getQueue( queueId ) )
                {
                    cbTasks( taskInfo.getTask() );
                }
            }

            virtual void wait(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in_opt              const bool                                  cancel = false
                ) OVERRIDE
            {
                waitInternal( task, true /* prioritize */, cancel );
            }

            virtual void waitNoPrioritize( SAA_in const om::ObjPtr< Task >& task ) OVERRIDE
            {
                waitInternal( task, false /* prioritize */, false /* cancel */ );
            }

            virtual bool prioritize(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  wait
                ) OVERRIDE
            {
                if( wait )
                {
                    m_lock.lock();
                }
                else if( ! m_lock.try_lock() )
                {
                    return false;
                }

                BL_SCOPE_EXIT( m_lock.unlock(); );

                auto taskInfo = getTaskInfoPtrFromTask( task.get() );
                const auto ownerQueue = taskInfo -> getOwnerQueue();

                if( TaskInfo::Pending == ownerQueue )
                {
                    /*
                     * If the task is in the pending queue we must prioritize it at the head
                     * of the queue, so it will complete as soon as possible
                     */

                    taskInfo -> unlink();
                    m_pending.push_front( *taskInfo );

                    return true;
                }

                return false;
            }

            virtual void flush(
                SAA_in_opt              const bool                                  discardPending = false,
                SAA_in_opt              const bool                                  nothrowIfFailed = false,
                SAA_in_opt              const bool                                  discardReady = false,
                SAA_in_opt              const bool                                  cancelExecuting = false
                ) OVERRIDE
            {
                os::mutex_unique_lock g( m_lock );

                flushInternal( g, true /* wait */, discardPending, nothrowIfFailed, discardReady, cancelExecuting );
            }

            virtual void forceFlushNoThrow( SAA_in const bool wait = true ) NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                os::mutex_unique_lock g( m_lock );

                flushInternal(
                    g,
                    wait,
                    true /* discardPending */,
                    true /* nothrowIfFailed */,
                    true /* discardReady */,
                    true /* cancelExecuting */
                    );

                BL_ASSERT( false == wait || isEmptyInternal() );

                BL_NOEXCEPT_END()
            }

            virtual bool cancel(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in_opt              const bool                                  wait = true
                ) OVERRIDE
            {
                if( wait )
                {
                    /*
                     * This is just a wrapper for wait( ... )
                     *
                     * If we wait on the task we know it cannot be truly canceled
                     */

                    this_type::wait( task, true /* cancel */ );

                    return false;
                }

                /*
                 * This is async cancellation
                 *
                 * If the task is already executing we can't cancel it truly,
                 * but we can request cancellation and return false
                 */

                BL_MUTEX_GUARD( m_lock );

                auto taskInfo = getTaskInfoPtrFromTask( task.get(), true /* allowMissing */ );

                if( taskInfo )
                {
                    const auto ownerQueue = taskInfo -> getOwnerQueue();

                    if( TaskInfo::Pending == ownerQueue )
                    {
                        /*
                         * If the task is in the pending queue then it can be canceled
                         *
                         * If the OptionKeepCanceled flag is set we just move the task
                         * directly into the ready queue
                         */

                        if( m_options & OptionKeepCanceled )
                        {
                            moveTaskToReadyQueue( taskInfo );
                        }
                        else
                        {
                            unlinkAndDestroy( taskInfo );
                        }

                        return true;
                    }
                }

                /*
                 * The task cannot be canceled because of one of the following:
                 *
                 * -- it is not in the queue
                 * -- it has already started executing
                 * -- it has executed already
                 */

                return false;
            }

            /**
             * @brief Cancels all tasks in the queue and discards
             * any pending ones and ready ones
             *
             * @param wait If true the queue is assumed to be in
             * static state (i.e. no one is pushing items in it
             * and no one is expected to be pulling items from it)
             * and it will wait until all tasks are finished
             *
             * Note: don't pass true to 'wait' parameter if the queue
             * is in 'dynamic state' (as per the description above)
             * otherwise you may have a hang
             */

            virtual void cancelAll( SAA_in const bool wait ) NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock g( m_lock );

                /*
                 * If wait=true then this is just a wrapper for flush( ... )
                 *
                 * Otherwise it will just cancel without waiting
                 */

                flushInternal(
                   g,
                   wait,
                   true  /* discardPending */,
                   true  /* nothrowIfFailed */,
                   true  /* discardReady */,
                   true  /* cancelExecuting */
                   );
            }

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                os::mutex_unique_lock g( m_lock );

                flushInternal(
                    g,
                    true /* wait */,
                    true /* discardPending */,
                    true /* nothrowIfFailed */,
                    true /* discardReady */,
                    true /* cancelExecuting */
                    );

                BL_ASSERT( isEmptyInternal() );

                if( m_observerThis )
                {
                    m_observerThis -> disconnect();
                    m_observerThis.reset();
                }

                BL_LOG( Logging::trace(), BL_MSG() << "Execution queue " << this << " was shut down" );

                BL_NOEXCEPT_END()
            }
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_EXECUTIONQUEUEIMPL_H_ */
