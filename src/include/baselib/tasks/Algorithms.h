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

#ifndef __BL_TASKS_ALGORITHMS_H_
#define __BL_TASKS_ALGORITHMS_H_

#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/Task.h>

#include <baselib/tasks/TasksIncludes.h>

#include <baselib/core/ThreadPool.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        namespace detail
        {
            /**
             * @brief class Algorithms
             */
            template
            <
                typename E = void
            >
            class AlgorithmsT
            {
            protected:

                template
                <
                    typename Functor
                >
                static void scheduleAndExecuteInParallelInternal(
                    SAA_in          Functor&&                                           schedulerCallback,
                    SAA_in_opt      const om::ObjPtr< ExecutionQueue >&                 executionQueue = nullptr
                    )
                {
                    om::ObjPtrDisposable< ExecutionQueue > localQueue;

                    if( ! executionQueue )
                    {
                        localQueue =
                            ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepFailed );
                    }

                    const auto& eq = executionQueue ? executionQueue : localQueue;

                    try
                    {
                        schedulerCallback( eq );

                        /*
                         * Flush the queue and discard the ready tasks (if the queue was
                         * configured to keep them); if some of the tasks failed an
                         * exception will be thrown
                         */

                        eq -> flushAndDiscardReady();
                    }
                    catch( std::exception& )
                    {
                        /*
                         * If an exception is thrown during the scheduling operation we must force flush the queue
                         * before re-throwing the exception. If the queue is not empty after the forced flush then
                         * something is very wrong and we should abort the application immediately.
                         */

                        eq -> forceFlushNoThrow();

                        if( ! eq -> isEmpty() )
                        {
                            BL_RIP_MSG( "Unable to flush execution queue after exception in scheduling tasks" );
                        }

                        throw;
                    }

                    /*
                     * The queue must always be empty here because we created it with keepSuccessful=true and
                     * also we passed nothrowIfFailed=false during flush
                     */

                    BL_ASSERT( eq -> isEmpty() );
                }

            public:

                template
                <
                    typename Functor
                >
                static void scheduleAndExecuteInParallel(
                    SAA_in          Functor&&                                           schedulerCallback,
                    SAA_in_opt      const om::ObjPtr< ExecutionQueue >&                 executionQueue = nullptr
                    )
                {
                    BL_CHK(
                        nullptr,
                        ThreadPoolDefault::getDefault( ThreadPoolId::GeneralPurpose ).get(),
                        BL_MSG()
                            << "No global default thread pool has been configured yet"
                        );

                    BL_CHK(
                        nullptr,
                        ThreadPoolDefault::getDefault( ThreadPoolId::NonBlocking ).get(),
                        BL_MSG()
                            << "No global default non-blocking thread pool has been configured yet"
                        );

                    scheduleAndExecuteInParallelInternal(
                        std::forward< Functor >( schedulerCallback ),
                        executionQueue
                        );
                }

            };

            typedef AlgorithmsT<> Algorithms;

        } // detail

        template
        <
            typename Functor
        >
        inline void scheduleAndExecuteInParallel(
            SAA_in          Functor&&                                           schedulerCallback,
            SAA_in_opt      const om::ObjPtr< ExecutionQueue >&                 executionQueue = nullptr
            )
        {
            detail::Algorithms::scheduleAndExecuteInParallel(
                std::forward< Functor >( schedulerCallback ),
                executionQueue
                );
        }

        template
        <
            typename Iterator,
            typename Return,
            typename Argument
        >
        inline std::vector< Return > parallelMap(
            SAA_in      Iterator                                        begin,
            SAA_in      const Iterator                                  end,
            SAA_in      std::function< Return ( SAA_in Argument ) >     functor,
            SAA_in      const std::size_t                               maxExecuting = 8
            )
        {
            std::vector< Return > result( std::distance( begin, end ) );

            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setThrottleLimit( maxExecuting );

                    std::size_t index = 0;

                    while( begin != end )
                    {
                        eq -> push_back(
                            [ =, &result ]() -> void
                            {
                                result[ index ] = functor( *begin );
                            }
                            );

                        ++begin;
                        ++index;
                    }
                }
                );

            return result;
        }

    } // tasks

} // bl

#endif /* __BL_TASKS_ALGORITHMS_H_ */
