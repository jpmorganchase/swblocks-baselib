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

#ifndef __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERCMDLINE_H_
#define __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERCMDLINE_H_

#include <baselib/cmdline/CmdLineBase.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace echo
    {
        /**
         * @brief The messaging broker default inbound port
         */

        enum : os::port_t
        {
            MessagingBrokerDefaultInboundPort = 29300U,
        };

        /**
         * @brief The bl-messaging-echo-server command line parser
         */

        template
        <
            typename E = void
        >
        class MessagingEchoServerCmdLineT FINAL : public cmdline::CmdLineBase
        {
        public:

            cmdline::HelpSwitch             m_help;

            BL_CMDLINE_OPTION(
                m_peerId,
                StringOption,
                "peer-id",
                "The server peer id (must be UUID)",
                cmdline::Required
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
                m_maxProcessingDelayInMilliseconds,
                ULongOption,
                "max-processing-delay-in-milliseconds",
                "The maximum simulated random processing delay in milliseconds (zero means no delay)",
                0U /* The default value */
                )

            BL_CMDLINE_OPTION(
                m_quietMode,
                BoolSwitch,
                "quiet-mode",
                "If quiet mode is enabled - i.e. no printing of the message data"
                )

            MessagingEchoServerCmdLineT()
                :
                cmdline::CmdLineBase( "Usage: bl-messaging-echo server [options]" )
            {
                addOption(
                    m_help,
                    m_peerId,
                    m_tokenTypeDefault,
                    m_tokenDataDefault,
                    m_connections,
                    m_brokerEndpoints,
                    m_verifyRootCA,
                    m_maxProcessingDelayInMilliseconds,
                    m_quietMode
                    );
            }
        };

        typedef MessagingEchoServerCmdLineT<> MessagingEchoServerCmdLine;

    } // echo

} // bl

#endif /* __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERCMDLINE_H_ */
