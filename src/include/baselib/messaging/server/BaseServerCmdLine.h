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

#ifndef __BL_MESSAGING_SERVER_BASESERVERCMDLINE_H_
#define __BL_MESSAGING_SERVER_BASESERVERCMDLINE_H_

#include <baselib/messaging/server/BaseServerPorts.h>

#include <baselib/cmdline/CmdLineBase.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The base server command line parser
         */

        template
        <
            typename E = void
        >
        class BaseServerCmdLineT : public cmdline::CmdLineBase
        {
        public:

            cmdline::HelpSwitch m_help;

            BL_CMDLINE_OPTION(
                m_peerId,
                StringOption,
                "peer-id",
                "The server peer id (must be UUID)",
                cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_brokerEndpoints,
                MultiStringOption,
                "broker-endpoints",
                "The list of messaging broker endpoints to be used",
                cmdline::RequiredMultiValue
                )

            BL_CMDLINE_OPTION(
                m_connections,
                UShortOption,
                "connections",
                "The number of connections to messaging broker to be used by the forwarding backend",
                16U /* The default value */
                )

            BL_CMDLINE_OPTION(
                m_verifyRootCA,
                StringOption,
                "verify-root-ca",
                "An additional root CA to be used"
                )

            BL_CMDLINE_OPTION(
                m_jarBasePath,
                StringOption,
                "jar-base-path",
                "Path to directory containing server main JAR(s) and dependent JAR files in 'lib' sub-directory",
                bl::cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_jvmDebugPort,
                UShortOption,
                "jvm-debug-port",
                "Start JVM in debug mode, listening on the specified port",
                0U /* The default value */
                )

            BaseServerCmdLineT(
                SAA_in          std::string&&                       serverName,
                SAA_in          const bool                          enableJvm
                )
                :
                cmdline::CmdLineBase(
                    resolveMessage(
                        BL_MSG()
                            << "Usage: "
                            << serverName
                            << " [options]"
                        )
                    )
            {
                addOption(
                    m_help,
                    m_peerId,
                    m_brokerEndpoints,
                    m_connections,
                    m_verifyRootCA
                    );

                if( enableJvm )
                {
                    addOption( m_jarBasePath, m_jvmDebugPort );
                }
            }
        };

        typedef BaseServerCmdLineT<> BaseServerCmdLine;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_SERVER_BASESERVERCMDLINE_H_ */
