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

#ifndef __BL_TASKS_UTILS_SHUTDOWNTASK_H_
#define __BL_TASKS_UTILS_SHUTDOWNTASK_H_

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TaskControlToken.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/TasksIncludes.h>

#include <vector>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class ShutdownTask - a simple shutdown task which monitors signals (SIGINT, SIGTERM, SIGQUIT)
         * and invokes a set of callbacks when triggered typically to initiate a graceful shutdown of a server
         * application process
         */

        template
        <
            typename E = void
        >
        class ShutdownTaskT : public TaskBase
        {
            BL_CTR_DEFAULT( ShutdownTaskT, protected )
            BL_DECLARE_OBJECT_IMPL( ShutdownTaskT )

        public:

            typedef ShutdownTaskT< E >                                                  this_type;

        protected:

            cpp::SafeUniquePtr< asio::signal_set >                                      m_signals;
            std::vector< cpp::void_callback_t >                                         m_callbacksToNotify;

            virtual void cancelTask() OVERRIDE
            {
                if( m_signals )
                {
                    /*
                     * Just cancel all operations associated with the signal set
                     */

                    m_signals -> cancel();
                }
            }

            void onStop(
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const int                                       signalNumber
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                /*
                 * Simple tasks must complete in one shot
                 * and then we notify the EQ we're done
                 */

                BL_UNUSED( signalNumber );
                BL_ASSERT( m_signals );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Shutdown requested..."
                    );

                for( const auto& callback : m_callbacksToNotify )
                {
                    callback();
                }

                m_callbacksToNotify.clear();
                m_signals.reset();

                BL_TASKS_HANDLER_END()
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                /*
                 * Just push the task in background
                 */

                BL_ASSERT( eq );
                BL_UNUSED( eq );

                /*
                 * Always schedule this task on the general purpose thread pool
                 */

                const auto threadPool = ThreadPoolDefault::getDefault( getThreadPoolId() );
                BL_ASSERT( threadPool );

                m_signals.reset( new asio::signal_set( threadPool -> aioService() ) );

                m_signals -> add( SIGINT );
                m_signals -> add( SIGTERM );

                #if defined( SIGQUIT )
                m_signals -> add( SIGQUIT );
                #endif // defined(SIGQUIT)

                m_signals -> async_wait(
                    cpp::bind(
                        &this_type::onStop,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::signal_number
                        )
                    );
            }

        public:

            void registerTask( SAA_in const om::ObjPtr< Task >& task )
            {
                registerCallback(
                    cpp::bind(
                        &Task::requestCancel,
                        om::ObjPtrCopyable< Task >::acquireRef( task.get() )
                        )
                    );
            }

            void registerCallback( SAA_in const cpp::void_callback_t& callback )
            {
                BL_MUTEX_GUARD( m_lock );

                m_callbacksToNotify.emplace_back( callback );
            }
        };

        typedef om::ObjectImpl< ShutdownTaskT<> > ShutdownTaskImpl;

    } // tasks

} // bl

#endif /* __BL_TASKS_UTILS_SHUTDOWNTASK_H_ */
