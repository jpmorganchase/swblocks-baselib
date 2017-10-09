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

#ifndef __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERAPP_H_
#define __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERAPP_H_

#include <baselib/examples/echoserver/EchoServerProcessingContext.h>

#include <apps/bl-messaging-echo-server/MessagingEchoServerCmdLine.h>

#include <baselib/messaging/ForwardingBackendProcessingImpl.h>

#include <build/PluginBuildId.h>

#include <baselib/data/models/Http.h>

#include <baselib/http/Globals.h>

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
    namespace echo
    {
        /**
         * @brief Messaging echo server application.
         */

        template
        <
            typename E = void
        >
        class MessagingEchoServerAppT
        {
            BL_DECLARE_STATIC( MessagingEchoServerAppT )

        protected:

            template
            <
                typename E2 = void
            >
            class MessagingEchoServerAppImplT :
                public cmdline::CmdLineAppBase< MessagingEchoServerAppImplT< E2 > >
            {
                BL_CTR_DEFAULT( MessagingEchoServerAppImplT, public )
                BL_NO_COPY_OR_MOVE( MessagingEchoServerAppImplT )

            public:

                typedef cmdline::CmdLineAppBase< MessagingEchoServerAppImplT< E2 > >    base_type;

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
                    using namespace bl::echo;

                    MessagingEchoServerCmdLine cmdLine;
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

                    const auto brokerEndpoints = cmdLine.m_brokerEndpoints.getValue();
                    const auto peerId = uuids::string2uuid( cmdLine.m_peerId.getValue() );

                    const auto noOfConnectionRequested = cmdLine.m_connections.getValue();

                    BL_LOG_MULTILINE(
                        Logging::notify(),
                        BL_MSG()
                            << "BASELIB messaging HTTP gateway parameters:\n"
                            << "\nBuild number: "
                            << BL_PLUGINS_BUILD_ID
                            << "\nServer peer id: "
                            << peerId
                            << "\nEndpoints list: "
                            << str::vectorToString( brokerEndpoints )
                            << "\nSecurity token type default: "
                            << cmdLine.m_tokenTypeDefault.getValue( "<empty>" /* defaultValue */ )
                            << "\nSecurity token data default: "
                            << cmdLine.m_tokenDataDefault.getValue( "<empty>" /* defaultValue */ )
                            << "\nNumber of connections: "
                            << noOfConnectionRequested
                            << "\nMax processing time in microseconds: "
                            << cmdLine.m_maxProcessingDelayInMicroseconds.getValue( 0UL )
                            << "\nQuiet mode: "
                            << cmdLine.m_quietMode.hasValue()
                        );

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
                        SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

                    const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

                    const auto minNoOfConnectionsPerEndpoint = 8U;

                    const auto noOfConnections = std::max< std::size_t >(
                        minNoOfConnectionsPerEndpoint * brokerEndpoints.size(),
                        noOfConnectionRequested
                        );

                    BL_LOG_MULTILINE(
                        Logging::notify(),
                        BL_MSG()
                            << "\nBASELIB messaging echo server is starting...\n"
                        );

                    const auto backendReference = om::ProxyImpl::createInstance< om::Proxy >( false /* strongRef*/ );

                    const auto processingContext = om::lockDisposable(
                        EchoServerProcessingContext::createInstance< messaging::AsyncBlockDispatcher >(
                            cmdLine.m_quietMode.hasValue(),
                            cmdLine.m_maxProcessingDelayInMicroseconds.getValue( 0UL ),
                            cmdLine.m_tokenTypeDefault.getValue( "" ),
                            cmdLine.m_tokenDataDefault.getValue( "" ),
                            om::copy( dataBlocksPool ),
                            om::copy( backendReference )
                            )
                        );

                    {
                        const auto forwardingBackend = om::lockDisposable(
                            ForwardingBackendProcessingFactoryDefaultSsl::create(
                                MessagingBrokerDefaultInboundPort       /* defaultInboundPort */,
                                om::copy( controlToken ),
                                peerId,
                                noOfConnections,
                                cpp::copy( brokerEndpoints ),
                                dataBlocksPool,
                                0U                                      /* threadsCount */,
                                0U                                      /* maxConcurrentTasks */,
                                true                                    /* waitAllToConnect */
                                )
                            );

                        {
                            auto proxy = om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );
                            proxy -> connect( processingContext.get() );
                            forwardingBackend -> setHostServices( std::move( proxy ) );
                        }

                        {
                            BL_SCOPE_EXIT(
                                {
                                    backendReference -> disconnect();
                                }
                                );

                            backendReference -> connect( forwardingBackend.get() );

                            EchoServerProcessingContext::waitOnForwardingBackend( controlToken, forwardingBackend );
                        }
                    }

                    /*
                     * The messaging echo task should always exit with non-zero exit code to trigger alerts
                     */

                    base_type::m_exitCode = 1;
                }
            };

            typedef MessagingEchoServerAppImplT<> MessagingEchoServerAppImpl;

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

                MessagingEchoServerAppImpl app;

                return app.main( argc, argv );
            }
        };

        typedef MessagingEchoServerAppT<> MessagingEchoServerApp;

    } // echo

} // bl

#endif /* __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERAPP_H_ */
