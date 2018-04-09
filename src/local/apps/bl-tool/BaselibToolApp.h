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

#ifndef __APPS_BLTOOL_BASELIBTOOLAPP_H_
#define __APPS_BLTOOL_BASELIBTOOLAPP_H_

#include <apps/bl-tool/CmdLine.h>

#include <baselib/cmdline/CmdLineAppBase.h>

#include <baselib/crypto/TrustedRoots.h>

#include <baselib/http/Globals.h>

#include <baselib/core/AppInitDone.h>
#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    /**
     * @brief class BaselibToolApp
     */

    template
    <
        typename E = void
    >
    class BaselibToolAppT
    {
        BL_DECLARE_STATIC( BaselibToolAppT )

        /**
         * @brief class BaselibToolAppImpl
         */

        template
        <
            typename E2 = void
        >
        class BaselibToolAppImplT : public bl::cmdline::CmdLineAppBase< BaselibToolAppImplT< E2 > >
        {
            BL_CTR_DEFAULT( BaselibToolAppImplT, public )
            BL_NO_COPY_OR_MOVE( BaselibToolAppImplT )

        public:

            typedef bl::cmdline::CmdLineAppBase< BaselibToolAppImplT< E2 > > base_type;

            void appMain(
                SAA_in                      std::size_t                 argc,
                SAA_in_ecount( argc )       const char* const*          argv
                )
            {
                CmdLine cmdLine;
                bl::cmdline::CommandBase* command = nullptr;

                try
                {
                    command = cmdLine.parseCommandLine( argc, argv );
                }
                catch ( bl::po::error& e )
                {
                    BL_LOG(
                        bl::Logging::error(),
                        BL_MSG()
                            << "Invalid command line: "
                            << e.what()
                        );

                    return;
                }
                catch ( bl::UserMessageException& e )
                {
                    BL_LOG(
                        bl::Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    return;
                }

                bl::Logging::setLevel(
                    cmdLine.getGlobalOptions().m_debug.getValue()
                        ? static_cast< int >( bl::Logging::LL_DEBUG )
                        : bl::Logging::LL_DEFAULT
                    );

                cmdLine.executeCommand( command );
            }
        };

        typedef BaselibToolAppImplT<> BaselibToolAppImpl;

    public:

        static int main(
            SAA_in                          std::size_t                 argc,
            SAA_in_ecount( argc )           const char* const*          argv
            )
        {
            bl::crypto::initGlobalTrustedRootsCallback( &bl::crypto::detail::TrustedRoots::initAllGlobalTrustedRoots );

            /*
             * Make sure to configure the default user agent string for the HTTP client to be the "bot" version
             * so the 'http request' command is not blocked by some web sites which expect either well known
             * user agents / browsers or private bot clients only
             */

            bl::http::Parameters::userAgentDefault( bl::cpp::copy( bl::http::HttpHeader::g_userAgentBotDefault ) );

            BaselibToolAppImpl app;

            return app.main( argc, argv );
        }
    };

    typedef BaselibToolAppT<> BaselibToolApp;

} // bltool

#endif /* __APPS_BLTOOL_BASELIBTOOLAPP_H_ */
