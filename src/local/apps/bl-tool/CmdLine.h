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

#ifndef __APPS_BLTOOL_CMDLINE_H_
#define __APPS_BLTOOL_CMDLINE_H_

#include <baselib/cmdline/CmdLineBase.h>

#include <apps/bl-tool/CmdLineStrings.h>
#include <apps/bl-tool/commands/Crypto.h>
#include <apps/bl-tool/commands/Path.h>
#include <apps/bl-tool/GlobalOptions.h>

namespace bltool
{
    /**
     * @brief class CmdLine
     */

    template
    <
        typename E = void
    >
    class CmdLineT : public bl::cmdline::CmdLineBase
    {
        GlobalOptions                       m_globalOptions;
        commands::Crypto                    m_crypto;
        commands::Path                      m_path;

    public:

        CmdLineT()
            :
            bl::cmdline::CmdLineBase( "bl-tool <commands> [options]" ),
            m_crypto( this, m_globalOptions ),
            m_path( this, m_globalOptions )
        {
            addOption(
                m_globalOptions.m_help,
                m_globalOptions.m_verbose,
                m_globalOptions.m_debug
                );

            setHelpMessage(
                "Usage: @CAPTION@\n"
                "Supported commands:\n"
                "@COMMANDS@\n"
                "Global Options:\n"
                "@OPTIONS@\n"
                );
        }

        virtual bl::cmdline::Result execute() OVERRIDE
        {
            BL_STDIO_TEXT(
                {
                    /*
                     * Display a brief help screen by default only
                     */

                    helpMessage( std::cout, false /* includeSubCommands */ );
                }
                );

            return 0;
        }

        virtual bl::cmdline::Result executeCommand( SAA_in bl::cmdline::CommandBase* command ) OVERRIDE
        {
            if( m_globalOptions.m_help.getValue() )
            {
                BL_STDIO_TEXT( { command -> helpMessage( std::cout ); } );

                return 0;
            }
            else
            {
                return command -> execute();
            }
        }

        const GlobalOptions& getGlobalOptions() const NOEXCEPT
        {
            return m_globalOptions;
        }
    };

    typedef CmdLineT<> CmdLine;

} // bltool

#endif /* __APPS_BLTOOL_CMDLINE_H_ */
