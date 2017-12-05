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

#ifndef __BL_TASKSUTILS_H_
#define __BL_TASKSUTILS_H_

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/TcpBaseTasks.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/utils/ShutdownTask.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief A small timer helper class that starts a timer task in an execution queue and also stops it
         * when it is no longer needed (or when the object is destructed)
         */

        template
        <
            typename E = void
        >
        class SimpleTimerT
        {
        protected:

            typedef SimpleTimerT< E >                                                   this_type;

            om::ObjPtr< AdjustableTimerTask >                                           m_timerTask;
            const om::ObjPtrDisposable< ExecutionQueue >                                m_timerQueue;

            static auto onTimerNoThrow(
                SAA_in                  const AdjustableTimerCallback&                  callback,
                SAA_in                  const time::time_duration&                      defaultDuration
                ) NOEXCEPT
                -> time::time_duration
            {
                /*
                 * The main purpose of this callback wrapper is to handle exceptions and convert
                 * them to warnings + the typical exception dump via BL_WARN_NOEXCEPT_* macros
                 */

                auto duration = defaultDuration;

                BL_WARN_NOEXCEPT_BEGIN()

                duration = callback();

                BL_WARN_NOEXCEPT_END( "SimpleTimerT::onTimerNoThrow()" )

                /*
                 * If an exception is thrown then we will use the default duration which the timer
                 * was constructed with
                 */

                return duration;
            }

        public:

            SimpleTimerT(
                SAA_in                  const AdjustableTimerCallback&                  callback,
                SAA_in                  time::time_duration&&                           defaultDuration,
                SAA_in_opt              time::time_duration&&                           initDelay = time::seconds( 0L ),
                SAA_in_opt              bool                                            dontStart = false
                )
                :
                m_timerQueue(
                    ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepNone )
                    )
            {
                auto guard = BL_SCOPE_GUARD( this_type::stop(); );

                m_timerTask = AdjustableTimerTask::createInstance(
                    cpp::bind(
                        &this_type::onTimerNoThrow,
                        callback,
                        defaultDuration
                        ),
                    BL_PARAM_FWD( initDelay )
                    );

                if( ! dontStart )
                {
                    start();
                }

                guard.dismiss();
            }

            ~SimpleTimerT() NOEXCEPT
            {
                stop();
            }

            bool isStarted() NOEXCEPT
            {
                return m_timerTask && m_timerQueue -> size();
            }

            void wait()
            {
                BL_CHK(
                    false,
                    isStarted(),
                    BL_MSG()
                        << "Attempting to wait on a simple timer object that has not been started"
                    );

                m_timerQueue -> wait( om::qi< Task >( m_timerTask ) );
            }

            void runNow()
            {
                BL_CHK(
                    false,
                    isStarted(),
                    BL_MSG()
                        << "Attempting to wait on a simple timer object that has not been started"
                    );

                m_timerTask -> runNow();
            }

            void start()
            {
                BL_CHK(
                    nullptr,
                    m_timerTask,
                    BL_MSG()
                        << "Attempting to start a simple timer object that is already disposed"
                    );

                if( isStarted() )
                {
                    /*
                     * It is already started
                     */

                    return;
                }

                m_timerQueue -> push_back( om::qi< Task >( m_timerTask ) );
            }

            void stop() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( isStarted() )
                {
                    m_timerTask -> requestCancel();
                    m_timerQueue -> wait( om::qi< Task >( m_timerTask ) );
                    m_timerQueue -> forceFlushNoThrow();
                    m_timerTask.reset();
                }

                BL_NOEXCEPT_END()
            }
        };

        typedef SimpleTimerT<> SimpleTimer;

        namespace detail
        {
            /**
             * @brief class TasksUtils - tasks utility code
             */

            template
            <
                typename E = void
            >
            class TasksUtilsT
            {
                BL_DECLARE_STATIC( TasksUtilsT )

            public:

                static void executeQueueAndCancelOnFailure( SAA_in  const om::ObjPtr< ExecutionQueue >& eq )
                {
                    bool cancelled = false;
                    std::exception_ptr eptr = nullptr;

                    /*
                     * We execute all tasks and on the first failure we cancel
                     * the rest and then we wait until the cancellation is done
                     *
                     * If one of the task fails the exception is re-thrown and
                     * if other exception is thrown as part of the wait we will
                     * force-flush the queue and re-throw the exception
                     */

                    try
                    {
                        for( ;; )
                        {
                            if( eq -> isEmpty() )
                            {
                                break;
                            }

                            const auto task = eq -> pop( true /* wait */ );

                            if( cancelled )
                            {
                                eq -> cancelAll( false /* wait */ );
                                continue;
                            }

                            if( task && task -> isFailed() )
                            {
                                eptr = task -> exception();
                                BL_ASSERT( eptr );

                                eq -> cancelAll( false /* wait */ );
                                cancelled = true;
                            }
                        }
                    }
                    catch( std::exception& )
                    {
                        eq -> forceFlushNoThrow();
                        throw;
                    }

                    BL_ASSERT( eq -> isEmpty() );

                    if( eptr )
                    {
                        cpp::safeRethrowException( eptr );
                    }
                }

                static void waitForSuccessOrCancel(
                    SAA_in              const om::ObjPtr< ExecutionQueue >&             eq,
                    SAA_in              const om::ObjPtr< Task >&                       task
                    )
                {
                    try
                    {
                        eq -> waitForSuccess( task );
                    }
                    catch( eh::system_error& e )
                    {
                        /*
                         * The task failed due to cancellation with an expected error code,
                         * so this specific exception should be filtered out and ignored
                         */

                        if( e.code() == asio::error::operation_aborted )
                        {
                            BL_LOG_MULTILINE(
                                Logging::trace(),
                                BL_MSG()
                                    << "\nIgnored an expected exception when task is canceled:\n"
                                    << eh::diagnostic_information( e )
                                );
                        }
                        else
                        {
                            throw;
                        }
                    }
                }

                static void cancelAndWaitForSuccess(
                    SAA_in              const om::ObjPtr< ExecutionQueue >&             eq,
                    SAA_in              const om::ObjPtr< Task >&                       task
                    )
                {
                    task -> requestCancel();

                    waitForSuccessOrCancel( eq, task );
                }

                template
                <
                    typename Acceptor
                >
                static void startAcceptor(
                    SAA_in              const om::ObjPtr< Acceptor >&                   acceptor,
                    SAA_in              const om::ObjPtr< ExecutionQueue >&             eq
                    )
                {
                    const auto taskAcceptor = om::qi< Task >( acceptor );

                    const auto shutdownWatcher = ShutdownTaskImpl::createInstance();
                    shutdownWatcher -> registerTask( taskAcceptor );

                    const auto taskShutdownWatcher = om::qi< tasks::Task >( shutdownWatcher );

                    eq -> push_back( taskAcceptor );
                    eq -> push_back( taskShutdownWatcher );

                    /*
                     * Note: we need to wait on the taskAcceptor because it may finish
                     * with either failure or due to it being canceled by the shutdown
                     * watcher task; if it is the former case then we need to propagate
                     * the exception and take down the blob server process
                     */

                    waitForSuccessOrCancel( eq, taskAcceptor );

                    /*
                     * If the acceptor task happens to finish with success it must be
                     * because it was canceled from the shutdown watcher; however if
                     * this was not the case we want to cancel the shutdown watcher
                     * task and proceed to graceful shutdown (this might be the case
                     * when shutdown was requested externally by some command)
                     *
                     * We also don't care if the shutdown watcher task succeeds or not,
                     * so we just use wait(), but not waitForSuccess()
                     */

                    taskShutdownWatcher -> requestCancel();
                    eq -> wait( taskShutdownWatcher );
                }
            };

            typedef TasksUtilsT<> TasksUtils;

        } // detail

        template
        <
            typename Acceptor
        >
        inline void startAcceptor(
            SAA_in              const om::ObjPtr< Acceptor >&                   acceptor,
            SAA_in              const om::ObjPtr< ExecutionQueue >&             eq
            )
        {
            detail::TasksUtils::startAcceptor( acceptor, eq );
        }

        inline void executeQueueAndCancelOnFailure( SAA_in  const om::ObjPtr< ExecutionQueue >& eq )
        {
            detail::TasksUtils::executeQueueAndCancelOnFailure( eq );
        }

        inline void waitForSuccessOrCancel(
            SAA_in              const om::ObjPtr< ExecutionQueue >&             eq,
            SAA_in              const om::ObjPtr< Task >&                       task
            )
        {
            detail::TasksUtils::waitForSuccessOrCancel( eq, task );
        }

        inline void cancelAndWaitForSuccess(
            SAA_in              const om::ObjPtr< ExecutionQueue >&             eq,
            SAA_in              const om::ObjPtr< Task >&                       task
            )
        {
            detail::TasksUtils::cancelAndWaitForSuccess( eq, task );
        }

    } // tasks

} // bl

#endif /* __BL_TASKSUTILS_H_ */
