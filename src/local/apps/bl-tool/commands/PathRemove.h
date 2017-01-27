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

#ifndef __APPS_BLTOOL_COMMANDS_PATHREMOVE_H_
#define __APPS_BLTOOL_COMMANDS_PATHREMOVE_H_

#include <apps/bl-tool/GlobalOptions.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/core/BaseIncludes.h>
#include <baselib/core/FsUtils.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class PathRemove
         */

        template
        <
            typename E = void
        >
        class PathRemoveT : public bl::cmdline::CommandBase
        {
            const GlobalOptions& m_globalOptions;

        public:

            BL_CMDLINE_OPTION( m_path,  StringOption,   PathName,   PathDesc,   bl::cmdline::Required )
            BL_CMDLINE_OPTION( m_force, BoolSwitch, "force,f", "Ignore non-existent paths" )

            PathRemoveT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "remove", "bl-tool @FULLNAME@ [options]" ),
                m_globalOptions( globalOptions )
            {
                addOption( m_path );
                addOption( m_force );

                setHelpMessage(
                    "Safely and consistently remove a path.\n"
                    "Will remove paths with long file names regardless of platform.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                auto path = bl::fs::nolfn::path( m_path.getValue() );

                path.make_preferred();

                if ( m_force.getValue() )
                {
                    bl::fs::safeRemoveAllIfExists( path );
                }
                else
                {
                    bl::fs::safeRemoveAll( path );
                }

                return 0;
            }
        };

        typedef PathRemoveT<> PathRemove;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_PATHREMOVE_H_ */
