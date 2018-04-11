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

#include <baselib/crypto/TrustedRoots.h>

#include <baselib/core/FsUtils.h>
#include <baselib/core/FileEncoding.h>
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

            BL_CMDLINE_OPTION( m_host,      StringOption,       "host,h",       "The host name",        bl::cmdline::Required )
            BL_CMDLINE_OPTION( m_path,      StringOption,       "path",         "The relative path",    bl::cmdline::Required )
            BL_CMDLINE_OPTION( m_port,      UShortOption,       "port,p",       "The port (optional)" )
            BL_CMDLINE_OPTION( m_isSsl,     BoolSwitch,         "ssl,s",        "Use SSL connection" )
            BL_CMDLINE_OPTION( m_out,       StringOption,       "out,o",        "Output file to store the response" )
            BL_CMDLINE_OPTION( m_in,        StringOption,       "in,i",         "Input file to load the request from" )
            BL_CMDLINE_OPTION( m_method,    StringOption,       "method,m",     "Method - GET/PUT/POST/DELETE (default: GET)", "GET" /* default */ )
            BL_CMDLINE_OPTION( m_cookies,   StringOption,       "cookies,c",    "The request cookies" )
            BL_CMDLINE_OPTION( m_headers,   MultiStringOption,  "headers",      "Custom headers in format: name1=value1 name2=value2" )
            BL_CMDLINE_OPTION( m_agent,     StringOption,       "agent,a",      "The user agent string to use" )

            BL_CMDLINE_OPTION(
                m_verifyRootCA,
                StringOption,
                "verify-root-ca",
                "An additional root CA to be used"
                )

            HttpRequestT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "request", "bl-tool @FULLNAME@ [options]" ),
                m_globalOptions( globalOptions )
            {
                addOption(
                    m_host,
                    m_path,
                    m_port,
                    m_isSsl,
                    m_out
                    );

                addOption(
                    m_in,
                    m_method,
                    m_cookies,
                    m_headers,
                    m_agent,
                    m_verifyRootCA
                    );

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
            auto executeHttpRequest( SAA_in const bl::os::port_t port ) -> std::string
            {
                using namespace bl;
                using namespace bl::http;
                using namespace bl::tasks;

                std::string response;

                scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                    {
                        auto content =
                            m_in.hasValue() ? encoding::readTextFile( m_in.getValue() ) : std::string();

                        HeadersMap headers;

                        if ( m_headers.hasValue() )
                        {
                            headers = bl::str::parsePropertiesList( bl::cpp::copy( m_headers.getValue() ) );
                        }

                        if( m_cookies.hasValue() )
                        {
                            headers[ Parameters::HttpHeader::g_cookie ] = m_cookies.getValue();
                        }

                        if( ! content.empty() )
                        {
                            /*
                             * We assume the content is UTF8 encoded text
                             */

                            headers[ Parameters::HttpHeader::g_contentType ] =
                                Parameters::HttpHeader::g_contentTypePlainTextUtf8;
                        }

                        bool restoreUserAgent = false;
                        std::string oldUserAgent;

                        if( m_agent.hasValue() )
                        {
                            oldUserAgent = bl::http::Parameters::userAgentDefault();
                            bl::http::Parameters::userAgentDefault( bl::cpp::copy( m_agent.getValue() ) );
                            restoreUserAgent = true;
                        }

                        BL_SCOPE_EXIT(
                            {
                                if( restoreUserAgent )
                                {
                                    bl::http::Parameters::userAgentDefault( std::move( oldUserAgent ) );
                                }
                            }
                            );

                        const auto taskImpl = IMPL::template createInstance(
                            cpp::copy( m_host.getValue() ),
                            port,
                            str::uriEncodeUnsafeOnly( m_path.getValue(), false /* escapePercent */ ),
                            m_method.getValue( "GET" ) /* action */,
                            std::move( content ),
                            std::move( headers )
                            );

                        taskImpl -> isExpectUtf8Content( true );

                        eq -> push_back( om::qi< Task >( taskImpl ) );
                        eq -> flush();

                        taskImpl -> getResponseLvalue().swap( response );
                    });

                return response;
            }

            void saveResponse(
                SAA_in              const std::string&                  response,
                SAA_inout           std::ostream&                       os
                )
            {
                bl::cpp::SafeInputStringStream is( response );

                std::string line;

                while( is.good() )
                {
                    std::getline( is, line );

                    os << line << std::endl;
                }
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;
                using namespace bl::http;
                using namespace bl::tasks;

                const bool isSsl = m_isSsl.hasValue() ? true : ( 443U == m_port.getValue() );

                const auto port = m_port.hasValue() ? m_port.getValue() : ( isSsl ? 443U : 80U );

                if( m_verifyRootCA.hasValue() )
                {
                    /*
                     * An additional root CA was provided on command line (to be used / registered)
                     */

                    const fs::path verifyRootCAPath = m_verifyRootCA.getValue();

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

                const auto response = isSsl ?
                    executeHttpRequest< SimpleHttpSslTaskImpl >( port )
                    :
                    executeHttpRequest< SimpleHttpTaskImpl >( port );

                if( m_out.hasValue() )
                {
                    fs::SafeOutputFileStreamWrapper file( m_out.getValue() );
                    saveResponse( response, file.stream() );
                }
                else
                {
                    saveResponse( response, std::cout );
                }

                return 0;
            }
        };

        typedef HttpRequestT<> HttpRequest;

    } // commands

} // bltool

#endif /* __BL_APPS_BL_TOOL_COMMANDS_HTTPREQUEST_H_ */
