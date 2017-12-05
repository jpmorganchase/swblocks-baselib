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

#ifndef __BL_APPS_BL_TOOL_COMMANDS_GENERATEUUIDS_H_
#define __BL_APPS_BL_TOOL_COMMANDS_GENERATEUUIDS_H_

#include <apps/bl-tool/GlobalOptions.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class GenerateUuids - a simple command to generate UUID definitions
         */

        template
        <
            typename E = void
        >
        class GenerateUuidsT : public bl::cmdline::CommandBase
        {
            const GlobalOptions& m_globalOptions;

        public:

            BL_CMDLINE_OPTION(
                m_count,
                IntOption,
                "count",
                "The number of UUID definitions to be generated (default is 1)",
                1 /* default */
                )

            GenerateUuidsT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "uuids", "bl-tool @FULLNAME@ [options]" ),
                m_globalOptions( globalOptions )
            {
                addOption( m_count );

                setHelpMessage(
                    "Generate a list of UUID definitions.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                const auto count = m_count.getValue();

                for( int i = 0; i < count; ++i )
                {
                    BL_LOG(
                        bl::Logging::notify(),
                        BL_MSG()
                            << bl::uuids::generateUuidDef( bl::uuids::create() )
                        );
                }

                return 0;
            }
        };

        typedef GenerateUuidsT<> GenerateUuids;

    } // commands

} // bltool

#endif /* __BL_APPS_BL_TOOL_COMMANDS_GENERATEUUIDS_H_ */
