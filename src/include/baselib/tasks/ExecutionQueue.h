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

#ifndef __BL_TASKS_EXECUTIONQUEUE_H_
#define __BL_TASKS_EXECUTIONQUEUE_H_

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueueNotify.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( ExecutionQueue, "96312cd1-2017-4e86-8ede-99d87847ba46" )

namespace bl
{
    namespace tasks
    {
        /**
         * @brief The execution queue interface
         */

        class ExecutionQueue : public om::Disposable
        {
            BL_DECLARE_INTERFACE( ExecutionQueue )

        public:

            enum
            {
                OptionKeepNone              = 0,
                OptionKeepSuccessful        = 1 << 0,
                OptionKeepFailed            = 1 << 1,
                OptionKeepCanceled          = 1 << 2,
                OptionKeepExecuted          = OptionKeepSuccessful | OptionKeepFailed,
                OptionKeepAll               = OptionKeepSuccessful | OptionKeepFailed | OptionKeepCanceled,
            };

            enum QueueId
            {
                Ready,
                Executing,
                Pending,
            };

            typedef cpp::function< void ( SAA_in const om::ObjPtr< Task >& task ) > task_callback_type;

            virtual bool isEmpty() const NOEXCEPT = 0;

            virtual bool hasReady() const NOEXCEPT = 0;

            virtual bool hasExecuting() const NOEXCEPT = 0;

            virtual bool hasPending() const NOEXCEPT = 0;

            virtual std::size_t size() const NOEXCEPT = 0;

            virtual void setThrottleLimit( SAA_in const std::size_t maxExecuting = 0 ) = 0;

            virtual void setOptions( SAA_in const unsigned options = OptionKeepFailed ) = 0;

            virtual void setNotifyCallback(
                SAA_in                  om::ObjPtr< om::Proxy >&&                   notifyCB,
                SAA_in                  const unsigned                              eventsMask = ExecutionQueueNotify::AllEvents
                ) = 0;

            virtual ThreadPool* getLocalThreadPool() const NOEXCEPT = 0;

            virtual void setLocalThreadPool( SAA_inout ThreadPool* threadPool ) NOEXCEPT = 0;

            /**
             * @brief A wrapper for ExecutionQueue::push_back( const om::ObjPtr< Task >&, const bool )
             */

            virtual om::ObjPtr< Task > push_back(
                SAA_in                  cpp::void_callback_t&&                      cbTask,
                SAA_in                  const bool                                  dontSchedule = false
                ) = 0;

            /**
             * @brief Pushes a task on the back of the pending or ready queue depending on dontSchedule
             *
             * If dontSchedule=true it will be added directly to the back of the ready queue (to be
             * potentially scheduled later on). Normally dontSchedule is false, but it could be true
             * for cases where we're preparing a queue of worker tasks to be rescheduled continuously
             */

            virtual void push_back(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  dontSchedule = false
                ) = 0;

            /**
             * @brief A wrapper for ExecutionQueue::push_front( const om::ObjPtr< Task >&, const bool )
             */

            virtual om::ObjPtr< Task > push_front(
                SAA_in                  cpp::void_callback_t&&                      cbTask,
                SAA_in                  const bool                                  dontSchedule = false
                ) = 0;

            /**
             * @brief Pushes a task on the front of the pending or ready queue depending on dontSchedule
             *
             * If dontSchedule=true it will be added directly to the front of the ready queue (to be
             * potentially scheduled later on). Normally dontSchedule is false, but it could be true
             * for cases where we're preparing a queue of worker tasks to be rescheduled continuously
             */

            virtual void push_front(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  dontSchedule = false
                ) = 0;

            virtual om::ObjPtr< Task > pop( SAA_in const bool wait = true ) = 0;

            virtual om::ObjPtr< Task > top( SAA_in const bool wait = true ) = 0;

            virtual void scanQueue(
                SAA_in                  const QueueId                               queueId,
                SAA_in                  const task_callback_type&                   cbTasks
                ) = 0;

            /**
             * @brief Wait for a task to complete execution. The task will also be
             * prioritized if it is in the pending queue and cancel=false
             *
             * @param task The task to wait for
             * @param cancel Request the task to be canceled if it is not
             * executing already
             */

            virtual void wait(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in_opt              const bool                                  cancel = false
                ) = 0;

            /**
             * @brief Wait for a task to complete execution, but the task priority
             * and place in the execution queue will be preserved as is
             *
             * @param task The task to wait for
             */

            virtual void waitNoPrioritize( SAA_in const om::ObjPtr< Task >& task ) = 0;

            /**
             * @brief Prioritizes the task to be next in line the execution queue
             * if it hasn't been scheduled already
             *
             * @param task The task to prioritize
             * @param wait If the function should block and wait on the queue lock
             * @return true if the task was moved in front of the pending queue
             * and false if the task is already executing or ready
             */

            virtual bool prioritize(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in                  const bool                                  wait
                ) = 0;

            /**
             * @brief Flushes the execution queue
             */

            virtual void flush(
                SAA_in_opt              const bool                                  discardPending = false,
                SAA_in_opt              const bool                                  nothrowIfFailed = false,
                SAA_in_opt              const bool                                  discardReady = false,
                SAA_in_opt              const bool                                  cancelExecuting = false
                ) = 0;

            /**
             * @brief Forces a flush of the queue by canceling everything
             *
             * If wait=false it will return immediately
             *
             * Note: This is a nothrow operation; if an exception is thrown
             * by flush we'll abort the application
             */

            virtual void forceFlushNoThrow( SAA_in const bool wait = true ) NOEXCEPT = 0;

            /**
             * @brief Cancels a task
             *
             * @param task The task to cancel
             * @param wait Should we wait for the cancellation event
             *
             * @return true if the task was canceled successfully (if wait is false); if wait
             * is true the return value is always false
             */

            virtual bool cancel(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in_opt              const bool                                  wait = true
                ) = 0;

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

            virtual void cancelAll( SAA_in const bool wait ) NOEXCEPT = 0;


            /**
             * @brief Just a wrapper around flush( ... ) to discard the ready tasks
             *
             * Note: This method will throw if some of the ready tasks have failed
             */

            void flushAndDiscardReady()
            {
                flush(
                   false /* discardPending */,
                   false /* nothrowIfFailed */,
                   true  /* discardReady */,
                   false /* cancelExecuting */
                   );
            }

            /**
             * @brief Just a wrapper around flush( ... ) to flush the queue, but
             * nothrow if some of the tasks failed
             *
             * Note: This method will throw if something else failed, but not
             * because tasks failed. It should be used only when tasks are
             * expected to fail
             */

            void flushNoThrowIfFailed()
            {
                flush(
                   false /* discardPending */,
                   true  /* nothrowIfFailed */,
                   false /* discardReady */,
                   false /* cancelExecuting */
                   );
            }

            /**
             * @brief Waits for a task to complete successfully. If the task
             * fails then it throws the exception captured in the task
             */

            void waitForSuccess(
                SAA_in                  const om::ObjPtr< Task >&                   task,
                SAA_in_opt              const bool                                  cancel = false
                )
            {
                try
                {
                    wait( task, cancel );

                    if( task && task -> isFailed() )
                    {
                        cpp::safeRethrowException( task -> exception() );
                    }
                }
                catch( eh::system_error& e )
                {
                    /*
                     * The task failed due to cancellation with an expected error code,
                     * so this specific exception should be filtered out and ignored
                     */

                    if( cancel && e.code() == asio::error::operation_aborted )
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

            std::size_t getQueueSize( SAA_in const QueueId queueId ) NOEXCEPT
            {
                std::size_t count = 0U;

                BL_NOEXCEPT_BEGIN()

                scanQueue(
                    queueId,
                    [ &count ]( SAA_in const om::ObjPtr< Task >& )
                    {
                        ++count;
                    }
                    );

                BL_NOEXCEPT_END()

                return count;
            }

            static void disposeQueue( SAA_inout_opt om::ObjPtrDisposable< ExecutionQueue >& queue ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( queue )
                {
                    queue -> forceFlushNoThrow();
                    queue -> dispose();
                    queue.reset();
                }

                BL_NOEXCEPT_END()
            }
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_EXECUTIONQUEUE_H_ */
