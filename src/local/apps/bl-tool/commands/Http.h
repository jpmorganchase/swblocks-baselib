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

#ifndef __BL_APPS_BL_TOOL_COMMANDS_HTTP_H_
#define __BL_APPS_BL_TOOL_COMMANDS_HTTP_H_

#include <baselib/core/BaseIncludes.h>

#include <baselib/cmdline/CommandBase.h>

#include <apps/bl-tool/GlobalOptions.h>
#include <apps/bl-tool/commands/HttpRequest.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class Http - the command group for HTTP related commands
         */

        template
        <
            typename E = void
        >
        class HttpT : public bl::cmdline::CommandBase
        {
            HttpRequest                 m_httpRequest;

        public:

            HttpT(
                SAA_inout       bl::cmdline::CommandBase*           parent,
                SAA_in          const GlobalOptions&                globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "http" ),
                m_httpRequest( this, globalOptions )
            {
                setHelpMessage(
                    "Usage: @CAPTION@\n"
                    "Supported sub-commands:\n"
                    "@COMMANDS@\n"
                    );
            }
        };

        typedef HttpT<> Http;

    } // commands

} // bltool

#endif /* __BL_APPS_BL_TOOL_COMMANDS_HTTP_H_ */
