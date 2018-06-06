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

#include <baselib/messaging/server/BaseServerCmdLine.h>
#include <baselib/messaging/server/BaseServerPorts.h>

#include <baselib/cmdline/CmdLineBase.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace echo
    {
        using messaging::MessagingBrokerDefaultInboundPort;

        /**
         * @brief The bl-messaging-echo-server command line parser
         */

        template
        <
            typename E = void
        >
        class MessagingEchoServerCmdLineT FINAL : public messaging::BaseServerCmdLine
        {
        public:

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
                m_maxProcessingDelayInMicroseconds,
                ULongOption,
                "max-processing-delay-in-microseconds",
                "The maximum simulated random processing delay in microseconds (zero means no delay)",
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
                messaging::BaseServerCmdLine( "bl-messaging-echo-server" /* serverName */, false /* enableJvm */ )
            {
                addOption(
                    m_tokenTypeDefault,
                    m_tokenDataDefault,
                    m_maxProcessingDelayInMicroseconds,
                    m_quietMode
                    );
            }
        };

        typedef MessagingEchoServerCmdLineT<> MessagingEchoServerCmdLine;

    } // echo

} // bl

#endif /* __APPS_BLMESSAGINGECHOSERVER_MESSAGINGECHOSERVERCMDLINE_H_ */
