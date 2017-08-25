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

#ifndef __BL_OS_H_
#define __BL_OS_H_

#include <baselib/core/detail/OSBoostImports.h>
#include <baselib/core/detail/OSImpl.h>
#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/CPP.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/SecureStringWrapper.h>

#include <cstdint>
#include <vector>

namespace bl
{
    namespace os
    {
        namespace detail
        {
            /**
             * @brief class FilePtr2StreamSwitch
             */

            template
            <
                typename STREAM
            >
            class FilePtr2StreamSwitch;

        } // detail

        typedef detail::GlobalProcessInit           GlobalProcessInit;
        typedef detail::OS::DisableConsoleEcho      DisableConsoleEcho;
        typedef detail::OS::RobustNamedMutex        RobustNamedMutex;

        /*
         * Implementation of our own OS specific APIs
         */

        inline void invokeIdempotentGlobalProcessInitHelpers()
        {
            GlobalProcessInit::idempotentGlobalProcessInitHelpers();
        }

        /*****************************************************
         * Shared library support
         */

        typedef detail::OS::library_ref             library_ref;
        typedef detail::OS::process_ref             process_ref;

        inline library_ref loadLibrary( SAA_in const std::string& name )
        {
            return library_ref::attach( detail::OS::loadLibrary( name ) );
        }

        inline void* getProcAddress( SAA_in const library_ref& lib, SAA_in const std::string& name )
        {
            return detail::OS::getProcAddress( lib.get(), name );
        }

        inline void* getProcAddress( SAA_in const library_handle_t handle, SAA_in const std::string& name )
        {
            return detail::OS::getProcAddress( handle, name );
        }

        inline std::string getCurrentExecutablePath()
        {
            return detail::OS::getCurrentExecutablePath();
        }

        /*****************************************************
         * Environment variable support
         */

        inline string_ptr tryGetEnvironmentVariable( SAA_in const std::string& name )
        {
            return detail::OS::tryGetEnvironmentVariable( name );
        }

        inline std::string getEnvironmentVariable( SAA_in const std::string& name )
        {
            return detail::OS::getEnvironmentVariable( name );
        }

        inline void setEnvironmentVariable(
            SAA_in          const std::string&                          name,
            SAA_in          const std::string&                          value
            )
        {
            detail::OS::setEnvironmentVariable( name, value );
        }

        inline void unsetEnvironmentVariable( SAA_in const std::string& name )
        {
            detail::OS::unsetEnvironmentVariable( name );
        }

        inline fs::path getLocalAppDataDir()
        {
            if( onWindows() )
            {
                const auto localAppData = tryGetEnvironmentVariable( "LOCALAPPDATA" );

                if( localAppData )
                {
                    return *localAppData;
                }
                else
                {
                    return getEnvironmentVariable( "USERPROFILE" );
                }
            }
            else
            {
                return getEnvironmentVariable( "HOME" );
            }
        }

        inline fs::path getProgramDataDir()
        {
            if( ! onWindows() )
            {
                BL_THROW(
                    NotSupportedException(),
                    BL_MSG()
                        << "Program data directory is only supported on Windows"
                );
            }

            fs::path programDataDir;

            const auto programDataPath = os::tryGetEnvironmentVariable( "PROGRAMDATA" );

            if( programDataPath )
            {
                programDataDir = *programDataPath;
            }
            else
            {
                /*
                 * WinXP uses a different environment variable
                 */

                programDataDir = os::getEnvironmentVariable( "ALLUSERSPROFILE" );
            }

            return programDataDir;
        }

        inline fs::path getUsersDirectory()
        {
            return detail::OS::getUsersDirectory();
        }

        /*****************************************************
         * FILE* to i/o stream support
         */

        inline auto fileptr2istream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< std::istream >
        {
            return detail::OS::fileptr2istream( fileptr );
        }

        inline auto fileptr2ostream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< std::ostream >
        {
            return detail::OS::fileptr2ostream( fileptr );
        }

        template
        <
            typename STREAM
        >
        inline auto fileptr2stream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< STREAM >
        {
            return detail::FilePtr2StreamSwitch< STREAM >::fileptr2stream( fileptr );
        }

        /*****************************************************
         * Process creation and termination support
         */

        inline process_ref createProcess(
            SAA_in          const std::string&                          commandLine,
            SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
            SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
            SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
            )
        {
            return process_ref::attach(
                detail::OS::createProcess( commandLine, flags, callbackIos, callbackFile )
                );
        }

        inline process_ref createProcess(
            SAA_in          const std::vector< std::string >&           commandArguments,
            SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
            SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
            SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
            )
        {
            return process_ref::attach(
                detail::OS::createProcess( commandArguments, flags, callbackIos, callbackFile )
                );
        }

        inline process_ref createProcess(
            SAA_in          const std::string&                          userName,
            SAA_in          const std::string&                          commandLine,
            SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
            SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
            SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
            )
        {
            return process_ref::attach(
                detail::OS::createProcess( userName, commandLine, flags, callbackIos, callbackFile )
                );
        }

        inline process_ref createProcess(
            SAA_in          const std::string&                          userName,
            SAA_in          const std::vector< std::string >&           commandArguments,
            SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
            SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
            SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
            )
        {
            return process_ref::attach(
                detail::OS::createProcess( userName, commandArguments, flags, callbackIos, callbackFile )
                );
        }

        template
        <
            typename T
        >
        inline process_ref createRedirectedProcessAndWait(
            SAA_in          const T&                                    commandLine,
            SAA_in          const process_redirect_callback_t&          callback
            )
        {
            return createRedirectedProcessAndWait(
                str::empty(),
                commandLine,
                callback
                );
        }

        template
        <
            typename T
        >
        inline process_ref createRedirectedProcessAndWait(
            SAA_in          const std::string&                          userName,
            SAA_in          const T&                                    commandLine,
            SAA_in          const process_redirect_callback_t&          callback
            )
        {
            const auto callbackIos = [ & ](
                SAA_in              const process_handle_t    process,
                SAA_in_opt          std::istream*             out,
                SAA_in_opt          std::istream*             err,
                SAA_in_opt          std::ostream*             in
                ) -> void
            {
                BL_ASSERT( out );
                BL_ASSERT( err );
                BL_ASSERT( ! in );

                BL_UNUSED( in );

                callback( process, *out, *err );
            };

            const auto flags = ProcessCreateFlags::RedirectOutCloseIn | ProcessCreateFlags::WaitToFinish;

            return createProcess( userName, commandLine, flags, callbackIos, process_redirect_callback_file_t() );
        }

        template
        <
            typename T
        >
        inline process_ref createRedirectedProcessMergeOutputAndWait(
            SAA_in          const T&                                    commandLine,
            SAA_in          const process_redirect_callback_merged_t&   callback
            )
        {
            return createRedirectedProcessMergeOutputAndWait(
                str::empty(),
                commandLine,
                callback
                );
        }

        template
        <
            typename T
        >
        inline process_ref createRedirectedProcessMergeOutputAndWait(
            SAA_in          const std::string&                          userName,
            SAA_in          const T&                                    commandLine,
            SAA_in          const process_redirect_callback_merged_t&   callback
            )
        {
            const auto callbackIos = [ & ](
                SAA_in              const process_handle_t    process,
                SAA_in_opt          std::istream*             out,
                SAA_in_opt          std::istream*             err,
                SAA_in_opt          std::ostream*             in
                ) -> void
            {
                BL_ASSERT( out );

                BL_ASSERT( ! err );
                BL_UNUSED( err );

                BL_ASSERT( ! in );
                BL_UNUSED( in );

                callback( process, *out );
            };

            const auto flags = ProcessCreateFlags::RedirectOutMergedCloseIn | ProcessCreateFlags::WaitToFinish;

            return createProcess( userName, commandLine, flags, callbackIos, process_redirect_callback_file_t() );
        }

        template
        <
            typename T
        >
        inline process_ref createRedirectedProcessWithIOAndWait(
            SAA_in          const std::string&                          userName,
            SAA_in          const T&                                    commandLine,
            SAA_in          const process_redirect_callback_ios_t&      callback
            )
        {
            const auto flags = ProcessCreateFlags::RedirectAll | ProcessCreateFlags::WaitToFinish;

            return createProcess( userName, commandLine, flags, callback, process_redirect_callback_file_t() );
        }

        inline bool tryTimedAwaitTermination(
            SAA_in      const process_ref&          process,
            SAA_out_opt int*                        exitCode = nullptr,
            SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
            )
        {
            return detail::OS::tryTimedAwaitTermination( process.get(), exitCode, timeoutMs );
        }

        inline int tryAwaitTermination(
            SAA_in      const process_ref&          process,
            SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
            )
        {
            return detail::OS::tryAwaitTermination( process.get(), timeoutMs );
        }

        inline bool terminateProcess(
            SAA_in      const process_ref&          process,
            SAA_inout   int*                        exitCode,
            SAA_in_opt  const bool                  force = false,
            SAA_in_opt  const bool                  includeSubprocesses = true,
            SAA_in_opt  const int                   timeoutMs = 2 * 60 * 1000 /* 2min */
            )
        {
            detail::OS::terminateProcess( process.get(), force, includeSubprocesses );

            /*
             * Wait for the process to fully terminate
             */

            return tryTimedAwaitTermination( process, exitCode, timeoutMs );
        }

        inline void terminateProcess(
            SAA_in      const process_ref&          process,
            SAA_in_opt  const bool                  force = true,
            SAA_in_opt  const bool                  includeSubprocesses = false
            )
        {
            detail::OS::terminateProcess( process.get(), force, includeSubprocesses );

            /*
             * Wait for the process to fully terminate
             */

            if( ! tryTimedAwaitTermination( process, nullptr, 2 * 60 * 1000 /* 2min */ ) )
            {
                BL_RIP_MSG( "Cannot terminate process" );
            }
        }

        inline void sendProcessStopEvent(
            SAA_in      const process_ref&          process,
            SAA_in      const bool                  includeSubprocesses
            )
        {
            detail::OS::sendProcessStopEvent( process.get(), includeSubprocesses );
        }

        inline void sendProcessStopEvent(
            SAA_in      const process_handle_t      process,
            SAA_in      const bool                  includeSubprocesses
            )
        {
            detail::OS::sendProcessStopEvent( process, includeSubprocesses );
        }

        inline bool tryTimedAwaitTermination(
            SAA_in      const process_handle_t      process,
            SAA_out_opt int*                        exitCode = nullptr,
            SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
            )
        {
            return detail::OS::tryTimedAwaitTermination( process, exitCode, timeoutMs );
        }

        inline int tryAwaitTermination(
            SAA_in      const process_handle_t      process,
            SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
            )
        {
            return detail::OS::tryAwaitTermination( process, timeoutMs );
        }

        inline bool terminateProcess(
            SAA_in      const process_handle_t      process,
            SAA_inout   int*                        exitCode,
            SAA_in_opt  const bool                  force = false,
            SAA_in_opt  const bool                  includeSubprocesses = true,
            SAA_in_opt  const int                   timeoutMs = 2 * 60 * 1000 /* 2min */
            )
        {
            detail::OS::terminateProcess( process, force, includeSubprocesses );

            /*
             * Wait for the process to fully terminate
             */

            return tryTimedAwaitTermination( process, exitCode, timeoutMs );
        }

        inline void terminateProcess(
            SAA_in      const process_handle_t      process,
            SAA_in_opt  const bool                  force = true,
            SAA_in_opt  const bool                  includeSubprocesses = false
            )
        {
            detail::OS::terminateProcess( process, force, includeSubprocesses );

            /*
             * Wait for the process to fully terminate
             */

            if( ! tryTimedAwaitTermination( process, nullptr, 2 * 60 * 1000 /* 2min */ ) )
            {
                BL_RIP_MSG( "Cannot terminate process" );
            }
        }

        inline bool sendProcessStopEventAndWait(
            SAA_in      const process_handle_t      process,
            SAA_inout   int*                        exitCode,
            SAA_in      const bool                  includeSubprocesses,
            SAA_in_opt  const int                   timeoutMs = 2 * 60 * 1000 /* 2min */
            )
        {
            detail::OS::sendProcessStopEvent( process, includeSubprocesses );

            /*
             * Wait for the process to fully terminate
             */

            return tryTimedAwaitTermination( process, exitCode, timeoutMs );
        }

        inline bool sendProcessStopEventAndWait(
            SAA_in      const process_ref&          process,
            SAA_inout   int*                        exitCode,
            SAA_in      const bool                  includeSubprocesses,
            SAA_in_opt  const int                   timeoutMs = 2 * 60 * 1000 /* 2min */
            )
        {
            return sendProcessStopEventAndWait( process.get(), exitCode, includeSubprocesses, timeoutMs );
        }

        inline void sendSignal(
            SAA_in      const process_handle_t&     process,
            SAA_in      const int                   signal,
            SAA_in      const bool                  includeSubprocesses
            )
        {
            detail::OS::sendSignal( process, signal, includeSubprocesses );
        }

        /**
         * @brief utility helper that calls t.join() only if the thread is join-able.
         */

        inline void safeThreadJoin( SAA_inout thread& t )
        {
            if( t.joinable() )
            {
                t.join();
            }
        }

        /**
         * @brief utility helper that calls t.timed_join() but only if the thread is join-able.
         */

        template
        <
            typename TimeDuration
        >
        inline bool safeThreadTimedJoin( SAA_inout thread& t, const TimeDuration& duration )
        {
            if( t.joinable() )
            {
                return t.timed_join( duration );
            }

            return true;
        }

        inline std::uint64_t getPid( SAA_in_opt const process_ref& process = nullptr )
        {
            return detail::OS::getPid( process );
        }

        /*
         * Host to network byte order and vice versa functions
         *
         * Just import the platform agnostic implementations from boost asio
         * private namespace (boost::asio::detail::socket_ops); the include
         * file is: boost/asio/detail/socket_ops.hpp
         */

        inline std::uint32_t network2HostLong( SAA_in const std::uint32_t value ) NOEXCEPT
        {
            return boost::asio::detail::socket_ops::network_to_host_long( value );
        }

        inline std::uint32_t host2NetworkLong( SAA_in const std::uint32_t value ) NOEXCEPT
        {
            return boost::asio::detail::socket_ops::host_to_network_long( value );
        }

        inline std::uint16_t network2HostShort( SAA_in const std::uint16_t value ) NOEXCEPT
        {
            return boost::asio::detail::socket_ops::network_to_host_short( value );
        }

        inline std::uint16_t host2NetworkShort( SAA_in const std::uint16_t value ) NOEXCEPT
        {
            return boost::asio::detail::socket_ops::host_to_network_short( value );
        }

        inline void host2NetworkLongLong(
            SAA_in          const std::uint64_t                 input,
            SAA_out         std::uint32_t&                      outputLo,
            SAA_out         std::uint32_t&                      outputHi
            ) NOEXCEPT
        {
            outputLo = host2NetworkLong( ( std::uint32_t )( input % detail::OS::g_twoPower32 ) );
            outputHi = host2NetworkLong( ( std::uint32_t )( input / detail::OS::g_twoPower32 ) );
        }

        inline std::uint64_t network2HostLongLong(
            SAA_in          const std::uint32_t                 inputLo,
            SAA_in          const std::uint32_t                 inputHi
            ) NOEXCEPT
        {
            const auto hostHi = network2HostLong( inputHi ) * detail::OS::g_twoPower32;

            return hostHi + network2HostLong( inputLo );
        }

        /*****************************************************
         * File I/O support
         */

        inline stdio_file_ptr fopen( SAA_in const fs::path& path, const char* mode )
        {
            return detail::OS::fopen( path, mode );
        }

        inline void fread(
            SAA_in          const stdio_file_ptr&               fileptr,
            SAA_in_bcount( sizeInBytes ) void*                  buffer,
            SAA_in          const std::size_t                   sizeInBytes
            )
        {
            const std::size_t bytesRead = std::fread(
                buffer,
                1                                   /* element size */,
                sizeInBytes                         /* count of elements */,
                fileptr.get()
                );

            if( bytesRead != sizeInBytes )
            {
                if( errno )
                {
                    /*
                     * This is a real error and errno is set; just report it
                     */

                    BL_CHK_ERRNO_NM( false, bytesRead == sizeInBytes );
                }

                /*
                 * If the errno is not set that means we have attempted to read
                 * pas the end of file and this was a partial read; throw some
                 * system error (e.g. operation_not_supported)
                 */

                 BL_CHK_EC(
                    eh::errc::make_error_code( eh::errc::operation_not_permitted ),
                    BL_MSG()
                        << "Reading past the end of file"
                    );
            }
        }

        inline void fwrite(
            SAA_in          const stdio_file_ptr&               fileptr,
            SAA_in_bcount( sizeInBytes ) const void*            buffer,
            SAA_in          const std::size_t                   sizeInBytes,
            SAA_in          const bool                          noflush = false
            )
        {
            const std::size_t bytesWritten = std::fwrite(
                buffer,
                1                                   /* element size */,
                sizeInBytes                         /* count of elements */,
                fileptr.get()
                );

            BL_CHK_ERRNO_NM( false, bytesWritten == sizeInBytes );

            if( false == noflush )
            {
                /*
                 * By default we want to call std::fflush( ... ) after every write
                 * to avoid having to flush during the call to std::fclose( ... )
                 * when errors usually can't be handled gracefully
                 *
                 * If the user sets noflush=true then they have to ensure they call
                 * std::fflush( ... ) prior to the file being closed since the
                 * default closer will call BL_RIP_MSG if an error occurs during the
                 * time we make the std::fclose( ... ) call
                 */

                BL_CHK_ERRNO_NM( false, 0 == std::fflush( fileptr.get() ) );
            }
        }

        inline void fseek(
            SAA_in          const stdio_file_ptr&               fileptr,
            SAA_in          const std::uint64_t                 offset,
            SAA_in          const int                           origin
            )
        {
            detail::OS::fseek( fileptr, offset, origin );
        }

        inline std::uint64_t ftell(
            SAA_in          const stdio_file_ptr&               fileptr
            )
        {
            return detail::OS::ftell( fileptr );
        }

        inline std::time_t getFileCreateTime( SAA_in const fs::path& path )
        {
            return detail::OS::getFileCreateTime( path );
        }

        inline void setFileCreateTime(
            SAA_in          const fs::path&                     path,
            SAA_in          const std::time_t&                  newTime
            )
        {
            detail::OS::setFileCreateTime( path, newTime );
        }

        inline const std::string& getInvalidFileNameCharacters() NOEXCEPT
        {
            return detail::OSImplBase::getInvalidFileNameCharacters();
        }

        inline const std::string& getNonPortableFileNameCharacters() NOEXCEPT
        {
            return detail::OSImplBase::getNonPortableFileNameCharacters();
        }

        inline const std::string& getNonPortablePathNameCharacters() NOEXCEPT
        {
            return detail::OSImplBase::getNonPortablePathNameCharacters();
        }

        inline bool getConsoleSize(
            SAA_out         int&                                columns,
            SAA_out         int&                                rows
            )
        {
            return detail::OS::getConsoleSize( columns, rows );
        }

        inline std::string readFromInput()
        {
            std::string input;

            std::getline( std::cin, input );

            return input;
        }

        inline std::string readFromInput( SAA_in const std::string& prompt )
        {
            BL_STDIO_TEXT(
                {
                    std::cout << std::endl << prompt << ": ";
                }
                );

            auto input = readFromInput();

            BL_STDIO_TEXT(
                {
                    std::cout << std::endl << std::endl;
                }
                );

            return input;
        }

        static str::SecureStringWrapper readFromInputHidden()
        {
            str::SecureStringWrapper input;

            DisableConsoleEcho guard;

            auto dupStdinFilePtr =
                detail::OS::getDuplicatedFileDescriptorAsFilePtr( 0, true /* readOnly */ );

            BL_CHK_ERRNO(
                false,
                ( 0 == std::setvbuf( dupStdinFilePtr.get(), NULL, _IONBF, BUFSIZ ) ),
                "Cannot disable buffering on duplicated stdin"
                );

            int ch = 0;

            while( ( ch = std::fgetc( dupStdinFilePtr.get() ) ) != EOF )
            {
                if( ch == '\n' )
                {
                    break;
                }

                input.append( ch );
            }

            const auto rc = std::ferror( dupStdinFilePtr.get() );

            BL_CHK_EC(
                eh::error_code( rc, eh::generic_category() ),
                "Error while reading fron standard input in hidden mode"
                );

            return input;
        }

        inline str::SecureStringWrapper readPasswordFromInput( SAA_in const std::string& prompt )
        {
            BL_STDIO_TEXT(
                {
                    std::cout << std::endl << prompt << ": ";
                }
                );

            auto password = readFromInputHidden();

            BL_STDIO_TEXT(
                {
                    std::cout << std::endl << std::endl;
                }
                );

            return password;
        }

        inline std::string getUserName()
        {
            return detail::OS::getUserName();
        }

        inline auto getLoggedInUserNames() -> std::vector< std::string >
        {
            return detail::OS::getLoggedInUserNames();
        }

        inline bool isClientOS()
        {
            return detail::OS::isClientOS();
        }

        inline std::string getFileOwner( SAA_in const std::string& filename )
        {
            return detail::OS::getFileOwner( filename );
        }

        inline void createJunction(
            SAA_in          const fs::path&             to,
            SAA_in          const fs::path&             junction
            )
        {
            detail::OS::createJunction( to, junction );
        }

        inline void deleteJunction( SAA_in const fs::path& junction )
        {
            detail::OS::deleteJunction( junction );
        }

        inline bool isJunction( SAA_in const fs::path& path )
        {
            return detail::OS::isJunction( path );
        }

        inline fs::path getJunctionTarget( SAA_in const fs::path& junction )
        {
            return detail::OS::getJunctionTarget( junction );
        }

        inline void createHardlink(
            SAA_in          const fs::path&             to,
            SAA_in          const fs::path&             hardlink
            )
        {
            detail::OS::createHardlink( to, hardlink );
        }

        inline std::size_t getHardlinkCount( SAA_in const fs::path& path )
        {
            return detail::OS::getHardlinkCount( path );
        }

        inline void setAbstractPriorityDefault( SAA_in_opt const AbstractPriority priority ) NOEXCEPT
        {
            detail::OS::setAbstractPriorityDefault( priority );
        }

        inline AbstractPriority getAbstractPriorityDefault() NOEXCEPT
        {
            return detail::OS::getAbstractPriorityDefault();
        }

        inline void setAbstractPriority( SAA_in_opt const AbstractPriority priority )
        {
            detail::OS::setAbstractPriority( priority );
        }

        inline bool trySetAbstractPriority( SAA_in_opt const AbstractPriority priority )
        {
            try
            {
                detail::OS::setAbstractPriority( priority );

                return true;
            }
            catch( eh::system_error& e )
            {
                /*
                 * We only want to handle EPERM and EACCES on UNIX systems which
                 * don't allow resetting back the priority to higher value without
                 * root privileges
                 *
                 * These are the only expected errors - everything else will
                 * be re-thrown
                 */

                if( ! os::onUNIX() || e.code().category() != eh::generic_category() )
                {
                    throw;
                }

                const int code = e.code().value();

                if( EPERM != code && EACCES != code )
                {
                    throw;
                }
            }

            return false;
        }

        inline void daemonize()
        {
            detail::OS::daemonize();
        }

        inline std::string tryGetUserDomain()
        {
            return detail::OS::tryGetUserDomain();
        }

        inline std::string getUserDomain()
        {
            auto domain = detail::OS::tryGetUserDomain();

            if( domain.empty() )
            {
                BL_THROW_USER_FRIENDLY(
                    NotSupportedException(),
                    BL_MSG()
                        << "User domain name is not available"
                    );
            }

            return domain;
        }

        inline std::string getSPNEGOtoken(
            SAA_in          const std::string&                  server,
            SAA_in_opt      const std::string&                  domain = getUserDomain()
            )
        {
            return detail::OS::getSPNEGOtoken( server, domain );
        }

        inline os::string_ptr tryGetRegistryValue(
            SAA_in         const std::string                    key,
            SAA_in         const std::string                    name,
            SAA_in_opt     const bool                           currentUser = false
            )
        {
            return detail::OS::tryGetRegistryValue( key, name, currentUser );
        }

        inline std::string getRegistryValue(
            SAA_in         const std::string                    key,
            SAA_in         const std::string                    name,
            SAA_in_opt     const bool                           currentUser = false
            )
        {
            return detail::OS::getRegistryValue( key, name, currentUser );
        }

        inline bool isUserAdministrator()
        {
            return detail::OS::isUserAdministrator();
        }

        inline bool isUserInteractive()
        {
            if( ! onWindows() )
            {
                BL_THROW(
                    NotSupportedException(),
                    BL_MSG()
                        << "Test if user is interactive is only supported on Windows"
                );
            }

            return detail::OS::isUserInteractive();
        }

        inline int getSessionId()
        {
            return detail::OS::getSessionId();
        }

        inline void copyDirectoryPermissions(
            SAA_in      const fs::path&             srcDirPath,
            SAA_in      const fs::path&             dstDirPath
            )
        {
            detail::OS::copyDirectoryPermissions( srcDirPath, dstDirPath );
        }

        inline void setLinuxPathPermissions(
            SAA_in      const fs::path&             path,
            SAA_in      const fs::perms             permissions
            )
        {
            detail::OS::setLinuxPathPermissions( path, permissions );
        }

        inline void setWindowsPathPermissions(
            SAA_in      const fs::path&             path,
            SAA_in      const fs::perms             ownerPermissions,
            SAA_in_opt  const std::string&          groupName = str::empty(),
            SAA_in_opt  const fs::perms             groupPermissions = fs::perms::no_perms
            )
        {
            detail::OS::setWindowsPathPermissions( path, ownerPermissions, groupName, groupPermissions );
        }

        inline void sendDebugMessage( SAA_in const std::string& message ) NOEXCEPT
        {
            detail::OS::sendDebugMessage( message );
        }

        inline bool createNewFile( SAA_in const fs::path& path )
        {
            return detail::OS::createNewFile( path );
        }

        inline bool isFileInUseError( SAA_in const eh::error_code& ec )
        {
            return detail::OS::isFileInUseError( ec );
        }

        inline eh::error_code tryRemoveFileOnReboot( SAA_in const fs::path& path )
        {
            return detail::OS::tryRemoveFileOnReboot( path );
        }

        /*
         * APIs in this 'unsafe' namespace are not meant to be used directly in the code
         *
         * Please, find and use the safe versions instead
         */

        namespace unsafe
        {
            inline void updateFileAttributes(
                SAA_in          const fs::path&                     path,
                SAA_in          const FileAttributes                attributes,
                SAA_in          const bool                          remove = false
                )
            {
                detail::OS::updateFileAttributes( path, attributes, remove );
            }

            inline FileAttributes getFileAttributes( SAA_in const fs::path& path )
            {
                return detail::OS::getFileAttributes( path );
            }

        } // unsafe

        namespace detail
        {
            /**
             * @brief Specializations of class FilePtr2StreamSwitch
             */

            template
            <
            >
            class FilePtr2StreamSwitch< std::istream >
            {
            public:

                static auto fileptr2stream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< std::istream >
                {
                    return fileptr2istream( fileptr );
                }
            };

            template
            <
            >
            class FilePtr2StreamSwitch< std::ostream >
            {
            public:

                static auto fileptr2stream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< std::ostream >
                {
                    return fileptr2ostream( fileptr );
                }
            };

        } // detail

    } // os

} // bl

#endif /* __BL_OS_H_ */
