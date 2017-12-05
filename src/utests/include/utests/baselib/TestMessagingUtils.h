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

#ifndef __UTEST_TESTMESSAGINGUTILS_H_
#define __UTEST_TESTMESSAGINGUTILS_H_

#include <utests/baselib/DataModelTestUtils.h>
#include <utests/baselib/LoggerUtils.h>
#include <utests/baselib/TestUtils.h>
#include <utests/baselib/TestTaskUtils.h>
#include <utests/baselib/MachineGlobalTestLock.h>

#include <baselib/examples/echoserver/EchoServerProcessingContext.h>

#include <baselib/messaging/ProxyBrokerBackendProcessingFactory.h>
#include <baselib/messaging/ForwardingBackendProcessingImpl.h>
#include <baselib/messaging/ForwardingBackendProcessingFactory.h>
#include <baselib/messaging/ForwardingBackendSharedState.h>
#include <baselib/messaging/asyncrpc/ConversationProcessingBaseImpl.h>
#include <baselib/messaging/asyncrpc/ConversationProcessingTask.h>
#include <baselib/messaging/MessagingUtils.h>
#include <baselib/messaging/BrokerBackendProcessing.h>
#include <baselib/messaging/MessagingClientObjectDispatch.h>
#include <baselib/messaging/MessagingClientObject.h>
#include <baselib/messaging/BrokerFacade.h>
#include <baselib/messaging/MessagingClientFactory.h>
#include <baselib/messaging/MessagingClientBlockDispatchLocal.h>
#include <baselib/messaging/TcpBlockTransferServer.h>
#include <baselib/messaging/BrokerErrorCodes.h>

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/TasksUtils.h>

#include <baselib/data/models/Http.h>

#include <baselib/security/AuthorizationCacheImpl.h>
#include <baselib/security/AuthorizationServiceRest.h>
#include <baselib/security/SecurityInterfaces.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>
#include <baselib/core/TimeUtils.h>

namespace utest
{
    /**
     * class TestHostServicesLoggingContext - a logging context implementation
     */

    template
    <
        typename E = void
    >
    class TestHostServicesLoggingContextT : public bl::messaging::AsyncBlockDispatcher
    {
        BL_CTR_DEFAULT( TestHostServicesLoggingContextT, protected )
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( TestHostServicesLoggingContextT, bl::messaging::AsyncBlockDispatcher )

    protected:

        typedef TestHostServicesLoggingContextT< E >                                        this_type;

        bl::cpp::ScalarTypeIniter< bool >                                                   m_messageLogged;

        void logMessageDetails(
            SAA_in                  const std::string&                                      message,
            SAA_in                  const bl::uuid_t&                                       targetPeerId,
            SAA_in                  const bl::om::ObjPtrCopyable< bl::data::DataBlock >&    dataBlock
            )
        {
            using namespace bl;
            using namespace bl::messaging;

            const auto pair = MessagingUtils::deserializeBlockToObjects( dataBlock );

            BL_LOG_MULTILINE(
                bl::Logging::debug(),
                BL_MSG()
                    << "\n**********************************************\n\n"
                    << message
                    << "\n\nTarget peer id: "
                    << uuids::uuid2string( targetPeerId )
                    << "\n\nBroker protocol message:\n"
                    << bl::dm::DataModelUtils::getDocAsPrettyJsonString( pair.first /* brokerProtocol */ )
                    << "\nPayload message:\n"
                    << bl::dm::DataModelUtils::getDocAsPrettyJsonString( pair.second /* payload */ )
                    << "\n\n"
                );

            m_messageLogged = true;
        }

        auto createDispatchTaskInternal(
            SAA_in                  const std::string&                                      message,
            SAA_in                  const bl::uuid_t&                                       targetPeerId,
            SAA_in                  const bl::om::ObjPtr< bl::data::DataBlock >&            data
            )
            -> bl::om::ObjPtr< bl::tasks::SimpleTaskImpl >
        {
            return bl::tasks::SimpleTaskImpl::createInstance(
                bl::cpp::bind(
                    &this_type::logMessageDetails,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    message,
                    targetPeerId,
                    bl::om::ObjPtrCopyable< bl::data::DataBlock >( data )
                    )
                );
        }

    public:

        bool messageLogged() const NOEXCEPT
        {
            return m_messageLogged;
        }

        virtual auto getAllActiveQueuesIds() -> std::unordered_set< bl::uuid_t > OVERRIDE
        {
            return std::unordered_set< bl::uuid_t >();
        }

        virtual auto tryGetMessageBlockCompletionQueue( SAA_in const bl::uuid_t& targetPeerId )
            -> bl::om::ObjPtr< bl::messaging::MessageBlockCompletionQueue > OVERRIDE
        {
            BL_UNUSED( targetPeerId );

            return nullptr;
        }

        virtual auto createDispatchTask(
            SAA_in                  const bl::uuid_t&                                   targetPeerId,
            SAA_in                  const bl::om::ObjPtr< bl::data::DataBlock >&        data
            )
            -> bl::om::ObjPtr< bl::tasks::Task > OVERRIDE
        {
            return bl::om::moveAs< bl::tasks::Task >(
                createDispatchTaskInternal( "Logging context received message", targetPeerId, data )
                );
        }
    };

    typedef bl::om::ObjectImpl< TestHostServicesLoggingContextT<> > TestHostServicesLoggingContext;
    typedef TestHostServicesLoggingContext logging_context_t;

    /*
     * This class mocks AuthorizationCache in order to remove dependency on real service
     * for some unit tests, which in turn makes them also more robust.
     */

    template
    <
        typename E = void
    >
    class DummyAuthorizationCacheT : public bl::security::AuthorizationCache
    {
        BL_CTR_DEFAULT( DummyAuthorizationCacheT, protected )

        BL_DECLARE_OBJECT_IMPL_ONEIFACE( DummyAuthorizationCacheT, AuthorizationCache )

    protected:

        static const std::string                                                    g_dummyTokenType;
        static const std::string                                                    g_dummyTokenData;
        static const std::string                                                    g_dummyTokenDataUnauthorized;
        static const std::string                                                    g_dummySid;
        static const std::string                                                    g_dummyCookieName;

    public:

        static auto dummyTokenType() NOEXCEPT -> const std::string&
        {
            return g_dummyTokenType;
        }

        static auto dummyTokenData() NOEXCEPT -> const std::string&
        {
            return g_dummyTokenData;
        }

        static auto dummyTokenDataUnauthorized() NOEXCEPT -> const std::string&
        {
            return g_dummyTokenDataUnauthorized;
        }

        static auto dummySid() NOEXCEPT -> const std::string&
        {
            return g_dummySid;
        }

        static auto dummyCookieName() NOEXCEPT -> const std::string&
        {
            return g_dummyCookieName;
        }

        static auto getTestSecurityPrincipal(
            SAA_in_opt          const bl::om::ObjPtr< bl::data::DataBlock >&        authenticationToken = nullptr
            )
            -> bl::om::ObjPtr< bl::security::SecurityPrincipal >
        {
            return bl::security::SecurityPrincipal::createInstance(
                bl::cpp::copy( dummySid() ),
                "John",
                "Smith",
                "john.smith@host.com",
                "principalType1",
                bl::om::copy( authenticationToken )
                );
        }

        virtual auto tokenType() const NOEXCEPT -> const std::string& OVERRIDE
        {
            return dummyTokenType();
        }

        virtual void configureFreshnessInterval(
            SAA_in_opt          const bl::time::time_duration&                      freshnessInterval = bl::time::neg_infin
            ) OVERRIDE
        {
            BL_UNUSED( freshnessInterval );

            BL_THROW(
                bl::NotSupportedException(),
                "DummyAuthorizationCache::configureFreshnessInterval() is unimplemented"
                );
        }

        virtual auto tryGetAuthorizedPrinciplal(
            SAA_in              const bl::om::ObjPtr< bl::data::DataBlock >&            authenticationToken
            )
            -> bl::om::ObjPtr< bl::security::SecurityPrincipal > OVERRIDE
        {
            std::string tokenData;

            tokenData.assign( authenticationToken -> begin(), authenticationToken -> end() );

            const auto properties = bl::str::parsePropertiesList( tokenData );

            const auto pos = properties.find( g_dummyCookieName );

            BL_CHK(
                false,
                pos != properties.end(),
                BL_MSG()
                    << "The provided authentication token is invalid"
                );

            if( "authorized" == pos -> second )
            {
                return getTestSecurityPrincipal( authenticationToken );
            }

            if( "unauthorized" == pos -> second )
            {
                BL_THROW(
                    bl::SecurityException()
                        << bl::eh::errinfo_error_code(
                            bl::eh::errc::make_error_code( bl::eh::errc::permission_denied )
                            ),
                    BL_MSG()
                        << "Authorization request has failed"
                    );
            }

            BL_THROW(
                bl::UnexpectedException(),
                BL_MSG()
                    << "Unexpected exception during authorization"
                );
        }

        virtual auto createAuthorizationTask(
            SAA_in              const bl::om::ObjPtr< bl::data::DataBlock >&            authenticationToken
            )
            -> bl::om::ObjPtr< bl::tasks::Task > OVERRIDE
        {
            BL_UNUSED( authenticationToken );

            BL_THROW(
                bl::NotSupportedException(),
                "DummyAuthorizationCache::createAuthorizationTask() is unimplemented"
                );
        }

        virtual auto tryUpdate(
            SAA_in              const bl::om::ObjPtr< bl::data::DataBlock >&            authenticationToken,
            SAA_in_opt          const bl::om::ObjPtr< bl::tasks::Task >&                authorizationTask = nullptr
            )
            -> bl::om::ObjPtr< bl::security::SecurityPrincipal > OVERRIDE
        {
            BL_UNUSED( authenticationToken );
            BL_UNUSED( authorizationTask );

            BL_THROW(
                bl::NotSupportedException(),
                "DummyAuthorizationCache::tryUpdate() is unimplemented"
                );
        }

        virtual auto update(
            SAA_in              const bl::om::ObjPtr< bl::data::DataBlock >&            authenticationToken,
            SAA_in_opt          const bl::om::ObjPtr< bl::tasks::Task >&                authorizationTask = nullptr
            )
            -> bl::om::ObjPtr< bl::security::SecurityPrincipal > OVERRIDE
        {
            BL_UNUSED( authenticationToken );
            BL_UNUSED( authorizationTask );

            BL_THROW(
                bl::NotSupportedException(),
                "DummyAuthorizationCache::update() is unimplemented"
                );
        }

        virtual void evict(
            SAA_in              const bl::om::ObjPtr< bl::data::DataBlock >&            authenticationToken
            ) OVERRIDE
        {
            BL_UNUSED( authenticationToken );

            BL_THROW(
                bl::NotSupportedException(),
                "DummyAuthorizationCache::evict() is unimplemented"
                );
        }
    };

    BL_DEFINE_STATIC_CONST_STRING( DummyAuthorizationCacheT, g_dummyTokenType ) = "DummyTokenType";
    BL_DEFINE_STATIC_CONST_STRING( DummyAuthorizationCacheT, g_dummyTokenData ) = "dummyCookieName=authorized";
    BL_DEFINE_STATIC_CONST_STRING( DummyAuthorizationCacheT, g_dummyTokenDataUnauthorized ) = "dummyCookieName=unauthorized";
    BL_DEFINE_STATIC_CONST_STRING( DummyAuthorizationCacheT, g_dummySid ) = "sid1234";
    BL_DEFINE_STATIC_CONST_STRING( DummyAuthorizationCacheT, g_dummyCookieName ) = "dummyCookieName";

    typedef bl::om::ObjectImpl< DummyAuthorizationCacheT<> > DummyAuthorizationCache;

    /**
     * @brief Helpers for aiding implementation of messaging tests
     */

    template
    <
        typename E = void
    >
    class TestMessagingUtilsT
    {
        BL_DECLARE_STATIC( TestMessagingUtilsT )

    protected:

        static std::string                                                                  g_tokenType;
        static bl::os::mutex                                                                g_tokenTypeLock;

        typedef bl::om::ObjectImpl
        <
            bl::security::AuthorizationCacheImpl< bl::security::AuthorizationServiceRest >
        >
        cache_t;

    public:

        typedef bl::data::datablocks_pool_type                                              datablocks_pool_t;
        typedef bl::tasks::ExecutionQueue                                                   ExecutionQueue;
        typedef bl::messaging::BackendProcessing                                            BackendProcessing;

        typedef bl::messaging::MessagingClientBlockDispatch                                 block_dispatch_t;
        typedef bl::messaging::MessagingClientObjectDispatch                                object_dispatch_t;

        typedef bl::messaging::MessagingClientFactorySsl                                    client_factory_t;
        typedef client_factory_t::messaging_client_t                                        client_t;

        typedef client_factory_t::connection_establisher_t                                  connector_t;
        typedef client_factory_t::async_wrapper_t                                           async_wrapper_t;

        typedef client_factory_t::sender_connection_t                                       sender_connection_t;
        typedef client_factory_t::receiver_connection_t                                     receiver_connection_t;

        typedef bl::tasks::TaskControlTokenRW                                               token_t;
        typedef bl::om::ObjPtrCopyable< token_t >                                           token_ptr_t;
        typedef bl::om::ObjPtrCopyable< datablocks_pool_t >                                 pool_ptr_t;

        typedef bl::messaging::BrokerProtocol                                               BrokerProtocol;
        typedef bl::messaging::Payload                                                      Payload;

        typedef std::vector
            <
                std::pair< bl::om::ObjPtr< connector_t >, bl::om::ObjPtr< connector_t > >
            >
            connections_list_t;

        typedef std::vector
            <
                std::pair< bl::uuid_t, bl::om::ObjPtrDisposable< bl::messaging::MessagingClientObject > >
            >
            clients_list_t;

        typedef bl::cpp::function
            <
                void (
                    SAA_in          const std::string&                                      cookiesText,
                    SAA_in          const bl::om::ObjPtr< datablocks_pool_t >&              dataBlocksPool,
                    SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
                    SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
                    SAA_in          const bl::om::ObjPtr< async_wrapper_t >&                asyncWrapper
                    )
            >
            callback_t;

        static auto createConnections( SAA_in const std::size_t maxConcurrentConnections = 16U ) -> connections_list_t
        {
            BL_CHK(
                false,
                test::UtfArgsParser::connections() >= 2U,
                BL_MSG()
                    << "The minimum value of the --connections parameter is 2"
                );

            return createNoOfConnections(
                test::UtfArgsParser::connections()      /* noOfConnections */,
                test::UtfArgsParser::host(),
                test::UtfArgsParser::port(),
                maxConcurrentConnections
                );
        }

        static auto createNoOfConnections(
            SAA_in          const std::size_t                                               noOfConnections,
            SAA_in_opt      const std::string&                                              host = test::UtfArgsParser::host(),
            SAA_in_opt      const unsigned short                                            port = test::UtfArgsParser::port(),
            SAA_in_opt      const std::size_t                                               maxConcurrentConnections = 16U
            )
            -> connections_list_t
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::messaging;

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "Establishing broker connections for "
                    << noOfConnections
                    << " test messaging clients"
                );

            const auto inboundQueue = lockDisposable(
                ExecutionQueueImpl::createInstance< ExecutionQueue >(
                    ExecutionQueue::OptionKeepAll,
                    maxConcurrentConnections
                    )
                );

            const auto outboundQueue = lockDisposable(
                ExecutionQueueImpl::createInstance< ExecutionQueue >(
                    ExecutionQueue::OptionKeepAll,
                    maxConcurrentConnections
                    )
                );

            for( std::size_t i = 0; i < noOfConnections; ++i )
            {
                inboundQueue -> push_back( connector_t::createInstance< Task >( bl::cpp::copy( host ), port ) );

                outboundQueue -> push_back( connector_t::createInstance< Task >( bl::cpp::copy( host ), port + 1 ) );
            }

            const auto startTime = time::second_clock::universal_time();

            for( ;; )
            {
                os::sleep( time::seconds( 2 ) );

                if(
                    ! inboundQueue -> hasPending()      &&
                    ! inboundQueue -> hasExecuting()    &&
                    ! outboundQueue -> hasPending()     &&
                    ! outboundQueue -> hasExecuting()
                    )
                {
                    break;
                }

                const auto elapsed = time::second_clock::universal_time() - startTime;

                /*
                 * Note: the checks below are to handle an apparently OS issue with connect
                 * operations hanging occasionally
                 */

                if( elapsed > time::minutes( 5 ) )
                {
                    inboundQueue -> cancelAll( false /* wait */ );
                    outboundQueue -> cancelAll( false /* wait */ );
                }

                if( elapsed > time::minutes( 15 ) )
                {
                    BL_RIP_MSG( "Connection tasks are hung more than 15 minutes" );
                }
            }

            UTF_REQUIRE_EQUAL( inboundQueue -> size(), noOfConnections );
            UTF_REQUIRE_EQUAL( inboundQueue -> size(), outboundQueue -> size() );

            connections_list_t result;
            result.reserve( noOfConnections );

            for( std::size_t i = 0; i < noOfConnections; ++i )
            {
                result.push_back(
                    std::make_pair(
                        om::qi< connector_t >( inboundQueue -> pop() )          /* inboundConnection */,
                        om::qi< connector_t >( outboundQueue -> pop() )         /* outboundConnection */
                        )
                    );
            }

            return result;
        }

        static auto getTokenData() -> const std::string&
        {
            return (
                ( test::UtfArgsParser::path().empty() || test::UtfArgsParser::password().empty() ) ?
                    DummyAuthorizationCache::dummyTokenData()
                    :
                    test::UtfArgsParser::password()
                );
        }

        static auto getTokenType() -> const std::string&
        {
            return (
                ( test::UtfArgsParser::path().empty() || test::UtfArgsParser::userId().empty() ) ?
                    DummyAuthorizationCache::dummyTokenType()
                    :
                    test::UtfArgsParser::userId()
                );
        }

        static auto createBrokerProtocolMessage(
            SAA_in                  const bl::messaging::MessageType::Enum          messageType,
            SAA_in                  const bl::uuid_t&                               conversationId,
            SAA_in_opt              const std::string&                              cookiesText,
            SAA_in_opt              const bl::uuid_t&                               messageId = bl::uuids::create(),
            SAA_in_opt              const std::string&                              tokenType = bl::str::empty()
            )
            -> bl::om::ObjPtr< bl::messaging::BrokerProtocol >
        {
            if( tokenType.empty() )
            {
                BL_MUTEX_GUARD( g_tokenTypeLock );

                if( g_tokenType.empty() )
                {
                    using namespace bl::security;

                    g_tokenType =
                        ( test::UtfArgsParser::path().empty() || test::UtfArgsParser::password().empty() ) ?
                            DummyAuthorizationCache::dummyTokenType()
                            :
                            cache_t::template createInstance< AuthorizationCache >(
                                AuthorizationServiceRest::create( test::UtfArgsParser::path() )
                                )
                                -> tokenType();
                }
            }

            return bl::messaging::MessagingUtils::createBrokerProtocolMessage(
                messageType,
                conversationId,
                tokenType.empty() ? g_tokenType : tokenType,
                cookiesText,
                messageId
                );
        }

        static void executeMessagingTests(
            SAA_in          const bl::om::ObjPtr< object_dispatch_t >&                      incomingObjectChannel,
            SAA_in          const callback_t&                                               callback
            )
        {
            using namespace bl;
            using namespace bl::messaging;

            tasks::scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( ExecutionQueue::OptionKeepNone );

                    const auto& cookiesText = getTokenData();

                    const auto dataBlocksPool = datablocks_pool_t::createInstance();

                    const auto incomingBlockChannel = om::lockDisposable(
                        MessagingClientBlockDispatchFromObject::createInstance< MessagingClientBlockDispatch >(
                            om::copy( incomingObjectChannel )
                            )
                        );

                    const auto backend = om::lockDisposable(
                        client_factory_t::createClientBackendProcessingFromBlockDispatch(
                            om::copy( incomingBlockChannel )
                            )
                        );

                    const auto asyncWrapper = om::lockDisposable(
                        client_factory_t::createAsyncWrapperFromBackend(
                            om::copy( backend ),
                            0U              /* threadsCount */,
                            0U              /* maxConcurrentTasks */,
                            om::copy( dataBlocksPool )
                            )
                        );

                    callback( cookiesText, dataBlocksPool, eq, backend, asyncWrapper );

                    eq -> flush();

                    UTF_REQUIRE( eq -> isEmpty() );
                }
                );
        }

        static void waitForKeyOrTimeout()
        {
            if( test::UtfArgsParser::timeoutInSeconds() == 0 )
            {
                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "Press [Enter] to stop messaging client tests"
                    );

                ::getchar();
            }
            else
            {
                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "Executed messaging client tests for "
                        << test::UtfArgsParser::timeoutInSeconds()
                        << " seconds"
                    );

                bl::os::sleep(
                    bl::time::seconds( static_cast< long >( test::UtfArgsParser::timeoutInSeconds() ) )
                    );
            }

            BL_LOG(
                bl::Logging::debug(),
                "Closing all messaging client connections"
                );
        }

        static void dispatchCallback(
            SAA_in              const bl::om::ObjPtrCopyable< bl::om::Proxy >&  clientSink,
            SAA_in_opt          const bl::uuid_t&                               targetPeerIdExpected,
            SAA_in              const bl::uuid_t&                               targetPeerId,
            SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
            SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
            )
        {
            if( targetPeerIdExpected != bl::uuids::nil() )
            {
                UTF_REQUIRE_EQUAL( targetPeerId, targetPeerIdExpected );
            }

            bl::os::mutex_unique_lock guard;

            {
                const auto target = clientSink -> tryAcquireRef< object_dispatch_t >(
                    object_dispatch_t::iid(),
                    &guard
                    );

                if( target )
                {
                    target -> pushMessage( targetPeerId, brokerProtocol, payload );
                }
            }
        }

        static void verifyUniformMessageDistribution( SAA_in const clients_list_t& clients )
        {
            using namespace bl;

            /*
             * Verify that all channels dispatched at least 2 messages or more
             * including the incoming channels
             */

            std::uint64_t receivedLower = std::numeric_limits< std::uint64_t >::max();
            std::uint64_t receivedUpper = std::numeric_limits< std::uint64_t >::min();

            std::uint64_t sentLower = std::numeric_limits< std::uint64_t >::max();
            std::uint64_t sentUpper = std::numeric_limits< std::uint64_t >::min();

            for( std::size_t i = 0U, count = clients.size(); i < count; ++i )
            {
                const auto& client = clients[ i ].second;

                const auto clientImpl = om::qi< client_t >( client -> outgoingBlockChannel() );

                UTF_REQUIRE( clientImpl -> noOfBlocksReceived() > 2U );
                UTF_REQUIRE( clientImpl -> noOfBlocksSent() > 2U );

                if( clientImpl -> noOfBlocksReceived() < receivedLower )
                {
                    receivedLower = clientImpl -> noOfBlocksReceived();
                }

                if( clientImpl -> noOfBlocksReceived() > receivedUpper )
                {
                    receivedUpper = clientImpl -> noOfBlocksReceived();
                }

                if( clientImpl -> noOfBlocksSent() < sentLower )
                {
                    sentLower = clientImpl -> noOfBlocksSent();
                }

                if( clientImpl -> noOfBlocksSent() > sentUpper )
                {
                    sentUpper = clientImpl -> noOfBlocksSent();
                }
            }

            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "Message bounds: "
                    << "receivedLower="
                    << receivedLower
                    << "; receivedUpper="
                    << receivedUpper
                    << "; sentLower="
                    << sentLower
                    << "; sentUpper="
                    << sentUpper
                );
        }

        static void flushQueueWithRetriesOnTargetPeerNotFound( SAA_in const bl::om::ObjPtr< ExecutionQueue >& eq )
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::messaging;

            const std::size_t maxRetries = 2000U;
            std::map< Task*, std::size_t > retryCountsMap;

            for( ;; )
            {
                const auto task = eq -> top( true /* wait */ );

                if( ! task )
                {
                    BL_ASSERT( eq -> isEmpty() );

                    break;
                }

                if( ! task -> isFailed() )
                {
                    eq -> pop( true /* wait */ );

                    continue;
                }

                /*
                 * The task has failed - check if we need to do a retry
                 */

                try
                {
                    cpp::safeRethrowException( task -> exception() );
                }
                catch( ServerErrorException& e )
                {
                    const auto* ec = e.errorCode();

                    if( ec && eh::errc::make_error_code( BrokerErrorCodes::TargetPeerNotFound ) == *ec )
                    {
                        auto& retryCount = retryCountsMap[ task.get() ];

                        if( retryCount >= maxRetries )
                        {
                            throw;
                        }

                        ++retryCount;

                        os::sleep( time::milliseconds( 200L ) );

                        eq -> push_back( task );

                        continue;
                    }

                    throw;
                }
            }
        }

        static auto getDefaultProxyInboundPort() -> unsigned short
        {
            return test::UtfArgsParser::port() + 2U;
        }

        static auto getTestEndpointsList(
            SAA_in_opt          const std::string&                  brokerHostName = test::UtfArgsParser::host(),
            SAA_in_opt          const unsigned short                brokerInboundPort = test::UtfArgsParser::port(),
            SAA_in_opt          const std::size_t                   noOfEndpoints = 3U
            )
            -> std::vector< std::string >
        {
            std::vector< std::string > endpoints;

            bl::cpp::SafeOutputStringStream os;

            os
                << brokerHostName
                << ":"
                << brokerInboundPort;

            const auto endpoint = os.str();

            for( std::size_t i = 0U; i < noOfEndpoints; ++i )
            {
                endpoints.push_back( endpoint );
            }

            return endpoints;
        }

        static void startBrokerProxy(
            SAA_in_opt          const token_ptr_t&                  controlToken = nullptr,
            SAA_in_opt          const bl::cpp::void_callback_t&     callback = bl::cpp::void_callback_t(),
            SAA_in_opt          const unsigned short                proxyInboundPort = getDefaultProxyInboundPort(),
            SAA_in_opt          const std::size_t                   noOfConnections = test::UtfArgsParser::connections(),
            SAA_in_opt          const std::string&                  brokerHostName = test::UtfArgsParser::host(),
            SAA_in_opt          const unsigned short                brokerInboundPort = test::UtfArgsParser::port(),
            SAA_in_opt          const pool_ptr_t&                   dataBlocksPool = datablocks_pool_t::createInstance(),
            SAA_in_opt          const bl::time::time_duration&      heartbeatInterval = bl::time::neg_infin,
            SAA_inout_opt       bl::om::Object**                    backendRef = nullptr
            )
        {
            using namespace bl;
            using namespace bl::data;
            using namespace bl::messaging;

            const auto controlTokenLocal =
                controlToken ?
                    bl::om::copy( controlToken )
                    :
                    tasks::SimpleTaskControlTokenImpl::createInstance< tasks::TaskControlTokenRW >();

            const auto peerId = uuids::create();

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Proxy clients peerId: "
                    << peerId
                );

            /*
             * For the unit test case we want to pass waitAllToConnect=true
             *
             * Also for testing purpose we want to set maxNoOfSmallBlocks and minSmallBlocksDeltaToLog
             * to some small values, so we can test correctly all the code paths and the logging, etc
             */

            const auto maxNoOfSmallBlocks = test::UtfArgsParser::isServer() ?
                50 * test::UtfArgsParser::connections() : test::UtfArgsParser::connections();

            const auto minSmallBlocksDeltaToLogDefault = maxNoOfSmallBlocks / 10U;

            const auto proxyBackend = bl::om::lockDisposable(
                ProxyBrokerBackendProcessingFactorySsl::create(
                    test::UtfArgsParser::port()                     /* defaultInboundPort */,
                    bl::om::copy( controlTokenLocal ),
                    peerId,
                    noOfConnections,
                    getTestEndpointsList( brokerHostName, brokerInboundPort, 3U /* noOfEndpoints */ ),
                    dataBlocksPool,
                    0U                                              /* threadsCount */,
                    0U                                              /* maxConcurrentTasks */,
                    true                                            /* waitAllToConnect */,
                    maxNoOfSmallBlocks,
                    minSmallBlocksDeltaToLogDefault < 5U ?
                        5U : minSmallBlocksDeltaToLogDefault        /* minSmallBlocksDeltaToLog */
                    )
                );

            if( backendRef )
            {
                *backendRef = proxyBackend.get();
            }

            bl::messaging::BrokerFacade::execute(
                proxyBackend,
                test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
                test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
                proxyInboundPort                                    /* inboundPort */,
                proxyInboundPort + 1U                               /* outboundPort */,
                test::UtfArgsParser::threadsCount(),
                0U                                                  /* maxConcurrentTasks */,
                callback,
                om::copy( controlTokenLocal ),
                dataBlocksPool,
                cpp::copy( heartbeatInterval )
                );
        }

        static auto createTestMessagingBackend() -> bl::om::ObjPtr< bl::messaging::BackendProcessing >
        {
            using namespace bl;
            using namespace bl::security;

            return messaging::BrokerBackendProcessing::createInstance< messaging::BackendProcessing >(
                ( test::UtfArgsParser::path().empty() || test::UtfArgsParser::password().empty() ) ?
                    DummyAuthorizationCache::createInstance< AuthorizationCache >()
                    :
                    cache_t::template createInstance< AuthorizationCache >(
                        AuthorizationServiceRest::create( test::UtfArgsParser::path() )
                        )
                );
        }

        static void forwardingBackendTests(
            SAA_in_opt      bl::om::ObjPtr< bl::tasks::TaskControlTokenRW >&&       controlToken,
            SAA_in_opt      const std::string&                                      cookiesText = getTokenData(),
            SAA_in_opt      const std::string&                                      tokenType = getTokenType(),
            SAA_in_opt      const std::string&                                      brokerHostName = test::UtfArgsParser::host(),
            SAA_in_opt      const unsigned short                                    brokerInboundPort = test::UtfArgsParser::port(),
            SAA_in          const std::size_t                                       noOfConnections = 4U
            )
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::messaging;

            const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                    const auto peerId1 = uuids::create();
                    const auto peerId2 = uuids::create();

                    BL_LOG_MULTILINE(
                        bl::Logging::debug(),
                        BL_MSG()
                            << "Peer id 1: "
                            << uuids::uuid2string( peerId1 )
                            << "\nPeer id 2: "
                            << uuids::uuid2string( peerId2 )
                        );

                    const auto backendReference = om::ProxyImpl::createInstance< om::Proxy >( false /* strongRef*/ );

                    const auto echoContext = om::lockDisposable(
                        echo::EchoServerProcessingContext::createInstance(
                            false                                           /* isQuietMode */,
                            0UL                                             /* maxProcessingDelayInMicroseconds */,
                            cpp::copy( tokenType ),
                            cpp::copy( cookiesText )                        /* tokenData */,
                            om::copy( dataBlocksPool ),
                            om::copy( backendReference )
                            )
                        );

                    {

                        const auto loggingContext = logging_context_t::createInstance();

                        {
                            const auto backend1 = om::lockDisposable(
                                ForwardingBackendProcessingFactoryDefaultSsl::create(
                                    brokerInboundPort       /* defaultInboundPort */,
                                    om::copy( controlToken ),
                                    peerId1,
                                    noOfConnections,
                                    getTestEndpointsList( brokerHostName, brokerInboundPort ),
                                    dataBlocksPool,
                                    0U                      /* threadsCount */,
                                    0U                      /* maxConcurrentTasks */,
                                    true                    /* waitAllToConnect */
                                    )
                                );

                            {
                                auto proxy = om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );
                                proxy -> connect( loggingContext.get() );
                                backend1 -> setHostServices( std::move( proxy ) );
                            }

                            const auto backend2 = om::lockDisposable(
                                ForwardingBackendProcessingFactoryDefaultSsl::create(
                                    brokerInboundPort       /* defaultInboundPort */,
                                    om::copy( controlToken ),
                                    peerId2,
                                    noOfConnections,
                                    getTestEndpointsList( brokerHostName, brokerInboundPort ),
                                    dataBlocksPool,
                                    0U                      /* threadsCount */,
                                    0U                      /* maxConcurrentTasks */,
                                    true                    /* waitAllToConnect */
                                    )
                                );

                            {
                                auto proxy = om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );
                                proxy -> connect(
                                    static_cast< messaging::AsyncBlockDispatcher* >( echoContext.get() )
                                    );
                                backend2 -> setHostServices( std::move( proxy ) );
                            }

                            os::sleep( time::seconds( 2L ) );

                            {
                                BL_SCOPE_EXIT(
                                    {
                                        backendReference -> disconnect();
                                    }
                                    );

                                backendReference -> connect( backend2.get() );

                                const auto conversationId = uuids::create();

                                const auto brokerProtocol = createBrokerProtocolMessage(
                                    MessageType::AsyncRpcDispatch,
                                    conversationId,
                                    cookiesText,
                                    uuids::create() /* messageId */,
                                    tokenType
                                    );

                                const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                                    TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                                    );

                                const auto dataBlock = MessagingUtils::serializeObjectsToBlock(
                                    brokerProtocol,
                                    payload,
                                    dataBlocksPool
                                    );

                                const auto messageTask = backend1 -> createBackendProcessingTask(
                                    BackendProcessing::OperationId::Put,
                                    BackendProcessing::CommandId::None,
                                    uuids::nil()                                    /* sessionId */,
                                    BlockTransferDefs::chunkIdDefault(),
                                    peerId1                                         /* sourcePeerId */,
                                    peerId2                                         /* targetPeerId */,
                                    dataBlock
                                    );

                                eq -> push_back( messageTask );
                                eq -> waitForSuccess( messageTask );

                                os::sleep( time::seconds( 2L ) );

                                UTF_REQUIRE( loggingContext -> messageLogged() );
                                UTF_REQUIRE_EQUAL( 1UL, echoContext -> messagesProcessed() );
                            }
                        }
                    }

                    controlToken -> requestCancel();
                }
                );
        }
    };

    BL_DEFINE_STATIC_STRING( TestMessagingUtilsT, g_tokenType );
    BL_DEFINE_STATIC_MEMBER( TestMessagingUtilsT, bl::os::mutex, g_tokenTypeLock );

    typedef TestMessagingUtilsT<> TestMessagingUtils;

} // utest

#endif /* __UTEST_TESTMESSAGINGUTILS_H_ */
