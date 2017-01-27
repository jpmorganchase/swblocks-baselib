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

#ifndef __APPS_BLMESSAGINGBROKER_MESSAGINGBROKERCMDLINE_H_
#define __APPS_BLMESSAGINGBROKER_MESSAGINGBROKERCMDLINE_H_

#include <baselib/cmdline/CmdLineBase.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief Messaging broker default inbound port,
         * default outbound port is the following port number (+1)
         */

        enum : os::port_t
        {
            MessagingBrokerDefaultInboundPort = 29300U,
        };

        /**
         * @brief The bl-messaging-broker command line parser
         */

        template
        <
            typename E = void
        >
        class MessagingBrokerCmdLineT FINAL : public cmdline::CmdLineBase
        {
        public:

            cmdline::HelpSwitch             m_help;

            BL_CMDLINE_OPTION(
                m_authorizationConfigFile,
                StringOption,
                "authorization-config-file",
                "The authorization config file",
                cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_privateKeyFile,
                StringOption,
                "private-key-file",
                "SSL private key file in PEM format",
                cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_certificateFile,
                StringOption,
                "certificate-file",
                "SSL certificate file in PEM format",
                cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_inboundPort,
                UShortOption,
                "inbound-port",
                "The inbound port to be used",
                MessagingBrokerDefaultInboundPort
                )

            BL_CMDLINE_OPTION(
                m_outboundPort,
                UShortOption,
                "outbound-port",
                "The outbound port to be used, if not specified then 'inbound-port + 1' used"
                )

            BL_CMDLINE_OPTION(
                m_processingThreadsCount,
                UShortOption,
                "processing-threads-count",
                "The number of threads to be used for the message processing thread pool",
                32U /* The default value */
                )

            BL_CMDLINE_OPTION(
                m_maxOutstandingOperations,
                UShortOption,
                "max-outstanding-operations",
                "The maximum number of concurrent outstanding async operations (to control memory usage)",
                4096U /* The default value */
                )

            BL_CMDLINE_OPTION(
                m_proxyEndpoints,
                MultiStringOption,
                "proxy-endpoints",
                "The list of proxy endpoints (to be used in proxy mode)",
                bl::cmdline::MultiValue
                )

            BL_CMDLINE_OPTION(
                m_verifyRootCA,
                StringOption,
                "verify-root-ca",
                "An additional root CA to be used"
                )

            MessagingBrokerCmdLineT()
                :
                cmdline::CmdLineBase( "Usage: bl-messaging-broker [options]" )
            {
                addOption(
                    m_help,
                    m_authorizationConfigFile,
                    m_privateKeyFile,
                    m_certificateFile,
                    m_inboundPort,
                    m_outboundPort,
                    m_processingThreadsCount,
                    m_maxOutstandingOperations
                    );

                addOption(
                    m_proxyEndpoints,
                    m_verifyRootCA
                    );
            }
        };

        typedef MessagingBrokerCmdLineT<> MessagingBrokerCmdLine;

    } // messaging

} // bl

#endif /* __APPS_BLMESSAGINGBROKER_MESSAGINGBROKERCMDLINE_H_ */
