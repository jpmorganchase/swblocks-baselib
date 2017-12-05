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

#ifndef __BL_MESSAGING_ASYNCRPC_CONVERSATIONPROCESSINGTASK_H_
#define __BL_MESSAGING_ASYNCRPC_CONVERSATIONPROCESSINGTASK_H_

#include <baselib/messaging/MessagingCommonTypes.h>
#include <baselib/messaging/MessagingClientObjectDispatch.h>
#include <baselib/messaging/MessagingUtils.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/Task.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The task template implementation which encapsulates and implements the execution of
         * the conversation processing state machine
         */

        template
        <
            typename IMPL
        >
        class ConversationProcessingTaskT :
            public tasks::WrapperTaskBase,
            public MessagingClientObjectDispatch
        {
            BL_DECLARE_OBJECT_IMPL( ConversationProcessingTaskT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY_CHAIN_BASE( tasks::WrapperTaskBase )
                BL_QITBL_ENTRY( MessagingClientObjectDispatch )
            BL_QITBL_END( tasks::Task )

        public:

            typedef tasks::Task                                                         Task;
            typedef tasks::SimpleTaskImpl                                               SimpleTaskImpl;
            typedef tasks::SimpleTimerTask                                              SimpleTimerTask;

            typedef ConversationProcessingTaskT< IMPL >                                 this_type;
            typedef tasks::WrapperTaskBase                                              base_type;

            const om::ObjPtr< IMPL >                                                    m_impl;
            const om::ObjPtr< SimpleTaskImpl >                                          m_processingTaskImpl;
            const om::ObjPtr< Task >                                                    m_processingTask;
            const om::ObjPtr< SimpleTimerTask >                                         m_timerTaskImpl;
            const om::ObjPtr< Task >                                                    m_timerTask;

            cpp::ScalarTypeIniter< bool >                                               m_stopWasRequested;

        protected:

            ConversationProcessingTaskT( SAA_in om::ObjPtr< IMPL >&& impl )
                :
                m_impl( BL_PARAM_FWD( impl ) ),
                m_processingTaskImpl(
                    SimpleTaskImpl::createInstance(
                        cpp::bind(
                            &IMPL::onProcessing,
                            om::ObjPtrCopyable< IMPL >::acquireRef( m_impl.get() )
                            ),
                        "ConversationProcessingTask" /* taskName */
                        )
                    ),
                m_processingTask( om::qi< Task >( m_processingTaskImpl ) ),
                m_timerTaskImpl(
                    SimpleTimerTask::createInstance(
                        []() -> bool
                        {
                            return false;
                        },
                        time::neg_infin                                             /* duration */,
                        cpp::copy( m_impl -> timeout() )                            /* initDelay */
                        )
                    ),
                m_timerTask( om::qi< Task >( m_timerTaskImpl ) )
            {
                m_wrappedTask = om::copy( m_processingTask );
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                m_stopWasRequested = true;

                BL_NOEXCEPT_END()

                base_type::requestCancel();
            }

            virtual auto continuationTask() -> om::ObjPtr< Task > OVERRIDE
            {
                auto task = base_type::handleContinuationForward();

                if( task )
                {
                    return task;
                }

                /*
                 * Just alternate between the processing task and the timer task until
                 * we encounter an error or the state machine is is complete (which
                 * will be indicated by the isFinished() method)
                 */

                auto exception = this_type::exception();

                BL_MUTEX_GUARD( m_lock );

                if( m_impl -> isFinished() )
                {
                    return nullptr;
                }

                if( m_stopWasRequested )
                {
                    /*
                     * If stop was requested then we are going to finish immediately
                     * no matter what (i.e. no retries)
                     *
                     * Also if there the task already has failed with an exception then
                     * we honor it and if not then we simply force throw
                     * asio::error::operation_aborted which will result in the task
                     * failing with this exception (asio::error::operation_aborted)
                     */

                    if( exception )
                    {
                        return nullptr;
                    }

                    BL_CHK_EC_NM( asio::error::operation_aborted );
                }

                if( exception && ! m_impl -> retryProcessingTask( exception ) )
                {
                    return nullptr;
                }

                /*
                 * The processing task execution might have resulted in
                 * implementation's own processing task, execute it now if so
                 */

                auto implTask = m_impl -> tryPopProcessingTask();

                if( implTask )
                {
                    m_wrappedTask = std::move( implTask );
                }
                else
                {
                    if( m_wrappedTask == m_processingTask )
                    {
                        m_timerTaskImpl -> setInitDelay( m_impl -> timeout() );

                        m_wrappedTask = om::copy( m_timerTask );
                    }
                    else
                    {
                        m_wrappedTask = om::copy( m_processingTask );
                    }
                }

                return om::copyAs< Task >( this );
            }

        public:

            /*
             * Implement MessagingClientObjectDispatch
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_NOEXCEPT_END()
            }

            virtual void pushMessage(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                /*
                 * Note: the completionCallback is expected to always be empty here
                 */

                BL_UNUSED( completionCallback );
                BL_ASSERT( ! completionCallback );

                m_impl -> pushMessage( targetPeerId, brokerProtocol, payload );

                if( m_wrappedTask == m_timerTask )
                {
                    /*
                     * We are currently running the timer task let's cancel it, so the messages
                     * can be processed immediately
                     */

                    m_timerTask -> requestCancel();
                }
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return m_impl -> isConnected();
            }
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_ASYNCRPC_CONVERSATIONPROCESSINGTASK_H_ */
