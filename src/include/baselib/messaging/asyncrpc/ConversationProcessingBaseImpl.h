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

#ifndef __BL_MESSAGING_ASYNCRPC_CONVERSATIONPROCESSINGBASEIMPL_H_
#define __BL_MESSAGING_ASYNCRPC_CONVERSATIONPROCESSINGBASEIMPL_H_

#include <baselib/messaging/MessagingUtils.h>
#include <baselib/messaging/MessagingCommonTypes.h>
#include <baselib/messaging/MessagingClientObjectDispatch.h>
#include <baselib/messaging/BrokerErrorCodes.h>

#include <baselib/data/models/JsonMessaging.h>
#include <baselib/data/eh/ServerErrorHelpers.h>

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
         * @brief Async RPC policy default implementation
         * (usually just typedefs of the async RPC types)
         */

        class AsyncRpcPolicyDefault
        {
            BL_DECLARE_STATIC( AsyncRpcPolicyDefault )

        public:

            typedef AsyncRpcRequest                                                 request_t;
            typedef AsyncRpcResponse                                                response_t;
            typedef AsyncRpcPayload                                                 payload_t;

            struct MessageInfo
            {
                om::ObjPtrCopyable< BrokerProtocol >                                brokerProtocol;
                om::ObjPtrCopyable< payload_t >                                     payload;
            };

            static auto exceptionToServerErrorJson( SAA_in const std::exception_ptr& eptr )
                -> om::ObjPtr< dm::ServerErrorJson >
            {
                return dm::ServerErrorHelpers::createServerErrorObject( eptr );
            }

            static auto serverErrorJsonToException( SAA_in const om::ObjPtr< dm::ServerErrorJson >& serverErrorJson )
                -> std::exception_ptr
            {
                return dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson );
            }

            static auto castToBasePayload( SAA_in const om::ObjPtr< payload_t >& payload ) -> om::ObjPtr< Payload >
            {
                return dm::DataModelUtils::castTo< Payload >( payload );
            }

            static auto castToPayload( SAA_in const om::ObjPtr< Payload >& payload ) -> om::ObjPtr< payload_t >
            {
                return dm::DataModelUtils::castTo< payload_t >( payload );
            }
        };

        inline std::ostream& operator<<(
            SAA_inout       std::ostream&                                                           os,
            SAA_in          const AsyncRpcPolicyDefault::MessageInfo&                               message
            )
        {
            os
                << message.brokerProtocol
                << "\n"
                << message.payload;

            return os;
        }

        /**
         * @brief Async RPC conversation processing base generic implementation
         */

        template
        <
            typename ASYNCRPCPOLICY = AsyncRpcPolicyDefault
        >
        class ConversationProcessingBaseImpl : public om::ObjectDefaultBase
        {
        public:

            typedef MessagingClientObjectDispatch                                       object_dispatch_t;

            typedef typename ASYNCRPCPOLICY::request_t                                  request_t;
            typedef typename ASYNCRPCPOLICY::response_t                                 response_t;
            typedef typename ASYNCRPCPOLICY::payload_t                                  payload_t;

            typedef typename ASYNCRPCPOLICY::MessageInfo                                MessageInfo;

        protected:

            enum : std::size_t
            {
                BLOCK_QUEUE_SIZE                        = 32U,
                MAX_MESSAGE_DELIVERY_ATTEMPTS           = 5
            };

            enum : long
            {
                TIMEOUT_IN_MILLISECONDS_DEFAULT         = 1000L,
            };

            enum : long
            {
                /*
                 * Both of these timeouts could be conversation specific, but these
                 * are some smart defaults which should make sense as defaults:
                 *
                 * 30 seconds for acknowledgment timeout
                 * and 5 minutes for message timeout
                 */

                TIMEOUT_IN_MILLISECONDS_ACK             = 30 * 1000L,
                TIMEOUT_IN_MILLISECONDS_MSG             = 300 * 1000L,
            };

            const uuid_t                                                                m_peerId;
            const uuid_t                                                                m_targetPeerId;
            const uuid_t                                                                m_conversationId;
            const om::ObjPtr< object_dispatch_t >                                       m_objectDispatcher;
            const std::string                                                           m_authenticationCookies;

            MessageInfo                                                                 m_seedMessage;
            time::time_duration                                                         m_ackTimeout;
            time::time_duration                                                         m_msgTimeout;
            cpp::circular_buffer< MessageInfo >                                         m_pendingQueue;
            time::ptime                                                                 m_lastUnacknowledgedMessageSent;
            time::ptime                                                                 m_lastMessageReceived;
            time::time_duration                                                         m_timeout;
            cpp::ScalarTypeIniter< bool >                                               m_isFinished;
            cpp::ScalarTypeIniter< bool >                                               m_wasLastMessageSent;
            cpp::ScalarTypeIniter< bool >                                               m_isAckExpected;
            uuid_t                                                                      m_lastSentMessageId;
            MessageInfo                                                                 m_currentMessage;
            om::ObjPtr< tasks::Task >                                                   m_processingTask;

            cpp::ScalarTypeIniter< std::size_t >                                        m_retryCount;
            MessageInfo                                                                 m_retryMessage;

            mutable os::mutex                                                           m_lock;

            ConversationProcessingBaseImpl(
                SAA_in              const uuid_t&                                       peerId,
                SAA_in              const uuid_t&                                       targetPeerId,
                SAA_in              const uuid_t&                                       conversationId,
                SAA_inout           om::ObjPtr< object_dispatch_t >&&                   objectDispatcher,
                SAA_inout_opt       std::string&&                                       authenticationCookies = std::string(),
                SAA_inout_opt       MessageInfo&&                                       seedMessage = MessageInfo()
                )
                :
                m_peerId( peerId ),
                m_targetPeerId( targetPeerId ),
                m_conversationId( conversationId ),
                m_objectDispatcher( BL_PARAM_FWD( objectDispatcher ) ),
                m_authenticationCookies( BL_PARAM_FWD( authenticationCookies ) ),
                m_seedMessage( BL_PARAM_FWD( seedMessage ) ),
                m_ackTimeout( time::milliseconds( TIMEOUT_IN_MILLISECONDS_ACK ) ),
                m_msgTimeout( time::milliseconds( TIMEOUT_IN_MILLISECONDS_MSG ) ),
                m_pendingQueue( BLOCK_QUEUE_SIZE ),
                m_lastUnacknowledgedMessageSent( time::neg_infin ),
                m_lastMessageReceived( time::microsec_clock::universal_time() ),
                m_timeout( time::milliseconds( TIMEOUT_IN_MILLISECONDS_DEFAULT ) )
            {
            }

            virtual void processCurrentMessage() = 0;

            /*
             * Helper to create an error payload from exception pointer
             */

            static auto createErrorPayloadMessage( SAA_in const std::exception_ptr& eptr )
                -> om::ObjPtr< payload_t >
            {
                auto response = response_t::template createInstance();

                response -> serverErrorJson( ASYNCRPCPOLICY::exceptionToServerErrorJson( eptr ) );

                auto payload = payload_t::template createInstance();
                payload -> asyncRpcResponse( std::move( response ) );

                return payload;
            }

            /*
             * This function has to be implemented in the derived classes if
             * defaultProcessRequest() common wrapper is used. It should basically
             * unpack the request, execute relevant code, create and return response.
             */

            virtual auto processRequestImpl( SAA_in const om::ObjPtr< request_t >& request )
                -> om::ObjPtr< response_t >
            {
                BL_UNUSED( request );

                BL_THROW(
                    UnexpectedException(),
                    "ConversationProcessingBaseImpl::processRequestImpl() has to be overridden in derived class"
                    " if it uses 'defaultProcessRequest()' as common request processing wrapper"
                    );
            }

            /*
             * This is common code wrapper for request processing. It is intended to be called
             * from processCurrentMessage() for simple request -> response conversations.
             */

            void defaultProcessRequest( SAA_in const std::string& processorName )
            {
                auto asyncRpcResponse = response_t::template createInstance();

                utils::tryCatchLog(
                    [ & ]() -> std::string
                    {
                        return resolveMessage(
                            BL_MSG()
                                << processorName
                                << " has failed while processing request:\n"
                                << m_currentMessage
                            );
                    } /* cbMessage */,
                    [ & ]() -> void
                    {
                        BL_CHK(
                            false,
                            MessagingUtils::isRequestPayload( m_currentMessage.payload ),
                            BL_MSG()
                                << processorName
                                << " expects request to be present in the message payload"
                            );

                        asyncRpcResponse = processRequestImpl( m_currentMessage.payload -> asyncRpcRequest() );

                        BL_ASSERT( asyncRpcResponse );
                    } /* cbBlock */,
                    [ & ]() -> void
                    {
                        asyncRpcResponse -> serverErrorJson(
                            ASYNCRPCPOLICY::exceptionToServerErrorJson( std::current_exception() )
                            );
                    } /* cbOnError */
                    );

                const auto responsePayload = payload_t::template createInstance();

                responsePayload -> asyncRpcResponse( std::move( asyncRpcResponse ) );

                const auto brokerProtocol =
                    MessagingUtils::createResponseProtocolMessage(
                        uuids::string2uuid( m_currentMessage.brokerProtocol -> conversationId() )
                        );

                sendMessage( true /* isLastMessage */, brokerProtocol, responsePayload );
            }

            /*
             * This is common code wrapper for response processing. It is intended to be called
             * from processCurrentMessage() for simple request -> response conversations.
             */

            void defaultProcessResponse( SAA_in const std::string& processorName )
            {
                if( ! MessagingUtils::isResponsePayload( m_currentMessage.payload ) )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << processorName
                            << " expects response in the message payload\n"
                            << m_currentMessage
                        );

                    BL_THROW(
                        UnexpectedException(),
                        BL_MSG()
                            << processorName
                            << " expects response in the message payload"
                        );
                }

                m_isFinished = true;
            }

            void sendMessage(
                SAA_in              const bool                                          isLastMessage,
                SAA_in              const om::ObjPtr< BrokerProtocol >&                 brokerProtocol,
                SAA_in_opt          const om::ObjPtr< payload_t >&                      payload
                )
            {
                const bool isAckMessage =
                    MessageType::AsyncRpcAcknowledgment == MessageType::toEnum( brokerProtocol -> messageType() );

                BL_ASSERT( ! isLastMessage || ! isAckMessage );

                BL_LOG_MULTILINE(
                    Logging::trace(),
                    BL_MSG()
                        << "Sending "
                        << ( isLastMessage ? "last " : " " )
                        << "message:\n"
                        << brokerProtocol
                        << "\n"
                        << payload
                    );

                createProcessingTask( brokerProtocol, payload );

                m_retryCount = 0;
                m_retryMessage.brokerProtocol = om::copy( brokerProtocol );
                m_retryMessage.payload = om::copy( payload );

                if( ! isAckMessage )
                {
                    m_lastSentMessageId = uuids::string2uuid( brokerProtocol -> messageId() );
                    m_lastUnacknowledgedMessageSent = time::microsec_clock::universal_time();
                    m_isAckExpected = true;
                    m_wasLastMessageSent = isLastMessage;
                }
            }

            void createProcessingTask(
                SAA_in              const om::ObjPtr< BrokerProtocol >&                 brokerProtocol,
                SAA_in_opt          const om::ObjPtr< payload_t >&                      payload
                )
            {
                m_processingTask =
                    tasks::ExternalCompletionTaskImpl::createInstance< tasks::Task >(
                        cpp::bind(
                            &object_dispatch_t::pushMessageCopyCallback,
                            om::ObjPtrCopyable< object_dispatch_t >::acquireRef( m_objectDispatcher.get() ),
                            m_targetPeerId,
                            om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                            om::ObjPtrCopyable< Payload >(
                                payload ? ASYNCRPCPOLICY::castToBasePayload( payload ) : nullptr
                                ),
                            _1 /* onReady - the completion callback */
                            )
                        );
            }

            void sendErrorResponse(
                SAA_in              const eh::errc::errc_t                              errorCondition,
                SAA_in_opt          std::string&&                                       message =
                    BL_SYSTEM_ERROR_DEFAULT_MSG
                )
            {
                const auto brokerProtocol = MessagingUtils::createResponseProtocolMessage( m_conversationId );

                const auto payload = createErrorPayloadMessage(
                    std::make_exception_ptr(
                        SystemException::create(
                            eh::errc::make_error_code( errorCondition ),
                            std::move( message )
                            )
                        )
                    );

                sendMessage( true /* isLastMessage */, brokerProtocol, payload );
            }

            auto getAsyncRpcResponseOrThrowIfError() const -> om::ObjPtr< response_t >
            {

                if( m_currentMessage.payload == nullptr )
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "The message returned by the remote host does not contain any payload:\n"
                            << m_currentMessage
                        );

                    BL_THROW(
                        UnexpectedException(),
                        "The message returned by the remote host does not contain any payload"
                    );
                }

                const auto& response = m_currentMessage.payload -> asyncRpcResponse();

                if( response == nullptr )
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "The message returned by the remote host does not contain any response data:\n"
                            << m_currentMessage
                        );

                    BL_THROW(
                        UnexpectedException(),
                        "The message returned by the remote host does not contain any response data"
                    );
                }

                if( response -> serverErrorJson() )
                {
                    const auto eptr =
                        ASYNCRPCPOLICY::serverErrorJsonToException( response -> serverErrorJson() );

                    cpp::safeRethrowException( eptr );
                }

                return om::copy( response );
            }

        public:

            bool isFinished() const NOEXCEPT
            {
                return m_isFinished;
            }

            auto timeout() const NOEXCEPT -> const time::time_duration&
            {
                return m_timeout;
            }

            auto ackTimeout() const NOEXCEPT -> const time::time_duration&
            {
                return m_ackTimeout;
            }

            void ackTimeout( SAA_in const time::time_duration& ackTimeout ) NOEXCEPT
            {
                m_ackTimeout = ackTimeout;
            }

            auto msgTimeout() const NOEXCEPT -> const time::time_duration&
            {
                return m_msgTimeout;
            }

            void msgTimeout( SAA_in const time::time_duration& msgTimeout ) NOEXCEPT
            {
                m_msgTimeout = msgTimeout;
            }

            auto tryPopProcessingTask() NOEXCEPT -> om::ObjPtr< tasks::Task >
            {
                return om::moveAs< tasks::Task >( m_processingTask );
            }

            bool retryProcessingTask( SAA_in const std::exception_ptr& eptr )
            {
                if(
                    ++m_retryCount >= MAX_MESSAGE_DELIVERY_ATTEMPTS ||
                    m_retryMessage.brokerProtocol == nullptr
                    )
                {
                    return false;
                }

                bool doRetry = MessagingUtils::isRetryableMessagingBrokerError( eptr );

                if( doRetry )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Retry # "
                            << m_retryCount
                            << " to send message payload to peer with id "
                            << m_targetPeerId
                        );

                    createProcessingTask( m_retryMessage.brokerProtocol, m_retryMessage.payload );
                }

                return doRetry;
            }

            void onProcessing()
            {
                /*
                 * The basic state machine loop is as follows - unless finished:
                 *
                 * 1. if it is finished then exit
                 * 2. if acknowledgment message is expected then wait until it gets delivered
                 * 3. if there are no current message pop one from the queue (if any)
                 * 4. call processing of the current message
                 * 5. if the processing function is done processing the message it should set
                 *    it to empty
                 */

                BL_MUTEX_GUARD( m_lock );

                if( m_isFinished || m_processingTask )
                {
                    return;
                }

                if( m_isAckExpected )
                {
                    BL_ASSERT( ! m_lastUnacknowledgedMessageSent.is_special() );

                    /*
                     * We are waiting for acknowledgment message for a message that we just sent
                     */

                    const auto delta =
                        time::microsec_clock::universal_time() - m_lastUnacknowledgedMessageSent;

                    BL_CHK_T_USER_FRIENDLY(
                        true,
                        delta >= m_ackTimeout,
                        TimeoutException()
                            << eh::errinfo_error_uuid( uuiddefs::ErrorUuidResponseTimeout() ),
                        BL_MSG()
                            << "Messaging client did not receive acknowledgment within the specified interval "
                            << m_ackTimeout
                        );

                    if( m_pendingQueue.empty() )
                    {
                        return;
                    }

                    const auto& messageInfo = m_pendingQueue.front();
                    const auto& protocol = messageInfo.brokerProtocol;

                    BL_LOG_MULTILINE(
                        Logging::trace(),
                        BL_MSG()
                            << "Acknowledgment message received:\n"
                            << messageInfo
                        );

                    if(
                        messageInfo.payload ||
                        MessageType::AsyncRpcAcknowledgment != MessageType::toEnum( protocol -> messageType() ) ||
                        m_lastSentMessageId != uuids::string2uuid( protocol -> messageId() )
                        )
                    {
                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "Acknowledgment message is expected but a different "
                                << "one was received or the message id does not match:\n"
                                << messageInfo
                            );

                        BL_THROW_EC(
                            eh::errc::make_error_code( eh::errc::invalid_argument ),
                            BL_MSG()
                                << "Acknowledgment message is expected but a different "
                                << "one was received or the message id does not match"
                            );
                    }

                    m_lastSentMessageId = uuids::nil();
                    m_pendingQueue.pop_front();

                    m_lastUnacknowledgedMessageSent = time::neg_infin;
                    m_isAckExpected = false;
                }

                if( m_wasLastMessageSent )
                {
                    /*
                     * The last message was sent and acknowledged for this conversation
                     * and we are ready to finish the processing now
                     */

                    if( ! m_pendingQueue.empty() )
                    {
                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "Message was received for conversation "
                                << str::quoteString( uuids::uuid2string( m_conversationId ) )
                                << " after it has ended:\n"
                                << m_pendingQueue.front()
                            );

                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Message was received for conversation "
                                << str::quoteString( uuids::uuid2string( m_conversationId ) )
                                << " after it has ended"
                        );
                    }

                    m_isFinished = true;

                    return;
                }

                if( m_seedMessage.brokerProtocol )
                {
                    /*
                     * Check to see if there is a seed message to be sent first
                     */

                    sendMessage(
                        false /* isLastMessage */,
                        m_seedMessage.brokerProtocol,
                        m_seedMessage.payload
                        );

                    m_seedMessage = MessageInfo();

                    return;
                }

                if( nullptr == m_currentMessage.brokerProtocol && ! m_pendingQueue.empty() )
                {
                    /*
                     * Check if this is a request type message and if so then we will insist that it
                     * is authenticated
                     */

                    m_currentMessage = std::move( m_pendingQueue.front() );
                    m_pendingQueue.pop_front();

                    const auto messageType =
                        MessageType::toEnum( m_currentMessage.brokerProtocol -> messageType() );

                    BL_LOG_MULTILINE(
                        Logging::trace(),
                        BL_MSG()
                            << "Processing message:\n"
                            << m_currentMessage
                        );

                    if(
                        MessageType::AsyncRpcDispatch == messageType &&
                        m_currentMessage.payload &&
                        m_currentMessage.payload -> asyncRpcRequest()
                        )
                    {
                        const auto& identityInfo =
                            m_currentMessage.brokerProtocol -> principalIdentityInfo();

                        if( ! identityInfo || ! identityInfo -> securityPrincipal() )
                        {
                            sendErrorResponse(
                                eh::errc::permission_denied                 /* errorCondition */,
                                "Request messages must be authenticated"        /* message */
                                );

                            m_currentMessage = MessageInfo();

                            return;
                        }
                    }

                    m_lastMessageReceived = time::microsec_clock::universal_time();
                }

                if( m_currentMessage.brokerProtocol )
                {
                    utils::tryCatchLog(
                        "Message conversation failed to process current message",
                        [ & ]() -> void
                        {
                            processCurrentMessage();
                        } /* cbBlock */,
                        [ & ]() -> void
                        {
                            throw;
                        } /* cbOnError */
                        );
                }
                else
                {
                    /*
                     * Check if we have waited too much and we should timeout
                     */

                    const auto delta =
                        time::microsec_clock::universal_time() - m_lastMessageReceived;

                    BL_CHK_T_USER_FRIENDLY(
                        true,
                        delta >= m_msgTimeout,
                        TimeoutException()
                            << eh::errinfo_error_uuid( uuiddefs::ErrorUuidResponseTimeout() ),
                        BL_MSG()
                            << "Messaging client did not receive response within the specified interval "
                            << m_msgTimeout
                        );
                }
            }

            void pushMessage(
                SAA_in              const uuid_t&                                       targetPeerId,
                SAA_in              const om::ObjPtr< BrokerProtocol >&                 brokerProtocol,
                SAA_in_opt          const om::ObjPtr< Payload >&                        payload
                )
            {
                BL_MUTEX_GUARD( m_lock );

                BL_CHK(
                    false,
                    targetPeerId == m_peerId,
                    BL_MSG()
                        << "The target peer id "
                        << str::quoteString( uuids::uuid2string( targetPeerId ) )
                        << " does not match the expected peer id "
                        << str::quoteString( uuids::uuid2string( m_peerId ) )
                    );

                if( m_pendingQueue.full() )
                {
                    BL_THROW_EC(
                        eh::errc::make_error_code( BrokerErrorCodes::TargetPeerQueueFull ),
                        BL_MSG()
                            << "The task with conversation id "
                            << str::quoteString( uuids::uuid2string( m_conversationId ) )
                            << " has its queue full and can't receive messages"
                        );
                }

                /*
                 * Before we place the message into the local ring buffer check if this message was
                 * delivered from a remote peer (i.e. ! sourcePeerId.empty()) and if so then we immediately
                 * send back acknowledgment message to the remote peer (unless the message received is
                 * of acknowledgment message type of course)
                 */

                const auto messageType = MessageType::toEnum( brokerProtocol -> messageType() );

                if( MessageType::AsyncRpcAcknowledgment != messageType )
                {
                    const auto& sourcePeerId = brokerProtocol -> sourcePeerId();

                    BL_CHK(
                        true,
                        sourcePeerId.empty(),
                        BL_MSG()
                            << "Invalid source peer id "
                            << str::quoteString( sourcePeerId )
                        );

                    BL_CHK(
                        false,
                        uuids::string2uuid( sourcePeerId ) == m_targetPeerId,
                        BL_MSG()
                            << "The source peer id "
                            << str::quoteString( sourcePeerId )
                            << " does not match the expected peer id "
                            << str::quoteString( uuids::uuid2string( m_targetPeerId ) )
                        );

                    sendMessage(
                        false /* isLastMessage */,
                        MessagingUtils::createAcknowledgmentMessage(
                            m_conversationId,
                            uuids::string2uuid( brokerProtocol -> messageId() )
                            ),
                        nullptr /* payload */
                        );
                }

                MessageInfo messageInfo;

                messageInfo.brokerProtocol = brokerProtocol;
                messageInfo.payload = payload ? ASYNCRPCPOLICY::castToPayload( payload ) : nullptr;

                m_pendingQueue.push_back( std::move( messageInfo ) );
            }

            bool isConnected() const NOEXCEPT
            {
                return m_objectDispatcher -> isConnected();
            }
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_ASYNCRPC_CONVERSATIONPROCESSINGBASEIMPL_H_ */
