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

#ifndef __APPS_BLMESSAGINGHTTPGATEWAY_MESSAGINGHTTPGATEWAYAPP_H_
#define __APPS_BLMESSAGINGHTTPGATEWAY_MESSAGINGHTTPGATEWAYAPP_H_

#include <apps/bl-messaging-http-gateway/MessagingHttpGatewayCmdLine.h>

#include <baselib/rest/HttpServerBackendMessagingBridge.h>

#include <baselib/messaging/ForwardingBackendProcessingImpl.h>

#include <baselib/httpserver/HttpServer.h>

#include <build/PluginBuildId.h>

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/ExecutionQueue.h>

#include <baselib/cmdline/CmdLineAppBase.h>

#include <baselib/crypto/TrustedRoots.h>
#include <baselib/crypto/CryptoBase.h>

#include <baselib/core/FileEncoding.h>
#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace rest
    {
        /**
         * @brief Messaging HTTP gateway application.
         */

        template
        <
            typename E = void
        >
        class MessagingHttpGatewayAppT
        {
            BL_DECLARE_STATIC( MessagingHttpGatewayAppT )

        protected:

            template
            <
                typename E2 = void
            >
            class MessagingHttpGatewayAppImplT :
                public cmdline::CmdLineAppBase< MessagingHttpGatewayAppImplT< E2 > >
            {
                BL_CTR_DEFAULT( MessagingHttpGatewayAppImplT, public )
                BL_NO_COPY_OR_MOVE( MessagingHttpGatewayAppImplT )

            public:

                typedef cmdline::CmdLineAppBase< MessagingHttpGatewayAppImplT< E2 > >   base_type;
                typedef bl::httpserver::ServerBackendProcessing                         ServerBackendProcessing;

                void parseArgs(
                    SAA_in                      std::size_t                             argc,
                    SAA_in_ecount( argc )       const char* const*                      argv
                    )
                {
                    BL_UNUSED( argc );
                    BL_UNUSED( argv );

                    /*
                     * We need to set base_type::m_isServer here to ensure the CmdLineAppBase
                     * base class configures logging, priorities and other global stuff correctly
                     */

                    base_type::m_isServer = true;
                }

                void appMain(
                    SAA_in                      std::size_t                             argc,
                    SAA_in_ecount( argc )       const char* const*                      argv
                    )
                {
                    using namespace bl::messaging;
                    using namespace bl::tasks;
                    using namespace bl::rest;

                    MessagingHttpGatewayCmdLine cmdLine;
                    const auto* command = cmdLine.parseCommandLine( argc, argv );

                    BL_ASSERT( command && command == &cmdLine );
                    BL_UNUSED( command );

                    if( cmdLine.m_help.getValue() )
                    {
                        BL_STDIO_TEXT(
                            {
                                cmdLine.helpMessage( std::cout );
                            }
                            );

                        return;
                    }

                    const fs::path privateKeyPath = cmdLine.m_privateKeyFile.getValue();
                    const fs::path certificatePath = cmdLine.m_certificateFile.getValue();
                    const auto inboundPort = cmdLine.m_inboundPort.getValue();
                    const auto brokerEndpoints = cmdLine.m_brokerEndpoints.getValue();

                    const auto sourcePeerId = cmdLine.m_sourcePeerId.hasValue() ?
                        uuids::string2uuid( cmdLine.m_sourcePeerId.getValue() ) : uuids::create();

                    const auto targetPeerId = uuids::string2uuid( cmdLine.m_targetPeerId.getValue() );

                    std::unordered_set< std::string > tokenCookieNames;

                    if( cmdLine.m_tokenCookieName.hasValue() )
                    {
                        const auto values = cmdLine.m_tokenCookieName.getValue();

                        std::copy(
                            std::begin( values ),
                            std::end( values ),
                            std::inserter( tokenCookieNames, std::end( tokenCookieNames ) )
                            );
                    }

                    time::time_duration requestTimeout = time::neg_infin;

                    if( cmdLine.m_requestTimeoutInSeconds.hasValue() )
                    {
                        requestTimeout = time::seconds( cmdLine.m_requestTimeoutInSeconds.getValue() );
                    }

                    const auto noOfConnectionRequested = cmdLine.m_connections.getValue();

                    BL_LOG_MULTILINE(
                        Logging::notify(),
                        BL_MSG()
                            << "BASELIB messaging HTTP gateway parameters:\n"
                            << "\nBuild number: "
                            << BL_PLUGINS_BUILD_ID
                            << "\nInbound HTTPS port: "
                            << inboundPort
                            << "\nSource peer id: "
                            << sourcePeerId
                            << "\nTarget peer id: "
                            << targetPeerId
                            << "\nEndpoints list: "
                            << str::vectorToString( brokerEndpoints )
                            << "\nSecurity token cookie names list: "
                            << str::vectorToString( cmdLine.m_tokenCookieName.getValue() )
                            << "\nSecurity token type default: "
                            << cmdLine.m_tokenTypeDefault.getValue( "<empty>" /* defaultValue */ )
                            << "\nSecurity token data default: "
                            << cmdLine.m_tokenDataDefault.getValue( "<empty>" /* defaultValue */ )
                            << "\nRequest timeout in seconds: "
                            << cmdLine.m_requestTimeoutInSeconds.getValue( 0L )
                            << "\nNumber of connections: "
                            << noOfConnectionRequested
                        );

                    const auto privateKeyPem = encoding::readTextFile( fs::normalize( privateKeyPath ) );
                    const auto certificatePem = encoding::readTextFile( fs::normalize( certificatePath ) );

                    if( cmdLine.m_verifyRootCA.hasValue() )
                    {
                        /*
                         * An additional root CA was provided on command line (to be used / registered)
                         */

                        const fs::path verifyRootCAPath = cmdLine.m_verifyRootCA.getValue();

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Registering an additional root CA: "
                                << verifyRootCAPath
                            );

                        crypto::registerTrustedRoot(
                            encoding::readTextFile( fs::normalize( verifyRootCAPath ) ) /* certificatePemText */
                            );
                    }

                    const auto controlToken =
                        tasks::SimpleTaskControlTokenImpl::createInstance< tasks::TaskControlTokenRW >();

                    const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

                    const auto minNoOfConnectionsPerEndpoint = 8U;

                    const auto noOfConnections = std::max< std::size_t >(
                        minNoOfConnectionsPerEndpoint * brokerEndpoints.size(),
                        noOfConnectionRequested
                        );

                    BL_LOG_MULTILINE(
                        Logging::notify(),
                        BL_MSG()
                            << "\nBASELIB messaging HTTP gateway is starting...\n"
                        );

                    const auto messagingBackend = om::lockDisposable(
                        ForwardingBackendProcessingFactoryDefaultSsl::create(
                            MessagingBrokerDefaultInboundPort       /* defaultInboundPort */,
                            om::copy( controlToken ),
                            sourcePeerId,
                            noOfConnections,
                            cpp::copy( brokerEndpoints ),
                            dataBlocksPool,
                            0U                                      /* threadsCount */,
                            0U                                      /* maxConcurrentTasks */,
                            true                                    /* waitAllToConnect */
                            )
                        );


                    {
                        const auto expectedSecurityId = cmdLine.m_expectedSecurityId.getValue();

                        const auto httpBackend = om::lockDisposable(
                            rest::HttpServerBackendMessagingBridge::createInstance< ServerBackendProcessing >(
                                om::copy( messagingBackend ),
                                sourcePeerId,
                                targetPeerId,
                                om::copy( dataBlocksPool ),
                                std::move( tokenCookieNames ),
                                ! expectedSecurityId.empty() || ! cmdLine.m_noServerAuthenticationRequired.getValue(),
                                cpp::copy( expectedSecurityId ),
                                cmdLine.m_logUnauthorizedMessages.getValue(),
                                cmdLine.m_tokenTypeDefault.getValue( std::string() /* defaultValue */ ),
                                cmdLine.m_tokenDataDefault.getValue( std::string() /* defaultValue */ ),
                                requestTimeout
                                )
                            );

                            scheduleAndExecuteInParallel(
                                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                                {
                                    eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                                    const auto acceptor = bl::httpserver::HttpSslServer::createInstance(
                                        om::copy( httpBackend ),
                                        controlToken,
                                        "0.0.0.0"                                           /* host */,
                                        inboundPort,
                                        privateKeyPem                                       /* privateKeyPem */,
                                        certificatePem                                      /* certificatePem */
                                        );

                                    tasks::startAcceptor( acceptor, eq );
                                });
                    }

                    /*
                     * The messaging HTTP gateway should always exit with non-zero exit code to trigger alerts
                     */

                    base_type::m_exitCode = 1;
                }
            };

            typedef MessagingHttpGatewayAppImplT<> MessagingHttpGatewayAppImpl;

        public:

            static int main(
                SAA_in                      std::size_t                             argc,
                SAA_in_ecount( argc )       const char* const*                      argv
                )
            {
                Logging::setLevel( Logging::LL_DEBUG, true /* global */ );

                /*
                 * Relax the protocol to TLS 1.0 for allowing the gateway to use
                 * authorization servers that might not yet support TLS 1.1 and TLS 1.2
                 */

                crypto::CryptoBase::isEnableTlsV10( true );

                MessagingHttpGatewayAppImpl app;

                return app.main( argc, argv );
            }
        };

        typedef MessagingHttpGatewayAppT<> MessagingHttpGatewayApp;

    } // rest

} // bl

#endif /* __APPS_BLMESSAGINGHTTPGATEWAY_MESSAGINGHTTPGATEWAYAPP_H_ */
