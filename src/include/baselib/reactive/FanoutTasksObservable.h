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

#ifndef __BL_REACTIVE_FANOUTTASKSOBSERVABLE_H_
#define __BL_REACTIVE_FANOUTTASKSOBSERVABLE_H_

#include <baselib/reactive/Observable.h>
#include <baselib/reactive/ObservableBase.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace reactive
    {
        /**
         * @brief class FanoutTasksObservable
         */
        template
        <
            typename E = void
        >
        class FanoutTasksObservableT :
            public ObservableBase,
            public tasks::ExecutionQueueNotifyBase
        {
        public:

            typedef FanoutTasksObservableT< E >                                     this_type;
            typedef ObservableBase                                                  base_type;

            typedef tasks::Task                                                     Task;
            typedef tasks::ExecutionQueueNotify                                     ExecutionQueueNotify;

        private:

            BL_CTR_DEFAULT( FanoutTasksObservableT, protected )
            BL_DECLARE_OBJECT_IMPL( FanoutTasksObservableT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY_CHAIN_BASE( base_type )
                BL_QITBL_ENTRY_CHAIN_BASE( tasks::ExecutionQueueNotifyBase )
            BL_QITBL_END( Task )

        protected:

            enum
            {
                /*
                 * Let's enforce a throttle limit of no more than 1K tasks in ready
                 * or executing state. This will limit the memory consumption when
                 * the pipeline is slow to push data through the pipe
                 */

                MAX_READY_OR_EXECUTING_TASKS = 1024,
            };

            om::ObjPtr< om::Proxy >                                                 m_notifyCB;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                           m_eqChildTasks;

            virtual void tryStopObservable() OVERRIDE
            {
                if( m_eqChildTasks )
                {
                    m_eqChildTasks -> cancelAll( false /* wait */ );
                }
            }

            void chk2InitAndBeginProcessing()
            {
                if( ! m_eqChildTasks )
                {
                    /*
                     * We're starting now...
                     */

                    m_eqChildTasks = tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                        tasks::ExecutionQueue::OptionKeepAll
                        );

                    m_notifyCB = om::ProxyImpl::createInstance< om::Proxy >();
                    m_notifyCB -> connect( static_cast< Task* >( this ) );

                    m_eqChildTasks -> setNotifyCallback(
                        om::copy( m_notifyCB ),
                        tasks::ExecutionQueueNotify::AllTasksCompleted /* eventsMask */
                        );

                    const auto task = createSeedingTask();

                    if( task )
                    {
                        try
                        {
                            m_eqChildTasks -> push_back( task );
                        }
                        catch( std::exception& )
                        {
                            m_notifyCB -> disconnect();
                            m_notifyCB.reset();
                            throw;
                        }
                    }
                }
            }

            bool isShuttingDown() const NOEXCEPT
            {
                return ( m_stopRequested || m_completed || isFailedOrFailing() );
            }

            time::time_duration chk2LoopUntilDoneInternal( SAA_in const bool nothrow )
            {
                if( ! m_notifyCB )
                {
                    BL_ASSERT( m_eqChildTasks -> isEmpty() );

                    return time::neg_infin;
                }

                if( m_stopRequested || isFailedOrFailing() )
                {
                    /*
                     * An error has occurred or cancel was requested; just keep popping
                     * the child tasks until we're done and try to cancel all remaining
                     * ones without waiting.
                     *
                     * Note: cancelAll() will remove all ready tasks
                     * so we don't need to keep popping here.
                     */

                    if( false == m_eqChildTasks -> isEmpty() )
                    {
                        m_eqChildTasks -> cancelAll( false /* wait */ );
                    }
                }
                else
                {
                    /*
                     * Just keep processing all tasks until we're done
                     */

                    for( ;; )
                    {
                        const auto task = m_eqChildTasks -> top( false /* wait */ );

                        if( task )
                        {
                            if( task -> isFailed() && false == allowPushingOfFailedChildTasks() )
                            {
                                /*
                                 * We have encountered an error for the first time
                                 * Save the exception and cancel all tasks
                                 *
                                 * Note: cancelAll() will remove all ready tasks
                                 * so we don't need to keep popping here.
                                 */

                                m_exception = task -> exception();
                                BL_ASSERT( m_exception );

                                break;
                            }

                            if( canAcceptReadyTask() )
                            {
                                /*
                                 * If pushReadyTask returns true it means the task was processed
                                 * and it doesn't need to be rescheduled
                                 *
                                 * If it returns false it means the task was processed, but it
                                 * needs to be re-scheduled
                                 */

                                if( pushReadyTask( task ) )
                                {
                                    BL_VERIFY( task == m_eqChildTasks -> pop( false /* wait */ ) );
                                }
                                else
                                {
                                    m_eqChildTasks -> push_back( task );
                                }

                                continue;
                            }

                            /*
                             * No failed tasks, but we can't push ready tasks now;
                             * just wait...
                             */

                            return time::milliseconds( DEFAULT_WAIT_TIME_IN_MILLISECONDS );
                        }

                        break;
                    }
                }

                bool ready2Disconnect = false;

                if(
                    m_eqChildTasks -> isEmpty() &&
                    ( isShuttingDown() || false == isWaitingExternalInput() )
                    )
                {
                    std::exception_ptr eptr = nullptr;

                    try
                    {
                        if( ! flushAllPendingTasks() )
                        {
                            return time::milliseconds( DEFAULT_WAIT_TIME_IN_MILLISECONDS );
                        }

                        ready2Disconnect = true;
                    }
                    catch( std::exception& )
                    {
                        if( ! nothrow )
                        {
                            throw;
                        }
                    }
                }

                if( ready2Disconnect )
                {
                    /*
                     * Everything has completed successfully or we had
                     * an exception from flushAllPendingTasks; in both
                     * cases we have to disconnect and deal accordingly
                     */

                    if( m_notifyCB )
                    {
                        m_notifyCB -> disconnect();
                        m_notifyCB.reset();
                    }

                    return time::neg_infin;
                }

                /*
                 * Currently there are no tasks in the queue, but the queue
                 * is not empty. Wait for some tasks to finish.
                 */

                return time::milliseconds( DEFAULT_WAIT_TIME_IN_MILLISECONDS );
            }

            virtual om::ObjPtr< Task > createSeedingTask() = 0;
            virtual bool canAcceptReadyTask() = 0;
            virtual bool pushReadyTask( SAA_in const om::ObjPtrCopyable< Task >& task ) = 0;
            virtual bool flushAllPendingTasks() = 0;
            virtual bool isWaitingExternalInput() NOEXCEPT = 0;

            /**
             * @brief The derived class will determine if it wants to handle
             * child tasks failures, but the default would be to handle these
             * as early as possible
             */

            virtual bool allowPushingOfFailedChildTasks() const NOEXCEPT
            {
                return false;
            }

            virtual std::size_t maxReadyOrExecuting() const NOEXCEPT OVERRIDE
            {
                /*
                 * Limit the queue size of ready and executing queues to
                 * MAX_READY_OR_EXECUTING_TASKS at any point in time
                 *
                 * See more comments above (at the definition of
                 * MAX_READY_OR_EXECUTING_TASKS)
                 */

                return MAX_READY_OR_EXECUTING_TASKS;
            }

            virtual void onEvent(
                SAA_in                      const ExecutionQueueNotify::EventId     eventId,
                SAA_in_opt                  const om::ObjPtrCopyable< Task >&       task
                ) NOEXCEPT OVERRIDE
            {
                /*
                 * We only subscribed to this event, so nothing else should be
                 * arriving
                 */

                BL_ASSERT( ExecutionQueueNotify::AllTasksCompleted == eventId );
                BL_UNUSED( eventId );

                BL_ASSERT( nullptr == task );
                BL_UNUSED( task );
            }

            virtual time::time_duration chk2LoopUntilShutdownFinished() NOEXCEPT OVERRIDE
            {
                time::time_duration duration;

                BL_NOEXCEPT_BEGIN()

                duration = chk2LoopUntilDoneInternal( true /* nothrow */ );

                BL_NOEXCEPT_END()

                return duration;
            }

            virtual time::time_duration chk2LoopUntilFinished() OVERRIDE
            {
                chk2InitAndBeginProcessing();

                /*
                 * chk2LoopUntilShutdownFinished() handles both - the graceful
                 * shutdown and the error case shutdown
                 */

                const auto duration = chk2LoopUntilDoneInternal( false /* nothrow */ );

                if( duration.is_special() && isFailedOrFailing() )
                {
                    /*
                     * We're exiting with an error because
                     * some of the child tasks have failed
                     */

                    cpp::safeRethrowException( m_exception );
                }

                return duration;
            }
        };

        typedef FanoutTasksObservableT<> FanoutTasksObservable;

    } // reactive

} // bl

#endif /* __BL_REACTIVE_FANOUTTASKSOBSERVABLE_H_ */
