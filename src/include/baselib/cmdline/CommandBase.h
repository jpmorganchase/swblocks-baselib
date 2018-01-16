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

#ifndef __BL_CMDLINE_COMMANDBASE_H_
#define __BL_CMDLINE_COMMANDBASE_H_

#include <baselib/core/BaseIncludes.h>
#include <baselib/core/Logging.h>
#include <baselib/core/Utils.h>
#include <baselib/core/FsUtils.h>
#include <baselib/cmdline/Option.h>
#include <baselib/cmdline/Result.h>

#include <map>
#include <unordered_map>

namespace bl
{
    namespace cmdline
    {
        /**
         * @brief class CommandBase - command mode base class
         *
         * All commands requiring special command line options will need to derive from this class
         */

        template
        <
            typename E = void
        >
        class CommandBaseT
        {
            BL_NO_COPY_OR_MOVE( CommandBaseT )

        public:

            typedef CommandBaseT< E >                               this_type;

        protected:

            const cpp::ScalarTypeIniter< this_type* >               m_parent;
            const std::string                                       m_name;
            const std::string                                       m_helpCaption;
            std::string                                             m_helpMessage;

            std::map< std::string, this_type* >                     m_commandsLookup;
            std::vector< this_type* >                               m_commands;

            std::unordered_map< std::string, OptionBase* >          m_optionsLookup;
            std::vector< OptionBase* >                              m_options;

        protected:

            /**
             * @brief Global level command constructor.
             *
             * All commands may have distinct help screen captions.
             */

            CommandBaseT( SAA_inout_opt std::string&& helpCaption = "" )
                :
                m_helpCaption( BL_PARAM_FWD( helpCaption ) )
            {
            }

            /**
             * @brief Nested command constructor.
             *
             * All commands at the same level must have unique names and they may have
             * distinct help screen captions.
             */

            CommandBaseT(
                SAA_inout       this_type*          parent,
                SAA_inout       std::string&&       commandName,
                SAA_inout_opt   std::string&&       helpCaption = ""
                )
                :
                m_parent( parent ),
                m_name( BL_PARAM_FWD( commandName ) ),
                m_helpCaption( BL_PARAM_FWD( helpCaption ) )
            {
                BL_ASSERT( m_parent != nullptr );
                BL_ASSERT( ! m_name.empty() );

                parent -> addCommand( *this );
            }

        protected:

            virtual ~CommandBaseT() NOEXCEPT
            {
                if( m_parent )
                {
                    getParent() -> removeCommand( *this );
                }
            }

        public:

            const std::string& getCommandName() const NOEXCEPT
            {
                return m_name;
            }

            std::string getFullName() const
            {
                std::string name( m_name );

                for( this_type* cmd = m_parent; cmd != nullptr; cmd = cmd -> m_parent )
                {
                    const auto length = cmd -> m_name.length();
                    if( length != 0 )
                    {
                        name.insert( 0, length + 1, ' ' );
                        name.replace( 0, length, cmd -> m_name );
                    }
                }

                return name;
            }

            std::string getHelpCaption() const
            {
                return formatMessage( m_helpCaption );
            }

            void setHelpMessage( SAA_inout std::string&& message )
            {
                m_helpMessage = BL_PARAM_FWD( message );
            }

            auto getParent() const NOEXCEPT -> this_type*
            {
                return m_parent;
            }

            auto getRootCommand() const NOEXCEPT -> const this_type*
            {
                if( m_parent.value() )
                {
                    return m_parent.value() -> getRootCommand();
                }
                else
                {
                    return this;
                }
            }

            bool hasCommands() const NOEXCEPT
            {
                return ! m_commands.empty();
            }

            void addCommand( SAA_in this_type& command )
            {
                const auto result = m_commandsLookup.insert( std::make_pair( command.getCommandName(), &command ) );

                if( ! result.second )
                {
                    BL_THROW(
                        UnexpectedException(),
                        BL_MSG()
                            << "Duplicate command: '"
                            << command.getCommandName()
                            << "'"
                        );
                }

                m_commands.push_back( &command );
                BL_ASSERT( m_commands.size() == m_commandsLookup.size() );
            }

            auto findCommand( SAA_in const std::string& name ) const NOEXCEPT -> this_type*
            {
                const auto iter = m_commandsLookup.find( name );
                return iter == m_commandsLookup.end() ? nullptr : iter -> second;
            }

            void removeCommand( SAA_in const this_type& command ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( m_commandsLookup.erase( command.getCommandName() ) != 0 )
                {
                    /*
                     * Destructors are called in strictly reverse order, hence it's most likely
                     * that the command being removed will be right at the end.
                     */

                    const auto iter = utils::find_last( m_commands.begin(), m_commands.end(), &command );
                    if( iter != m_commands.end() )
                    {
                        m_commands.erase( iter );
                    }
                }

                BL_ASSERT( m_commands.size() == m_commandsLookup.size() );

                BL_NOEXCEPT_END()
            }

            /**
             * @brief Add option value holder class instance
             *
             * @note Note that the option is passed by a reference and it remains owned by the caller
             *
             * Since this class keeps a pointer to the option and the option's value is injected directly
             * by the command line parser, the option class instance must be valid all the time while
             * this class is being used. Options are intended to be member variables of a class derived
             * from this one. It is not advisable to create options directly on the stack or on the heap.
             */

            void addOption( SAA_in OptionBase& option )
            {
                const auto result = m_optionsLookup.insert( std::make_pair( option.getName(), &option ) );

                if( ! result.second )
                {
                    BL_THROW(
                        UnexpectedException(),
                        BL_MSG()
                            << "Duplicate command line option: '"
                            << option.getName()
                            << "'"
                        );
                }

                /*
                 * Remember the order in which the options were added
                 */

                m_options.push_back( &option );
                BL_ASSERT( m_options.size() == m_optionsLookup.size() );
            }

            auto findOption(
                SAA_in      const std::string&  name,
                SAA_in_opt  bool                lookupParent = false
                ) const NOEXCEPT -> OptionBase*
            {
                const auto iter = m_optionsLookup.find( name );
                OptionBase* result = iter == m_optionsLookup.end() ? nullptr : iter -> second;

                if( ! result && lookupParent && m_parent.value() )
                {
                    result = m_parent.value() -> findOption( name, lookupParent );
                }

                return result;
            }

            void removeOption( SAA_in const OptionBase& option ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( m_optionsLookup.erase( option.getName() ) != 0 )
                {
                    const auto iter = utils::find_last( m_options.begin(), m_options.end(), &option );
                    if( iter != m_options.end() )
                    {
                        m_options.erase( iter );
                    }
                }

                BL_ASSERT( m_options.size() == m_optionsLookup.size() );

                BL_NOEXCEPT_END()
            }

            auto getAllOptions() const NOEXCEPT -> const std::vector< OptionBase* >&
            {
                return m_options;
            }

            /**
             * @brief Convenience methods to add multiple option class instances in one call.
             */

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2
                )
            {
                addOption( option1 );
                addOption( option2 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4,
                SAA_in      OptionBase&         option5
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
                addOption( option5 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4,
                SAA_in      OptionBase&         option5,
                SAA_in      OptionBase&         option6
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
                addOption( option5 );
                addOption( option6 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4,
                SAA_in      OptionBase&         option5,
                SAA_in      OptionBase&         option6,
                SAA_in      OptionBase&         option7
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
                addOption( option5 );
                addOption( option6 );
                addOption( option7 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4,
                SAA_in      OptionBase&         option5,
                SAA_in      OptionBase&         option6,
                SAA_in      OptionBase&         option7,
                SAA_in      OptionBase&         option8
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
                addOption( option5 );
                addOption( option6 );
                addOption( option7 );
                addOption( option8 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4,
                SAA_in      OptionBase&         option5,
                SAA_in      OptionBase&         option6,
                SAA_in      OptionBase&         option7,
                SAA_in      OptionBase&         option8,
                SAA_in      OptionBase&         option9
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
                addOption( option5 );
                addOption( option6 );
                addOption( option7 );
                addOption( option8 );
                addOption( option9 );
            }

            void addOption(
                SAA_in      OptionBase&         option1,
                SAA_in      OptionBase&         option2,
                SAA_in      OptionBase&         option3,
                SAA_in      OptionBase&         option4,
                SAA_in      OptionBase&         option5,
                SAA_in      OptionBase&         option6,
                SAA_in      OptionBase&         option7,
                SAA_in      OptionBase&         option8,
                SAA_in      OptionBase&         option9,
                SAA_in      OptionBase&         option10
                )
            {
                addOption( option1 );
                addOption( option2 );
                addOption( option3 );
                addOption( option4 );
                addOption( option5 );
                addOption( option6 );
                addOption( option7 );
                addOption( option8 );
                addOption( option9 );
                addOption( option10 );
            }

            /**
             * @brief Execute the command
             *
             * The command line has been successfully parsed and validated.
             * You are free to do anything you want to here, even throw exceptions.
             *
             * @note Always override this method. It prints the help screen by default.
             */

            virtual Result execute()
            {
                BL_STDIO_TEXT(
                    {
                        helpMessage( std::cout );
                    }
                    );

                return Result();
            }

            /**
             * @brief Commands can have various application-specific attributes
             */

            virtual bool getBoolAttribute( SAA_in int /* attributeId */ )
            {
                return false;
            }

            /**
             * @brief Output the command usage message(s) to a stream
             */

            bool helpMessage(
                SAA_inout   std::ostream&       stream,
                SAA_in_opt  const bool          includeSubCommands = true
                ) const
            {
                const auto msg = formatHelpMessage();
                stream << msg;

                auto lastNotEmpty = ! msg.empty();
                auto helpNotEmpty = lastNotEmpty;

                if( includeSubCommands )
                {
                    /*
                     * Walk through the (sub)commands in the order they were added.
                     * Add line breaks between (sub)command help screens unless they are empty.
                     */

                    for( const auto& command : m_commands )
                    {
                        if( lastNotEmpty )
                        {
                            stream << std::endl;
                        }

                        lastNotEmpty = command -> helpMessage( stream, includeSubCommands );
                        helpNotEmpty |= lastNotEmpty;
                    }
                }

                return helpNotEmpty;
            }

            /**
             * @brief Get the command usage message(s) as a string.
             */

            std::string helpMessage( SAA_in_opt const bool includeSubCommands = true ) const
            {
                cpp::SafeOutputStringStream oss;

                helpMessage( oss, includeSubCommands );

                return oss.str();
            }

            /**
             * @brief Aggregate option descriptions in the command hierarchy from the root
             * down to this particular command.
             *
             * This order is vital for positional options.
             */

            void getOptionDescriptionHierarchy(
                SAA_inout   cpp::SafeUniquePtr< po::options_description >&              options,
                SAA_inout   po::positional_options_description&                         positional
                ) const
            {
                if( m_parent == nullptr )
                {
                    options = getOptionDescriptions();
                }
                else
                {
                    getParent() -> getOptionDescriptionHierarchy( options, positional );

                    options -> add( *getOptionDescriptions() );
                }

                getPositionalDescriptions( positional );
            }

            void getMissingOptions(
                SAA_inout   std::vector< OptionBase* >&     missing,
                SAA_inout   bool&                           override
                ) const
            {
                if( m_parent != nullptr )
                {
                    getParent() -> getMissingOptions( missing, override );
                }

                for( const auto& option : m_options )
                {
                    if( ! option -> hasValue() )
                    {
                        if ( option -> isRequired() )
                        {
                            missing.push_back( option );
                        }
                    }
                    else
                    {
                        if( option -> isOverride() )
                        {
                            override = true;
                        }
                    }
                }
            }

            virtual bool isDryRunApplicable() const
            {
                return true;
            }

            static void loadListFile(
                SAA_inout   std::vector< std::string >&     result,
                SAA_in      const std::string&              filename
                )
            {
                if( filename.empty() )
                {
                    BL_THROW_USER(
                        "List @file name must be specified"
                        );
                }

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Loading list file "
                        << filename
                    );

                fs::SafeInputFileStreamWrapper file( filename );
                auto& is = file.stream();

                int lineNo = 1;
                std::string line;
                line.reserve( 50 );

                while( std::getline( is, line ) )
                {
                    str::trim( line );

                    if( ! line.empty() )
                    {
                         result.emplace_back( line );
                    }

                    ++lineNo;
                }

                /*
                 * getline() sets the failbit if the file ends in an empty line, ignore the error
                 */

                if( is.fail() && ! is.eof() )
                {
                    BL_THROW_USER(
                        BL_MSG()
                            << "Failed to read list file '"
                            << filename
                            << "' at line number "
                            << lineNo
                        );
                }
            }

            static std::vector< std::string > expandList( SAA_in const std::vector< std::string >& values )
            {
                std::vector< std::string > result;
                result.reserve( values.size() );

                /*
                 * Replace all @<file> entries with the contents of the file(s)
                 */

                for( const auto& value : values )
                {
                    if( value.empty() )
                    {
                        BL_THROW_USER(
                            "List may not contain empty values"
                            );
                    }

                    if( value[ 0 ] == '@' )
                    {
                        loadListFile( result, value.substr( 1 ) );
                    }
                    else
                    {
                        result.push_back( value );
                    }
                }

                return result;
            }

        protected:

            virtual std::string formatHelpMessage() const
            {
                std::string msg;

                if( ! m_helpMessage.empty() )
                {
                    msg = m_helpMessage;
                }
                else
                {
                    if( ! m_helpCaption.empty() )
                    {
                        msg = "@CAPTION@\n@OPTIONS@";
                    }
                    else
                    {
                        msg = "@OPTIONS@";
                    }
                }

                return formatMessage( msg );
            }

            virtual std::string formatMessage( SAA_in std::string msg ) const
            {
                replaceToken( msg, "@CMDNAME@",    std::mem_fn( &this_type::getCommandName ) );
                replaceToken( msg, "@FULLNAME@",   std::mem_fn( &this_type::getFullName ) );
                replaceToken( msg, "@CAPTION@",    std::mem_fn( &this_type::getHelpCaption ) );
                replaceToken( msg, "@COMMANDS@",   std::mem_fn( &this_type::getCommandsHelp ) );
                replaceToken( msg, "@OPTIONS@",    std::mem_fn( &this_type::getOptionsHelp ) );

                return msg;
            }

            void replaceToken(
                SAA_inout   std::string&                                                    text,
                SAA_in      const std::string&                                              token,
                SAA_in      const std::function< std::string( SAA_in const this_type& ) >   getReplacement
                ) const
            {
                std::string::size_type pos = 0;
                bool haveReplacement = false;
                std::string replacement;

                while( ( pos = text.find( token, pos ) ) != std::string::npos )
                {
                    if( ! haveReplacement )
                    {
                        replacement = getReplacement( *this );
                        haveReplacement = true;
                    }

                    text.replace( pos, token.length(), replacement );
                    pos += replacement.length();
                }
            }

            std::string getCommandsHelp() const
            {
                cpp::SafeOutputStringStream oss;

                getCommandsHelpRecursive( oss );

                return oss.str();
            }

            void getCommandsHelpRecursive( SAA_inout std::ostream& stream ) const
            {
                for( const auto& command : m_commands )
                {
                    if( ! command -> getHelpCaption().empty() )
                    {
                        stream << "  " << command -> getFullName() << std::endl;
                    }

                    command -> getCommandsHelpRecursive( stream );
                }
            }

            std::string getOptionsHelp() const
            {
                cpp::SafeOutputStringStream oss;

                for( const auto& option : m_options )
                {
                    if( option -> isPositional() && ! option -> isHidden() )
                    {
                        oss << "  <" << option -> getName() << "> ";
                        const int padding = std::max( 0, ( int ) ( 19 - option -> getName().length() ) );
                        oss << std::setw( padding ) << "" << std::setw( 0 );
                        oss << formatMessage( option -> getDescription() ) << std::endl;
                    }
                }

                getOptionDescriptions( false /* includeAll */, true /* formatDescriptions */ ) -> print( oss );

                /*
                 * Display global options if last command in chain
                 */

                if( m_commands.empty() )
                {
                    const auto rootCmd = getRootCommand();

                    if( rootCmd != this )
                    {
                        hideNonApplicableParentOptions();
                        oss << rootCmd -> getOptionsHelp();
                        unhideNonApplicableParentOptions();
                    }
                }

                return oss.str();
            }

            void hideNonApplicableParentOptions() const
            {
                if( ! isDryRunApplicable() )
                {
                    auto option = findOption( "dryrun,n", true /* lookupParent */ );

                    if( option )
                    {
                        option -> setFlags( Hidden );
                    }
                }
            }

            void unhideNonApplicableParentOptions() const
            {
                if( ! isDryRunApplicable() )
                {
                    auto option = findOption( "dryrun,n", true /* lookupParent */ );

                    if( option )
                    {
                        option -> unsetFlags( Hidden );
                    }
                }
            }

            virtual unsigned getScreenColumns() const
            {
                return m_parent.value() ?
                    m_parent.value() -> getScreenColumns() :
                    po::options_description::m_default_line_length;
            }

            auto getOptionDescriptions(
                SAA_in_opt  const bool              includeAll = true,
                SAA_in_opt  const bool              formatDescriptions = false
                ) const
                -> cpp::SafeUniquePtr< po::options_description >
            {
                const auto columns = getScreenColumns();
                auto options = cpp::SafeUniquePtr< po::options_description >::attach(
                    new po::options_description( columns, columns / 2 )
                    );

                for( const auto& option : m_options )
                {
                    if( includeAll || ( option -> isPositional() || option -> isHidden() ) == false )
                    {
                        /*
                         * Issue: This does NOT throw duplicate_variable_error exception if the command line
                         * option already exists
                         *
                         * See https://svn.boost.org/trac/boost/ticket/3813
                         */

                        if( formatDescriptions )
                        {
                            options -> add_options()
                                ( option -> getName().c_str(), option -> getSemantic(), formatMessage( option -> getDescription() ).c_str() );
                        }
                        else
                        {
                            options -> add_options()
                                ( option -> getName().c_str(), option -> getSemantic() );
                        }
                    }
                }

                return options;
            }

            void getPositionalDescriptions( SAA_inout po::positional_options_description& positional ) const
            {
                for( const auto& option : m_options )
                {
                    if( option -> isPositional() )
                    {
                        positional.add( option -> getName().c_str(), option -> getPositionalCount() );
                    }
                }
            }
        };

        typedef CommandBaseT<> CommandBase;

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_COMMANDBASE_H_ */
