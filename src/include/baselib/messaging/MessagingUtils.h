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

#ifndef __BL_MESSAGING_MESSAGINGUTILS_H_
#define __BL_MESSAGING_MESSAGINGUTILS_H_

#include <baselib/messaging/MessagingCommonTypes.h>
#include <baselib/messaging/MessagingClientBlock.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>
#include <baselib/messaging/MessagingClientObject.h>
#include <baselib/messaging/MessagingClientObjectDispatch.h>
#include <baselib/messaging/MessagingClientFactory.h>

#include <baselib/data/eh/ServerErrorHelpers.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <cmath>
#include <queue>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief Messaging core utility functions
         */

        template
        <
            typename E = void
        >
        class MessagingUtilsT
        {
            BL_DECLARE_STATIC( MessagingUtilsT )

        protected:

            static void verifyRequiredProperty(
                SAA_in                  const char*                                     propertyName,
                SAA_in                  const std::string&                              propertyValue
                )
            {
                BL_CHK_T(
                    true,
                    propertyValue.empty(),
                    InvalidDataFormatException(),
                    BL_MSG()
                        << "The "
                        << str::quoteString( propertyName )
                        << " property cannot be empty"
                    );
            }

            static std::size_t defaultCapacity() NOEXCEPT
            {
                return DataBlock::defaultCapacity();
            }

        public:

            typedef data::datablocks_pool_type                                          datablocks_pool_type;
            typedef data::DataBlock                                                     DataBlock;

            static bool isRetryableMessagingBrokerError( SAA_in const std::exception_ptr& eptr )
            {
                bool doRetry = false;

                try
                {
                    cpp::safeRethrowException( eptr );
                }
                catch( ServerErrorException& e )
                {
                    const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );

                    if( ec && BrokerErrorCodes::isExpectedErrorCode( *ec ) )
                    {
                        switch( ec -> value() )
                        {
                            case BrokerErrorCodes::TargetPeerNotFound:
                            case BrokerErrorCodes::TargetPeerQueueFull:
                                doRetry = true;
                                break;
                        }
                    }
                }
                catch( SystemException& e )
                {
                    const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );

                    doRetry = tasks::TcpSocketCommonBase::isExpectedSocketException(
                        false /* isCancelExpected */,
                        ec
                        );

                    if( ! doRetry )
                    {
                        doRetry = tasks::TcpSslSocketAsyncBase::isExpectedSslException( eptr, e, ec );
                    }
                }
                catch( std::exception& e )
                {
                    const auto* uuid = eh::get_error_info< eh::errinfo_error_uuid >( e );

                    if( uuid && *uuid == uuiddefs::ErrorUuidNotConnectedToBroker() )
                    {
                        doRetry = true;
                    }
                }

                return doRetry;
            }

            static void verifyBrokerProtocolMessage( SAA_in const om::ObjPtr< BrokerProtocol >& brokerProtocol )
            {
                MessageType::Enum messageTypeEnum;

                const auto& messageType = brokerProtocol -> messageType();

                BL_CHK_T(
                    false,
                    MessageType::tryToEnum( messageType , messageTypeEnum ),
                    InvalidDataFormatException(),
                    BL_MSG()
                        << "The message type specified is invalid "
                        << str::quoteString( messageType )
                    );

                verifyRequiredProperty( "messageId" /* propertyName */, brokerProtocol -> messageId() );
                verifyRequiredProperty( "conversationId" /* propertyName */, brokerProtocol -> conversationId() );

                if( ! brokerProtocol -> principalIdentityInfo() )
                {
                    return;
                }

                const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();

                const bool isValidPrincipalIdentityInfo =
                    ( principalIdentityInfo -> authenticationToken() || principalIdentityInfo -> securityPrincipal() ) &&
                    ! ( principalIdentityInfo -> authenticationToken() && principalIdentityInfo -> securityPrincipal() );

                BL_CHK_T(
                    false,
                    isValidPrincipalIdentityInfo,
                    InvalidDataFormatException(),
                    BL_MSG()
                        << "Principal identity info is invalid: either security principal or authentication token must be provided"
                    );

                if( principalIdentityInfo -> authenticationToken() )
                {
                    const auto& token = principalIdentityInfo -> authenticationToken();

                    BL_CHK_T(
                        true,
                        token -> type().empty(),
                        InvalidDataFormatException(),
                        BL_MSG()
                            << "The authentication token type specified is invalid"
                        );

                    BL_CHK_T(
                        true,
                        token -> data().empty(),
                        InvalidDataFormatException(),
                        BL_MSG()
                            << "The authentication token data is invalid or unavailable"
                        );
                }

                if( principalIdentityInfo -> securityPrincipal() )
                {
                    const auto& principal = principalIdentityInfo -> securityPrincipal();

                    BL_CHK_T(
                        true,
                        principal -> sid().empty(),
                        InvalidDataFormatException(),
                        BL_MSG()
                            << "The security principal information specified is invalid as one of the required fields is empty"
                        );
                }
            }

            static void updateBrokerProtocolMessageInBlock(
                SAA_in              const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in              const om::ObjPtr< DataBlock >&                  data,
                SAA_in              const uuid_t&                                   sourcePeerId,
                SAA_in              const uuid_t&                                   targetPeerId,
                SAA_in_opt          const bool                                      skipUpdateIfUnchanged = false
                )
            {
                bool messageChanged = false;

                if( brokerProtocol -> sourcePeerId().empty() )
                {
                    brokerProtocol -> sourcePeerId( uuids::uuid2string( sourcePeerId ) );
                    messageChanged = true;
                }

                if( brokerProtocol -> targetPeerId().empty() )
                {
                    brokerProtocol -> targetPeerId( uuids::uuid2string( targetPeerId ) );
                    messageChanged = true;
                }

                if( ! messageChanged && skipUpdateIfUnchanged )
                {
                    return;
                }

                const auto jsonString = dm::DataModelUtils::getDocAsPackedJsonString( brokerProtocol );

                const auto protocolDataOffset = data -> offset1();

                if( protocolDataOffset + jsonString.size() > data -> capacity() )
                {
                    BL_THROW_SERVER_ERROR(
                        BrokerErrorCodes::ProtocolValidationFailed,
                        BL_MSG()
                            << "DataBlock capacity is too small. capacity "
                            <<  data -> capacity()
                            << ", size "
                            <<  data -> size()
                            << ", old protocol data size "
                            <<  data -> size() - protocolDataOffset
                            << ", new protocol data size "
                            <<  jsonString.size()
                        );
                }

                std::memcpy(
                    data -> begin() + protocolDataOffset,
                    jsonString.data(),
                    jsonString.size()
                    );

                data -> setSize( protocolDataOffset + jsonString.size() );
            }

            static void verifyPayloadMessage(
                SAA_in              const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt          const om::ObjPtr< Payload >&                    payload
                )
            {
                const auto messageType = MessageType::toEnum( brokerProtocol -> messageType() );

                if( messageType == MessageType::AsyncRpcDispatch )
                {
                    const auto asyncRpcPayload =
                        payload ?
                            dm::DataModelUtils::castTo< dm::messaging::AsyncRpcPayload >( payload )
                            :
                            nullptr;

                    BL_CHK_T(
                        false,
                        asyncRpcPayload &&
                            ( asyncRpcPayload -> asyncRpcRequest() || asyncRpcPayload -> asyncRpcResponse() ),
                        InvalidDataFormatException(),
                        BL_MSG()
                            << "Payload message has to contain either a request or a response"
                        );
                }
            }

            template
            <
                typename T
            >
            static bool isRequestPayload( SAA_in const om::ObjPtr< T >& payload )
            {
                return payload && payload -> asyncRpcRequest();
            }

            template
            <
                typename T
            >
            static bool isResponsePayload( SAA_in const om::ObjPtr< T >& payload )
            {
                return payload && payload -> asyncRpcResponse();
            }

            template
            <
                typename T
            >
            static bool isNotificationPayload( SAA_in const om::ObjPtr< T >& payload )
            {
                return payload && payload -> notificationData();
            }

            static auto createBrokerProtocolMessage(
                SAA_in                  const MessageType::Enum                         messageType,
                SAA_in                  const uuid_t&                                   conversationId,
                SAA_in_opt              const std::string&                              tokenType,
                SAA_in_opt              const std::string&                              tokenData,
                SAA_in_opt              const uuid_t&                                   messageId = uuids::create()
                )
                -> om::ObjPtr< BrokerProtocol >
            {
                auto brokerProtocol = BrokerProtocol::createInstance();
                brokerProtocol -> messageType( MessageType::toString( messageType ) );
                brokerProtocol -> conversationId( uuids::uuid2string( conversationId ) );
                brokerProtocol -> messageId( uuids::uuid2string( messageId ) );

                if( ! tokenType.empty() && ! tokenData.empty() )
                {
                    const auto principalIdentityInfo = PrincipalIdentityInfo::createInstance();
                    brokerProtocol -> principalIdentityInfo( principalIdentityInfo );

                    const auto authenticationToken = AuthenticationToken::createInstance();
                    authenticationToken -> type( tokenType );
                    authenticationToken -> data( tokenData );
                    principalIdentityInfo -> authenticationToken( authenticationToken );
                }

                return brokerProtocol;
            }

            static auto createAcknowledgmentMessage(
                SAA_in                  const uuid_t&                                   conversationId,
                SAA_in_opt              const uuid_t&                                   messageId = uuids::create()
                )
                -> om::ObjPtr< BrokerProtocol >
            {
                return createBrokerProtocolMessage(
                    MessageType::AsyncRpcAcknowledgment,
                    conversationId,
                    str::empty() /* tokenType */,
                    str::empty() /* tokenData */,
                    messageId
                    );
            }

            static auto createResponseProtocolMessage(
                SAA_in                  const uuid_t&                                   conversationId,
                SAA_in_opt              const uuid_t&                                   messageId = uuids::create()
                )
                -> om::ObjPtr< BrokerProtocol >
            {
                return createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    str::empty() /* tokenType */,
                    str::empty() /* tokenData */,
                    messageId
                    );
            }

            static auto createNotificationProtocolMessage(
                SAA_in                  const uuid_t&                                   conversationId,
                SAA_in_opt              const std::string&                              tokenType,
                SAA_in_opt              const std::string&                              tokenData
                )
                -> om::ObjPtr< BrokerProtocol >
            {
                return createBrokerProtocolMessage(
                    MessageType::AsyncNotification,
                    conversationId,
                    tokenType,
                    tokenData
                    );
            }

            static auto createAssociateProtocolMessage(
                SAA_in                  const uuid_t&                                   physicalTargetPeerId,
                SAA_in                  const uuid_t&                                   logicalPeerId
                )
                -> om::ObjPtr< BrokerProtocol >
            {
                /*
                 * Create a BackendAssociateTargetPeerId message to be sent it to real backend
                 * - to register the (logical) peer id that is currently connecting with our
                 * own (physical) peer id
                 */

                auto message =  createBrokerProtocolMessage(
                    MessageType::BackendAssociateTargetPeerId,
                    uuids::create()     /* conversationId */,
                    str::empty()        /* tokenType */,
                    str::empty()        /* tokenData */
                    );

                message -> sourcePeerId( uuids::uuid2string( physicalTargetPeerId ) );
                message -> targetPeerId( uuids::uuid2string( logicalPeerId ) );

                return message;
            }

            static auto serializeObjectsToBlock(
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool = nullptr,
                SAA_in_opt              const std::size_t                               capacity = defaultCapacity()
                )
                -> om::ObjPtr< DataBlock >
            {
                auto dataBlock = DataBlock::get( dataBlocksPool, capacity );

                const auto protocolDataString =
                    dm::DataModelUtils::getDocAsPackedJsonString( brokerProtocol );

                const auto payloadDataString =
                    payload ? dm::DataModelUtils::getDocAsPackedJsonString( payload ) : std::string();

                if( payloadDataString.size() )
                {
                    dataBlock -> write( payloadDataString.c_str(), payloadDataString.size() );
                    dataBlock -> setOffset1( payloadDataString.size() );
                }

                dataBlock -> write( protocolDataString.c_str(), protocolDataString.size() );

                return dataBlock;
            }

            static auto deserializeBlockToObjects(
                SAA_in                  const om::ObjPtr< DataBlock >&                  dataBlock,
                SAA_in                  const bool                                      brokerProtocolOnly = false
                )
                -> std::pair< om::ObjPtr< BrokerProtocol >, om::ObjPtr< Payload > /* optional */ >
            {
                const std::string protocolDataString(
                    dataBlock -> begin() + dataBlock -> offset1(),
                    dataBlock -> begin() + dataBlock -> size()
                    );

                auto brokerProtocol =
                    dm::DataModelUtils::loadFromJsonText< BrokerProtocol >( protocolDataString );

                verifyBrokerProtocolMessage( brokerProtocol );

                om::ObjPtr< Payload > payload;

                if( dataBlock -> offset1() && ! brokerProtocolOnly )
                {
                    const std::string payloadDataString(
                        dataBlock -> begin(),
                        dataBlock -> begin() + dataBlock -> offset1()
                        );

                    payload = dm::DataModelUtils::loadFromJsonText< Payload >( payloadDataString );

                    verifyPayloadMessage( brokerProtocol, payload );
                }

                return std::make_pair( std::move( brokerProtocol ), std::move( payload ) );
            }

            /**
             * @brief Create as many endpoints as requested, but align it on endpoints.size() and
             * also ensure that endpoints.size() is the minimum # of endpoints
             */

            static auto expandEndpoints(
                SAA_in          const std::size_t                                       noOfRequestedEndpoints,
                SAA_in          std::vector< std::string >&&                            endpoints
                )
                -> std::vector< std::string >
            {
                BL_CHK(
                    true,
                    endpoints.empty(),
                    "Empty endpoints array provided to expandEndpoints"
                    );

                const auto factor = noOfRequestedEndpoints / endpoints.size();

                const auto modulo = noOfRequestedEndpoints % endpoints.size();

                const auto factorIncrement = ( 0U == factor || 0U != modulo ) ? 1U : 0U;

                const auto noOfExpandedEndpoints = endpoints.size() * ( factorIncrement + factor );

                std::vector< std::string > expandedEndpoints;

                std::size_t pos = 0U;

                for( std::size_t i = 0U; i < noOfExpandedEndpoints; ++i )
                {
                    expandedEndpoints.emplace_back( endpoints[ pos ] );

                    pos = ( pos + 1U ) % endpoints.size();
                }

                return expandedEndpoints;
            }
        };

        typedef MessagingUtilsT<> MessagingUtils;

        /**
         * @brief Implements core messaging dispatch callback interface adaptor for objects from blocks
         */

        template
        <
            typename E = void
        >
        class MessagingClientObjectDispatchFromBlockT : public MessagingClientObjectDispatch
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE(
                MessagingClientObjectDispatchFromBlockT,
                MessagingClientObjectDispatch
                )

        public:

            typedef MessagingClientBlockDispatch                                        block_dispatch_t;

            typedef data::datablocks_pool_type                                          datablocks_pool_type;
            typedef data::DataBlock                                                     DataBlock;

        protected:

            const om::ObjPtr< block_dispatch_t >                                        m_target;
            const om::ObjPtr< datablocks_pool_type >                                    m_dataBlocksPool;

            MessagingClientObjectDispatchFromBlockT(
                SAA_in                  om::ObjPtr< block_dispatch_t >&&                target,
                SAA_in_opt              om::ObjPtr< datablocks_pool_type >&&            dataBlocksPool = nullptr
                )
                :
                m_target( BL_PARAM_FWD( target ) ),
                m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) )
            {
            }

        public:

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                m_target -> dispose();

                BL_NOEXCEPT_END()
            }

            virtual void pushMessage(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) OVERRIDE
            {
                const auto dataBlock =
                    MessagingUtils::serializeObjectsToBlock( brokerProtocol, payload, m_dataBlocksPool );

                m_target -> pushBlock( targetPeerId, dataBlock, BL_PARAM_FWD( completionCallback ) );
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return m_target -> isConnected();
            }
        };

        typedef om::ObjectImpl< MessagingClientObjectDispatchFromBlockT<> > MessagingClientObjectDispatchFromBlock;

        /**
         * @brief Advanced dispatch interface implementation using vector of MessagingClient[*]Dispatch objects
         *
         * On each pushMessage() the code will rotate through connected dispatch objects starting from
         * a randomly selected one. The result will be client-side load balancing and improved failover
         */

        template
        <
            typename DISPATCH,
            typename CLIENT
        >
        class RotatingMessagingClientDispatchBaseT : public DISPATCH
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE( RotatingMessagingClientDispatchBaseT, DISPATCH )

        public:

            typedef std::vector< om::ObjPtr< DISPATCH > >                               DispatchList;
            typedef std::vector< om::ObjPtrDisposable< CLIENT > >                       ClientsList;

        protected:

            typedef cpp::function
            <
                const om::ObjPtr< DISPATCH >& ( SAA_in const CLIENT& client ) NOEXCEPT
            >
            getter_callback_t;

            typedef cpp::function< void ( SAA_in DISPATCH* target ) >                   invoke_callback_t;

            DispatchList                                                                m_targets;
            std::size_t                                                                 m_targetIndex;
            os::mutex                                                                   m_indexLock;

            RotatingMessagingClientDispatchBaseT(
                SAA_in                  const ClientsList&                              clients,
                SAA_in                  const getter_callback_t&                        getter
                )
            {
                for( const auto& client : clients )
                {
                    m_targets.emplace_back( om::copy( getter( *client.get() ) ) );
                }

                initTargetIndex();
            }

            RotatingMessagingClientDispatchBaseT( SAA_in DispatchList&& targets )
                :
                m_targets( BL_PARAM_FWD( targets ) )
            {
                initTargetIndex();
            }

            void initTargetIndex()
            {
                BL_ASSERT( ! m_targets.empty() );

                m_targetIndex = random::getUniformRandomUnsignedValue< std::size_t >( m_targets.size() - 1 );
            }

            auto invokeImpl( SAA_in_opt const invoke_callback_t& callback ) -> om::ObjPtr< DISPATCH >
            {
                /*
                 * If the currently selected target isn't connected try the rest
                 */

                DISPATCH* result = nullptr;

                const auto size = m_targets.size();

                {
                    BL_MUTEX_GUARD( m_indexLock );

                    for( std::size_t n = 0; n < size; ++n )
                    {
                        auto* target = m_targets[ m_targetIndex ].get();

                        m_targetIndex = ( m_targetIndex + 1 ) % size;

                        if( target -> isConnected() )
                        {
                            result = target;

                            break;
                        }
                    }
                }

                if( ! result )
                {
                    BL_THROW_USER_FRIENDLY(
                        NotSupportedException()
                            << eh::errinfo_error_uuid( uuiddefs::ErrorUuidNotConnectedToBroker() ),
                        "Messaging client is not connected to messaging broker"
                        );
                }

                if( callback )
                {
                    callback( result );

                    return nullptr;
                }

                return om::copy( result );
            }

        public:

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                for( const auto& target : m_targets )
                {
                    target -> dispose();
                }

                m_targets.clear();

                BL_NOEXCEPT_END()
            }

            bool isConnected() const NOEXCEPT OVERRIDE
            {
                for( const auto& target : m_targets )
                {
                    if( target -> isConnected() )
                    {
                        return true;
                    }
                }

                return false;
            }
        };

        template
        <
            typename E = void
        >
        class RotatingMessagingClientObjectDispatchT :
            public RotatingMessagingClientDispatchBaseT
            <
                MessagingClientObjectDispatch,
                MessagingClientObject
            >
        {
            BL_DECLARE_OBJECT_IMPL( RotatingMessagingClientObjectDispatchT )

        public:

            typedef RotatingMessagingClientDispatchBaseT
            <
                MessagingClientObjectDispatch,
                MessagingClientObject
            >
            base_type;

            typedef base_type::DispatchList                                             DispatchList;
            typedef base_type::ClientsList                                              ClientsList;

        protected:

            typedef base_type::invoke_callback_t                                        invoke_callback_t;

            RotatingMessagingClientObjectDispatchT( SAA_in const ClientsList& clients )
                :
                base_type( clients, cpp::mem_fn( &MessagingClientObject::outgoingObjectChannel ) )
            {
            }

            RotatingMessagingClientObjectDispatchT( SAA_in DispatchList&& targets )
                :
                base_type( BL_PARAM_FWD( targets ) )
            {
            }

        public:

            auto getNextDispatch() -> om::ObjPtr< MessagingClientObjectDispatch >
            {
                return base_type::invokeImpl( invoke_callback_t() );
            }

            virtual void pushMessage(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) OVERRIDE
            {
                BL_VERIFY(
                    nullptr == base_type::invokeImpl(
                        cpp::bind(
                            &MessagingClientObjectDispatch::pushMessageCopyCallback,
                            _1,
                            cpp::cref( targetPeerId ),
                            cpp::cref( brokerProtocol ),
                            cpp::cref( payload ),
                            cpp::cref( completionCallback )
                            )
                        )
                    );
            }
        };

        typedef om::ObjectImpl< RotatingMessagingClientObjectDispatchT<> > RotatingMessagingClientObjectDispatch;

        template
        <
            typename E = void
        >
        class RotatingMessagingClientBlockDispatchT :
            public RotatingMessagingClientDispatchBaseT
            <
                MessagingClientBlockDispatch,
                MessagingClientBlock
            >
        {
            BL_DECLARE_OBJECT_IMPL( RotatingMessagingClientBlockDispatchT )

        public:

            typedef RotatingMessagingClientDispatchBaseT
            <
                MessagingClientBlockDispatch,
                MessagingClientBlock
            >
            base_type;

            typedef base_type::DispatchList                                             DispatchList;
            typedef base_type::ClientsList                                              ClientsList;

        protected:

            typedef base_type::invoke_callback_t                                        invoke_callback_t;

            RotatingMessagingClientBlockDispatchT( SAA_in const ClientsList& clients )
                :
                base_type( clients, cpp::mem_fn( &MessagingClientBlock::outgoingBlockChannel ) )
            {
            }

            RotatingMessagingClientBlockDispatchT( SAA_in DispatchList&& targets )
                :
                base_type( BL_PARAM_FWD( targets ) )
            {
            }

        public:

            auto getNextDispatch() -> om::ObjPtr< MessagingClientBlockDispatch >
            {
                return base_type::invokeImpl( invoke_callback_t() );
            }

            virtual void pushBlock(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&            dataBlock,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) OVERRIDE
            {
                BL_VERIFY(
                    nullptr == base_type::invokeImpl(
                        cpp::bind(
                            &MessagingClientBlockDispatch::pushBlockCopyCallback,
                            _1,
                            cpp::cref( targetPeerId ),
                            cpp::cref( dataBlock ),
                            cpp::cref( completionCallback )
                            )
                        )
                    );
            }

            virtual uuid_t channelId() const NOEXCEPT OVERRIDE
            {
                return uuids::nil();
            }

            virtual bool isNoCopyDataBlocks() const NOEXCEPT OVERRIDE
            {
                BL_RIP_MSG( "MessagingClientBlockDispatchFromCallbackT:: Unsupported operation: isNoCopyDataBlocks" );

                return false;
            }

            virtual void isNoCopyDataBlocks( SAA_in bool isNoCopyDataBlocks ) NOEXCEPT OVERRIDE
            {
                BL_UNUSED( isNoCopyDataBlocks );

                BL_RIP_MSG( "MessagingClientBlockDispatchFromCallbackT:: Unsupported operation: isNoCopyDataBlocks" );
            }
        };

        typedef om::ObjectImpl< RotatingMessagingClientBlockDispatchT<> > RotatingMessagingClientBlockDispatch;

        /**
         * @brief Implements core messaging dispatch callback interface adaptor for blocks from objects
         */

        template
        <
            typename E = void
        >
        class MessagingClientBlockDispatchFromObjectT : public MessagingClientBlockDispatch
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE(
                MessagingClientBlockDispatchFromObjectT,
                MessagingClientBlockDispatch
                )

        public:

            typedef data::DataBlock                                                     DataBlock;
            typedef MessagingClientObjectDispatch                                       object_dispatch_t;

        protected:

            const om::ObjPtr< object_dispatch_t >                                   m_target;

            MessagingClientBlockDispatchFromObjectT( SAA_in om::ObjPtr< object_dispatch_t >&& target )
                :
                m_target( BL_PARAM_FWD( target ) )
            {
            }

        public:

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                m_target -> dispose();

                BL_NOEXCEPT_END()
            }

            virtual void pushBlock(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  dataBlock,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) OVERRIDE
            {
                const auto pair = MessagingUtils::deserializeBlockToObjects( dataBlock );

                m_target -> pushMessage(
                    targetPeerId,
                    pair.first          /* brokerProtocol */,
                    pair.second         /* payload */,
                    BL_PARAM_FWD( completionCallback )
                    );
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return m_target -> isConnected();
            }

            virtual uuid_t channelId() const NOEXCEPT OVERRIDE
            {
                return uuids::nil();
            }

            virtual bool isNoCopyDataBlocks() const NOEXCEPT OVERRIDE
            {
                return true;
            }

            virtual void isNoCopyDataBlocks( SAA_in bool isNoCopyDataBlocks ) NOEXCEPT OVERRIDE
            {
                BL_UNUSED( isNoCopyDataBlocks );

                BL_RIP_MSG( "MessagingClientBlockDispatchFromCallbackT:: Unsupported operation: isNoCopyDataBlocks" );
            }
        };

        typedef om::ObjectImpl< MessagingClientBlockDispatchFromObjectT<> > MessagingClientBlockDispatchFromObject;

        /**
         * @brief Implementation of MessagingClientObject based on outgoing and incoming block channels
         */

        template
        <
            typename E = void
        >
        class MessagingClientObjectImplDefaultT : public MessagingClientObject
        {
            BL_DECLARE_OBJECT_IMPL( MessagingClientObjectImplDefaultT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( MessagingClientObject )
                BL_QITBL_ENTRY( MessagingClientBlock )
                BL_QITBL_ENTRY( om::Disposable )
            BL_QITBL_END( MessagingClientObject )

        public:

            typedef MessagingClientBlockDispatch                                        block_dispatch_t;

            typedef MessagingClientObjectDispatchFromBlock                              object_adaptor_t;
            typedef data::datablocks_pool_type                                          datablocks_pool_type;

        protected:

            const om::ObjPtr< datablocks_pool_type >                                    m_dataBlocksPool;
            const om::ObjPtr< MessagingClientBlockDispatch >                            m_outgoingBlockChannel;
            const om::ObjPtr< MessagingClientObjectDispatch >                           m_outgoingObjectChannel;

            MessagingClientObjectImplDefaultT(
                SAA_in                  const om::ObjPtr< block_dispatch_t >&           outgoingBlockChannel,
                SAA_in_opt              const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool = nullptr
                )
                :
                m_dataBlocksPool( om::copy( dataBlocksPool ) ),
                m_outgoingBlockChannel( om::copy( outgoingBlockChannel ) ),
                m_outgoingObjectChannel(
                    object_adaptor_t::createInstance< MessagingClientObjectDispatch >(
                        om::copy( outgoingBlockChannel ),
                        om::copy( dataBlocksPool )
                        )
                    )
            {
            }

        public:

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                m_outgoingBlockChannel -> dispose();

                BL_NOEXCEPT_END()
            }

            virtual auto outgoingBlockChannel() const NOEXCEPT -> const om::ObjPtr< MessagingClientBlockDispatch >& OVERRIDE
            {
                return m_outgoingBlockChannel;
            }

            virtual auto outgoingObjectChannel() const NOEXCEPT -> const om::ObjPtr< MessagingClientObjectDispatch >& OVERRIDE
            {
                return m_outgoingObjectChannel;
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return m_outgoingBlockChannel -> isConnected();
            }
        };

        typedef om::ObjectImpl< MessagingClientObjectImplDefaultT<> > MessagingClientObjectImplDefault;

        /**
         * @brief The messaging client object factory helper
         */

        class MessagingClientObjectFactory
        {
            BL_DECLARE_STATIC( MessagingClientObjectFactory )

        public:

            typedef MessagingClientBlockDispatch                                        block_dispatch_t;
            typedef MessagingClientObjectDispatch                                       object_dispatch_t;

            typedef data::datablocks_pool_type                                          datablocks_pool_type;

            typedef MessagingClientFactorySsl::connection_establisher_t                 connection_establisher_t;
            typedef MessagingClientFactorySsl::async_wrapper_t                          async_wrapper_t;

        protected:

            template
            <
                typename BLOCKDISPATCHIMPL
            >
            static auto createFromBlockDispatchLocal(
                SAA_in                  const om::ObjPtr< block_dispatch_t >&           incomingBlockChannel,
                SAA_in_opt              const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool = nullptr
                )
                -> om::ObjPtr< MessagingClientObject >
            {
                const auto outgoingBlockChannel = BLOCKDISPATCHIMPL::template createInstance< block_dispatch_t >(
                    om::copy( incomingBlockChannel ),
                    om::copy( dataBlocksPool )
                    );

                return MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                    outgoingBlockChannel,
                    dataBlocksPool
                    );
            }

        public:

            static auto createFromObjectDispatchTcp(
                SAA_in                  const om::ObjPtr< object_dispatch_t >&          incomingObjectChannel,
                SAA_in                  const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool,
                SAA_in                  const uuid_t&                                   peerId,
                SAA_in                  const std::string&                              host,
                SAA_in                  const unsigned short                            inboundPort,
                SAA_in_opt              const unsigned short                            outboundPort = 0U,
                SAA_in_opt              om::ObjPtr< connection_establisher_t >&&        inboundConnection = nullptr,
                SAA_in_opt              om::ObjPtr< connection_establisher_t >&&        outboundConnection = nullptr
                )
                -> om::ObjPtr< MessagingClientObject >
            {
                auto incomingBlockChannel =
                    MessagingClientBlockDispatchFromObject::createInstance< block_dispatch_t >(
                        om::copy( incomingObjectChannel )           /* target */
                        );

                auto clientImpl = om::lockDisposable(
                    MessagingClientFactorySsl::createWithSmartDefaults(
                        peerId,
                        std::move( incomingBlockChannel ),
                        host,
                        inboundPort,
                        outboundPort,
                        BL_PARAM_FWD( inboundConnection ),
                        BL_PARAM_FWD( outboundConnection ),
                        0U                                          /* threadsCount */,
                        0U                                          /* maxConcurrentTasks */,
                        om::copy( dataBlocksPool )
                        )
                    );

                auto result = MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                    om::qi< block_dispatch_t >( clientImpl ),
                    dataBlocksPool
                    );

                clientImpl.detachAsObjPtr();

                return result;
            }

            template
            <
                typename CLIENT
            >
            static auto createFromBackendTcp(
                SAA_in                  om::ObjPtr< BackendProcessing >&&               backend,
                SAA_in                  om::ObjPtr< async_wrapper_t >&&                 asyncWrapper,
                SAA_in                  const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool,
                SAA_in                  const uuid_t&                                   peerId,
                SAA_in                  const std::string&                              host,
                SAA_in                  const unsigned short                            inboundPort,
                SAA_in_opt              const unsigned short                            outboundPort = 0U,
                SAA_in_opt              om::ObjPtr< connection_establisher_t >&&        inboundConnection = nullptr,
                SAA_in_opt              om::ObjPtr< connection_establisher_t >&&        outboundConnection = nullptr
                )
                -> om::ObjPtr< CLIENT >
            {
                auto clientImpl = om::lockDisposable(
                    MessagingClientFactorySsl::createWithSmartDefaults(
                        nullptr /* eq */,
                        peerId,
                        BL_PARAM_FWD( backend ),
                        BL_PARAM_FWD( asyncWrapper ),
                        host,
                        inboundPort,
                        outboundPort,
                        BL_PARAM_FWD( inboundConnection ),
                        BL_PARAM_FWD( outboundConnection ),
                        om::copy( dataBlocksPool )
                        )
                    );

                auto result = MessagingClientObjectImplDefault::createInstance< CLIENT >(
                    om::qi< block_dispatch_t >( clientImpl ),
                    dataBlocksPool
                    );

                clientImpl.detachAsObjPtr();

                return result;
            }

            template
            <
                typename BLOCKDISPATCHIMPL
            >
            static auto createFromObjectDispatchLocal(
                SAA_in                  const om::ObjPtr< object_dispatch_t >&      incomingObjectChannel,
                SAA_in_opt              const om::ObjPtr< datablocks_pool_type >&   dataBlocksPool = nullptr
                )
                -> om::ObjPtr< MessagingClientObject >
            {
                return createFromBlockDispatchLocal< BLOCKDISPATCHIMPL >(
                    MessagingClientBlockDispatchFromObject::createInstance< block_dispatch_t >(
                        om::copy( incomingObjectChannel )
                        ),
                    dataBlocksPool
                    );
            }

            static auto createFromEndpoints(
                SAA_in      const os::port_t                                            defaultPort,
                SAA_in      std::vector< std::string >&&                                endpoints,
                SAA_in      const om::ObjPtr< BackendProcessing >&                      backend,
                SAA_in      const om::ObjPtr< async_wrapper_t >&                        asyncWrapper,
                SAA_in      const om::ObjPtr< datablocks_pool_type >&                   dataBlocksPool,
                SAA_in      const uuid_t&                                               peerId
                )
                -> std::vector< om::ObjPtrDisposable< MessagingClientObject > >
            {
                std::vector< om::ObjPtrDisposable< MessagingClientObject > > clientObjects;

                for( auto& endpoint : endpoints )
                {
                    /*
                     * Handle both endpoint string formats: 'host' or 'host:port'
                     */

                    std::string host;
                    os::port_t port;

                    if( ! net::tryParseEndpoint( endpoint, host, port ) )
                    {
                        host = std::move( endpoint );
                        port = defaultPort;
                    }

                    clientObjects.emplace_back(
                        om::lockDisposable(
                            createFromBackendTcp< MessagingClientObject >(
                                om::copy( backend ),
                                om::copy( asyncWrapper ),
                                dataBlocksPool,
                                peerId,
                                host,
                                port
                                )
                            )
                        );
                }

                return clientObjects;
            }
        };

    } // messaging

} // bl

namespace bl
{
    namespace om
    {
        inline std::ostream& operator<<(
            SAA_inout       std::ostream&                                               oss,
            SAA_in          const om::ObjPtr< messaging::BrokerProtocol >&      brokerProtocol
            )
        {
            oss
                << "Broker protocol: ";

            if( brokerProtocol )
            {
                om::ObjPtr< messaging::PrincipalIdentityInfo > savePrincipal;

                BL_SCOPE_EXIT(
                    if( savePrincipal )
                    {
                        brokerProtocol -> principalIdentityInfoLvalue().swap( savePrincipal );
                    }
                    );

                if( brokerProtocol -> principalIdentityInfo() )
                {
                    brokerProtocol -> principalIdentityInfoLvalue().swap( savePrincipal );
                }

                oss << dm::DataModelUtils::getDocAsPrettyJsonString( brokerProtocol );
            }
            else
            {
                oss << "{}";
            }

            return oss;
        }

        /*
         * We will define a placeholder operators for bl::dm::Payload and bl::messaging::AsyncRpcPayload
         * which will simply dump if the object is null vs. non-null as we don't really know what data
         * is being carried there and we can't be sure if it is safe to dump in the logs from security
         * perspective
         */

        inline std::ostream& operator<<(
            SAA_inout       std::ostream&                                               oss,
            SAA_in          const om::ObjPtr< bl::dm::Payload >&                        payload
            )
        {
            oss
                << "Payload: "
                << ( payload ? "{<non null generic payload>}" : "{}" );

            return oss;
        }

        inline std::ostream& operator<<(
            SAA_inout       std::ostream&                                               oss,
            SAA_in          const om::ObjPtr< bl::messaging::AsyncRpcPayload >&         payload
            )
        {
            oss
                << "Payload: "
                << ( payload ? "{<non null generic async RPC payload>}" : "{}" );

            return oss;
        }

    } // om

} // bl

#endif /* __BL_MESSAGING_MESSAGINGUTILS_H_ */
