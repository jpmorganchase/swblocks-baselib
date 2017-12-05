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

#ifndef __APPS_BLTOOL_COMMANDS_PATH_H_
#define __APPS_BLTOOL_COMMANDS_PATH_H_

#include <baselib/core/BaseIncludes.h>

#include <baselib/cmdline/CommandBase.h>

#include <apps/bl-tool/GlobalOptions.h>
#include <apps/bl-tool/commands/PathRemove.h>
#include <apps/bl-tool/commands/PathSpaceUsed.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class Path
         */

        template
        <
            typename E = void
        >
        class PathT : public bl::cmdline::CommandBase
        {
            PathRemove                  m_pathRemove;
            PathSpaceUsed               m_pathSpaceUsed;

        public:

            PathT(
                SAA_inout       bl::cmdline::CommandBase*           parent,
                SAA_in          const GlobalOptions&                globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "path" ),
                m_pathRemove( this, globalOptions ),
                m_pathSpaceUsed( this, globalOptions )
            {
                setHelpMessage(
                    "Usage: @CAPTION@\n"
                    "Supported sub-commands:\n"
                    "@COMMANDS@\n"
                    );
            }
        };

        typedef PathT<> Path;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_PATH_H_ */
