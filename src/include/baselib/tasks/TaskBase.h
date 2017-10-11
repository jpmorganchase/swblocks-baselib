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

#ifndef __BL_TASKS_TASKBASE_H_
#define __BL_TASKS_TASKBASE_H_

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/TasksIncludes.h>

#include <baselib/core/BaseIncludes.h>

/*****************************************************************************
 * Some important design information to aid correct implementation
 * of task objects and to avoid deadlocks
 *
 * Tasks are normally scheduled on an execution queue object (the execution
 * queue is the task orchestration tool / mechanism)
 *
 * To schedule the pending tasks the execution queue normally (re-)uses
 * the threads which push the tasks for execution, as well as arbitrary
 * threads from the thread pool which call back to notify when a task
 * is ready
 *
 * All tasks which derive from TaskBase have a member called m_lock
 * which below will also be referred to as the 'task lock'; this lock
 * is meant to protect the internal task state in general
 *
 * Task objects should never call the ready callback while holding
 * the task lock; the notifyReady() helper encapsulates this logic
 * which means calls to notifyReady() should always be made while *not*
 * holding the task lock
 *
 * The BL_TASKS_HANDLER_* macros below normally acquire the task lock
 * except for the BL_TASKS_HANDLER_BEGIN_NOLOCK() macro which should
 * be used very carefully
 *
 * Since the execution queue will hold the queue lock while trying to
 * schedule tasks that means that the order in which the locks should
 * always be acquired is the following:
 *
 * 1) The execution queue lock
 * 2) The task lock
 *
 * And this order of locking is already determined by the execution queue
 * design and implementation (i.e. it is an invariant in the current
 * execution queue design)
 *
 * What that means is that these two locks should of course *always* be
 * acquired in that order to avoid deadlocks and what that means in
 * practice is the following:
 *
 * Task code which holds the task lock should *never* make calls back into
 * the execution queue which owns the task; BTW note that the execution queue
 * we discuss here is the one which already owns the task or it can
 * potentially be owning / executing the task in question - i.e. a task can
 * of course call into execution queues which do not own the task in question
 * and it is known that these execution queues can never attempt to schedule
 * this task
 *
 * Another very important design invariant is that tasks which block on other
 * tasks are best to be avoided, but especially if they are running on the
 * same thread pool and / or on the same execution queue. The main issue
 * with tasks blocking on each other is that 1) they will require a minimum
 * # of threads in the thread pool that is larger than 1 which means we can't
 * run the code in single-threaded mode which will hinder debugging and 2)
 * even if you have large # of threads in the pool you can still end up with
 * a global deadlock situation where all the tasks in the thread pool are
 * waiting on other dependent tasks, but none of these dependent tasks can
 * be scheduled as there are no available threads in the thread pool (as we
 * have fixed size thread pool)
 *
 * As a general rule it is best to design your tasks to not have to block
 * on each other and more generally to not block on anything for prolonged
 * period of time (as if they are long running they are going to be consuming
 * threads from the thread pool which is a scarce resource) and if you
 * absolutely have to create tasks that block on other tasks then you need to
 * make sure 1) they are very small # (ideally no more than 1-2) and 2) that
 * blocking their execution due to lack of threads is not going to be feasible
 * scenario in practice (e.g. if your thread pool size is much bigger than the
 * number of such tasks that will ever be scheduled)
 *
 * Another way to deal with tasks which need to block on other tasks is to make
 * sure they always run on separate thread pools - e.g. a processing task
 * executing on the general purpose thread pool can block / wait on I/O task
 * which is executing on the (separate) I/O thread pool and that would never
 * cause a deadlock situation
 */

/*****************************************************************************
 * Basic tasks utility macros
 */

#define BL_TASKS_HANDLER_BEGIN_IMPL( lockExpr ) \
    BL_NOEXCEPT_BEGIN() \
    std::exception_ptr __eptr42 = nullptr; \
    bool __isExpectedException42 = false; \
    { \
        try \
        { \
            do \
            { \
                lockExpr \

#define BL_TASKS_HANDLER_BEGIN() \
    BL_TASKS_HANDLER_BEGIN_IMPL( BL_MUTEX_GUARD( bl::tasks::TaskBase::m_lock ); ) \

#define BL_TASKS_HANDLER_BEGIN_NOLOCK() \
    BL_TASKS_HANDLER_BEGIN_IMPL( ; ) \

#define BL_TASKS_HANDLER_CHK_CANCEL_IMPL_THROW() \
    if( bl::tasks::TaskBase::isCanceled() ) \
    { \
         BL_CHK_EC_NM( bl::asio::error::operation_aborted ); \
    } \

#define BL_TASKS_HANDLER_CHK_ASYNC_RESULT_IMPL_THROW( result ) \
    if( result.exception ) \
    { \
        bl::cpp::safeRethrowException( result.exception ); \
    } \
    BL_CHK_EC_NM( result.code ); \
    BL_TASKS_HANDLER_CHK_CANCEL_IMPL_THROW()

#define BL_TASKS_HANDLER_CHK_EC( ec ) \
    if( ec ) \
    { \
        auto __exception42 = bl::SystemException::create( ec, BL_SYSTEM_ERROR_DEFAULT_MSG ); \
        this -> enhanceException( __exception42 ); \
        __eptr42 = std::make_exception_ptr( std::move( __exception42 ) ); \
        if( \
            bl::asio::error::operation_aborted == __exception42.code() || \
            this -> isExpectedException( __eptr42, __exception42, &__exception42.code() ) \
            ) \
        { \
            __isExpectedException42 = true; \
        } \
        break; \
    } \

#define BL_TASKS_HANDLER_CHK_CANCEL_IMPL() \
    if( bl::tasks::TaskBase::isCanceled() ) \
    { \
         BL_TASKS_HANDLER_CHK_EC( bl::asio::error::operation_aborted ); \
    } \

#define BL_TASKS_HANDLER_BEGIN_CHK_EC() \
    BL_TASKS_HANDLER_BEGIN() \
    BL_TASKS_HANDLER_CHK_EC( ec ); \
    BL_TASKS_HANDLER_CHK_CANCEL_IMPL() \

/**
 * @brief This macro implements the default handler prolog code for an async
 * operation callback
 *
 * Note that the TaskBase::cancelTask() implementation would normally clear
 * the async operation object pointer as it can't be used after cancel has
 * been called on it and we need to guard against this case and ensure we
 * throw bl::asio::error::operation_aborted in the case the operation was
 * canceled, but the callback was still called with Result object
 * indicating success
 */

#define BL_TASKS_HANDLER_CHK_ASYNC_RESULT_IMPL( result ) \
    if( result.exception ) \
    { \
        __eptr42 = result.exception; \
        break; \
    } \
    BL_TASKS_HANDLER_CHK_EC( result.code ); \
    BL_TASKS_HANDLER_CHK_CANCEL_IMPL()

#define BL_TASKS_HANDLER_BEGIN_CHK_ASYNC_RESULT() \
    BL_TASKS_HANDLER_BEGIN() \
    BL_TASKS_HANDLER_CHK_ASYNC_RESULT_IMPL( result )

#define BL_TASKS_HANDLER_END_IMPL( expr ) \
            } \
            while( false ); \
            if( __eptr42 ) \
            { \
                bl::tasks::TaskBase::notifyReady( __eptr42, __isExpectedException42 ); \
            } \
            else \
            { \
                expr \
            } \
        } \
        catch( bl::eh::exception& e ) \
        { \
            this -> enhanceException( e ); \
            __eptr42 = std::current_exception(); \
        } \
        catch( std::exception& ) \
        { \
            __eptr42 = std::current_exception(); \
        } \
    } \
    if( __eptr42 ) \
    { \
        bl::tasks::TaskBase::notifyReady( __eptr42, __isExpectedException42 ); \
    } \
    BL_NOEXCEPT_END() \

#define BL_TASKS_HANDLER_END() \
    BL_TASKS_HANDLER_END_IMPL( bl::tasks::TaskBase::notifyReady(); ) \

#define BL_TASKS_HANDLER_END_NOTREADY() \
    BL_TASKS_HANDLER_END_IMPL( ; ) \

namespace bl
{
    namespace tasks
    {
        typedef cpp::function< om::ObjPtr< Task > () >                                          GetContinuationCallback;
        typedef cpp::function< om::ObjPtr< Task > ( SAA_inout Task* finishedTask ) >            ContinuationCallback;

        typedef cpp::function< void ( SAA_in_opt const std::exception_ptr& eptr ) NOEXCEPT >    CompletionCallback;
        typedef cpp::function< void ( SAA_in const CompletionCallback& onReady ) >              ScheduleCallback;

        /**
         * @returns - bool
         *
         * 'true' means the async operation was scheduled and the callback will be
         * invoked once completed where 'false' means that the async operation was
         * not scheduled and the callback will *not* be invoked as it did not have
         * to be scheduled or because it completed synchronously
         */

        typedef cpp::function< bool ( SAA_in const CompletionCallback& onReady ) >              IfScheduleCallback;

        typedef cpp::function< time::time_duration () >                                         AdjustableTimerCallback;

        /**
         * @brief class ForwarderTaskBase - A forwarder task class which is intended to facilitate for a task
         * to forward calls to another task (target task) which is obtained dynamically each time
         *
         * This mechanism allows for one task to execute multiple other tasks dynamically and switch the tasks
         * as necessary on each call or via continuations (e.g. one possible typical such use is for wrapper
         * tasks - see description of WrapperTaskBase class below)
         */

        template
        <
            typename E = void
        >
        class ForwarderTaskBaseT :
            public Task
        {
            BL_CTR_DEFAULT( ForwarderTaskBaseT, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( ForwarderTaskBaseT, Task )

        protected:

            virtual auto getTargetTask( SAA_inout os::mutex_unique_lock& guard ) const NOEXCEPT
                -> const om::ObjPtr< Task >& = 0;

        public:

            virtual bool isFailed() const NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                return getTargetTask( guard ) -> isFailed();
            }

            virtual bool isFailedOrFailing() const NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                return getTargetTask( guard ) -> isFailedOrFailing();
            }

            virtual Task::State getState() const NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                return getTargetTask( guard ) -> getState();
            }

            virtual void setCompletedState() NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                getTargetTask( guard ) -> setCompletedState();
            }

            virtual std::exception_ptr exception() const NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                return getTargetTask( guard ) -> exception();
            }

            virtual void exception( SAA_in_opt const std::exception_ptr& exception ) NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                getTargetTask( guard ) -> exception( exception );
            }

            virtual const std::string& name() const NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                return getTargetTask( guard ) -> name();
            }

            virtual void scheduleNothrow(
                SAA_in                  const std::shared_ptr< ExecutionQueue >&    eq,
                SAA_in                  cpp::void_callback_noexcept_t&&             callbackReady
                ) NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                getTargetTask( guard ) -> scheduleNothrow( eq, BL_PARAM_FWD( callbackReady ) );
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock guard;

                getTargetTask( guard ) -> requestCancel();
            }

            virtual om::ObjPtr< Task > continuationTask() OVERRIDE
            {
                os::mutex_unique_lock guard;

                return getTargetTask( guard ) -> continuationTask();
            }
        };

        typedef ForwarderTaskBaseT<> ForwarderTaskBase;

        /**
         * @brief class WrapperTaskBase - A wrapper task class which is intended to facilitate for a task
         * to forward calls to another task (wrapped task) which allows for one task to execute multiple
         * external tasks via continuations (after each task is executed the wrapped task is replaced
         * with the new task and the continuationTask() method returns the same object until there are no
         * more external tasks to be executed)
         */

        template
        <
            typename E = void
        >
        class WrapperTaskBaseT :
            public ForwarderTaskBase
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( WrapperTaskBaseT, Task )

        protected:

            om::ObjPtr< Task >                                                      m_wrappedTask;
            mutable os::mutex                                                       m_lock;

            WrapperTaskBaseT( SAA_in_opt om::ObjPtr< Task >&& wrappedTask = nullptr )
                :
                m_wrappedTask( BL_PARAM_FWD( wrappedTask ) )
            {
            }

            virtual auto getTargetTask( SAA_inout os::mutex_unique_lock& guard ) const NOEXCEPT
                -> const om::ObjPtr< Task >& OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                os::mutex_unique_lock( m_lock ).swap( guard );

                BL_ASSERT( m_wrappedTask );

                BL_NOEXCEPT_END()

                return m_wrappedTask;
            }

            auto handleContinuationForward() -> om::ObjPtr< Task >
            {
                BL_MUTEX_GUARD( m_lock );

                if( m_wrappedTask )
                {
                    auto continuation = m_wrappedTask -> continuationTask();

                    if( continuation )
                    {
                        m_wrappedTask = std::move( continuation );

                        return om::copyAs< Task >( this );
                    }
                }

                return nullptr;
            }

        public:

            /**
             * @brief Allows for setting the wrapped task externally
             */

            void setWrappedTask( SAA_in_opt om::ObjPtr< Task >&& wrappedTask = nullptr ) NOEXCEPT
            {
                BL_MUTEX_GUARD( m_lock );

                m_wrappedTask = BL_PARAM_FWD( wrappedTask );
            }

            auto getWrappedTask() -> om::ObjPtr< Task >
            {
                os::mutex_unique_lock guard;

                return om::copy( getTargetTask( guard ) );
            }

            template
            <
                typename T
            >
            auto getWrappedTaskT() -> om::ObjPtr< T >
            {
                os::mutex_unique_lock guard;

                return om::qi< T >( getTargetTask( guard ) );
            }
        };

        typedef WrapperTaskBaseT<> WrapperTaskBase;

        /**
         * @brief class TaskBase
         */

        template
        <
            typename E = void
        >
        class TaskBaseT :
            public Task
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( TaskBaseT, Task )

        protected:

            typedef TaskBaseT< E >                                                  this_type;

        private:

            /*
             * m_cancelRequested should be declared private, so it can't be changed or tested in
             * the base classes, but we will provide a protected getter which will allow us to
             * control the visibility of this state in the derived classes (e.g. some classes
             * such as the observable objects want to disable cancel and re-define the getter as
             * private, so their derived classes can't change or even use this property)
             *
             * Note that this needs to be atomic_bool as some tasks, such as for example simple
             * CPU based tasks can't really be truly cancelled once they start and for these tasks
             * we don't want to try to acquire the task lock if the task has already started as
             * this would be unnecessary and / or even can easily cause deadlock in some cases -
             * see requestCancelInternalMarkOnlyNoLock() below
             */

            std::atomic_bool                                                        m_cancelRequested;

            /**
             * @brief Notifies the execution queue that the task is ready. It should
             * not be called while holding the lock
             */

            void notifyReadyImpl(
                SAA_in_opt              const bool                                  allowFinishContinuations,
                SAA_in_opt              const std::exception_ptr&                   eptrIn,
                SAA_in_opt              const bool                                  isExpectedException
                ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                cpp::void_callback_noexcept_t cbReady;
                std::exception_ptr eptr = eptrIn;

                {
                    BL_MUTEX_GUARD( m_lock );

                    if( allowFinishContinuations )
                    {
                        try
                        {
                            if( scheduleTaskFinishContinuation( eptr ) )
                            {
                                return;
                            }
                        }
                        catch( std::exception& )
                        {
                            const auto eptrTask = m_exception ? m_exception : eptr;

                            if( eptrTask )
                            {
                                /*
                                 * We can't chain the original task exception because the exception thrown may not
                                 * be eh::exception, but even if it was eh::exception then we still don't know if
                                 * it is safe to chain it because if it happens to be the same exception as the
                                 * original task exception then a circular reference might happen
                                 *
                                 * We simply log it as a warning and then discard it
                                 */

                                utils::tryCatchLog(
                                    "Ignoring original exception overridden by shutdown continuation exception",
                                    [ & ]() -> void
                                    {
                                        cpp::safeRethrowException( eptrTask );
                                    }
                                    );
                            }

                            eptr = std::current_exception();
                            m_exception = eptr;
                        }
                    }

                    eptr = onTaskStoppedNothrow( eptr );

                    if( m_notifyCalled )
                    {
                        /*
                         * Some tasks, e.g. timer tasks, due to the nature of how events
                         * are queued may have to call notifyReady() multiple times, so
                         * we want to have a guard to protect for this.
                         */

                        return;
                    }

                    if( ! m_name.empty() )
                    {
                        if( ! isExpectedException && ( eptr || m_exception ) )
                        {
                            /*
                             * The exception is available via m_exception or eptr (m_exception takes priority)
                             */

                            try
                            {
                                cpp::safeRethrowException( m_exception ? m_exception : eptr );
                            }
                            catch( eh::system_error& ex )
                            {
                                const auto ec = ex.code();
                                chk2DumpException( std::current_exception(), ex, &ec );
                            }
                            catch( std::exception& ex )
                            {
                                chk2DumpException(
                                    std::current_exception(),
                                    ex,
                                    eh::get_error_info< eh::errinfo_error_code >( ex )
                                    );
                            }
                        }
                        else
                        {
                            /*
                             * Task which we want to notify on success should be prefixed with 'success:'
                             */

                            if( 0 == m_name.find( "success:" ) )
                            {
                                BL_LOG_MULTILINE(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Task '"
                                        << m_name
                                        << "' completed successfully"
                                    );
                            }
                        }
                    }

                    /*
                     * Ensure we clear the onReady() callback, so it is
                     * never called again unless the task is restarted
                     */

                    BL_ASSERT( m_cbReady );

                    if( ! m_exception )
                    {
                        m_exception = eptr;
                    }

                    m_cbReady.swap( cbReady );

                    m_state = PendingCompletion;
                    m_notifyCalled = true;
                }

                cbReady();

                BL_NOEXCEPT_END()
            }

        protected:

            /*
             * Note: to avoid complicating the construction logic and require m_name parameter to be
             * propagated in all derived classes constructors we won't declare m_name const even
             * though it is logically immutable and should only be set in the constructors of some
             * of the derived classes
             *
             * It is important that m_name never changes after the object is constructed as it is
             * exposed as const reference by the name() getter and modification to it will make the
             * interface not thread safe
             */

            std::string                                                             m_name;

            State                                                                   m_state;
            std::exception_ptr                                                      m_exception;
            cpp::void_callback_noexcept_t                                           m_cbReady;
            bool                                                                    m_notifyCalled;
            mutable os::mutex                                                       m_lock;
            ContinuationCallback                                                    m_continuationCallback;

            TaskBaseT()
                :
                m_cancelRequested( false ),
                m_state( Created ),
                m_exception( nullptr ),
                m_notifyCalled( false )
            {
            }

            ~TaskBaseT()
            {
                /*
                 * Ensure the task is not in running mode
                 */

                BL_RT_ASSERT( State::Running != m_state, "Task should not be in running state" );

                BL_ASSERT( ! m_cbReady );
            }

            om::ObjPtr< ThreadPool > getThreadPool( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) NOEXCEPT
            {
                om::ObjPtr< ThreadPool > threadPool;

                BL_NOEXCEPT_BEGIN()

                /*
                 * If local thread pool is provided in the execution queue then we use it
                 * but otherwise we just use the general purpose thread pool
                 */

                BL_ASSERT( eq );

                threadPool = om::copy( eq -> getLocalThreadPool() );

                if( ! threadPool )
                {
                    threadPool = ThreadPoolDefault::getDefault( getThreadPoolId() );
                }

                BL_NOEXCEPT_END()

                return threadPool;
            }

            std::string getErrorMessage( SAA_in const eh::error_code& ec )
            {
                if( ec )
                {
                    return resolveMessage( BL_MSG() << " ( error code message: '" << ec.message() << "' )" );
                }

                return "";
            }

            std::string getBasicTaskInfo() const
            {
                return resolveMessage(
                    BL_MSG()
                        << "[cancelRequested="
                        << m_cancelRequested
                        << "]"
                    );
            }

            void chk2DumpException(
                SAA_in                  const std::exception_ptr&                   eptr,
                SAA_in                  const std::exception&                       exception,
                SAA_in_opt              const eh::error_code*                       ec
                ) NOEXCEPT
            {
                /*
                 * We only dump the exception in the log if it is not expected
                 */

                if( ! isExpectedException( eptr, exception, ec ) )
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "Task '"
                            << m_name
                            << "' failed with the following exception:\n"
                            << eh::diagnostic_information( exception )
                        );
                }
            }

            /**
             * @brief Schedules the task to start execution. Note that function should assume the
             * task lock is held and should never attempt to execute synchronously and call
             * notifyReady().
             */

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) = 0;

            /**
             * @brief This function allows the derived tasks to request to have full control and
             * handle the task cancellation logic by requesting that the task is scheduled even
             * if it was already canceled before it has started execution
             *
             * Normally for most tasks this can be handled in the base class as the task won't
             * even be actually scheduled (by calling scheduleTask), but just internally
             * scheduled for completion with the exception that is typically expected for
             * cancellation (SystemException with an error code asio::error::operation_aborted)
             *
             * However some tasks (e.g. timer tasks) do require to have full control over this
             * cancellation process and require to be scheduled even if they were canceled before
             * they have started executing; this method will allow them to request this behavior
             */

            virtual bool scheduleEvenIfAlreadyCanceled()
            {
                return false;
            }

            /**
             * @brief If it returns true that means the task has scheduled a shutdown continuation
             * and it should not complete
             *
             * This will be invoked when the task is about to finish and if it returns true then
             * the task completion will be postponed as it means the function has scheduled some
             * shutdown continuation of the task which need to finish first before the task truly
             * can complete
             *
             * This is typically needed for cases where the completion of the task requires cleanup
             * operations that are to be considered as part of the task execution (e.g. async
             * cleanup operations such as SSL shutdown requests and the like)
             *
             * Note that at some point this function needs to return false for the task to complete
             * (or if the cleanup operation can be performed synchronously or not needed at all)
             *
             * Also note that it is responsibility of the task to remember the original exception,
             * which would be eptrIn if non-null or m_exception otherwise, so if the shutdown
             * operation completes with its own error then when the task truly completes the
             * original exception can be chained
             */

            virtual bool scheduleTaskFinishContinuation( SAA_in_opt const std::exception_ptr& eptrIn = nullptr )
            {
                BL_UNUSED( eptrIn );

                return false;
            }

            /**
             * @brief enhances the exception with relevant properties
             */

            virtual void enhanceException( SAA_in eh::exception& exception ) const
            {
                /*
                 * Inject some basic task info and then check to inject time thrown if it is not injected already
                 */

                exception << eh::errinfo_task_info( getBasicTaskInfo() );

                const std::string* timeThrown = eh::get_error_info< eh::errinfo_time_thrown >( exception );

                if( ! timeThrown )
                {
                    exception << eh::errinfo_time_thrown( time::getCurrentLocalTimeISO() );
                }
            }

            /**
             * @brief returns true if the exception is expected
             *
             * This will indicate to the base task if it needs to dump the exception info in the log or not
             */

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                   eptr,
                SAA_in                  const std::exception&                       exception,
                SAA_in_opt              const eh::error_code*                       ec
                ) NOEXCEPT
            {
                BL_UNUSED( eptr );
                BL_UNUSED( exception );
                BL_UNUSED( ec );

                return false;
            }

            /**
             * @brief This is a callback which will be called when the task is terminated
             *
             * This can be used as a task destructor method to make final cleanup if necessary
             *
             * Most tasks do not need special cleanup, but some tasks may need to unwind
             * state if e.g. they terminate unexpectedly (e.g. with exception)
             *
             * This task can also be used to re-map the final exception if desired
             */

            virtual auto onTaskStoppedNothrow( SAA_in_opt const std::exception_ptr& eptrIn ) NOEXCEPT
                -> std::exception_ptr
            {
                return eptrIn;
            }

            /**
             * @brief Notifies the execution queue that the task is ready. It should
             * not be called while holding the lock
             */

            void notifyReady(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_in_opt              const bool                                  isExpectedException = false
                ) NOEXCEPT
            {
                notifyReadyImpl( true /* allowFinishContinuations */, eptrIn, isExpectedException );
            }

            void requestCancelInternalMarkOnlyNoLock() NOEXCEPT
            {
                m_cancelRequested = true;
            }

            void requestCancelInternal() NOEXCEPT
            {
                if( m_cancelRequested )
                {
                    return;
                }

                m_cancelRequested = true;

                /*
                 * cancelTask() should only be called for running tasks
                 */

                if( Task::Running != m_state )
                {
                    return;
                }

                /*
                 * See the doc for TaskBase::cancelTask() below
                 *
                 * Errors are ignored as they could happen for some tasks (i.e. IO tasks)
                 */

                utils::tryCatchLog(
                    "TaskBase::requestCancelInternal threw an exception",
                    [ & ]() -> void
                    {
                        cancelTask();
                    },
                    cpp::void_callback_t(),
                    utils::LogFlags::DEBUG_ONLY
                    );
            }

            /**
             * @brief It will schedule the actual cancellation
             * calls on open sockets, resolvers, acceptors or other
             * asio objects which are used to implement the task or
             * do whatever else is necessary to force the
             * cancellation as soon as possible
             */

            virtual void cancelTask()
            {
                /*
                 * NOP by default
                 */
            }

            virtual ThreadPoolId getThreadPoolId() const NOEXCEPT
            {
                /*
                 * By default tasks use the general purpose thread pool
                 */

                return ThreadPoolId::GeneralPurpose;
            }

        public:

            bool isCanceled() const NOEXCEPT
            {
                return m_cancelRequested;
            }

            /**
             * @brief Sets the continuation callback for this task
             */

            void setContinuationCallback( SAA_in ContinuationCallback&& continuationCallback ) NOEXCEPT
            {
                m_continuationCallback = BL_PARAM_FWD( continuationCallback );
            }

            virtual bool isFailed() const NOEXCEPT OVERRIDE
            {
                return ( m_state >= Task::PendingCompletion && nullptr != m_exception );
            }

            virtual bool isFailedOrFailing() const NOEXCEPT OVERRIDE
            {
                return ( nullptr != m_exception );
            }

            virtual Task::State getState() const NOEXCEPT OVERRIDE
            {
                return m_state;
            }

            virtual void setCompletedState() NOEXCEPT OVERRIDE
            {
                m_state = Task::Completed;
            }

            virtual std::exception_ptr exception() const NOEXCEPT OVERRIDE
            {
                std::exception_ptr result;

                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                result = m_exception;

                BL_NOEXCEPT_END()

                return result;
            }

            virtual void exception( SAA_in_opt const std::exception_ptr& exception ) NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                m_exception = exception;

                BL_NOEXCEPT_END()
            }

            virtual const std::string& name() const NOEXCEPT OVERRIDE
            {
                return m_name;
            }

            virtual void scheduleNothrow(
                SAA_in                  const std::shared_ptr< ExecutionQueue >&    eq,
                SAA_in                  cpp::void_callback_noexcept_t&&             callbackReady
                ) NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                /*
                 * The task can belong to a single execution queue only
                 */

                BL_ASSERT( Running != m_state );

                if( Created != m_state )
                {
                    /*
                     * If we're restarting a task make sure we clear the error
                     * info in case the previous execution has failed and also
                     * clear the m_notifyCalled flag
                     */

                    m_exception = nullptr;
                    m_notifyCalled = false;
                    m_cancelRequested = false;
                }

                m_cbReady.swap( callbackReady );

                bool isExpectedException = false;

                try
                {
                    m_state = Running;

                    if( m_cancelRequested && ! scheduleEvenIfAlreadyCanceled() )
                    {
                        /*
                         * If we are here that simply means the task was already cancelled even
                         * before it has started execution
                         */

                        BL_THROW_EC( asio::error::operation_aborted, BL_SYSTEM_ERROR_DEFAULT_MSG );

                        isExpectedException = true;
                    }

                    scheduleTask( eq );
                }
                catch( std::exception& )
                {
                    /*
                     * The scheduling of the task has failed - just schedule a
                     * call on the thread pool to mark the task as completed with
                     * the respective exception
                     *
                     * Note that post() can potentially throw, but only in low
                     * memory situations and in these cases there is not much that
                     * can be done other than to terminate the process (which will
                     * be done by the BL_NOEXCEPT_* macros)
                     */

                    getThreadPool( eq ) -> aioService().post(
                        cpp::bind(
                            &this_type::notifyReadyImpl,
                            om::ObjPtrCopyable< this_type >::acquireRef( this ),
                            false /* allowFinishContinuations */,
                            std::current_exception(),
                            isExpectedException
                            )
                        );
                }

                BL_NOEXCEPT_END()
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                requestCancelInternal();

                BL_NOEXCEPT_END()
            }

            virtual om::ObjPtr< Task > continuationTask() OVERRIDE
            {
                if( m_continuationCallback )
                {
                    return m_continuationCallback( this );
                }

                return nullptr;
            }
        };

        typedef TaskBaseT<> TaskBase;

        /******************************************************************************************
         * ================================== SimpleCompletedTask =================================
         */

        /**
         * @brief A simple task that is not meant for execution, but to simulate an executed task
         * and carry error information / exception
         */

        template
        <
            typename E = void
        >
        class SimpleCompletedTaskT : public TaskBase
        {
        public:

            typedef TaskBase                                                        base_type;

        private:

            BL_DECLARE_OBJECT_IMPL( SimpleCompletedTaskT )

        protected:

            SimpleCompletedTaskT( SAA_in_opt const std::exception_ptr& eptr = std::exception_ptr() )
            {
                base_type::m_exception = eptr;
                base_type::m_state = Task::Completed;
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                BL_UNUSED( eq );

                BL_RIP_MSG( "SimpleCompletedTask should never be scheduled" );
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                /*
                 * No need to acquire lock to request cancellation for this task
                 * as no real cancellation is possible - we simply set the flag and the task
                 * can check it, but that is thread-safe as the flag is of atomic_bool type
                 */

                requestCancelInternalMarkOnlyNoLock();
            }
        };

        typedef om::ObjectImpl< SimpleCompletedTaskT<> > SimpleCompletedTask;

        /******************************************************************************************
         * ==================================== SimpleTaskBase ====================================
         */

        /**
         * @brief class SimpleTaskBase
         */

        template
        <
            typename E = void
        >
        class SimpleTaskBaseT : public TaskBase
        {
        public:

            typedef SimpleTaskBaseT< E >                                            this_type;
            typedef TaskBase                                                        base_type;

        private:

            BL_CTR_DEFAULT( SimpleTaskBaseT, protected )
            BL_DECLARE_OBJECT_IMPL( SimpleTaskBaseT )

        protected:

            std::shared_ptr< tasks::ExecutionQueue >                                m_eq;

            virtual void onExecute() NOEXCEPT = 0;

            om::ObjPtr< ThreadPool > getThreadPool() NOEXCEPT
            {
                return base_type::getThreadPool( m_eq );
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                /*
                 * Just push the task in background
                 */

                m_eq = eq;

                getThreadPool() -> aioService().post(
                    cpp::bind(
                        &this_type::onExecute,
                        om::ObjPtrCopyable< this_type >::acquireRef( this )
                        )
                    );
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                /*
                 * No need to acquire lock to request cancellation for this class of tasks
                 * as no real cancellation is possible - we simply set the flag and the task
                 * can check it, but that is thread-safe as the flag is of atomic_bool type
                 */

                requestCancelInternalMarkOnlyNoLock();
            }
        };

        typedef SimpleTaskBaseT<> SimpleTaskBase;

        /******************************************************************************************
         * ============================== SimpleTaskWithContinuation ==============================
         */

        /**
         * @brief A simple task that allows for the callback to return a continuation task
         */

        template
        <
            typename T = EmptyClass
        >
        class SimpleTaskWithContinuationT :
            public SimpleTaskBase,
            public T
        {
            BL_DECLARE_OBJECT_IMPL( SimpleTaskWithContinuationT )

        protected:

            typedef SimpleTaskWithContinuationT< T >                                this_type;

            GetContinuationCallback                                                 m_callback;

            SimpleTaskWithContinuationT(
                SAA_in_opt              GetContinuationCallback&&                   callback = GetContinuationCallback(),
                SAA_in_opt              std::string&&                               taskName = std::string()
                )
            {
                m_name = BL_PARAM_FWD( taskName );
                m_callback.swap( callback );
            }

            static auto continuationCallbackCopyTask(
                SAA_in                  const om::ObjPtrCopyable< Task >&           continuationTask,
                SAA_inout               Task*                                       finishedTask
                )
                -> om::ObjPtr< Task >
            {
                BL_UNUSED( finishedTask );

                return om::copy( continuationTask );
            }

            virtual void onExecute() NOEXCEPT OVERRIDE
            {
                BL_TASKS_HANDLER_BEGIN()

                if( isCanceled() )
                {
                    /*
                     * If we are here that simply means the task was already cancelled even
                     * before it has started execution
                     */

                    BL_TASKS_HANDLER_CHK_EC( asio::error::operation_aborted );
                }

                /*
                 * Simple tasks must complete in one shot
                 * and then we notify the EQ we're done
                 */

                if( m_callback )
                {
                    const auto continuationTask = m_callback();

                    if( continuationTask )
                    {
                        m_continuationCallback = cpp::bind(
                            &this_type::continuationCallbackCopyTask,
                            om::ObjPtrCopyable< Task >( continuationTask ),
                            _1
                            );
                    }
                }

                BL_TASKS_HANDLER_END()
            }
        };

        typedef om::ObjectImpl< SimpleTaskWithContinuationT<> > SimpleTaskWithContinuation;

        /******************************************************************************************
         * ====================================== SimpleTask ======================================
         */

        /**
         * @brief A very simple task which executes a callback and finishes (just a small extension
         * of SimpleTaskWithContinuationT)
         */

        template
        <
            typename T = EmptyClass
        >
        class SimpleTaskT :
            public SimpleTaskWithContinuationT< T >
        {
            BL_DECLARE_OBJECT_IMPL( SimpleTaskT )

        protected:

            typedef SimpleTaskT< T >                                                this_type;
            typedef SimpleTaskWithContinuationT< T >                                base_type;

            SimpleTaskT(
                SAA_in_opt              cpp::void_callback_t&&                      callback = cpp::void_callback_t(),
                SAA_in_opt              std::string&&                               taskName = std::string()
                )
                :
                base_type(
                    cpp::bind( &this_type::callbackAdaptor, callback ),
                    BL_PARAM_FWD( taskName )
                    )
            {
            }

            static auto callbackAdaptor( SAA_in_opt const cpp::void_callback_t& callback ) -> om::ObjPtr< Task >
            {
                if( callback )
                {
                    callback();
                }

                return nullptr;
            }

        public:

            void setCB( SAA_in cpp::void_callback_t&& callback ) NOEXCEPT
            {
                BL_ASSERT( Task::Running != base_type::m_state );

                base_type::m_callback = cpp::bind( &this_type::callbackAdaptor, callback );
            }
        };

        typedef om::ObjectImpl< SimpleTaskT<> > SimpleTaskImpl;

        /********************************************************************************************
         * ================================ ExternalCompletionTaskIf ================================
         */

        /**
         * @brief class ExternalCompletionTaskIf - an external completion if callback task
         * (for IfScheduleCallback)
         */

        template
        <
            typename T = EmptyClass
        >
        class ExternalCompletionTaskIfT :
            public SimpleTaskBase,
            public T
        {
            BL_DECLARE_OBJECT_IMPL( ExternalCompletionTaskIfT )

        protected:

            typedef ExternalCompletionTaskIfT< T >                                  this_type;
            typedef SimpleTaskBase                                                  base_type;

            const IfScheduleCallback                                                m_ifCallback;
            const cpp::void_callback_noexcept_t                                     m_cancelCallback;
            cpp::ScalarTypeIniter< bool >                                           m_scheduled;

            /*
             * Note that cancelCallback is only provided if the operation is cancel-able - i.e. it can
             * be potentially cancelled asynchronously after it has been scheduled
             *
             * Also note that the cancelCallback should *always* operate asynchronously and never try
             * to complete the task directly (e.g. invoking markCompleted, notifyReady, etc) as this
             * can (and most likely will) cause deadlock(s)
             */

            ExternalCompletionTaskIfT(
                SAA_in                  IfScheduleCallback&&                        ifCallback,
                SAA_in_opt              cpp::void_callback_noexcept_t&&             cancelCallback = cpp::void_callback_noexcept_t()
                )
                :
                m_ifCallback( BL_PARAM_FWD( ifCallback ) ),
                m_cancelCallback( BL_PARAM_FWD( cancelCallback ) )
            {
                BL_ASSERT( m_ifCallback );
            }

            void markCompleted( SAA_in const std::exception_ptr& eptr ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                {
                    BL_MUTEX_GUARD( base_type::m_lock );

                    if( ! m_scheduled )
                    {
                        BL_RIP_MSG( "If operation completed synchronously the callback should not be called" );
                    }
                }

                notifyReady( eptr );

                BL_NOEXCEPT_END()
            }

            virtual void onExecute() NOEXCEPT OVERRIDE
            {
                BL_TASKS_HANDLER_BEGIN()

                /*
                 * If scheduled the external completion tasks can complete only when the
                 * external completion callback is called
                 *
                 * Note that similar to the SimpleTaskImpl above once the task has started
                 * cancellation will not be possible
                 *
                 * In the future we may add support for cancellation, but note that this
                 * may not be trivial
                 */

                if( isCanceled() )
                {
                    /*
                     * If we are here that simply means the task was already cancelled even
                     * before it has started execution
                     */

                    BL_TASKS_HANDLER_CHK_EC( asio::error::operation_aborted );
                }

                BL_ASSERT( m_ifCallback );

                m_scheduled = m_ifCallback(
                    cpp::bind(
                        &this_type::markCompleted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        _1
                        )
                    );

                if( m_scheduled )
                {
                    return;
                }

                BL_TASKS_HANDLER_END()
            }

        public:

            bool completedSynchronously() const NOEXCEPT
            {
                BL_MUTEX_GUARD( base_type::m_lock );

                return
                    (
                        ! m_scheduled &&
                        ( base_type::m_state == Task::PendingCompletion || base_type::m_state == Task::Completed )
                    );
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                base_type::requestCancelInternalMarkOnlyNoLock();

                if( m_cancelCallback )
                {
                    BL_MUTEX_GUARD( base_type::m_lock );

                    if( m_scheduled && Task::Running == base_type::m_state )
                    {
                        m_cancelCallback();
                    }
                }

                BL_NOEXCEPT_END()
            }
        };

        typedef om::ObjectImpl< ExternalCompletionTaskIfT<> > ExternalCompletionTaskIfImpl;

        /**********************************************************************************************
         * ================================ ExternalCompletionTaskImpl ================================
         */

        /**
         * @brief class ExternalCompletionTaskImpl - an external completion callback task
         */

        template
        <
            typename T = EmptyClass
        >
        class ExternalCompletionTaskT :
            public ExternalCompletionTaskIfT< T >
        {
            BL_DECLARE_OBJECT_IMPL( ExternalCompletionTaskT )

        protected:

            typedef ExternalCompletionTaskT< T >                                    this_type;
            typedef ExternalCompletionTaskIfT< T >                                  base_type;

            ExternalCompletionTaskT(
                SAA_in                  ScheduleCallback&&                          callback,
                SAA_in_opt              cpp::void_callback_noexcept_t&&             cancelCallback = cpp::void_callback_noexcept_t()
                )
                :
                base_type( cpp::bind( &this_type::callbackAdaptor, callback, _1 ), BL_PARAM_FWD( cancelCallback ) )
            {
            }

            static bool callbackAdaptor(
                SAA_in              const ScheduleCallback&                         callback,
                SAA_in              const CompletionCallback&                       onReady
                )
            {
                callback( onReady );

                /*
                 * 'true' means the async operation was scheduled and the callback will be
                 * invoked
                 *
                 * See comment above for IfScheduleCallback
                 */

                return true;
            }
        };

        typedef om::ObjectImpl< ExternalCompletionTaskT<> > ExternalCompletionTaskImpl;

        /*****************************************************************************************
         * ==================================== TimerTaskBase ====================================
         */

        /**
         * @brief class TimerTaskBase
         */

        template
        <
            typename E = void
        >
        class TimerTaskBaseT : public TaskBase
        {
            BL_CTR_DEFAULT( TimerTaskBaseT, protected )
            BL_DECLARE_OBJECT_IMPL( TimerTaskBaseT )

        public:

            typedef TimerTaskBaseT< E >                                             this_type;
            typedef TaskBase                                                        base_type;

        protected:

            cpp::SafeUniquePtr< asio::deadline_timer >                              m_timer;
            std::shared_ptr< ExecutionQueue >                                       m_eq;

            void resetTimer()
            {
                m_timer.reset(
                    new asio::deadline_timer(
                        ThreadPoolDefault::getDefault( base_type::getThreadPoolId() ) -> aioService(),
                        time::milliseconds( 0 )
                        )
                    );
            }

            void scheduleTimerInternal( SAA_in const time::time_duration& fromNow = time::microseconds( 0 ) )
            {
                m_timer -> expires_from_now( fromNow );

                m_timer -> async_wait(
                    cpp::bind(
                        &this_type::onTimer,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );
            }

            /**
             * @brief This one has to be implemented in the derived classes
             * to control the execution flow and termination of the task
             *
             * @return time::neg_infin to indicate the task has completed
             * or some timeout to wait
             */

            virtual time::time_duration run()
            {
                /*
                 * By default we just finish immediately
                 */

                return getDuration();
            }

            void onTimer( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                if( ec && asio::error::operation_aborted != ec )
                {
                    /*
                     * If we're here that means some real error in asio has
                     * occurred and in this case we can't really throw here
                     * so our only option is to just terminate the application
                     *
                     * This shouldn't really happen except due to bugs or
                     * some other weird cases
                     */

                    BL_RIP_MSG( ec.message() );
                }

                /*
                 * In this case we have to just call run() and check
                 * to re-schedule the timer
                 */

                const auto duration = run();

                if( ! duration.is_special() )
                {
                    scheduleTimerInternal( duration );

                    return;
                }

                BL_TASKS_HANDLER_END()
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                /*
                 * Save the thread pool, create the timer and schedule it immediately
                 */

                BL_ASSERT( eq );

                m_eq = eq;

                resetTimer();

                scheduleTimerInternal( getInitDelay() );
            }

            virtual bool scheduleEvenIfAlreadyCanceled() OVERRIDE
            {
                /*
                 * For timer tasks scheduleTask( ... ) should be called even if the task was
                 * already canceled before it has started executing
                 */

                return true;
            }

            virtual void cancelTask() OVERRIDE
            {
                if( m_timer )
                {
                    /*
                     * Just schedule the timer immediately to start attempting
                     * cancellation as soon as possible
                     */

                    scheduleTimerInternal();
                }
            }

        public:

            void wakeUp()
            {
                if( Running == getState() && m_timer )
                {
                    /*
                     * Cancel the timer to execute the task immediately
                     * and replace it with a fresh (non-canceled) timer
                     */

                    resetTimer();
                }
            }

            void runNow()
            {
                BL_MUTEX_GUARD( base_type::m_lock );

                if( Running == base_type::m_state && m_timer )
                {
                    scheduleTimerInternal();
                }
            }

            virtual time::time_duration getDuration() const
            {
                return time::neg_infin;
            }

            virtual time::time_duration getInitDelay() const
            {
                return time::time_duration();
            }
        };

        typedef TimerTaskBaseT<> TimerTaskBase;

        /*****************************************************************************************
         * ================================= AdjustableTimerTask =================================
         */

        /**
         * @brief class AdjustableTimerTask
         */

        template
        <
            typename E = void
        >
        class AdjustableTimerTaskT : public TimerTaskBase
        {
            BL_CTR_DEFAULT( AdjustableTimerTaskT, protected )
            BL_DECLARE_OBJECT_IMPL( AdjustableTimerTaskT )

        protected:

            typedef TimerTaskBase                                               base_type;

            const AdjustableTimerCallback                                       m_callback;
            const time::time_duration                                           m_initDelay;

            AdjustableTimerTaskT(
                SAA_in              AdjustableTimerCallback&&                   callback,
                SAA_in_opt          time::time_duration&&                       initDelay = time::time_duration()
                )
                :
                m_callback( BL_PARAM_FWD( callback ) ),
                m_initDelay( BL_PARAM_FWD( initDelay ) )
            {
                BL_ASSERT( m_callback );
            }

            virtual time::time_duration run() OVERRIDE
            {
                if( isCanceled() )
                {
                    return time::neg_infin;
                }

                /*
                 * Request to schedule another timer event
                 */

                return m_callback();
            }

        public:

            virtual time::time_duration getInitDelay() const OVERRIDE
            {
                return m_initDelay;
            }
        };

        typedef om::ObjectImpl< AdjustableTimerTaskT<> > AdjustableTimerTask;

        /*****************************************************************************************
         * =================================== SimpleTimerTask ===================================
         */

        /**
         * @brief class SimpleTimerTask
         */

        template
        <
            typename E = void
        >
        class SimpleTimerTaskT : public TimerTaskBase
        {
            BL_CTR_DEFAULT( SimpleTimerTaskT, protected )
            BL_DECLARE_OBJECT_IMPL( SimpleTimerTaskT )

        protected:

            typedef TimerTaskBase                                               base_type;

            using base_type::m_eq;
            using base_type::m_timer;

            const cpp::bool_callback_t                                          m_cbTask;

            time::time_duration                                                 m_duration;
            mutable os::mutex                                                   m_durationLock;

            time::time_duration                                                 m_initDelay;
            mutable os::mutex                                                   m_initDelayLock;

            SimpleTimerTaskT(
                SAA_in              cpp::bool_callback_t&&                      cbTask,
                SAA_in              time::time_duration&&                       duration,
                SAA_in_opt          time::time_duration&&                       initDelay = time::time_duration()
                )
                :
                m_cbTask( BL_PARAM_FWD( cbTask ) ),
                m_duration( BL_PARAM_FWD( duration ) ),
                m_initDelay( BL_PARAM_FWD( initDelay ) )
            {
                BL_ASSERT( m_cbTask );
            }

            virtual time::time_duration run() OVERRIDE
            {
                if( isCanceled() || false == m_cbTask() )
                {
                    return time::neg_infin;
                }

                /*
                 * Request to schedule another timer event
                 */

                return getDuration();
            }

        public:

            virtual time::time_duration getDuration() const OVERRIDE
            {
                BL_MUTEX_GUARD( m_durationLock );

                return m_duration;
            }

            void setDuration( SAA_in const time::time_duration& duration )
            {
                BL_MUTEX_GUARD( m_durationLock );

                m_duration = duration;
            }

            virtual time::time_duration getInitDelay() const OVERRIDE
            {
                BL_MUTEX_GUARD( m_initDelayLock );

                return m_initDelay;
            }

            void setInitDelay( SAA_in const time::time_duration& initDelay )
            {
                BL_MUTEX_GUARD( m_initDelayLock );

                m_initDelay = initDelay;
            }
        };

        typedef om::ObjectImpl< SimpleTimerTaskT<> > SimpleTimerTask;

        /*****************************************************************************************
         * =================================== Retry-able wrapper task ============================
         */

        template
        <
            typename E = void
        >
        class RetryableWrapperTaskT : public WrapperTaskBase
        {
            BL_DECLARE_OBJECT_IMPL( RetryableWrapperTaskT )

        public:

            typedef WrapperTaskBase                                             base_type;

            typedef cpp::function< om::ObjPtr< Task > () >                      factory_callback_t;

            typedef cpp::function
            <
                bool ( SAA_in const om::ObjPtr< Task >& task )
            >
            verification_callback_t;

        protected:

            const factory_callback_t                                            m_taskFactory;
            const std::size_t                                                   m_maxRetryCount;
            const time::time_duration                                           m_retryTimeout;
            const verification_callback_t                                       m_verificationCallback;
            cpp::ScalarTypeIniter< std::size_t >                                m_currentRetryCount;
            cpp::ScalarTypeIniter< bool >                                       m_retrying;

            RetryableWrapperTaskT(
                SAA_in          factory_callback_t&&                            taskFactory,
                SAA_in          const std::size_t                               maxRetryCount,
                SAA_in          time::time_duration&&                           retryTimeout,
                SAA_in_opt      verification_callback_t&&                       verificationCallback
                    = verification_callback_t()
                )
                :
                WrapperTaskBase( taskFactory() ),
                m_taskFactory( BL_PARAM_FWD( taskFactory ) ),
                m_maxRetryCount( maxRetryCount ),
                m_retryTimeout( BL_PARAM_FWD( retryTimeout ) ),
                m_verificationCallback( BL_PARAM_FWD( verificationCallback ) )
            {
            }

            bool isTaskOk( SAA_in const om::ObjPtr< Task >& task ) const
            {
                if( m_verificationCallback )
                {
                    return m_verificationCallback( task );
                }

                return ! task -> isFailed();
            }

        public:

            virtual om::ObjPtr< Task > continuationTask() OVERRIDE
            {
                auto task = base_type::handleContinuationForward();

                if( task )
                {
                    return task;
                }

                if( ! m_retrying )
                {
                    /*
                     * Retry-able task has completed, sleep and retry if it failed
                     */

                    ++m_currentRetryCount;

                    if( isTaskOk( m_wrappedTask ) || m_currentRetryCount >= m_maxRetryCount )
                    {
                        return nullptr;
                    }

                    m_retrying = true;

                    exception( nullptr );

                    m_wrappedTask = SimpleTimerTask::createInstance< Task >(
                        []() -> bool
                        {
                            return false;
                        },
                        cpp::copy( m_retryTimeout ) /* duration */,
                        cpp::copy( m_retryTimeout ) /* initDelay */
                        );
                }
                else
                {
                    /*
                     * The sleep has elapsed, retry the work task
                     *
                     * Tasks are not restart-able in general so we may have to re-create
                     * them every time
                     */

                    if( exception() )
                    {
                        return nullptr;
                    }

                    m_retrying = false;

                    m_wrappedTask = m_taskFactory();
                }

                return om::copyAs< Task >( this );
            }
        };

        typedef om::ObjectImpl< RetryableWrapperTaskT<> > RetryableWrapperTask;

    } // tasks

} // bl

#endif /* __BL_TASKS_TASKBASE_H_ */
