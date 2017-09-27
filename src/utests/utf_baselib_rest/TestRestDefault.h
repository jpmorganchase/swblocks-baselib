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

#include <baselib/core/BuildInfo.h>

#include <baselib/rest/HttpServerBackendMessagingBridge.h>

#include <utests/baselib/TestMessagingUtils.h>
#include <utests/baselib/HttpServerHelpers.h>
#include <utests/baselib/UtfCrypto.h>

UTF_AUTO_TEST_CASE( RestServiceSslBackendTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;
    using namespace utest;
    using namespace utest::http;

    const auto forwardingBackendTests = [](
            SAA_in_opt      const om::ObjPtr< bl::tasks::TaskControlTokenRW >&      controlToken,
            SAA_in_opt      const std::string&                                      brokerHostName,
            SAA_in_opt      const unsigned short                                    brokerInboundPort,
            SAA_in          const std::size_t                                       noOfConnections
            )
        {
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

           const auto echoContext = echo_context_t::createInstance(
                om::copy( dataBlocksPool ),
                om::copy( backendReference )
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
                    proxy -> connect( echoContext.get() );
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
                            om::copy( backend1 )                                    /* messagingBackend */,
                            peerId1                                                 /* sourcePeerId */,
                            peerId2                                                 /* targetPeerId */,
                            om::copy( dataBlocksPool ),
                            std::unordered_set< std::string >()                     /* tokenCookieNames */,
                            std::string()                                           /* tokenTypeDefault */,
                            cpp::copy( TestMessagingUtils::getTokenData() )         /* tokenDataDefault */
                            )
                        );

                    {
                        const auto acceptor = bl::httpserver::HttpSslServer::createInstance(
                            om::copy( httpBackend ),
                            controlToken,
                            "0.0.0.0"                                           /* host */,
                            test::UtfArgsParser::port(),
                            test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
                            test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */
                            );

                        TestTaskUtils::startAcceptorAndExecuteCallback(
                            [ & ]() -> void
                            {
                                scheduleAndExecuteInParallel(
                                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                                    {
                                        eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                                        /*
                                         * Run an assorted set of HTTP requests for testing
                                         */

                                        const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                                            TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                                            );

                                        auto payloadDataString = bl::dm::DataModelUtils::getDocAsPackedJsonString( payload );

                                        const auto taskImpl = SimpleHttpSslPutTaskImpl::createInstance(
                                            cpp::copy( test::UtfArgsParser::host() ),
                                            test::UtfArgsParser::port(),
                                            "foo/bar",                          /* URI */
                                            std::move( payloadDataString )      /* content */
                                            );

                                        const auto task = om::qi< Task >( taskImpl );

                                        eq -> push_back( task );
                                        eq -> waitForSuccess( task );

                                        os::sleep( time::seconds( 2L ) );

                                        UTF_REQUIRE( echoContext -> messageLogged() );

                                        const auto responsePayload = bl::dm::DataModelUtils::loadFromJsonText< Payload >(
                                            taskImpl -> getResponse()
                                            );

                                        const auto response = bl::dm::DataModelUtils::getDocAsPrettyJsonString( responsePayload );

                                        BL_LOG_MULTILINE(
                                            Logging::debug(),
                                            BL_MSG()
                                                << "\n**********************************************\n\n"
                                                << "Response message:\n"
                                                << response
                                                << "\n\n"
                                            );
                                    });
                            },
                            acceptor
                            );
                    }
                }
            }

            controlToken -> requestCancel();
        };

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto callbackTests = [ & ]() -> void
    {
        forwardingBackendTests(
            om::copy( controlToken ),
            test::UtfArgsParser::host()                     /* brokerHostName */,
            test::UtfArgsParser::port() + 10U               /* brokerInboundPort */,
            4U /* noOfConnections */
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port() + 10U                   /* inboundPort */,
        test::UtfArgsParser::port() + 11U                   /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests,
        om::copy( controlToken )
        );
}
