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

#ifndef __BL_REACTIVE_FIXEDWORKERPOOLUNITBASE_H_
#define __BL_REACTIVE_FIXEDWORKERPOOLUNITBASE_H_

#include <baselib/reactive/FanoutTasksObservable.h>
#include <baselib/reactive/ObservableBase.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/Pool.h>
#include <baselib/core/BaseIncludes.h>

#include <cstdint>
#include <cstdio>

namespace bl
{
    namespace reactive
    {
        /**
         * @brief class FixedWorkerPoolUnitBase
         *
         * A base class to implement processing units which utilize fixed
         * worker pool of reusable tasks that will be continually scheduled
         */

        template
        <
            typename STREAM
        >
        class FixedWorkerPoolUnitBase :
            public FanoutTasksObservable
        {
        public:

            typedef FanoutTasksObservable                                                   base_type;

            enum
            {
                DEFAULT_POOL_SIZE = 16,
            };

        private:

            BL_DECLARE_OBJECT_IMPL( FixedWorkerPoolUnitBase )

        protected:

            enum
            {
                /*
                 * The maximum number of child tasks which can be pending
                 *
                 * Note: This is not the maximum # of child tasks in ready or pending
                 * state. This is defined in the base class (MAX_READY_OR_EXECUTING_TASKS)
                 * and currently this is 1024
                 */

                MAX_CHILD_TASKS_PENDING = 32 * 1024,
            };

            const cpp::ScalarTypeIniter< std::size_t >                                      m_tasksPoolSize;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                                   m_eqWorkerTasks;
            cpp::ScalarTypeIniter< bool >                                                   m_inputDisconnected;

            FixedWorkerPoolUnitBase(
                SAA_in              std::string&&                                           taskName,
                SAA_in              const std::size_t                                       tasksPoolSize = DEFAULT_POOL_SIZE
                )
                :
                m_tasksPoolSize( tasksPoolSize )
            {
                m_name = BL_PARAM_FWD( taskName );
            }

            virtual void tryStopObservable() OVERRIDE
            {
                if( m_eqWorkerTasks )
                {
                    m_eqWorkerTasks -> cancelAll( false /* wait */ );
                }

                base_type::tryStopObservable();
            }

            virtual om::ObjPtr< tasks::Task > createSeedingTask() OVERRIDE
            {
                /*
                 * There is no seeding task. Tasks will be delivered through the input connector
                 */

                return nullptr;
            }

            virtual bool isWaitingExternalInput() NOEXCEPT OVERRIDE
            {
                /*
                 * We're waiting for input only if the inputs are active and
                 * there was not an error
                 */

                return ( nullptr == m_exception && false == m_inputDisconnected );
            }

            virtual time::time_duration chk2LoopUntilFinished() OVERRIDE
            {
                if( ! m_eqWorkerTasks )
                {
                    m_eqWorkerTasks = tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                        tasks::ExecutionQueue::OptionKeepAll
                        );
                }

                return base_type::chk2LoopUntilFinished();
            }

            virtual time::time_duration chk2LoopUntilShutdownFinished() NOEXCEPT OVERRIDE
            {
                time::time_duration duration;

                BL_NOEXCEPT_BEGIN()

                if( isFailedOrFailing() && false == m_eqWorkerTasks -> isEmpty() )
                {
                    /*
                     * An error has occurred; just keep popping the
                     * tasks until we're done and try to cancel all
                     * remaining ones without waiting.
                     *
                     * Note: cancelAll() will remove all ready tasks
                     * so we don't need to keep popping here.
                     */

                    m_eqWorkerTasks -> cancelAll( false /* wait */ );
                }

                duration = base_type::chk2LoopUntilShutdownFinished();

                BL_NOEXCEPT_END()

                return duration;
            }

            bool verifyInputInternal( SAA_in const cpp::any& /* value */ )
            {
                if( m_exception )
                {
                    /*
                     * We want to rethrow the exception only when we're disconnected
                     */

                    cpp::safeRethrowException( m_exception );
                }

                base_type::chk2ThrowIfStopped();

                if( ( ! m_eqChildTasks ) || m_eqChildTasks -> size() >= MAX_CHILD_TASKS_PENDING )
                {
                    /*
                     * Our task hasn't started yet or there was an error; we can't accept input.
                     */

                    return false;
                }

                return true;
            }

        public:

            /*
             * This is a dummy connector point for input events to serve as a template for implementation
             * of connector points in the derived classes
             */

            bool onInputArrived( SAA_in const cpp::any& value )
            {
                BL_MUTEX_GUARD( m_lock );

                return verifyInputInternal( value );
            }

            /*
             * A default connector point for the input completed event
             */

            void onInputCompleted()
            {
                BL_MUTEX_GUARD( m_lock );

                m_inputDisconnected = true;
            }
        };

    } // reactive

} // bl

#endif /* __BL_REACTIVE_FIXEDWORKERPOOLUNITBASE_H_ */
