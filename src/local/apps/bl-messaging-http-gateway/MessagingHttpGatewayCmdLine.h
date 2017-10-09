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

#ifndef __APPS_BLMESSAGINGHTTPGATEWAY_MESSAGINGHTTPGATEWAYCMDLINE_H_
#define __APPS_BLMESSAGINGHTTPGATEWAY_MESSAGINGHTTPGATEWAYCMDLINE_H_

#include <baselib/cmdline/CmdLineBase.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace rest
    {
        /**
         * @brief The HTTPS server default inbound port and
         * the messaging broker default inbound port (used only if the endpoint
         * string doesn't specify it)
         */

        enum : os::port_t
        {
            HttpDefaultSecureInboundPort = 443U,
            MessagingBrokerDefaultInboundPort = 29300U,
        };

        /**
         * @brief The bl-messaging-http-gateway command line parser
         */

        template
        <
            typename E = void
        >
        class MessagingHttpGatewayCmdLineT FINAL : public cmdline::CmdLineBase
        {
        public:

            cmdline::HelpSwitch             m_help;

            BL_CMDLINE_OPTION(
                m_sourcePeerId,
                StringOption,
                "source-peer-id",
                "The source peer id for the HTTP gateway forwarding backend (if provided it must be UUID)"
                )

            BL_CMDLINE_OPTION(
                m_targetPeerId,
                StringOption,
                "target-peer-id",
                "The target peer id of the real backend / server (must be UUID)",
                cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_tokenCookieName,
                MultiStringOption,
                "token-cookie-name",
                "The security token cookie names list",
                bl::cmdline::MultiValue
                )

            BL_CMDLINE_OPTION(
                m_tokenTypeDefault,
                StringOption,
                "token-type-default",
                "The security token type default"
                )

            BL_CMDLINE_OPTION(
                m_tokenDataDefault,
                StringOption,
                "token-data-default",
                "The security token data default"
                )

            BL_CMDLINE_OPTION(
                m_requestTimeoutInSeconds,
                LongOption,
                "request-timeout-in-seconds",
                "The request timeout in seconds"
                )

            BL_CMDLINE_OPTION(
                m_inboundPort,
                UShortOption,
                "inbound-port",
                "The inbound HTTPS port to be used (443 by default)",
                HttpDefaultSecureInboundPort
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
                m_connections,
                UShortOption,
                "connections",
                "The number of connections to messaging broker to be used by the forwarding backend",
                16U /* The default value */
                )

            BL_CMDLINE_OPTION(
                m_brokerEndpoints,
                MultiStringOption,
                "broker-endpoints",
                "The list of broker endpoints to be used",
                bl::cmdline::MultiValue
                )

            BL_CMDLINE_OPTION(
                m_verifyRootCA,
                StringOption,
                "verify-root-ca",
                "An additional root CA to be used"
                )

            BL_CMDLINE_OPTION(
                m_noServerAuthenticationRequired,
                BoolSwitch,
                "no-server-authentication-required",
                "If provided server authentication will not be required and enforced"
                )

            BL_CMDLINE_OPTION(
                m_logUnauthorizedMessages,
                BoolSwitch,
                "log-unauthorized-messages",
                "Should the HTTP gateway log the unauthorized messages from server"
                )

            BL_CMDLINE_OPTION(
                m_expectedSecurityId,
                StringOption,
                "expected-security-id",
                "The expected authenticated security id of the server"
                )

            BL_CMDLINE_OPTION(
                m_graphqlErrorFormatting,
                BoolSwitch,
                "graphql-error-formatting",
                "Request to use the GraphQL JSON error formatting"
                )

            MessagingHttpGatewayCmdLineT()
                :
                cmdline::CmdLineBase( "Usage: bl-messaging-http-gateway [options]" )
            {
                addOption(
                    m_sourcePeerId,
                    m_targetPeerId,
                    m_tokenCookieName,
                    m_tokenTypeDefault,
                    m_tokenDataDefault,
                    m_requestTimeoutInSeconds
                    );

                addOption(
                    m_inboundPort,
                    m_help,
                    m_privateKeyFile,
                    m_certificateFile,
                    m_connections,
                    m_brokerEndpoints,
                    m_verifyRootCA
                    );

                addOption(
                    m_noServerAuthenticationRequired,
                    m_logUnauthorizedMessages,
                    m_expectedSecurityId,
                    m_graphqlErrorFormatting
                    );
            }
        };

        typedef MessagingHttpGatewayCmdLineT<> MessagingHttpGatewayCmdLine;

    } // rest

} // bl

#endif /* __APPS_BLMESSAGINGHTTPGATEWAY_MESSAGINGHTTPGATEWAYCMDLINE_H_ */
