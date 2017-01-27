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

#ifndef __BL_MESSAGING_BROKERBACKENDPROCESSING_H_
#define __BL_MESSAGING_BROKERBACKENDPROCESSING_H_

#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/BrokerErrorCodes.h>
#include <baselib/messaging/AsyncBlockDispatcher.h>

#include <baselib/data/models/JsonMessaging.h>

#include <baselib/security/SecurityInterfaces.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The peer id routing cache
         *
         * It is a simple map which maps the "logical target peer id" to "physical" such
         *
         * The "logical target peer id" is the target peer id of a client which is behind
         * proxy / multiplexer while the "physical peer id" is the peer id of the proxy /
         * multiplexer client connection
         */

        template
        <
            typename E = void
        >
        class PeerIdRoutingCacheT : public om::ObjectDefaultBase
        {
            BL_CTR_DEFAULT( PeerIdRoutingCacheT, protected )

        protected:

            std::unordered_map< uuid_t, uuid_t >                                    m_routingTable;
            mutable os::shared_mutex                                                m_lock;

        public:

            void associateTargetPeerId(
                SAA_in_opt          const uuid_t&                                   sourcePeerId,
                SAA_in_opt          const uuid_t&                                   targetPeerId
                )
            {
                os::unique_lock< decltype( m_lock ) > exclusiveLock( m_lock );

                m_routingTable[ targetPeerId ] = sourcePeerId;
            }

            bool dissociateTargetPeerId( SAA_in const uuid_t& targetPeerId )
            {
                os::unique_lock< decltype( m_lock ) > exclusiveLock( m_lock );

                return 0U != m_routingTable.erase( targetPeerId );
            }

            auto tryResolveTargetPeerId( SAA_in_opt const uuid_t& targetPeerId ) const -> uuid_t
            {
                os::shared_lock< decltype( m_lock ) > sharedLock( m_lock );

                const auto pos = m_routingTable.find( targetPeerId );

                if( pos != m_routingTable.end() )
                {
                    return pos -> second;
                }

                return uuids::nil();
            }
        };

        typedef om::ObjectImpl< PeerIdRoutingCacheT<> > PeerIdRoutingCache;

        /**
         * @brief class BrokerBackendTask - A wrapper task that uses authorization cache and
         * performs an authentication if a token was not cached previously or already expired.
         * On success updates the protocol data with the authenticated SID.
         */

        template
        <
            typename E = void
        >
        class BrokerBackendTaskT : public tasks::WrapperTaskBase
        {
            BL_DECLARE_OBJECT_IMPL( BrokerBackendTaskT )

        private:

            typedef BrokerBackendTaskT< E >                                         this_type;
            typedef tasks::WrapperTaskBase                                          base_type;

            typedef tasks::SimpleTaskImpl                                           SimpleTaskImpl;
            typedef security::AuthorizationCache                                    AuthorizationCache;
            typedef security::SecurityPrincipal                                     SecurityPrincipal;

            typedef AsyncBlockDispatcher                                            dispatcher_t;

            const om::ObjPtr< om::Proxy >                                           m_hostServices;
            const om::ObjPtr< PeerIdRoutingCache >                                  m_peerIdRoutingCache;
            const om::ObjPtr< AuthorizationCache >                                  m_authorizationCache;

            const om::ObjPtr< data::DataBlock >                                     m_data;
            const uuid_t                                                            m_sourcePeerId;
            uuid_t                                                                  m_targetPeerId;

            om::ObjPtr< BrokerProtocol >                                            m_brokerProtocol;

            om::ObjPtr< SecurityPrincipal >                                         m_principal;
            om::ObjPtr< data::DataBlock >                                           m_authenticationToken;

            om::ObjPtr< tasks::Task >                                               m_authorizationTask;

            cpp::ScalarTypeIniter< bool >                                           m_isBackendOnlyMessage;
            uuid_t                                                                  m_resolvedTargetPeerId;

        protected:

            enum State : std::size_t
            {
                Preparation,
                Authorization,
                Dispatch,
                Process,
            };

            State                                                                   m_state;

            BrokerBackendTaskT(
                SAA_in_opt          const om::ObjPtr< om::Proxy >&                  hostServices,
                SAA_in_opt          const om::ObjPtr< PeerIdRoutingCache >&         peerIdRoutingCache,
                SAA_in              const om::ObjPtr< AuthorizationCache >&         authorizationCache,
                SAA_in              const om::ObjPtr< data::DataBlock >&            data,
                SAA_in_opt          const uuid_t&                                   sourcePeerId,
                SAA_in_opt          const uuid_t&                                   targetPeerId
                )
                :
                m_hostServices( om::copy( hostServices ) ),
                m_peerIdRoutingCache( om::copy( peerIdRoutingCache ) ),
                m_authorizationCache( om::copy( authorizationCache ) ),
                m_data( om::copy( data ) ),
                m_sourcePeerId( sourcePeerId ),
                m_targetPeerId( targetPeerId ),
                m_resolvedTargetPeerId( uuids::nil() ),
                m_state( Preparation )
            {
                BL_ASSERT( m_peerIdRoutingCache );

                m_wrappedTask =
                    SimpleTaskImpl::createInstance< Task >(
                        cpp::bind( &this_type::parseAndProcessProtocolData, this )
                        );
            }

            void serializeBrokerProtocolMessage()
            {
                MessagingUtils::updateBrokerProtocolMessageInBlock(
                    m_brokerProtocol,
                    m_data,
                    m_sourcePeerId,
                    m_targetPeerId
                    );
            }

            void parseAndProcessProtocolData()
            {
                using namespace dm::messaging;

                const auto protocolDataOffset = m_data -> offset1();

                const std::string protocolData(
                    m_data -> begin() + protocolDataOffset,
                    m_data -> size() - protocolDataOffset
                    );

                /*
                 * First we validate the message is in the expected JSON format
                 */

                m_brokerProtocol =  BrokerProtocol::createInstance();

                utils::tryCatchLog< void, JsonException >(
                    "Error while trying to parse broker protocol message",
                    [ & ]() -> void
                    {
                        m_brokerProtocol =
                            dm::DataModelUtils::loadFromJsonText< BrokerProtocol >( protocolData );
                    },
                    []() -> void
                    {
                        BL_THROW_SERVER_ERROR(
                            BrokerErrorCodes::ProtocolValidationFailed,
                            BL_MSG()
                                << "Input is not in the expected JSON format"
                            );
                    },
                    utils::LogFlags::DEBUG_ONLY
                    );

                /*
                 * Then we copy parameters which will be pass-through (e.g. sourcePeerId & sourcePeerId)
                 * and then validate and process the other parameters and important properties:
                 *
                 * -- messageType
                 * -- messageId
                 * -- conversationId
                 * -- principalIdentityInfo
                 */

                const auto& messageTypeAsString = m_brokerProtocol -> messageType();

                MessageType::Enum messageType;

                BL_CHK_SERVER_ERROR(
                    false,
                    MessageType::tryToEnum( messageTypeAsString, messageType ),
                    BrokerErrorCodes::ProtocolValidationFailed,
                    BL_MSG()
                        << "The message type specified is invalid "
                        << str::quoteString( messageTypeAsString )
                    );

                const auto validateAsUuid = []( SAA_in const std::string& id ) -> uuid_t
                {
                    return utils::tryCatchLog< uuid_t, std::ios_base::failure >(
                        "Error while trying to parse property of type UUID",
                        [ & ]() -> uuid_t
                        {
                            return uuids::string2uuid( id );
                        },
                        []() -> uuid_t
                        {
                            BL_THROW_SERVER_ERROR(
                                BrokerErrorCodes::ProtocolValidationFailed,
                                BL_MSG()
                                    << "Property is not in the expected UUID format"
                                );

                            return uuids::nil();
                        },
                        utils::LogFlags::DEBUG_ONLY
                        );
                };

                ( void ) validateAsUuid( m_brokerProtocol -> conversationId() );
                ( void ) validateAsUuid( m_brokerProtocol -> messageId() );

                /*
                 * The sourcePreeId and targetPeerId parameters in the brokerProtocol message
                 * are generally optional and can be both IN and OUT
                 *
                 * However if they are provided they must be validated as UUIDs
                 */

                uuid_t sourcePeerId = uuids::nil();
                uuid_t targetPeerId = uuids::nil();

                const auto& sourcePeerIdAsString = m_brokerProtocol -> sourcePeerId();
                const auto& targetPeerIdAsString = m_brokerProtocol -> targetPeerId();

                if( ! sourcePeerIdAsString.empty() )
                {
                    sourcePeerId = validateAsUuid( sourcePeerIdAsString );
                }

                if( ! targetPeerIdAsString.empty() )
                {
                    targetPeerId = validateAsUuid( targetPeerIdAsString );
                }

                /*
                 * Check if this is an associate / dissociate target peer id message which
                 * are meant for broker processing
                 */

                m_isBackendOnlyMessage = true;

                switch( messageType )
                {
                    default:
                        m_isBackendOnlyMessage = false;
                        break;

                    case MessageType::BackendAssociateTargetPeerId:
                    {
                        BL_CHK_SERVER_ERROR(
                            true,
                            sourcePeerIdAsString.empty() || targetPeerIdAsString.empty(),
                            BrokerErrorCodes::ProtocolValidationFailed,
                            BL_MSG()
                                << "The sourcePeerId and targetPeerId properties cannot be empty"
                            );

                        /*
                         * Note that we want to allow target peer id routing *only* for target
                         * peer ids which have not already connected directly to this backend
                         *
                         * Otherwise the proxy might try to keep trying to associate stale peer id
                         * which has disconnected and connected directly to the broker
                         *
                         * Note also that for these cases we don't want to fail the message as it
                         * is an expected situation (since the proxy can keep trying to associate
                         * stale peer ids after they have disconnected)
                         */

                        BL_ASSERT( m_hostServices );

                        os::mutex_unique_lock guard;

                        const auto blockDispatcher =
                            m_hostServices -> tryAcquireRef< dispatcher_t >( dispatcher_t::iid(), &guard );

                        BL_CHK(
                            false,
                            nullptr != blockDispatcher,
                            BL_MSG()
                                << "Host services do not provide block dispatching service"
                            );

                        const auto targetQueue =
                            blockDispatcher -> tryGetMessageBlockCompletionQueue( targetPeerId );

                        if( targetQueue )
                        {
                            BL_LOG(
                                Logging::trace(),
                                BL_MSG()
                                    << "Associate message for peer id "
                                    << str::quoteString( uuids::uuid2string( targetPeerId ) )
                                    << " was ignored as it is directly connected to the backend"
                                );
                        }
                        else
                        {
                            m_peerIdRoutingCache -> associateTargetPeerId( sourcePeerId, targetPeerId );
                        }
                    }
                    break;

                    case MessageType::BackendDissociateTargetPeerId:
                    {
                        BL_CHK_SERVER_ERROR(
                            true,
                            targetPeerIdAsString.empty(),
                            BrokerErrorCodes::ProtocolValidationFailed,
                            BL_MSG()
                                << "The targetPeerId property cannot be empty"
                            );

                        ( void ) m_peerIdRoutingCache -> dissociateTargetPeerId( targetPeerId );
                    }
                    break;
                }

                if( ! m_isBackendOnlyMessage )
                {
                    m_resolvedTargetPeerId = m_peerIdRoutingCache -> tryResolveTargetPeerId( m_targetPeerId );
                }

                const auto& principalIdentityInfo = m_brokerProtocol -> principalIdentityInfo();

                if( m_isBackendOnlyMessage || ! principalIdentityInfo )
                {
                    /*
                     * This message is broker only message or it does not carry authentication info -
                     * we are done with the broker processing
                     *
                     * Note that we only serialize back the broker protocol message if it is not
                     * meant for the broker backend
                     *
                     * If the message is for the broker backend (i.e. isBackendOnlyMessage=true) then
                     * the message is IN only and there is no output data to be passed back to the
                     * block dispatcher
                     */

                    if( m_isBackendOnlyMessage )
                    {
                        m_state = Process;
                    }
                    else
                    {
                        m_state = Dispatch;
                        serializeBrokerProtocolMessage();
                    }

                    return;
                }

                const auto& securityPrincipal = principalIdentityInfo -> securityPrincipal();

                BL_CHK_SERVER_ERROR(
                    false,
                    nullptr == securityPrincipal,
                    BrokerErrorCodes::ProtocolValidationFailed,
                    BL_MSG()
                        << "Security principal info cannot be provided as input"
                    );

                const auto& authenticationToken = principalIdentityInfo -> authenticationToken();

                BL_CHK_SERVER_ERROR(
                    false,
                    nullptr != authenticationToken,
                    BrokerErrorCodes::ProtocolValidationFailed,
                    BL_MSG()
                        << "Authentication token information is required"
                    );

                const auto& tokenType = authenticationToken -> type();

                BL_CHK_SERVER_ERROR(
                    false,
                    m_authorizationCache -> tokenType() == tokenType,
                    BrokerErrorCodes::ProtocolValidationFailed,
                    BL_MSG()
                        << "The specified authentication token type "
                        << str::quoteString( tokenType )
                        << " is invalid or not supported"
                    );

                const auto& cookiesText = authenticationToken -> data();

                m_authenticationToken = AuthorizationCache::createAuthenticationToken( cookiesText );

                m_principal = m_authorizationCache -> tryGetAuthorizedPrinciplal( m_authenticationToken );

                if( ! m_principal )
                {
                    /*
                     * The cache is not populated for this credential, so we need to do actual authorization
                     * with the authorization service
                     */

                    m_authorizationTask = m_authorizationCache -> createAuthorizationTask( m_authenticationToken );
                }
            }

            void postAuthorization()
            {
                if( m_authorizationTask )
                {
                    /*
                     * If we have performed authorization successfully let's update the authorization
                     * cache and obtain the security principal
                     */

                    m_principal = m_authorizationCache -> update( m_authenticationToken, m_authorizationTask );
                }

                BL_ASSERT( m_principal );

                authorizeProtocolMessage( m_brokerProtocol, m_principal );

                serializeBrokerProtocolMessage();
            }

            virtual om::ObjPtr< tasks::Task > continuationTask() OVERRIDE
            {
                auto task = base_type::handleContinuationForward();

                if( task )
                {
                    return task;
                }

                const auto eptr = exception();

                if( eptr )
                {
                    tasks::WrapperTaskBase::exception(
                        BackendProcessingBase::chkToRemapToServerError(
                            eptr,
                            "Broker backend operation" /* messagePrefix */,
                            BrokerErrorCodes::AuthorizationFailed
                            )
                        );

                    return nullptr;
                }

                BL_MUTEX_GUARD( m_lock );

                const auto requestDispatch = [ this ]() -> void
                {
                    m_wrappedTask = SimpleTaskImpl::createInstance< tasks::Task >(
                        cpp::bind( &this_type::postAuthorization, this )
                        );

                    m_state = Dispatch;
                };

                switch( m_state )
                {
                    case Preparation:

                        /*
                         * If we have the security principal that means we have obtained it
                         * from the cache, so we just proceed to the process step
                         *
                         * If principal is nullptr that means we need to do authorization
                         */

                        if( m_principal )
                        {
                            BL_ASSERT( ! m_authorizationTask );
                            requestDispatch();
                        }
                        else
                        {
                            BL_ASSERT( m_authorizationTask );
                            m_wrappedTask = om::copy( m_authorizationTask );
                            m_state = Authorization;
                        }
                        break;

                    case Authorization:
                        requestDispatch();
                        break;

                    case Dispatch:
                        {
                            /*
                             * The backend processing is done and it is time to chain
                             * in the final block dispatch task
                             */

                            BL_ASSERT( m_hostServices );

                            os::mutex_unique_lock guard;

                            const auto blockDispatcher =
                                m_hostServices -> tryAcquireRef< dispatcher_t >( dispatcher_t::iid(), &guard );

                            BL_CHK(
                                false,
                                nullptr != blockDispatcher,
                                BL_MSG()
                                    << "Host services do not provide block dispatching service"
                                );

                            m_wrappedTask = blockDispatcher -> createDispatchTask(
                                m_resolvedTargetPeerId != uuids::nil() ? m_resolvedTargetPeerId : m_targetPeerId,
                                m_data
                                );

                            BL_CHK(
                                false,
                                nullptr != m_wrappedTask,
                                BL_MSG()
                                    << "The block dispatching service did not create a dispatching task"
                                );

                            m_state = Process;
                        }
                        break;

                    case Process:

                        /*
                         * We are done
                         */

                        return nullptr;

                    default:

                        BL_ASSERT( false );
                }

                return om::copyAs< Task >( this );
            }

        public:

            /*
             * TODO: This is a small helper function currently exposed here to avoid restructuring the
             * code too much, but it should be moved to MessageHelpers in subsequent PRs
             */

            static void authorizeProtocolMessage(
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in                  const om::ObjPtr< SecurityPrincipal >&          principal
                )
            {
                auto securityPrincipal = dm::messaging::SecurityPrincipal::createInstance();

                securityPrincipal -> sid( principal -> secureIdentity() );
                securityPrincipal -> givenName( principal -> givenName() );
                securityPrincipal -> familyName( principal -> familyName() );
                securityPrincipal -> email( principal -> email() );

                if( nullptr == brokerProtocol -> principalIdentityInfo() )
                {
                    const auto principalIdentityInfo = PrincipalIdentityInfo::createInstance();
                    brokerProtocol -> principalIdentityInfo( principalIdentityInfo );
                }

                brokerProtocol -> principalIdentityInfo() -> authenticationToken( nullptr );
                brokerProtocol -> principalIdentityInfo() -> securityPrincipal( std::move( securityPrincipal ) );
            }
        };

        typedef om::ObjectImpl< BrokerBackendTaskT<> > BrokerBackendTask;

        /**
         * @brief class BrokerBackendProcessing
         */

        template
        <
            typename E = void
        >
        class BrokerBackendProcessingT :
            public BackendProcessingBase,
            public AcceptorNotify
        {
            BL_DECLARE_OBJECT_IMPL( BrokerBackendProcessingT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY_CHAIN_BASE( BackendProcessingBase )
                BL_QITBL_ENTRY( AcceptorNotify )
            BL_QITBL_END( BackendProcessing )

        private:

            typedef BackendProcessingBase                                               base_type;

            typedef security::AuthorizationCache                                        AuthorizationCache;

            using base_type::m_hostServices;
            using base_type::m_lock;

            const om::ObjPtr< PeerIdRoutingCache >                                      m_peerIdRoutingCache;
            const om::ObjPtr< AuthorizationCache >                                      m_authorizationCache;

        protected:

            BrokerBackendProcessingT( SAA_in om::ObjPtr< AuthorizationCache >&& authorizationCache )
                :
                m_peerIdRoutingCache( PeerIdRoutingCache::createInstance() ),
                m_authorizationCache( BL_PARAM_FWD( authorizationCache ) )
            {
            }

        public:

            /*
             * BackendProcessing implementation
             */

            virtual bool autoBlockDispatching() const NOEXCEPT OVERRIDE
            {
                return false;
            }

            virtual auto createBackendProcessingTask(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const CommandId                                 commandId,
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in_opt              const uuid_t&                                   sourcePeerId,
                SAA_in_opt              const uuid_t&                                   targetPeerId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
                {
                    BL_MUTEX_GUARD( m_lock );

                    base_type::validateParameters( operationId, commandId, sessionId, chunkId );

                    return BrokerBackendTask::createInstance< tasks::Task >(
                        m_hostServices,
                        m_peerIdRoutingCache,
                        m_authorizationCache,
                        data,
                        sourcePeerId,
                        targetPeerId
                        );
                }

            /*
             * AcceptorNotify implementation
             */

            virtual bool peerConnectedNotify(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              CompletionCallback&&                                completionCallback
                )
                OVERRIDE
            {
                BL_UNUSED( completionCallback );

                /*
                 * For peers which are connecting directly to the backend message routing should
                 * be disabled
                 *
                 * Return 'false' to indicate that the call completed synchronously and that the
                 * completion callback won't be invoked
                 */

                if( m_peerIdRoutingCache -> dissociateTargetPeerId( peerId ) )
                {
                    BL_LOG(
                        Logging::trace(),
                        BL_MSG()
                            << "Peer id "
                            << str::quoteString( uuids::uuid2string( peerId ) )
                            << " was removed from the routing table as it has connected directly to the backend"
                        );
                }

                return false;
            }

            virtual bool peerDisconnectedNotify(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              CompletionCallback&&                                completionCallback
                )
                OVERRIDE
            {
                BL_UNUSED( peerId );
                BL_UNUSED( completionCallback );

                /*
                 * Nothing to do for this notification - just return 'false' to indicate that the
                 * call completed synchronously and that the completion callback won't be invoked
                 */

                return false;
            }
        };

        typedef om::ObjectImpl< BrokerBackendProcessingT<> > BrokerBackendProcessing;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BROKERBACKENDPROCESSING_H_ */
