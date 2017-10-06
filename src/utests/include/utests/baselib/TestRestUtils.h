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

#ifndef __UTEST_TESTRESTUTILS_H_
#define __UTEST_TESTRESTUTILS_H_

#include <baselib/core/BuildInfo.h>

#include <baselib/examples/echoserver/EchoServerProcessingContext.h>

#include <baselib/rest/HttpServerBackendMessagingBridge.h>

#include <utests/baselib/TestMessagingUtils.h>
#include <utests/baselib/HttpServerHelpers.h>
#include <utests/baselib/UtfCrypto.h>

namespace utest
{
    /**
     * @brief Helpers for aiding implementation of REST tests
     */

    template
    <
        typename E = void
    >
    class TestRestUtilsT
    {
        BL_DECLARE_STATIC( TestRestUtilsT )

    public:

        static auto defaultToken() -> std::string
        {
            return TestMessagingUtils::getTokenData();
        }

        static auto defaultTokenType() -> std::string
        {
            return TestMessagingUtils::getTokenType();
        }

        static auto noCookieNames() -> std::unordered_set< std::string >
        {
            return std::unordered_set< std::string >();
        }

        static void httpRunSimpleRequest(
            SAA_in          const unsigned short                                            httpPort,
            SAA_in_opt      const std::string&                                              tokenData = bl::str::empty(),
            SAA_in_opt      const std::size_t                                               requestsCount = 1U,
            SAA_in_opt      const bool                                                      isQuietMode = false
            )
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::messaging;
            using namespace utest;
            using namespace utest::http;

            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                    /*
                     * Run HTTP requests for testing
                     */

                    const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                        TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                        );

                    auto payloadDataString = bl::dm::DataModelUtils::getDocAsPackedJsonString( payload );

                    std::vector< om::ObjPtr< SimpleHttpSslPutTaskImpl > > tasks;

                    {
                        utils::ExecutionTimer timer( "Executing many requests" );

                        for( std::size_t i = 0U; i < requestsCount; ++i )
                        {
                            bl::http::HeadersMap headers;

                            if( ! tokenData.empty() )
                            {
                                headers[ bl::http::HttpHeader::g_cookie ] = tokenData;
                            }

                            headers[ bl::http::HttpHeader::g_contentType ] =
                                bl::http::HttpHeader::g_contentTypeJsonUtf8;

                            auto taskImpl = SimpleHttpSslPutTaskImpl::createInstance(
                                cpp::copy( test::UtfArgsParser::host() ),
                                httpPort,
                                "/foo/bar",                         /* URI */
                                cpp::copy( payloadDataString )      /* content */,
                                std::move( headers )                /* headers */
                                );

                            eq -> push_back( om::qi< Task >( taskImpl ) );
                            tasks.push_back( std::move( taskImpl ) );
                        }

                        eq -> flush();
                    }

                    if( ! isQuietMode && ! tasks.empty() )
                    {
                        const auto& taskImpl = tasks.at( 0 );

                        const auto responsePayload = bl::dm::DataModelUtils::loadFromJsonText< Payload >(
                            taskImpl -> getResponse()
                            );

                        const auto response = bl::dm::DataModelUtils::getDocAsPrettyJsonString( responsePayload );

                        const auto& responseHeaders = taskImpl -> getResponseHeaders();

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\n**********************************************\n\n"
                                << "Response headers:\n"
                                << str::mapToString( responseHeaders )
                                << "\nResponse message:\n"
                                << response
                                << "\n\n"
                            );
                    }
                });
        }

        static void httpRestWithMessagingBackendTests(
            SAA_in          const bl::om::ObjPtr< bl::tasks::TaskControlTokenRW >&          controlToken,
            SAA_in          const std::string&                                              brokerHostName,
            SAA_in          const unsigned short                                            brokerInboundPort,
            SAA_in          const std::size_t                                               noOfConnections,
            SAA_in          std::string&&                                                   expectedSecurityId,
            SAA_in_opt      std::unordered_set< std::string >&&                             tokenCookieNames = noCookieNames(),
            SAA_in_opt      std::string&&                                                   tokenTypeDefault = defaultTokenType(),
            SAA_in_opt      std::string&&                                                   tokenDataDefault = defaultToken(),
            SAA_in_opt      std::string&&                                                   tokenData = std::string(),
            SAA_in_opt      const bl::time::time_duration&                                  requestTimeout = bl::time::neg_infin
            )
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::messaging;
            using namespace utest;
            using namespace utest::http;

            const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

            const auto peerId1 = uuids::create();
            const auto peerId2 = uuids::create();

            BL_LOG_MULTILINE(
                Logging::debug(),
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
                    cpp::copy( tokenTypeDefault )                   /* tokenType */,
                    cpp::copy( tokenDataDefault )                   /* tokenData */,
                    om::copy( dataBlocksPool ),
                    om::copy( backendReference )
                    )
                );

            {
                const auto backend1 = om::lockDisposable(
                    ForwardingBackendProcessingFactoryDefaultSsl::create(
                        brokerInboundPort       /* defaultInboundPort */,
                        om::copy( controlToken ),
                        peerId1,
                        noOfConnections,
                        TestMessagingUtils::getTestEndpointsList( brokerHostName, brokerInboundPort ),
                        dataBlocksPool,
                        0U                      /* threadsCount */,
                        0U                      /* maxConcurrentTasks */,
                        true                    /* waitAllToConnect */
                        )
                    );

                const auto backend2 = om::lockDisposable(
                    ForwardingBackendProcessingFactoryDefaultSsl::create(
                        brokerInboundPort       /* defaultInboundPort */,
                        om::copy( controlToken ),
                        peerId2,
                        noOfConnections,
                        TestMessagingUtils::getTestEndpointsList( brokerHostName, brokerInboundPort ),
                        dataBlocksPool,
                        0U                      /* threadsCount */,
                        0U                      /* maxConcurrentTasks */,
                        true                    /* waitAllToConnect */
                        )
                    );

                {
                    auto proxy = om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );
                    proxy -> connect( static_cast< messaging::AsyncBlockDispatcher* >( echoContext.get() ) );
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

                    const auto httpBackend = om::lockDisposable(
                        rest::HttpServerBackendMessagingBridge::createInstance< bl::httpserver::ServerBackendProcessing >(
                            om::copy( controlToken ),
                            om::copy( backend1 )                                    /* messagingBackend */,
                            peerId1                                                 /* sourcePeerId */,
                            peerId2                                                 /* targetPeerId */,
                            om::copy( dataBlocksPool ),
                            BL_PARAM_FWD( tokenCookieNames ),
                            true                                                    /* serverAuthenticationRequired */,
                            BL_PARAM_FWD( expectedSecurityId ),
                            true                                                    /* logUnauthorizedMessages */,
                            BL_PARAM_FWD( tokenTypeDefault ),
                            BL_PARAM_FWD( tokenDataDefault ),
                            requestTimeout
                            )
                        );

                    {
                        const os::port_t httpPort = brokerInboundPort + 100U;

                        const auto acceptor = bl::httpserver::HttpSslServer::createInstance(
                            om::copy( httpBackend ),
                            controlToken,
                            "0.0.0.0"                                           /* host */,
                            httpPort,
                            test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
                            test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */
                            );

                        TestTaskUtils::startAcceptorAndExecuteCallback(
                            bl::cpp::bind(
                                &httpRunSimpleRequest,
                                httpPort,
                                tokenData,
                                1U              /* requestsCount */,
                                false           /* isQuietMode */
                                ),
                            acceptor
                            );

                        UTF_REQUIRE_EQUAL( 1UL, echoContext -> messagesProcessed() );
                    }
                }
            }

            controlToken -> requestCancel();
        }
    };

    typedef TestRestUtilsT<> TestRestUtils;

} // utest

#endif /* __UTEST_TESTRESTUTILS_H_ */
