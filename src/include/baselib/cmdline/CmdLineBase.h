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

#ifndef __BL_CMDLINE_CMDLINEBASE_H_
#define __BL_CMDLINE_CMDLINEBASE_H_

#include <baselib/cmdline/CommandBase.h>
#include <baselib/core/ProgramOptions.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace cmdline
    {
        /**
         * @brief class CmdLineBase - The command line parser base
         *
         * To be extended by the client application
         */

        template
        <
            typename E = void
        >
        class CmdLineBaseT : public CommandBase
        {
        protected:

            po::variables_map                       m_parsedArgs;

            /**
             * @brief Command line option styles
             *
             * e.g. -short, --long, -long, case insensitive, etc
             */

            po::command_line_style::style_t         m_style;

            int                                     m_columns;

        public:

            /**
             * @brief Command line parser constructor.
             *
             * @param helpCaption   the help message caption
             */

            CmdLineBaseT( SAA_inout std::string&& helpCaption )
                :
                CommandBase( BL_PARAM_FWD( helpCaption ) )
            {
                using namespace po::command_line_style;

                /*
                 * Allow "-short" and "--long" options only
                 */

                m_style = unix_style;

                int rowsDummy;
                if( ! os::getConsoleSize( m_columns, rowsDummy ) )
                {
                    /*
                     * The application's output is being redirected to a file or via a pipe,
                     * or we are running without a console. Assume the default console size.
                     */

                    m_columns = po::options_description::m_default_line_length;
                }
            }

            CommandBase* parseCommandLine(
                SAA_in                      const std::size_t           argc,
                SAA_in_ecount( argc )       const char* const           argv[]
                )
            {
                /*
                 * Create a vector from argv[ 1 .. argc ], essentially ignoring
                 * the program name in argv[ 0 ]
                 */

                std::vector< std::string > args( &argv[ 1 ], &argv[ argc == 0 ? 1 : argc ] );

                return parseCommandLine( args );
            }

            CommandBase* parseCommandLine( SAA_in const std::string& commandLine )
            {
#ifdef _WIN32
                auto args = po::split_winmain( commandLine );
#else
                auto args = po::split_unix( commandLine );
#endif
                return parseCommandLine( args );
            }

            /**
             * @brief Parse command line arguments
             *
             * @throws Exceptions derived from po::error if the command line arguments cannot
             * be parsed. These include (among others):<p>
             *  - unknown_option        if the option name is not recognized.<p>
             *  - ambiguous_option      if the (possibly abbreviated) option name matches
             *                          two or more option definitions or if there exist
             *                          duplicate option definitions.<p>
             *  - invalid_option_value  if the option value cannot be parsed or is out of range.<p>
             *  - multiple_occurrences  if the option is specified more than once.<p>
             *  - too_many_positional_options_error if superfluous positional arguments are found.<p>
             *  - UserMessageException  if the command line specifies an invalid command name or
             *                          if any of the required options are missing.
             *
             * @note The vector of arguments gets modified.
             */

            CommandBase* parseCommandLine( SAA_inout std::vector< std::string >& args )
            {
                m_parsedArgs.clear();

                CommandBase* command = parseCommandNames( args );

                cpp::SafeUniquePtr< po::options_description >           optionDesc;
                po::positional_options_description                      positionalDesc;

                command -> getOptionDescriptionHierarchy( optionDesc, positionalDesc );

                po::store(
                    po::command_line_parser( args ).
                    options( *optionDesc ).
                    positional( positionalDesc ).
                    style( m_style ).
                    run(),
                    m_parsedArgs
                );

                po::notify( m_parsedArgs );

                checkRequiredOptions( command );
                checkNonApplicableOptions( command );

                return command;
            }

            virtual Result executeCommand( CommandBase* command )
            {
                if( command != nullptr )
                {
                    return command -> execute();
                }

                return -1;
            }

            const po::variables_map& getParsedArgs() const NOEXCEPT
            {
                return m_parsedArgs;
            }

        protected:

            virtual unsigned getScreenColumns() const OVERRIDE
            {
                return m_columns;
            }

        private:

            CommandBase* parseCommandNames( SAA_inout std::vector< std::string >& args )
            {
                CommandBase* command = this;
                std::size_t index = 0;

                while( index < args.size() && command -> hasCommands() )
                {
                    const auto& name = args[ index ];

                    if( ! name.empty() && name[ 0 ] == '-' )
                    {
                        /*
                         * This is an option, not a command name. Stop looking here.
                         */

                        break;
                    }

                    auto subCmd = command -> findCommand( name );

                    BL_CHK_T(
                        false,
                        subCmd != nullptr,
                        UserMessageException(),
                        BL_MSG()
                            << "Invalid command: '"
                            << name
                            << "'"
                        );

                    command = subCmd;
                    ++index;
                }

                /*
                 * Remove the parsed command name(s) from the front of the command line
                 */

                args.erase( args.begin(), args.begin() + index );

                return command;
            }

            static bool checkRequiredOptions( SAA_in const CommandBase* command )
            {
                std::vector< OptionBase* > missing;
                bool override = false;

                if( command != nullptr )
                {
                    command -> getMissingOptions( missing, override );
                }

                if( ! override && ! missing.empty() )
                {
                    BL_THROW_USER(
                        BL_MSG()
                            << "Invalid command line: "
                            << ( missing.size() == 1 ? "the option " : "the options " )
                            << str::joinFormatted< OptionBase* >( missing, ", ", " and ", &optionFormatter )
                            << ( missing.size() == 1 ? " is required but missing" : " are required but missing" )
                        );
                }

                return override;
            }

            virtual void checkNonApplicableOptions( SAA_in const CommandBase* /* command */ ) const
            {
            }

            static void optionFormatter(
                SAA_inout   std::ostream&               stream,
                SAA_in      const OptionBase*           option
                )
            {
                stream << "'" << option -> getDisplayName() << "'";
            }
        };

        typedef CmdLineBaseT<> CmdLineBase;

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_CMDLINEBASE_H_ */
