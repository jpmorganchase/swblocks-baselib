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

#ifndef __BL_CMDLINE_CMDLINEAPPBASE_H_
#define __BL_CMDLINE_CMDLINEAPPBASE_H_

#include <baselib/cmdline/CmdLineBase.h>
#include <baselib/cmdline/EhUtils.h>
#include <baselib/cmdline/ExitCodes.h>

#include <baselib/core/AppInitDone.h>
#include <baselib/core/ProgramOptions.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace cmdline
    {
        /**
         * @brief class CmdLineAppBase - a base class to be derived from by all
         * command line applications
         */

        template
        <
            typename T,
            typename APPINITDONEIMPL = AppInitDoneDefault
        >
        class CmdLineAppBase
        {
            BL_CTR_DEFAULT( CmdLineAppBase, protected )
            BL_NO_COPY_OR_MOVE( CmdLineAppBase )
            BL_NO_POLYMORPHIC_BASE( CmdLineAppBase )

        protected:

            enum
            {
                ERROR_CODE_DEFAULT = 1,
            };

            cpp::ScalarTypeIniter< std::size_t >            m_threadsCount;
            cpp::ScalarTypeIniter< bool >                   m_noThreadPool;
            cpp::ScalarTypeIniter< bool >                   m_isServer;
            cpp::ScalarTypeIniter< int >                    m_exitCode;
            cpp::ScalarTypeIniter< bool >                   m_daemonize;
            cpp::SafeUniquePtr< APPINITDONEIMPL >           m_appInit;
            cpp::ScalarTypeIniter< bool >                   m_isNoDefaultInit;

            void setOptions(
                SAA_in          const bool                  isServer
                ) NOEXCEPT
            {
                m_isServer = isServer;

                if( m_isServer )
                {
                    /*
                     * If the app is a server we always want to enable debug level by default
                     */

                    Logging::setLevel( Logging::LL_DEBUG, true /* default global logging level */ );
                }
            }

            void parseArgs(
                SAA_in                      std::size_t                 argc,
                SAA_in_ecount( argc )       const char* const*          argv
                )
            {
                BL_UNUSED( argc );
                BL_UNUSED( argv );
            }

            CommandBase* tryParseCommandLine(
                SAA_in                      std::size_t                 argc,
                SAA_in_ecount( argc )       const char* const*          argv,
                SAA_in                      CmdLineBase&                cmdLine
                )
            {
                try
                {
                    return cmdLine.parseCommandLine( argc, argv );
                }
                catch( po::error& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << "Invalid command line: "
                            << e.what()
                        );

                    m_exitCode = static_cast< int >( ExitCode::InvalidCommandLine );
                }
                catch( UserAuthenticationException& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    m_exitCode = static_cast< int >( ExitCode::UserLoginExpired );
                }
                catch( UserMessageException& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    m_exitCode = static_cast< int >( ExitCode::InvalidCommand );
                }

                return nullptr;
            }

            void tryExecuteCommand(
                SAA_in                      CmdLineBase&                cmdLine,
                SAA_in                      CommandBase*                command
                )
            {
                try
                {
                    cmdLine.executeCommand( command );
                }
                catch( UserAuthenticationException& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    m_exitCode = static_cast< int >( ExitCode::UserLoginExpired );
                }
                catch( UserMessageException& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    m_exitCode = static_cast< int >( ExitCode::InvalidCommand );
                }
                catch( std::exception& e )
                {
                    EhUtils::processException(
                        e,
                        e.what() /* errorMessage */,
                        e.what() /* debugMessage */
                        );

                    m_exitCode = static_cast< int >( ExitCode::UnhandledError );
                }
            }

        public:

            int main(
                SAA_in                      std::size_t                 argc,
                SAA_in_ecount( argc )       const char* const*          argv
                )
            {
                BoostAsioErrorCallback::installCallback( &EhUtils::asioErrorCallback );

                BL_SCOPE_EXIT(
                    {
                        if( m_appInit )
                        {
                            m_appInit -> dispose();
                        }
                    }
                    );

                try
                {
                    static_cast< T* >( this ) -> parseArgs( argc, argv );

                    if( m_isServer )
                    {
                        /*
                         * For server applications the default abstract priority should be 'Normal'
                         */

                        os::setAbstractPriorityDefault( os::AbstractPriority::Normal );
                    }

                    /*
                     * Daemonize the application before creating its thread pool with asio threads.
                     *
                     * It's simple to daemonize the application first and create asio threads afterwards.
                     * Otherwise, if we create asio threads before calling ::daemon in the parent process, then
                     * we need to use asio "notify_fork" API to recreate the asio::io_service in the child process.
                     */

                    if( m_daemonize )
                    {
                        os::daemonize();
                    }

                    if( ! m_isNoDefaultInit )
                    {
                        m_appInit.reset(
                            new APPINITDONEIMPL(
                                m_isServer ? Logging::LL_DEBUG : Logging::LL_INFO,
                                m_threadsCount ? m_threadsCount.value() : ThreadPoolImpl::THREADS_COUNT_DEFAULT,
                                nullptr /* sharedThreadPool */,
                                nullptr /* sharedNonBlockingThreadPool */,
                                m_noThreadPool
                            )
                        );
                    }

                    static_cast< T* >( this ) -> appMain( argc, argv );
                }
                catch( po::error& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << "Invalid command line: "
                            << e.what()
                        );

                    m_exitCode = ERROR_CODE_DEFAULT;
                }
                catch( UserMessageException& e )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    m_exitCode = ERROR_CODE_DEFAULT;
                }
                catch( std::exception& e )
                {
                    BL_LOG_MULTILINE(
                        Logging::error(),
                        BL_MSG()
                            << ( eh::isUserFriendly( e ) ? e.what() : BL_GENERIC_FRIENDLY_UNEXPECTED_MSG )
                        );

                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "\nUnhandled exception was encountered:\n"
                            << eh::diagnostic_information( e )
                        );

                    m_exitCode = ERROR_CODE_DEFAULT;

                    const bool isEhUnhandled =
                        ( nullptr != os::tryGetEnvironmentVariable( "BL_EH_UNHANDLED" ) );

                    const bool* isExpected = eh::get_error_info< eh::errinfo_is_expected >( e );

                    const bool isUnexpected = ( nullptr == isExpected || false == *isExpected );

                    /*
                     * C++ exceptions that are unexpected will be left unhandled if
                     * BL_EH_UNHANDLED=1, so crash dumps are generated and one-off
                     * issues can be debugged and investigated
                     *
                     */

                    if( isEhUnhandled && isUnexpected )
                    {
                        throw;
                    }
                    else
                    {
                        /*
                         * We only map the errorCode -> exitCode if  m_isServer=false
                         */

                        auto errorCode = eh::get_error_info< eh::errinfo_system_code >( e );

                        if( errorCode )
                        {
                            m_exitCode = *errorCode;
                        }
                        else
                        {
                            errorCode = eh::get_error_info< eh::errinfo_errno >( e );

                            if( errorCode )
                            {
                                m_exitCode = *errorCode;
                            }
                        }
                    }
                }

                return m_exitCode;
            }
        };

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_CMDLINEAPPBASE_H_ */
