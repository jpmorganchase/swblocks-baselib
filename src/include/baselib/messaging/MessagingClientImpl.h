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

#ifndef __BL_MESSAGING_MESSAGINGCLIENTIMPL_H_
#define __BL_MESSAGING_MESSAGINGCLIENTIMPL_H_

#include <baselib/messaging/TcpBlockServerMessageDispatcher.h>
#include <baselib/messaging/MessagingClientBackendProcessing.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>
#include <baselib/messaging/BrokerErrorCodes.h>
#include <baselib/messaging/MessagingCommonTypes.h>

#include <baselib/tasks/TasksUtils.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        namespace detail
        {
            /**
             * @brief The messaging client implementation
             */

            template
            <
                typename STREAM
            >
            class MessagingClientImpl : public MessagingClientBlockDispatch
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE( MessagingClientImpl, MessagingClientBlockDispatch )

            public:

                enum : long
                {
                    RECONNECT_TIMER_IMMEDIATE  = 0L,
                    RECONNECT_TIMER_IN_SECONDS = 5L,
                };

                typedef MessagingClientImpl< STREAM >                                       this_type;

                typedef STREAM                                                              stream_t;

                /*
                 * This is the server acceptor for the incoming connections - we are going to use this type
                 * as an interface to extract other important types and interfaces
                 */

                typedef tasks::TcpBlockServerMessageDispatcherImpl< stream_t >              acceptor_incoming_t;

                /*
                 * This is the server acceptor for the outgoing connections - we are going to use this type
                 * as an interface to extract other important types and interfaces
                 */

                typedef tasks::TcpBlockServerMessageDispatcherOutgoingImpl< stream_t >      acceptor_outgoing_t;

                /*
                 * Other important types and interfaces
                 */

                typedef typename acceptor_incoming_t::async_wrapper_t                       async_wrapper_t;
                typedef typename async_wrapper_t::backend_interface_t                       backend_interface_t;

                typedef typename acceptor_outgoing_t::stream_ref                            stream_ref;
                typedef tasks::TcpConnectionEstablisherConnectorImpl< stream_t >            connection_establisher_t;

                typedef typename acceptor_outgoing_t::connection_t                          sender_connection_t;
                typedef typename acceptor_incoming_t::connection_t                          receiver_connection_t;
                typedef typename acceptor_incoming_t::server_state_t                        receiver_state_t;

                typedef typename sender_connection_t::notify_callback_t                     notify_callback_t;

                typedef tasks::Task                                                         Task;
                typedef tasks::ExecutionQueue                                               ExecutionQueue;
                typedef tasks::ExecutionQueueImpl                                           ExecutionQueueImpl;
                typedef tasks::SimpleTimerTask                                              SimpleTimerTask;
                typedef data::datablocks_pool_type                                          datablocks_pool_type;

                static void completionCallbackDefault( SAA_in_opt const std::exception_ptr& eptr ) NOEXCEPT
                {
                    if( eptr )
                    {
                        /*
                         * Get the error code from the exception and see if it something that
                         * should be ignored
                         */

                        const auto ec = eh::errorCodeFromExceptionPtr( eptr );

                        if(
                            ec &&
                            tasks::TcpSocketCommonBase::isExpectedSocketException(
                                true /* isCancelExpected */,
                                &ec
                                )
                            )
                        {
                            /*
                             * This error code is an expected exception, so we should just
                             * skip it
                             */

                            return;
                        }

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "Messaging client received messaging broker exception:\n"
                                << eh::diagnostic_information( eptr )
                            );
                    }
                }

            protected:

                class SharedState : public om::DisposableObjectBase
                {
                protected:

                    mutable os::mutex                                                           m_lock;
                    uuid_t                                                                      m_channelId;
                    const om::ObjPtr< datablocks_pool_type >                                    m_dataBlocksPool;
                    const uuid_t                                                                m_peerId;
                    om::ObjPtr< sender_connection_t >                                           m_sender;
                    om::ObjPtr< connection_establisher_t >                                      m_senderConnector;
                    const std::string                                                           m_senderHost;
                    const unsigned short                                                        m_senderPort;
                    om::ObjPtr< receiver_connection_t >                                         m_receiver;
                    om::ObjPtr< connection_establisher_t >                                      m_receiverConnector;
                    const om::ObjPtr< receiver_state_t >                                        m_receiverState;
                    const std::string                                                           m_receiverHost;
                    const unsigned short                                                        m_receiverPort;
                    om::ObjPtr< ExecutionQueue >                                                m_eqConnections;
                    om::ObjPtrDisposable< async_wrapper_t >                                     m_asyncWrapper;
                    om::ObjPtrDisposable< BackendProcessing >                                   m_backend;
                    om::ObjPtrDisposable< ExecutionQueue >                                      m_eqLocal;
                    cpp::ScalarTypeIniter< bool >                                               m_isDisposed;
                    cpp::ScalarTypeIniter< bool >                                               m_isSenderConnected;
                    cpp::ScalarTypeIniter< bool >                                               m_isReceiverConnected;
                    eh::error_code                                                              m_lastConnectionErrorCode;
                    CompletionCallback                                                          m_completionCallback;
                    cpp::ScalarTypeIniter< bool >                                               m_isNoCopyDataBlocks;

                    SharedState(
                        SAA_in                  om::ObjPtr< datablocks_pool_type >&&            dataBlocksPool,
                        SAA_in                  const uuid_t&                                   peerId,
                        SAA_in_opt              om::ObjPtr< sender_connection_t >&&             sender,
                        SAA_in                  std::string&&                                   senderHost,
                        SAA_in                  const unsigned short                            senderPort,
                        SAA_in_opt              om::ObjPtr< receiver_connection_t >&&           receiver,
                        SAA_in                  om::ObjPtr< receiver_state_t >&&                receiverState,
                        SAA_in                  std::string&&                                   receiverHost,
                        SAA_in                  const unsigned short                            receiverPort,
                        SAA_in                  om::ObjPtr< ExecutionQueue >&&                  eqConnections,
                        SAA_in                  om::ObjPtrDisposable< async_wrapper_t >&&       asyncWrapper,
                        SAA_in                  om::ObjPtrDisposable< BackendProcessing >&&     backend,
                        SAA_in_opt              om::ObjPtrDisposable< ExecutionQueue >&&        eqLocal
                        )
                        :
                        m_channelId( uuids::create() ),
                        m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) ),
                        m_peerId( peerId ),
                        m_sender( BL_PARAM_FWD( sender ) ),
                        m_senderHost( BL_PARAM_FWD( senderHost ) ),
                        m_senderPort( senderPort ),
                        m_receiver( BL_PARAM_FWD( receiver ) ),
                        m_receiverState( BL_PARAM_FWD( receiverState ) ),
                        m_receiverHost( BL_PARAM_FWD( receiverHost ) ),
                        m_receiverPort( receiverPort ),
                        m_eqConnections( BL_PARAM_FWD( eqConnections ) ),
                        m_asyncWrapper( BL_PARAM_FWD( asyncWrapper ) ),
                        m_backend( BL_PARAM_FWD( backend ) ),
                        m_eqLocal( BL_PARAM_FWD( eqLocal ) ),
                        m_completionCallback( &completionCallbackDefault )
                    {
                        if( ! m_eqLocal )
                        {
                            m_eqLocal = om::lockDisposable(
                                ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepNone )
                                );
                        }
                    }

                    template
                    <
                        typename CONNECTION
                    >
                    bool checkIfReconnected(
                        SAA_in_opt              const om::ObjPtr< CONNECTION >&                 connectionTask,
                        SAA_in                  const std::string&                              host,
                        SAA_in                  const unsigned short                            port,
                        SAA_inout_opt           om::ObjPtr< connection_establisher_t >&         connectorTask,
                        SAA_inout               bool&                                           isConnected
                        )
                    {
                        if( connectionTask && Task::Completed != connectionTask -> getState() )
                        {
                            connectorTask.reset();

                            isConnected = true;

                            return false;
                        }

                        isConnected = false;

                        if( connectorTask && Task::Completed != connectorTask -> getState() )
                        {
                            return false;
                        }

                        if( ! connectorTask || connectorTask -> isFailed() )
                        {
                            /*
                             * If the connector task was never created or it has failed
                             * then simply create it and try to connect (again)
                             */

                            if( connectorTask && connectorTask -> isFailed() )
                            {
                                /*
                                 * Log the exception in case of failed connection. System error exceptions
                                 * will be only logged once per specific error code to avoid filling up the log
                                 * on each connection attempt in case of e.g. unreachable host. Other exceptions
                                 * will be always logged as these aren't expected to happen on a regular basis
                                 */

                                std::string exceptionDetails;

                                try
                                {
                                    cpp::safeRethrowException( connectorTask -> exception() );
                                }
                                catch( eh::system_error& e )
                                {
                                    const auto ec = e.code();

                                    if( ec != m_lastConnectionErrorCode )
                                    {
                                        /*
                                         * We only want to log if the error code is not expected
                                         */

                                        if(
                                            ! tasks::TcpSocketCommonBase::isExpectedSocketException(
                                                connectorTask -> isCanceled(),
                                                &ec
                                                )
                                            )
                                        {
                                            exceptionDetails = eh::diagnostic_information( e );
                                        }

                                        m_lastConnectionErrorCode = ec;
                                    }
                                }
                                catch( std::exception& e )
                                {
                                    /*
                                     * Unexpected exceptions are always logged
                                     */

                                    exceptionDetails = eh::diagnostic_information( e );
                                }

                                if( ! exceptionDetails.empty() )
                                {
                                    BL_LOG_MULTILINE(
                                        Logging::debug(),
                                        BL_MSG()
                                            << "Cannot establish connection with messaging broker "
                                            << str::quoteString( host )
                                            << ". Exception details:\n"
                                            << exceptionDetails
                                        );
                                }
                            }

                            connectorTask =
                                connection_establisher_t::template createInstance(
                                    cpp::copy( host ),
                                    port,
                                    false /* logExceptions */
                                    );

                            m_eqLocal -> push_back( om::qi< Task >( connectorTask ) );

                            return false;
                        }

                        BL_ASSERT(
                            Task::Completed == connectorTask -> getState() &&
                            ! connectorTask -> isFailed()
                            );

                        isConnected = true;

                        return true;
                    }

                public:

                    std::uint64_t noOfBlocksSent() const NOEXCEPT
                    {
                        BL_MUTEX_GUARD( m_lock );

                        return m_sender ? m_sender -> noOfBlocksTransferred() : 0U;
                    }

                    std::uint64_t noOfBlocksReceived() const NOEXCEPT
                    {
                        BL_MUTEX_GUARD( m_lock );

                        return m_receiver ? m_receiver -> noOfBlocksTransferred() : 0U;
                    }

                    auto completionCallback() const NOEXCEPT -> const CompletionCallback&
                    {
                        return m_completionCallback;
                    }

                    void completionCallback( SAA_in CompletionCallback&& completionCallback ) NOEXCEPT
                    {
                        completionCallback.swap( m_completionCallback );
                    }

                    bool isNoCopyDataBlocks() const NOEXCEPT
                    {
                        return m_isNoCopyDataBlocks;
                    }

                    void isNoCopyDataBlocks( SAA_in bool isNoCopyDataBlocks ) NOEXCEPT
                    {
                        m_isNoCopyDataBlocks = isNoCopyDataBlocks;
                    }

                    auto onReconnectTimer( SAA_in const time::time_duration& defaultDuration ) -> time::time_duration
                    {
                        BL_MUTEX_GUARD( m_lock );

                        if( m_isDisposed )
                        {
                            /*
                             * The object has been disposed; nothing to do more here
                             */

                            return defaultDuration;
                        }

                        {
                            const auto initiallyConnected = m_isSenderConnected;

                            if(
                                checkIfReconnected(
                                    m_sender,
                                    m_senderHost,
                                    m_senderPort,
                                    m_senderConnector,
                                    m_isSenderConnected.lvalue()
                                    )
                                )
                            {
                                m_sender = sender_connection_t::template createInstance(
                                    notify_callback_t(),
                                    m_senderConnector -> detachStream(),
                                    m_dataBlocksPool,
                                    m_peerId
                                    );

                                m_eqConnections -> push_back( om::qi< Task >( m_sender ) );

                                m_senderConnector.reset();
                            }

                            const auto currentlyConnected = m_isSenderConnected;

                            if( initiallyConnected != currentlyConnected )
                            {
                                /*
                                 * If the connection status changed from connected to disconnected or
                                 * vice versa then we have to update the channel id, so the caller
                                 * can know that the channel has been changed
                                 */

                                m_channelId = uuids::create();

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << ( currentlyConnected ? "Established" : "Lost" )
                                        << " connection with messaging broker endpoint "
                                        << str::quoteString( m_senderHost )
                                        << ":"
                                        << m_senderPort
                                    );
                            }
                        }

                        {
                            const auto initiallyConnected = m_isReceiverConnected;

                            if(
                                checkIfReconnected(
                                    m_receiver,
                                    m_receiverHost,
                                    m_receiverPort,
                                    m_receiverConnector,
                                    m_isReceiverConnected.lvalue()
                                    )
                                )
                            {
                                m_receiver = receiver_connection_t::template createInstance( m_receiverState, m_peerId );
                                m_receiver -> attachStream( m_receiverConnector -> detachStream() );

                                m_eqConnections -> push_back( om::qi< Task >( m_receiver ) );

                                m_receiverConnector.reset();
                            }

                            const auto currentlyConnected = m_isReceiverConnected;

                            if( initiallyConnected != currentlyConnected )
                            {
                                /*
                                 * If the connection status changed from connected to disconnected or
                                 * vice versa then we have to update the channel id, so the caller
                                 * can know that the channel has been changed
                                 */

                                m_channelId = uuids::create();

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << ( currentlyConnected ? "Established" : "Lost" )
                                        << " connection with messaging broker endpoint "
                                        << str::quoteString( m_receiverHost )
                                        << ":"
                                        << m_receiverPort
                                    );
                            }
                        }

                        return defaultDuration;
                    }

                    virtual void dispose() NOEXCEPT OVERRIDE
                    {
                        BL_NOEXCEPT_BEGIN()

                        BL_MUTEX_GUARD( m_lock );

                        /*
                         * We only dispose the sender and the receiver tasks if we own the queue
                         * otherwise we assume the queue as well as the tasks are not owned by
                         * the client object and generally will be managed externally
                         */

                        if( m_senderConnector )
                        {
                            m_senderConnector -> requestCancel();
                            m_eqLocal -> wait( om::qi< Task >( m_senderConnector ) );
                            m_senderConnector.reset();
                        }

                        if( m_sender )
                        {
                            m_sender -> requestCancel();

                            if( m_eqConnections )
                            {
                                m_eqConnections -> wait( om::qi< Task >( m_sender ) );
                            }

                            m_sender.reset();
                        }

                        if( m_receiverConnector )
                        {
                            m_receiverConnector -> requestCancel();
                            m_eqLocal -> wait( om::qi< Task >( m_receiverConnector ) );
                            m_receiverConnector.reset();
                        }

                        if( m_receiver )
                        {
                            m_receiver -> requestCancel();

                            if( m_eqConnections )
                            {
                                m_eqConnections -> wait( om::qi< Task >( m_receiver ) );
                            }

                            m_receiver.reset();
                        }

                        /*
                         * Simply dispose the objects being held (if owned)
                         *
                         * Note that m_eqLocal, m_backend & m_asyncWrapper are already om::ObjPtrDisposable,
                         * so just calling reset() is enough
                         *
                         * ExecutionQueue::disposeQueue will also reset the pointer to nullptr
                         */

                        ExecutionQueue::disposeQueue( m_eqLocal );

                        m_asyncWrapper.reset();
                        m_backend.reset();

                        if( ! m_isDisposed )
                        {
                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Messaging client "
                                    << this
                                    << " was disposed"
                                );
                        }

                        m_isDisposed = true;

                        BL_NOEXCEPT_END()
                    }

                    void pushBlock(
                        SAA_in                  const uuid_t&                                   targetPeerId,
                        SAA_in                  const om::ObjPtr< data::DataBlock >&            dataBlock,
                        SAA_in_opt              CompletionCallback&&                            completionCallback
                        )
                    {
                        BL_MUTEX_GUARD( m_lock );

                        BL_CHK(
                            true,
                            m_isDisposed,
                            BL_MSG()
                                << "Messaging client has been disposed already"
                            );

                        BL_CHK_T_USER_FRIENDLY(
                            false,
                            nullptr != m_sender,
                            NotSupportedException()
                                << eh::errinfo_error_uuid( uuiddefs::ErrorUuidNotConnectedToBroker() ),
                            "Messaging client is not connected to messaging broker"
                            );

                        /*
                         * Just make a copy of the data block and push it in the sender queue for processing
                         */

                        m_sender -> scheduleBlock(
                            targetPeerId,
                            m_isNoCopyDataBlocks ?
                                om::copy( dataBlock )
                                :
                                data::DataBlock::copy( dataBlock, m_dataBlocksPool ),
                            completionCallback ?
                                BL_PARAM_FWD( completionCallback ) : cpp::copy( m_completionCallback )
                            );
                    }

                    bool isConnected() const NOEXCEPT
                    {
                        auto isConnected = false;

                        BL_NOEXCEPT_BEGIN()

                        BL_MUTEX_GUARD( m_lock );

                        isConnected =
                            m_isReceiverConnected &&
                            m_receiver &&
                            m_receiver -> getState() != Task::Completed &&
                            m_receiver -> isRemotePeerIdAvailable() &&
                            m_isSenderConnected &&
                            m_sender &&
                            m_sender -> getState() != Task::Completed &&
                            m_sender -> isRemotePeerIdAvailable();

                        BL_NOEXCEPT_END()

                        return isConnected;
                    }

                    uuid_t channelId() const NOEXCEPT
                    {
                        uuid_t result;

                        {
                            BL_MUTEX_GUARD( m_lock );

                            result = m_channelId;
                        }

                        return result;
                    }
                };

                typedef om::ObjectImpl< SharedState >                                       SharedStateImpl;

                os::mutex                                                                   m_lock;
                cpp::ScalarTypeIniter< bool >                                               m_preConnected;
                om::ObjPtrDisposable< SharedStateImpl >                                     m_state;
                tasks::SimpleTimer                                                          m_reconnectTimer;

                MessagingClientImpl(
                    SAA_in                  om::ObjPtr< datablocks_pool_type >&&            dataBlocksPool,
                    SAA_in                  const uuid_t&                                   peerId,
                    SAA_in_opt              om::ObjPtr< sender_connection_t >&&             sender,
                    SAA_in                  std::string&&                                   senderHost,
                    SAA_in                  const unsigned short                            senderPort,
                    SAA_in_opt              om::ObjPtr< receiver_connection_t >&&           receiver,
                    SAA_in                  om::ObjPtr< receiver_state_t >&&                receiverState,
                    SAA_in                  std::string&&                                   receiverHost,
                    SAA_in                  const unsigned short                            receiverPort,
                    SAA_in                  om::ObjPtr< ExecutionQueue >&&                  eqConnections,
                    SAA_in                  om::ObjPtrDisposable< async_wrapper_t >&&       asyncWrapper,
                    SAA_in                  om::ObjPtrDisposable< BackendProcessing >&&     backend,
                    SAA_in_opt              om::ObjPtrDisposable< ExecutionQueue >&&        eqLocal
                    )
                    :
                    m_preConnected( sender && receiver ),
                    m_state(
                        SharedStateImpl::createInstance(
                            BL_PARAM_FWD( dataBlocksPool ),
                            peerId,
                            BL_PARAM_FWD( sender ),
                            BL_PARAM_FWD( senderHost ),
                            senderPort,
                            BL_PARAM_FWD( receiver ),
                            BL_PARAM_FWD( receiverState ),
                            BL_PARAM_FWD( receiverHost ),
                            receiverPort,
                            BL_PARAM_FWD( eqConnections ),
                            BL_PARAM_FWD( asyncWrapper ),
                            BL_PARAM_FWD( backend ),
                            BL_PARAM_FWD( eqLocal )
                            )
                        ),
                    m_reconnectTimer(
                        cpp::bind(
                            &SharedStateImpl::onReconnectTimer,
                            om::ObjPtrCopyable< SharedStateImpl >::acquireRef( m_state.get() ),
                            time::seconds( RECONNECT_TIMER_IN_SECONDS )                     /* defaultDuration */
                            ),
                        time::seconds( RECONNECT_TIMER_IN_SECONDS )                         /* defaultDuration */,
                        time::seconds(
                            m_preConnected ? RECONNECT_TIMER_IN_SECONDS : RECONNECT_TIMER_IMMEDIATE
                            )                                                               /* initDelay */
                        )
                {
                }

            public:

                std::uint64_t noOfBlocksSent() const NOEXCEPT
                {
                    return m_state -> noOfBlocksSent();
                }

                std::uint64_t noOfBlocksReceived() const NOEXCEPT
                {
                    return m_state -> noOfBlocksReceived();
                }

                auto completionCallback() const NOEXCEPT -> const CompletionCallback&
                {
                    return m_state -> completionCallback();
                }

                void completionCallback( SAA_in CompletionCallback&& completionCallback ) NOEXCEPT
                {
                    m_state -> completionCallback( BL_PARAM_FWD( completionCallback ) );
                }

                virtual void dispose() NOEXCEPT OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( m_lock );

                    m_reconnectTimer.stop();

                    if( m_state )
                    {
                        m_state -> dispose();
                        m_state.reset();
                    }

                    BL_NOEXCEPT_END()
                }

                virtual void pushBlock(
                    SAA_in                  const uuid_t&                                   targetPeerId,
                    SAA_in                  const om::ObjPtr< data::DataBlock >&            dataBlock,
                    SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                    ) OVERRIDE
                {
                    BL_MUTEX_GUARD( m_lock );

                    BL_CHK(
                        false,
                        nullptr != m_state,
                        BL_MSG()
                            << "Messaging client has been disposed already"
                        );

                    /*
                     * Just forward the call to the internal state object
                     */

                    m_state -> pushBlock( targetPeerId, dataBlock, BL_PARAM_FWD( completionCallback ) );
                }

                virtual bool isConnected() const NOEXCEPT OVERRIDE
                {
                    return m_state -> isConnected();
                }

                virtual uuid_t channelId() const NOEXCEPT OVERRIDE
                {
                    return m_state -> channelId();
                }

                virtual bool isNoCopyDataBlocks() const NOEXCEPT OVERRIDE
                {
                    return m_state -> isNoCopyDataBlocks();
                }

                virtual void isNoCopyDataBlocks( SAA_in bool isNoCopyDataBlocks ) NOEXCEPT OVERRIDE
                {
                    m_state -> isNoCopyDataBlocks( isNoCopyDataBlocks );
                }
            };

        } // detail

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCLIENTIMPL_H_ */
