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

#ifndef __BL_APPS_BL_TOOL_COMMANDS_HTTPREQUEST_H_
#define __BL_APPS_BL_TOOL_COMMANDS_HTTPREQUEST_H_

#include <apps/bl-tool/GlobalOptions.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/http/SimpleHttpSslTask.h>
#include <baselib/http/SimpleHttpTask.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/Algorithms.h>

#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class HttpRequest - an HTTP request command
         */

        template
        <
            typename E = void
        >
        class HttpRequestT : public bl::cmdline::CommandBase
        {
            const GlobalOptions& m_globalOptions;

        public:

            BL_CMDLINE_OPTION( m_host,  StringOption,   "host,h",   "The host name",        bl::cmdline::Required )
            BL_CMDLINE_OPTION( m_path,  StringOption,   "path",     "The relative path",    bl::cmdline::Required )
            BL_CMDLINE_OPTION( m_port,  UShortOption,   "port,p",   "The port (optional)",  80U /* defaultValue */ )
            BL_CMDLINE_OPTION( m_isSsl, BoolSwitch,     "ssl,s",    "Use SSL connection" )

            HttpRequestT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "request", "bl-tool @FULLNAME@ [options]" ),
                m_globalOptions( globalOptions )
            {
                addOption( m_host );
                addOption( m_path );
                addOption( m_port );
                addOption( m_isSsl );

                setHelpMessage(
                    "Execute HTTP request and return the result.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            template
            <
                typename IMPL
            >
            auto executeHttpRequest() -> std::string
            {
                using namespace bl;
                using namespace bl::http;
                using namespace bl::tasks;

                std::string response;

                scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                    {
                        const auto taskImpl = IMPL::template createInstance(
                            cpp::copy( m_host.getValue() ),
                            m_port.getValue(),
                            m_path.getValue(),
                            "GET"               /* action */,
                            ""                  /* content */,
                            HeadersMap()        /* requestHeaders */
                            );

                        eq -> push_back( om::qi< Task >( taskImpl ) );
                        eq -> flush();

                        taskImpl -> getResponseLvalue().swap( response );
                    });

                return response;
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;
                using namespace bl::http;
                using namespace bl::tasks;

                const bool isSsl = m_isSsl.hasValue() ? true : ( 443U == m_port.getValue() );

                const auto response = isSsl ?
                    executeHttpRequest< SimpleHttpSslTaskImpl >()
                    :
                    executeHttpRequest< SimpleHttpTaskImpl >();

                cpp::SafeInputStringStream is( response );

                std::string line;

                while( is.good() )
                {
                    std::getline( is, line );

                    std::cout << line;
                }

                return 0;
            }
        };

        typedef HttpRequestT<> HttpRequest;

    } // commands

} // bltool

#endif /* __BL_APPS_BL_TOOL_COMMANDS_HTTPREQUEST_H_ */
