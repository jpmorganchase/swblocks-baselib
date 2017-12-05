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

#ifndef __APPS_BLMESSAGINGBROKER_MESSAGINGBROKERAPP_H_
#define __APPS_BLMESSAGINGBROKER_MESSAGINGBROKERAPP_H_

#include <apps/bl-messaging-broker/MessagingBrokerCmdLine.h>

#include <baselib/security/AuthorizationCacheImpl.h>
#include <baselib/security/AuthorizationServiceRest.h>

#include <build/PluginBuildId.h>

#include <baselib/messaging/ProxyBrokerBackendProcessingFactory.h>
#include <baselib/messaging/BrokerBackendProcessing.h>
#include <baselib/messaging/BrokerFacade.h>

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
    namespace messaging
    {
        /**
         * @brief Messaging Broker application.
         */

        template
        <
            typename E = void
        >
        class MessagingBrokerAppT
        {
            BL_DECLARE_STATIC( MessagingBrokerAppT )

        protected:

            template
            <
                typename E2 = void
            >
            class MessagingBrokerAppImplT :
                public cmdline::CmdLineAppBase< MessagingBrokerAppImplT< E2 > >
            {
                BL_CTR_DEFAULT( MessagingBrokerAppImplT, public )
                BL_NO_COPY_OR_MOVE( MessagingBrokerAppImplT )

            public:

                typedef cmdline::CmdLineAppBase< MessagingBrokerAppImplT< E2 > >        base_type;

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
                    using namespace bl::security;
                    using namespace bl::tasks;

                    MessagingBrokerCmdLine cmdLine;
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

                    const fs::path authorizationConfigFile = cmdLine.m_authorizationConfigFile.getValue();
                    const fs::path privateKeyPath = cmdLine.m_privateKeyFile.getValue();
                    const fs::path certificatePath = cmdLine.m_certificateFile.getValue();

                    const auto inboundPort = cmdLine.m_inboundPort.getValue();

                    const auto outboundPort = cmdLine.m_outboundPort.hasValue() ?
                        cmdLine.m_outboundPort.getValue() :
                        inboundPort + 1;

                    const auto processingThreadsCount = cmdLine.m_processingThreadsCount.getValue();
                    const auto maxOutstandingOperations = cmdLine.m_maxOutstandingOperations.getValue();

                    const bool proxyMode = cmdLine.m_proxyEndpoints.hasValue();

                    BL_LOG_MULTILINE(
                        Logging::notify(),
                        BL_MSG()
                            << "BASELIB messaging broker parameters:\n"
                            << "\nBuild number: "
                            << BL_PLUGINS_BUILD_ID
                            << "\nInbound port: "
                            << inboundPort
                            << "\nOutbound port: "
                            << outboundPort
                            << "\nProcessing threads count: "
                            << processingThreadsCount
                            << "\nMax outstanding operations: "
                            << maxOutstandingOperations
                            << "\nProxy mode: "
                            << proxyMode
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

                    const auto createProxyBackend = [ & ]() -> om::ObjPtr< BackendProcessing >
                    {
                        const auto& endpoints = cmdLine.m_proxyEndpoints.getValue();

                        const auto minNoOfConnectionsPerEndpoint = 8U;

                        const auto noOfConnections = std::max< std::size_t >(
                            minNoOfConnectionsPerEndpoint * endpoints.size(),
                            32U
                            );

                        const auto peerId = uuids::create();

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Proxy clients peerId: "
                                << peerId
                            );

                        return ProxyBrokerBackendProcessingFactorySsl::create(
                            MessagingBrokerDefaultInboundPort,
                            om::copy( controlToken ),
                            peerId,
                            noOfConnections,
                            cpp::copy( endpoints ),
                            dataBlocksPool
                            );
                    };

                    BL_LOG_MULTILINE(
                        Logging::notify(),
                        BL_MSG()
                            << "\nBASELIB messaging broker has started\n"
                        );

                    const long cacheFreshnessIntervalInMinutes = 10L;

                    typedef om::ObjectImpl< AuthorizationCacheImpl< AuthorizationServiceRest > > cache_t;

                    auto authorizationService =
                        AuthorizationServiceRest::create( cpp::copy( authorizationConfigFile ) );

                    auto authorizationCache = cache_t::template createInstance< AuthorizationCache >(
                        std::move( authorizationService ),
                        time::minutes( cacheFreshnessIntervalInMinutes )
                        );

                    const auto processingBackend = om::lockDisposable(
                        proxyMode ?
                            createProxyBackend()
                            :
                            BrokerBackendProcessing::createInstance< BackendProcessing >(
                                std::move( authorizationCache )
                                )
                        );

                    BrokerFacade::execute(
                        processingBackend,
                        privateKeyPem,
                        certificatePem,
                        inboundPort,
                        outboundPort,
                        processingThreadsCount,
                        maxOutstandingOperations,
                        cpp::void_callback_t()              /* callback */,
                        om::copy( controlToken ),
                        dataBlocksPool
                        );

                    /*
                     * The messaging server should always exit with non-zero exit code to trigger alerts
                     */

                    base_type::m_exitCode = 1;
                }
            };

            typedef MessagingBrokerAppImplT<> MessagingBrokerAppImpl;

        public:

            static int main(
                SAA_in                      std::size_t                             argc,
                SAA_in_ecount( argc )       const char* const*                      argv
                )
            {
                Logging::setLevel( Logging::LL_DEBUG, true /* global */ );

                /*
                 * Relax the protocol to TLS 1.0 for allowing the broker to use
                 * authorization servers that might not yet support TLS 1.1
                 */

                crypto::CryptoBase::isEnableTlsV10( true );

                MessagingBrokerAppImpl app;

                return app.main( argc, argv );
            }
        };

        typedef MessagingBrokerAppT<> MessagingBrokerApp;

    } // messaging

} // bl

#endif /* __APPS_BLMESSAGINGBROKER_MESSAGINGBROKERAPP_H_ */
