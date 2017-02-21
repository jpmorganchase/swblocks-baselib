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

#ifndef __BL_OSIMPLPLATFORMCOMMON_H_
#define __BL_OSIMPLPLATFORMCOMMON_H_

#include <baselib/core/Logging.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/detail/AbortImpl.h>
#include <baselib/core/detail/OSBoostImports.h>
#include <baselib/core/detail/TimeBoostImports.h>
#include <baselib/core/MessageBuffer.h>
#include <baselib/core/BaseIncludes.h>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <locale>
#include <iostream>

#define BL_MUTEX_GUARD( lock ) \
    bl::os::mutex_guard BL_ANONYMOUS_VARIABLE( g ) ( lock )

namespace bl
{
    #if defined( _WIN32 )

    /*
     * As of GCC 4.8.1 the standard library doesn't implement wstring_convert yet
     * So far the conversion from/to Unicode is needed for Windows only hence
     * the below typedef and its use is platform specific
     */

    namespace cpp
    {
        typedef std::wstring_convert< boost::filesystem::detail::utf8_codecvt_facet > wstring_convert_t;

    } // cpp

    #endif // #if defined( _WIN32 )

    namespace os
    {
        /*
         * Platform common definitions. These will be visible
         * in the platform specific headers
         */

        typedef unsigned short                                  port_t;

        typedef struct library_handle_tag { /* opaque */ }      *library_handle_t;
        typedef struct process_handle_tag { /* opaque */ }      *process_handle_t;
        typedef void                                            *generic_handle_t;

        typedef cpp::SafeUniquePtr< std::string >               string_ptr;

        enum : int
        {
            TIMEOUT_INFINITE = -1,
        };

        enum FileAttributes : std::uint32_t
        {
            FileAttributeNone           = 0,
            FileAttributeHidden         = 0x1,

            FileAttributesMask          = FileAttributeHidden,
        };

        enum ProcessCreateFlags : std::uint32_t
        {
            NoRedirect                  = 0,

            WaitToFinish                = 1 << 0,

            MergeStdoutAndStderr        = 1 << 1,
            CloseStdin                  = 1 << 2,
            RedirectStdout              = 1 << 3,
            RedirectStderr              = 1 << 4,
            RedirectStdin               = 1 << 5,

            DetachProcess               = 1 << 6,

            RedirectOut                 = RedirectStdout | RedirectStderr,
            RedirectOutCloseIn          = RedirectOut | RedirectStdin | CloseStdin,
            RedirectOutMerged           = RedirectOut | MergeStdoutAndStderr,
            RedirectOutMergedCloseIn    = RedirectOutMerged | RedirectStdin | CloseStdin,
            RedirectAll                 = RedirectStdout | RedirectStderr | RedirectStdin,
        };

        BL_DECLARE_BITMASK( FileAttributes )
        BL_DECLARE_BITMASK( ProcessCreateFlags )

        typedef cpp::function
        <
            void (
                SAA_in              const process_handle_t      process,
                SAA_in_opt          std::istream*               out,
                SAA_in_opt          std::istream*               err,
                SAA_in_opt          std::ostream*               in
                )
        >
        process_redirect_callback_ios_t;

        typedef cpp::function
        <
            void (
                SAA_in              const process_handle_t      process,
                SAA_in_opt          std::FILE*                  out,
                SAA_in_opt          std::FILE*                  err,
                SAA_in_opt          std::FILE*                  in
                )
        >
        process_redirect_callback_file_t;

        typedef cpp::function
        <
            void (
                SAA_in              const process_handle_t      process,
                SAA_in              std::istream&               out
                )
        >
        process_redirect_callback_merged_t;

        typedef cpp::function
        <
            void (
                SAA_in              const process_handle_t      process,
                SAA_in_opt          std::istream&               out,
                SAA_in_opt          std::istream&               err
                )
        >
        process_redirect_callback_t;

        /*
         * Combined priority for CPU, I/O and Memory
         *
         * Note: The 'Greedy' option will not be supported on Windows currently
         */

        enum AbstractPriority : std::uint16_t
        {
            Normal                      = 0,
            Background                  = 1,
            Greedy                      = 2,
        };

        namespace detail
        {
            /*
             * Implement Boost I/O streams source and sink for std::FILE, so we can
             * instantiate streambuf for std::FILE in platform / OS agnostic way
             *
             * Note that GCC has extensions such as __gnu_cxx::stdio_filebuf< char >
             * which provide such (needed) abstraction, but these extensions are not
             * present on non-GCC toolchains such as Clang on Darwin with libc++ and
             * thus we need something else to cover that and possibly other
             * platforms and toolchains
             *
             * Note that these implementations mostly follows the Boost I/O streams
             * tutorial about how to write sources, sinks and devices in general:
             *
             * http://www.boost.org/doc/libs/1_63_0/libs/iostreams/doc/index.html
             */

            namespace ios = boost::iostreams;

            /**
             * @brief stdio_file_device_base - a base class to support Boost I/O streams
             * devices for std::FILE* (both sources and sinks)
             */

            template
            <
                typename CharT = char
            >
            class stdio_file_device_base
            {
                BL_CTR_COPY_DEFAULT( stdio_file_device_base )

            protected:

                std::FILE*                                      m_fileptr;

                /*
                 * Simply checks if the stream is in error state and throws if so
                 */

                void checkStream()
                {
                    const auto errNo = std::ferror( m_fileptr );

                    if( errNo )
                    {
                        BL_CHK_EC_NM( eh::error_code( errNo, eh::generic_category() ) );
                    }
                }

                stdio_file_device_base( SAA_inout std::FILE* fileptr )
                    :
                    m_fileptr( fileptr )
                {
                    BL_ASSERT( m_fileptr );

                    checkStream();
                }

            public:

                /*
                 * Implement the seek-able interface according to what is expected from the
                 * Boost I/O streams implementation:
                 *
                 * http://www.boost.org/doc/libs/1_63_0/libs/iostreams/doc/concepts/seekable_device.html
                 *
                 * Advances the read/write head by off characters,
                 * returning the new position, where the offset is
                 * calculated from:
                 *
                 *  - the start of the sequence if way == ios_base::beg
                 *  - the current position if way == ios_base::cur
                 *  - the end of the sequence if way == ios_base::end
                 *
                 */

                auto seek(
                    SAA_in          const ios::stream_offset            offset,
                    SAA_in          const std::ios_base::seekdir        direction
                    )
                    -> std::streampos
                {
                    int localDirection = 0;

                    switch( direction )
                    {
                        default:
                            BL_RIP_MSG( "stdio_file_device_base::seek: unknown seek direction value" );
                            break;

                        case std::ios_base::beg:
                            localDirection = SEEK_SET;
                            break;

                        case std::ios_base::cur:
                            localDirection = SEEK_CUR;
                            break;

                        case std::ios_base::end:
                            localDirection = SEEK_END;
                            break;
                    }

                    if( std::fseek( m_fileptr, offset, localDirection ) )
                    {
                        checkStream();

                        /*
                         * If we are here it means we did not seek properly but yet the
                         * underlying stream is not in error state
                         *
                         * This should not really happen, so I guess we should just throw
                         */

                       BL_THROW_EC(
                           eh::errc::make_error_code( eh::errc::invalid_seek ),
                           BL_MSG()
                               << "stdio_file_device_base::seek: cannot seek in the stream"
                           );
                    }

                    const auto newPos = std::ftell( m_fileptr );

                    if( newPos < 0 )
                    {
                        checkStream();

                        /*
                         * If we are here it means we did not seek properly but yet the
                         * underlying stream is not in error state
                         *
                         * This should not really happen, so I guess we should just throw
                         */

                       BL_THROW_EC(
                           eh::errc::make_error_code( eh::errc::invalid_seek ),
                           BL_MSG()
                               << "stdio_file_device_base::seek: cannot seek in the stream"
                           );
                    }

                    return std::streampos( newPos );
                }
            };

            /**
             * @brief stdio_file_source - Boost I/O streams device source for std::FILE*
             */

            template
            <
                typename CharT = char
            >
            class stdio_file_source : public stdio_file_device_base< CharT >
            {
                BL_CTR_COPY_DEFAULT( stdio_file_source )

            protected:

                typedef stdio_file_device_base< CharT >         base_type;

                using base_type::m_fileptr;

            public:

                stdio_file_source( SAA_inout std::FILE* fileptr )
                    :
                    base_type( fileptr )
                {
                }

                typedef CharT                                   char_type;

                struct category :
                    ios::input_seekable, ios::device_tag
                {
                };

                using base_type::seek;

                /*
                 * From the Boost I/O streams tutorial:
                 *
                 * http://www.boost.org/doc/libs/1_63_0/libs/iostreams/doc/tutorial/container_source.html
                 *
                 * Read up to countOfCharsToRead characters from the underlying data source
                 * into the buffer, returning the number of characters read; return -1 to indicate EOF
                 */

                auto read(
                    SAA_inout       char*                               buffer,
                    SAA_in          const std::streamsize               countOfCharsToRead
                    )
                    -> std::streamsize
                {
                    BL_ASSERT( buffer && countOfCharsToRead );

                    /*
                     * Before we attempt to read we check if the stream is not already in error
                     * state or in EOF state
                     *
                     * In the first case we throw and in the second case we simply return -1
                     * to satisfy the Boost I/O stream interface
                     */

                    base_type::checkStream();

                    if( std::feof( m_fileptr ) )
                    {
                        return -1;
                    }

                    const std::streamsize charsRead = std::fread(
                        buffer,
                        1U                          /* element size */,
                        countOfCharsToRead,
                        m_fileptr
                        );

                    if( charsRead != countOfCharsToRead )
                    {
                        /*
                         * If we read less chars than requested then we must check if this was
                         * because of an underlying error with the stream or because of EOF case
                         */

                        base_type::checkStream();

                        if( charsRead )
                        {
                            return charsRead;
                        }

                        if( std::feof( m_fileptr ) )
                        {
                            return -1;
                        }

                        /*
                         * If we are here it means we did not read any data into the buffer
                         * but yet the underlying stream is not in error state and also not
                         * in EOF state
                         *
                         * This should not really happen, so I guess we should just throw
                         */

                        BL_THROW_EC(
                            eh::errc::make_error_code( eh::errc::io_error ),
                            BL_MSG()
                                << "stdio_file_source::read: cannot read data from the stream"
                            );
                    }

                    return charsRead;
                }
            };

            typedef stdio_file_source< char >                           stdio_file_source_char_t;
            typedef ios::stream_buffer< stdio_file_source_char_t >      stdio_istreambuf_char_t;
            typedef ios::stream< stdio_file_source_char_t >             stdio_istream_char_t;

            /**
             * @brief stdio_file_sink - Boost I/O streams device sink for std::FILE*
             */

            template
            <
                typename CharT = char
            >
            class stdio_file_sink : public stdio_file_device_base< CharT >
            {
                BL_CTR_COPY_DEFAULT( stdio_file_sink )

            protected:

                typedef stdio_file_device_base< CharT >         base_type;

                using base_type::m_fileptr;

            public:

                typedef CharT                                   char_type;

                struct category :
                    ios::output_seekable, ios::device_tag
                {
                };

                stdio_file_sink( SAA_inout std::FILE* fileptr )
                    :
                    base_type( fileptr )
                {
                }

                using base_type::seek;

                /*
                 * From the Boost I/O streams tutorial:
                 *
                 * http://www.boost.org/doc/libs/1_58_0/libs/iostreams/doc/tutorial/container_sink.html
                 *
                 * Write up to countOfCharsToWrite characters to the underlying
                 * data sink from the buffer, returning the number of characters written
                 */

                auto write(
                    SAA_inout       const char*                         buffer,
                    SAA_in          const std::streamsize               countOfCharsToWrite
                    )
                    -> std::streamsize
                {
                    BL_ASSERT( buffer && countOfCharsToWrite );

                    /*
                     * Before we attempt to write we check if the stream is not already in
                     * error state
                     */

                    base_type::checkStream();

                    const std::streamsize charsWritten = std::fwrite(
                        buffer,
                        1U                          /* element size */,
                        countOfCharsToWrite,
                        m_fileptr
                        );

                    if( charsWritten != countOfCharsToWrite )
                    {
                        base_type::checkStream();

                        /*
                         * If we are here it means we did not read any data into the buffer
                         * but yet the underlying stream is not in error state and also not
                         * in EOF state
                         *
                         * This should not really happen, so I guess we should just throw
                         */

                        BL_THROW_EC(
                            eh::errc::make_error_code( eh::errc::io_error ),
                            BL_MSG()
                                << "stdio_file_sink::write: cannot write data to the stream"
                            );
                    }

                    if( std::fflush( m_fileptr ) )
                    {
                        /*
                         * An error has occurred during flush - check the stream to throw
                         * as appropriate
                         */

                        base_type::checkStream();
                    }

                   return charsWritten;
                }
            };

            typedef stdio_file_sink< char >                             stdio_file_sink_char_t;
            typedef ios::stream_buffer< stdio_file_sink_char_t >        stdio_ostreambuf_char_t;
            typedef ios::stream< stdio_file_sink_char_t >               stdio_ostream_char_t;

            /**
             * @brief class GlobalProcessInitBase (shared by all platforms)
             */

            template
            <
                typename E = void
            >
            class GlobalProcessInitBaseT
            {
                BL_NO_COPY_OR_MOVE( GlobalProcessInitBaseT )
                BL_DECLARE_ABSTRACT( GlobalProcessInitBaseT )

            protected:

                static void idempotentGlobalProcessInitBaseHelpers()
                {
                    /*
                     * Set the 'terminate()' handler to abort the process immediately (no clean-up)
                     */

                    std::set_terminate( terminateHandler );
                }

                SAA_noreturn
                static void terminateHandler()
                {
                    BL_RIP_MSG( "Exception handling failure" );
                }
            };

            typedef GlobalProcessInitBaseT<> GlobalProcessInitBase;

            /**
             * @brief Shared implementation of platform-specific code
             */

            template
            <
                typename E = void
            >
            class OSImplBaseT
            {
                BL_DECLARE_STATIC( OSImplBaseT )

            public:

                #if defined( _WIN32 )

                enum
                {
                    isWindows = true,
                    isUNIX = false,
                    isDarwin = false,
                    isLinux = false,
                };

                #else // defined( _WIN32 )

                enum
                {
                    isWindows = false,
                    isUNIX = true,

#ifdef __APPLE__
                    isDarwin = true,
#else
                    isDarwin = false,
#endif

#ifdef __linux__
                    isLinux = true,
#else
                    isLinux = false,
#endif
                };

                #endif // defined( _WIN32 )

                static string_ptr tryGetEnvironmentVariable( SAA_in const std::string& name )
                {
                    string_ptr value;

                    {
                        BL_MUTEX_GUARD( g_envLock );

                        const auto res = std::getenv( name.c_str() );

                        if( res )
                        {
                            value = string_ptr::attach( new std::string( res ) );
                        }
                    }

                    return value;
                }

                static std::string getEnvironmentVariable( SAA_in const std::string& name )
                {
                    const auto value = tryGetEnvironmentVariable( name );

                    BL_CHK_USER_FRIENDLY(
                        nullptr,
                        value.get(),
                        BL_MSG()
                            << "Environment variable '"
                            << name
                            << "' was not found"
                        );

                    return *value;
                }

                static void validateProcessRedirectFlags(
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    const ProcessCreateFlags anyRedirected =
                        ProcessCreateFlags::RedirectStdout |
                        ProcessCreateFlags::RedirectStderr |
                        ProcessCreateFlags::RedirectStdin;

                    if( 0U == ( flags & anyRedirected ) )
                    {
                        chkFlagsCond( 0U == flags );
                    }

                    if( flags & ProcessCreateFlags::MergeStdoutAndStderr )
                    {
                        chkFlagsCond(
                            0 != ( flags & ProcessCreateFlags::RedirectStdout ) &&
                            0 != ( flags & ProcessCreateFlags::RedirectStderr )
                            );
                    }

                    if( flags & ProcessCreateFlags::CloseStdin )
                    {
                        chkFlagsCond( 0 != ( flags & ProcessCreateFlags::RedirectStdin ) );
                    }

                    const bool hasCallback = ( callbackIos || callbackFile );

                    if( ( flags && false == hasCallback ) || ( 0U == flags && hasCallback ) )
                    {
                        BL_THROW(
                            ArgumentException(),
                            BL_MSG()
                                << "validateProcessRedirectFlags: the callback parameter is invalid"
                            );
                    }
                }

                static void setAbstractPriorityDefault( SAA_in_opt const AbstractPriority priority ) NOEXCEPT
                {
                    g_abstractPriorityDefault = priority;
                }

                static AbstractPriority getAbstractPriorityDefault() NOEXCEPT
                {
                    return g_abstractPriorityDefault;
                }

                static const std::string& getInvalidFileNameCharacters() NOEXCEPT
                {
                    return isWindows ? g_invalidWindowsFileNameCharacters : g_invalidUnixFileNameCharacters;
                }

                static const std::string& getNonPortableFileNameCharacters() NOEXCEPT
                {
                    return g_invalidWindowsFileNameCharacters;
                }

                static const std::string& getNonPortablePathNameCharacters() NOEXCEPT
                {
                    return g_nonPortablePathNameCharacters;
                }

                static AbstractPriority             g_abstractPriorityDefault;

                static const std::string            g_invalidWindowsFileNameCharacters;
                static const std::string            g_invalidUnixFileNameCharacters;
                static const std::string            g_nonPortablePathNameCharacters;

                static const std::uint64_t          g_twoPower32;

            protected:

                static os::mutex                    g_envLock;

            private:

                static void chkFlagsCond( SAA_in const bool cond )
                {
                    if( ! cond )
                    {
                        BL_THROW(
                            ArgumentException(),
                            BL_MSG()
                                << "Invalid ProcessCreateFlags value"
                            );
                    }
                }
            };

            /*
             * The default abstract priority for all applications should be background
             * unless it is set otherwise
             */

            BL_DEFINE_STATIC_MEMBER( OSImplBaseT, AbstractPriority, g_abstractPriorityDefault ) = AbstractPriority::Background;

            BL_DEFINE_STATIC_MEMBER( OSImplBaseT, os::mutex, g_envLock );

            /*
             * Windows disallows all ASCII control characters in file names
             * and also < > : " / \ | ? *
             */

            BL_DEFINE_STATIC_CONST_STRING( OSImplBaseT, g_invalidWindowsFileNameCharacters )(
                "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
                "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
                "<>:\"/\\|?*", 0x20 + 9 /* length */
                );

            /*
             * Linux allows all characters in file names except for / and embedded NULs
             */

            BL_DEFINE_STATIC_CONST_STRING( OSImplBaseT, g_invalidUnixFileNameCharacters )(
                "\x00/", 2 /* length */
                );

            /*
             * Non-portable pathname characters are those disallowed in the file names minus '/'
             */

            BL_DEFINE_STATIC_CONST_STRING( OSImplBaseT, g_nonPortablePathNameCharacters )(
                "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
                "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
                "<>:\"\\|?*", 0x20 + 8 /* length */
                );

            BL_DEFINE_STATIC_MEMBER( OSImplBaseT, const std::uint64_t, g_twoPower32 ) = 1ULL << 32;

            typedef OSImplBaseT<> OSImplBase;

            class CloseStdFileDeleter
            {
            public:

                void operator()( SAA_inout std::FILE* ptr ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_CHK_ERRNO_NM( false, 0 == std::fclose( ptr ) );

                    BL_NOEXCEPT_END()
                }
            };

        } // detail

        enum
        {
            isWindows   = detail::OSImplBase::isWindows,
            isUNIX      = detail::OSImplBase::isUNIX,
            isDarwin    = detail::OSImplBase::isDarwin,
            isLinux     = detail::OSImplBase::isLinux,
        };

        inline bool onWindows() NOEXCEPT
        {
            return isWindows;
        }

        inline bool onUNIX() NOEXCEPT
        {
            return isUNIX;
        }

        inline bool onDarwin() NOEXCEPT
        {
            return isDarwin;
        }

        inline bool onLinux() NOEXCEPT
        {
            return isLinux;
        }

        inline void sleep( SAA_in const time::time_duration& duration )
        {
            thread::sleep( get_system_time() + duration );
        }

        inline void interruptibleSleep(
            SAA_in        time::time_duration&&         duration,
            SAA_in_opt    const time::time_duration&    partialDuration = time::milliseconds( 50 ),
            SAA_in_opt    cpp::bool_callback_t&&        interruptCallback = []() -> bool { return false; }
            )
        {
            while( duration > partialDuration )
            {
                sleep( partialDuration );

                if( interruptCallback() )
                {
                    return;
                }

                duration -= partialDuration;
            }

            sleep( duration );
        }

        typedef cpp::SafeUniquePtr< std::FILE, detail::CloseStdFileDeleter > stdio_file_ptr;

        namespace detail
        {
            inline void stdioChkOffset( SAA_in const std::uint64_t offset )
            {
                BL_CHK_T(
                    true,
                    ( ( std::int64_t ) offset ) < 0,
                    ArgumentException(),
                    BL_MSG()
                        << "Invalid offset parameter value "
                        << offset
                        << " passed to stdio API"
                    );
            }

        } // detail

    } // os

    namespace fs
    {
        /*
         * nolfn namespace is importing the boost::filesystem entries directly
         * and these APIs will not have long filenames support; this is needed
         * only for some legitimate cases where the code cannot handle LFNs
         * for some reason
         */

        namespace nolfn
        {
            using boost::filesystem::absolute;
            using boost::filesystem::create_directory_symlink;
            using boost::filesystem::create_directory;
            using boost::filesystem::create_symlink;
            using boost::filesystem::current_path;
            using boost::filesystem::directory_entry;
            using boost::filesystem::directory_file;
            using boost::filesystem::directory_iterator;
            using boost::filesystem::directory_iterator;
            using boost::filesystem::exists;
            using boost::filesystem::file_size;
            using boost::filesystem::file_status;
            using boost::filesystem::file_type;
            using boost::filesystem::is_directory;
            using boost::filesystem::is_other;
            using boost::filesystem::is_regular_file;
            using boost::filesystem::is_symlink;
            using boost::filesystem::is_empty;
            using boost::filesystem::equivalent;
            using boost::filesystem::last_write_time;
            using boost::filesystem::path;
            using boost::filesystem::path;
            using boost::filesystem::permissions;
            using boost::filesystem::perms;
            using boost::filesystem::read_symlink;
            using boost::filesystem::recursive_directory_iterator;
            using boost::filesystem::recursive_directory_iterator;
            using boost::filesystem::regular_file;
            using boost::filesystem::rename;
            using boost::filesystem::reparse_file;
            using boost::filesystem::resize_file;
            using boost::filesystem::socket_file;
            using boost::filesystem::space;
            using boost::filesystem::status;
            using boost::filesystem::symlink_file;
            using boost::filesystem::symlink_status;
            using boost::filesystem::temp_directory_path;

        } // nolfn

        /* import names we will use directly from boost::filesystem namespace */
        using boost::filesystem::detail::utf8_codecvt_facet;
        using boost::filesystem::directory_entry;
        using boost::filesystem::directory_file;
        using boost::filesystem::file_status;
        using boost::filesystem::file_type;
        using boost::filesystem::perms;
        using boost::filesystem::regular_file;
        using boost::filesystem::reparse_file;
        using boost::filesystem::space_info;
        using boost::filesystem::socket_file;
        using boost::filesystem::symlink_file;

        const perms ExecutableFileMask = perms::owner_exe | perms::group_exe | perms::others_exe;

        namespace detail
        {
            namespace bfs
            {
                using boost::filesystem::absolute;
                using boost::filesystem::create_directories;
                using boost::filesystem::create_directory_symlink;
                using boost::filesystem::create_directory;
                using boost::filesystem::create_symlink;
                using boost::filesystem::current_path;
                using boost::filesystem::directory_iterator;
                using boost::filesystem::exists;
                using boost::filesystem::file_size;
                using boost::filesystem::is_directory;
                using boost::filesystem::is_other;
                using boost::filesystem::is_regular_file;
                using boost::filesystem::is_symlink;
                using boost::filesystem::is_empty;
                using boost::filesystem::equivalent;
                using boost::filesystem::last_write_time;
                using boost::filesystem::path;
                using boost::filesystem::permissions;
                using boost::filesystem::read_symlink;
                using boost::filesystem::recursive_directory_iterator;
                using boost::filesystem::remove_all;
                using boost::filesystem::remove;
                using boost::filesystem::rename;
                using boost::filesystem::resize_file;
                using boost::filesystem::copy;
                using boost::filesystem::copy_file;
                using boost::filesystem::copy_symlink;
                using boost::filesystem::copy_directory;
                using boost::filesystem::space;
                using boost::filesystem::status;
                using boost::filesystem::symlink_status;
                using boost::filesystem::temp_directory_path;

            } // bfs

            /**
             * @brief class WinLfnUtils - Windows long file names support utility code
             */

            template
            <
                typename E = void
            >
            class WinLfnUtilsT
            {
                BL_DECLARE_STATIC( WinLfnUtilsT )

            public:

                static const std::wstring       g_uncPrefix;
                static const std::string        g_lfnPrefix;
                static const std::string        g_lfnUncPrefix;
                static const bfs::path          g_lfnRootPath;
                static const bfs::path          g_lfnUncRootPath;

                static bfs::path chk2AddPrefix( SAA_in bfs::path&& path )
                {
                    bfs::path result;

                    if( ! path.empty() )
                    {
                        if( false == path.is_absolute() || path.root_path() == g_lfnRootPath )
                        {
                            /*
                             * Relative paths and paths that are already prefixes
                             * should not be prefixed
                             */

                            result.swap( path );
                        }
                        else
                        {
                            /*
                             * The path needs to be prefixed
                             *
                             * Check for the UNC vs. non-UNC case
                             */

                            const auto* psz = path.c_str();
                            BL_ASSERT( psz && psz[ 0 ] );

                            if( L'\\' == psz[ 0 ] && L'\\' == psz[ 1 ] && L'\\' != psz[ 2 ] && L'\0' != psz[ 2 ] )
                            {
                                result = g_lfnUncRootPath;
                                result += ( psz + 2 );
                            }
                            else
                            {
                                result = g_lfnRootPath;
                                result += std::move( path );
                            }
                        }
                    }

                    return result;
                }

                static bfs::path chk2RemovePrefix( SAA_in bfs::path&& path )
                {
                    bfs::path result;

                    if( path.root_path() != g_lfnRootPath )
                    {
                        path.swap( result );
                    }
                    else
                    {
                        /*
                         * The path is prefixed, let's remove the prefix
                         */

                        const auto pathStr = path.string();

                        BL_ASSERT( str::starts_with( pathStr, g_lfnPrefix ) );

                        if( str::starts_with( pathStr, g_lfnUncPrefix ) )
                        {
                            bfs::path( "\\\\" + pathStr.substr( g_lfnUncPrefix.size() ) ).swap( result );
                        }
                        else
                        {
                            bfs::path( pathStr.substr( g_lfnPrefix.size() ) ).swap( result );
                        }
                    }

                    return result;
                }
            };

            BL_DEFINE_STATIC_MEMBER( WinLfnUtilsT, const std::wstring, g_uncPrefix )  ( L"\\\\" );
            BL_DEFINE_STATIC_MEMBER( WinLfnUtilsT, const std::string, g_lfnPrefix )   ( "\\\\?\\" );
            BL_DEFINE_STATIC_MEMBER( WinLfnUtilsT, const std::string, g_lfnUncPrefix )( "\\\\?\\UNC\\" );
            BL_DEFINE_STATIC_MEMBER( WinLfnUtilsT, const bfs::path, g_lfnRootPath )   ( "\\\\?\\" );
            BL_DEFINE_STATIC_MEMBER( WinLfnUtilsT, const bfs::path, g_lfnUncRootPath )( "\\\\?\\UNC\\" );

            typedef WinLfnUtilsT<> WinLfnUtils;

        } // detail

        template
        <
            bool isWindows = true
        >
        class PathImplT;

        template
        <
        >
        class PathImplT< true /* isWindows */ > :
            public detail::bfs::path
        {
        public:

            typedef detail::bfs::path       base_type;
            typedef PathImplT< true >       this_type;

            PathImplT()
            {
            }

            /* *************************************************** */

            PathImplT( SAA_in const PathImplT& path )
                :
                base_type()
            {
                detail::WinLfnUtils::chk2AddPrefix( cpp::copy< base_type >( path ) ).swap( *this );
            }

            PathImplT( SAA_in PathImplT&& path )
                :
                base_type()
            {
                detail::WinLfnUtils::chk2AddPrefix( std::forward< base_type >( path ) ).swap( *this );
            }

            PathImplT( SAA_in const base_type& path )
                :
                base_type()
            {
                detail::WinLfnUtils::chk2AddPrefix( cpp::copy< base_type >( path ) ).swap( *this );
            }

            PathImplT( SAA_in base_type&& path )
                :
                base_type()
            {
                detail::WinLfnUtils::chk2AddPrefix( std::forward< base_type >( path ) ).swap( *this );
            }

            /* *************************************************** */

            PathImplT& operator=( SAA_in const PathImplT& path )
            {
                detail::WinLfnUtils::chk2AddPrefix( cpp::copy< base_type >( path ) ).swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in PathImplT&& path )
            {
                detail::WinLfnUtils::chk2AddPrefix( std::forward< base_type >( path ) ).swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in const base_type& path )
            {
                detail::WinLfnUtils::chk2AddPrefix( cpp::copy< base_type >( path ) ).swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in base_type&& path )
            {
                detail::WinLfnUtils::chk2AddPrefix( std::forward< base_type >( path ) ).swap( *this );
                return *this;
            }

            #if defined( _WIN32 )

            /*
             * Boost.Filesystem default implementation of this overload has a bug which causes a heap
             * corruption issue when application verifier is enabled - you can see the details in the
             * following JIRA:
             *
             * https://issuetracking.jpmchase.net/jira17/browse/APPDEPLOY-937
             *
             * The workaround is to overload these constructors and assignment operators and do the
             * conversion to wchar_t ourselves, so avoid hitting this buggy code
             */

            PathImplT( SAA_in const char* src )
            {
                cpp::wstring_convert_t conv;
                detail::WinLfnUtils::chk2AddPrefix( base_type( conv.from_bytes( src ) ) ).swap( *this );
            }

            PathImplT& operator=( SAA_in const char* src )
            {
                cpp::wstring_convert_t conv;
                detail::WinLfnUtils::chk2AddPrefix( base_type( conv.from_bytes( src ) ) ).swap( *this );
                return *this;
            }

            PathImplT( SAA_in std::string&& src )
            {
                cpp::wstring_convert_t conv;
                detail::WinLfnUtils::chk2AddPrefix( base_type( conv.from_bytes( src ) ) ).swap( *this );
            }

            PathImplT& operator=( SAA_in std::string&& src )
            {
                cpp::wstring_convert_t conv;
                detail::WinLfnUtils::chk2AddPrefix( base_type( conv.from_bytes( src ) ) ).swap( *this );
                return *this;
            }

            PathImplT( SAA_in const std::string& src )
            {
                cpp::wstring_convert_t conv;
                detail::WinLfnUtils::chk2AddPrefix( base_type( conv.from_bytes( src ) ) ).swap( *this );
            }

            PathImplT& operator=( SAA_in const std::string& src )
            {
                cpp::wstring_convert_t conv;
                detail::WinLfnUtils::chk2AddPrefix( base_type( conv.from_bytes( src ) ) ).swap( *this );
                return *this;
            }

            /*
             * Since we're not providing the generic overload we have to provide explicit
             * overloads for what is needed as long as it is safe
             */

            PathImplT( SAA_in const directory_entry& entry )
            {
                detail::WinLfnUtils::chk2AddPrefix( cpp::copy< base_type >( entry ) ).swap( *this );
            }

            PathImplT( SAA_in directory_entry&& entry )
            {
                detail::WinLfnUtils::chk2AddPrefix( std::forward< base_type >( entry ) ).swap( *this );
            }

            PathImplT& operator=( SAA_in const directory_entry& entry )
            {
                detail::WinLfnUtils::chk2AddPrefix( cpp::copy< base_type >( entry ) ).swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in directory_entry&& entry )
            {
                detail::WinLfnUtils::chk2AddPrefix( std::forward< base_type >( entry ) ).swap( *this );
                return *this;
            }

            #else // defined( _WIN32 )

            template
            <
                typename Source
            >
            PathImplT( SAA_in Source&& src )
            {
                detail::WinLfnUtils::chk2AddPrefix( base_type( std::forward< Source >( src ) ) ).swap( *this );
            }

            template
            <
                typename Source
            >
            PathImplT& operator=( SAA_in Source&& src )
            {
                detail::WinLfnUtils::chk2AddPrefix( base_type( std::forward< Source >( src ) ) ).swap( *this );
                return *this;
            }

            #endif // defined( _WIN32 )
        };

        template
        <
        >
        class PathImplT< false /* isWindows */ > :
            public detail::bfs::path
        {
        public:

            typedef detail::bfs::path       base_type;
            typedef PathImplT< false >      this_type;

            PathImplT()
            {
            }

            /* *************************************************** */

            PathImplT( SAA_in const PathImplT& path )
                :
                base_type()
            {
                cpp::copy< base_type >( path ).swap( *this );
            }

            PathImplT( SAA_in PathImplT&& path )
                :
                base_type()
            {
                path.swap( *this );
            }

            PathImplT( SAA_in const base_type& path )
                :
                base_type()
            {
                cpp::copy< base_type >( path ).swap( *this );
            }

            PathImplT( SAA_in base_type&& path )
                :
                base_type()
            {
                path.swap( *this );
            }

            template
            <
                typename Source
            >
            PathImplT( SAA_in Source&& src )
                :
                base_type()
            {
                base_type( std::forward< Source >( src ) ).swap( *this );
            }

            /* *************************************************** */

            PathImplT& operator=( SAA_in const PathImplT& path )
            {
                cpp::copy< base_type >( path ).swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in PathImplT&& path )
            {
                path.swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in const base_type& path )
            {
                cpp::copy< base_type >( path ).swap( *this );
                return *this;
            }

            PathImplT& operator=( SAA_in base_type&& path )
            {
                path.swap( *this );
                return *this;
            }

            template
            <
                typename Source
            >
            PathImplT& operator=( SAA_in Source&& src )
            {
                base_type( std::forward< Source >( src ) ).swap( *this );
                return *this;
            }
        };

        typedef PathImplT< os::isWindows > PathType;
        typedef PathType path;

        /**
         * @brief directory_iterator wrapper which takes only fs::path
         */

        class directory_iterator :
            public detail::bfs::directory_iterator
        {
        public:

            typedef detail::bfs::directory_iterator base_type;

            directory_iterator()
            {
            }

            explicit directory_iterator(
                SAA_in          const path&                 path
                )
                :
                base_type( path )
            {
            }

            explicit directory_iterator(
                SAA_in          const path&                 path,
                SAA_out         eh::error_code&             ec
                )
                :
                base_type( path, ec )
            {
            }

            explicit directory_iterator(
                SAA_in          path&&                      path
                )
                :
                base_type( std::forward< PathType >( path ) )
            {
            }

            explicit directory_iterator(
                SAA_in          path&&                      path,
                SAA_out         eh::error_code&             ec
                )
                :
                base_type( std::forward< PathType >( path ), ec )
            {
            }
        };

        /**
         * @brief recursive_directory_iterator wrapper which takes only fs::path
         */

        class recursive_directory_iterator :
            public detail::bfs::recursive_directory_iterator
        {
        public:

            typedef detail::bfs::recursive_directory_iterator base_type;

            recursive_directory_iterator()
            {
            }

            explicit recursive_directory_iterator(
                SAA_in          const path&                 path
                )
                :
                base_type( path )
            {
            }

            explicit recursive_directory_iterator(
                SAA_in          const path&                 path,
                SAA_out         eh::error_code&             ec
                )
                :
                base_type( path, ec )
            {
            }

            explicit recursive_directory_iterator(
                SAA_in          path&&                      path
                )
                :
                base_type( std::forward< PathType >( path ) )
            {
            }

            explicit recursive_directory_iterator(
                SAA_in          path&&                      path,
                SAA_out         eh::error_code&             ec
                )
                :
                base_type( std::forward< PathType >( path ), ec )
            {
            }
        };

        /******************************************************************
         * Overloads of boost::filesystem APIs which take fs::path
         *****************************************************************/

        /*
         * fs::absolute
         */

        inline path absolute(
            SAA_in          const path&                 path,
            SAA_in          const fs::path&             base = detail::bfs::current_path()
            )
        {
            return detail::bfs::absolute( path, base );
        }

        /*
         * fs::current_path
         */

        inline path current_path()
        {
            return detail::bfs::current_path();
        }

        inline void current_path(
            SAA_in          const path&                 path
            )
        {
            detail::bfs::current_path( path );
        }

        inline void current_path(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::current_path( path, ec );
        }

        /*
         * fs::create_directory_symlink
         */

        inline void create_directory_symlink(
            SAA_in          const path&                 to,
            SAA_in          const path&                 from
            )
        {
            detail::bfs::create_directory_symlink( to, from );
        }

        inline void create_directory_symlink(
            SAA_in          const path&                 to,
            SAA_in          const path&                 from,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::create_directory_symlink( to, from, ec );
        }

        /*
         * fs::create_symlink
         */

        inline void create_symlink(
            SAA_in          const path&                 to,
            SAA_in          const path&                 link
            )
        {
            detail::bfs::create_symlink( to, link );
        }

        inline void create_symlink(
            SAA_in          const path&                 to,
            SAA_in          const path&                 link,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::create_symlink( to, link, ec );
        }

        /*
         * fs::symlink_status
         */

        inline file_status symlink_status(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::symlink_status( path );
        }

        inline file_status symlink_status(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::symlink_status( path, ec );
        }

        /*
         * fs::exists
         */

        inline bool exists(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::exists( path );
        }

        inline bool exists(
            SAA_in          const file_status&          status
            )
        {
            return detail::bfs::exists( status );
        }

        inline bool exists(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::exists( path, ec );
        }

        inline bool path_exists(
            SAA_in          const path&                 path
            )
        {
            const auto status = symlink_status( path );

            return detail::bfs::exists( status );
        }

        inline bool path_exists(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            const auto status = symlink_status( path, ec );

            if( ec )
            {
                return false;
            }

            return detail::bfs::exists( status );
        }

        /*
         * fs::file_size
         */

        inline std::uint64_t file_size(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::file_size( path );
        }

        inline std::uint64_t file_size(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::file_size( path, ec );
        }

        /*
         * fs::is_directory
         */

        inline bool is_directory(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::is_directory( path );
        }

        inline bool is_directory(
            SAA_in          const file_status&          status
            )
        {
            return detail::bfs::is_directory( status );
        }

        inline bool is_directory(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::is_directory( path, ec );
        }

        /*
         * fs::is_other
         */

        inline bool is_other(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::is_other( path );
        }

        inline bool is_other(
            SAA_in          const file_status&          status
            )
        {
            return detail::bfs::is_other( status );
        }

        inline bool is_other(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::is_other( path, ec );
        }

        /*
         * fs::is_regular_file
         */

        inline bool is_regular_file(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::is_regular_file( path );
        }

        inline bool is_regular_file(
            SAA_in          const file_status&          status
            )
        {
            return detail::bfs::is_regular_file( status );
        }

        inline bool is_regular_file(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::is_regular_file( path, ec );
        }

        /*
         * fs::is_symlink
         */

        inline bool is_symlink(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::is_symlink( path );
        }

        inline bool is_symlink(
            SAA_in          const file_status&          status
            )
        {
            return detail::bfs::is_symlink( status );
        }

        inline bool is_symlink(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::is_symlink( path, ec );
        }

        /*
         * fs::is_empty
         */

        inline bool is_empty(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::is_empty( path );
        }

        inline bool is_empty(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::is_empty( path, ec );
        }

        /*
         * fs::equivalent
         */

        inline bool equivalent(
            SAA_in          const path&                 path1,
            SAA_in          const path&                 path2
            )
        {
            return detail::bfs::equivalent( path1, path2 );
        }

        inline bool equivalent(
            SAA_in          const path&                 path1,
            SAA_in          const path&                 path2,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::equivalent( path1, path2, ec );
        }

        /*
         * fs::last_write_time
         */

        inline std::time_t last_write_time(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::last_write_time( path );
        }

        inline std::time_t last_write_time(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::last_write_time( path, ec );
        }

        inline void last_write_time(
            SAA_in          const path&                 path,
            SAA_in          const std::time_t           newTime
            )
        {
            detail::bfs::last_write_time( path, newTime );
        }

        inline void last_write_time(
            SAA_in          const path&                 path,
            SAA_in          const std::time_t           newTime,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::last_write_time( path, newTime, ec );
        }

        /*
         * fs::permissions
         */

        inline void permissions(
            SAA_in          const path&                 path,
            SAA_in          const perms                 prms
            )
        {
            detail::bfs::permissions( path, prms );
        }

        inline void permissions(
            SAA_in          const path&                 path,
            SAA_in          const perms                 prms,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::permissions( path, prms, ec );
        }

        /*
         * fs::read_symlink
         */

        inline path read_symlink(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::read_symlink( path );
        }

        inline path read_symlink(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::read_symlink( path, ec );
        }

        /*
         * fs::resize_file
         */

        inline void resize_file(
            SAA_in          const path&                 path,
            SAA_in          const std::uint64_t         size
            )
        {
            detail::bfs::resize_file( path, size );
        }

        inline void resize_file(
            SAA_in          const path&                 path,
            SAA_in          const std::uint64_t         size,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::resize_file( path, size, ec );
        }

        /*
         * fs::copy
         */

        inline void copy(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath
            )
        {
            detail::bfs::copy( sourcePath, targetPath );
        }

        inline void copy(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::copy( sourcePath, targetPath, ec );
        }

        /*
         * fs::copy_file
         */

        inline void copy_file(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath
            )
        {
            detail::bfs::copy_file( sourcePath, targetPath );
        }

        inline void copy_file(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::copy_file( sourcePath, targetPath, ec );
        }

        /*
         * fs::copy_symlink
         */

        inline void copy_symlink(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath
            )
        {
            detail::bfs::copy_symlink( sourcePath, targetPath );
        }

        inline void copy_symlink(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::copy_symlink( sourcePath, targetPath, ec );
        }

        /*
         * fs::copy_directory
         */

        inline void copy_directory(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath
            )
        {
            detail::bfs::copy_directory( sourcePath, targetPath );
        }

        inline void copy_directory(
            SAA_in          const path&                 sourcePath,
            SAA_in          const path&                 targetPath,
            SAA_out         eh::error_code&             ec
            )
        {
            detail::bfs::copy_directory( sourcePath, targetPath, ec );
        }

        /*
         * fs::space
         */

        inline space_info space(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::space( path );
        }

        inline space_info space(
            SAA_in          const path&                 path,
            SAA_out         eh::error_code&             ec
            )
        {
            return detail::bfs::space( path, ec );
        }

        /*
         * fs::status
         */

        inline file_status status(
            SAA_in          const path&                 path
            )
        {
            return detail::bfs::status( path );
        }


        /*
         * fs::temp_directory_path
         */

        inline path temp_directory_path()
        {
            /*
             * The new boost implementation is reading the value from TMP / TEMP environment
             * variables, but it does not convert the backslashes which the older boost
             * implementations used to do as they were using GetTempPathW API, so we need to
             * call make_preferred() before we return the path to make sure we can handle
             * temp paths specified with backslashes (e.g. by msys / cygwin shells)
             */

            auto result = detail::bfs::temp_directory_path();
            result.make_preferred();
            return result;
        }

        inline path temp_directory_path(
            SAA_out         eh::error_code&             ec
            )
        {
            /*
             * See important comment above about why we need to call make_preferred()
             */

            auto result = detail::bfs::temp_directory_path( ec );

            if( ! ec )
            {
                result.make_preferred();
            }

            return result;
        }

        /*
         * This namespace will be to park some 'unsafe' APIs not meant to be
         * used directly
         *
         * These APIs may be unsafe due to bugs in boost or due to other reasons
         * such as being unstable due to AV or other intrusive software
         *
         * In either case a safe wrapper should be provided in FsUtils.h --
         * it will still be in the fs:: namespace, but the API will be prefixed
         * with 'safe' prefix (e.g. fs::safeRename)
         */

        namespace unsafe
        {
            /*
             * fs::create_directories
             */

            inline bool create_directories(
                SAA_in          const path&                 path
                )
            {
                return detail::bfs::create_directories( path );
            }

            inline bool create_directories(
                SAA_in          const path&                 path,
                SAA_out         eh::error_code&             ec
                )
            {
                return detail::bfs::create_directories( path, ec );
            }

            /*
             * fs::remove_all
             */

            inline std::uint64_t remove_all(
                SAA_in          const path&                 path
                )
            {
                return detail::bfs::remove_all( path );
            }

            inline std::uint64_t remove_all(
                SAA_in          const path&                 path,
                SAA_out         eh::error_code&             ec
                )
            {
                return detail::bfs::remove_all( path, ec );
            }

            /*
             * fs::remove
             */

            inline bool remove(
                SAA_in          const path&                 path
                )
            {
                return detail::bfs::remove( path );
            }

            inline bool remove(
                SAA_in          const path&                 path,
                SAA_out         eh::error_code&             ec
                )
            {
                return detail::bfs::remove( path, ec );
            }

            /*
             * fs::create_directory
             */

            inline bool create_directory(
                SAA_in          const path&                 path
                )
            {
                return detail::bfs::create_directory( path );
            }

            inline bool create_directory(
                SAA_in          const path&                 path,
                SAA_out         eh::error_code&             ec
                )
            {
                return detail::bfs::create_directory( path, ec );
            }

            /*
             * fs::rename
             */

            inline void rename(
                SAA_in          const path&                 oldPath,
                SAA_in          const path&                 newPath
                )
            {
                detail::bfs::rename( oldPath, newPath );
            }

            inline void rename(
                SAA_in          const path&                 oldPath,
                SAA_in          const path&                 newPath,
                SAA_out         eh::error_code&             ec
                )
            {
                detail::bfs::rename( oldPath, newPath, ec );
            }

        } // unsafe

        inline std::string normalizePathParameterForPrint( SAA_in const fs::path& path )
        {
            return resolveMessage(
                BL_MSG()
                    << detail::WinLfnUtils::chk2RemovePrefix( cpp::copy( path ) )
                );
        }

    } // fs

} // bl

namespace std
{
    /*
     * Provide specialization of hash function in the std namespace
     * for fs::path. This will allow us to use fs::path as key in the
     * new C++11 std containers (e.g. std::unordered_map)
     */

    template
    <
    >
    struct hash< bl::fs::path >
    {
        std::size_t operator()( const bl::fs::path& path ) const
        {
            boost::hash< bl::fs::path > hasher;
            return hasher( path );
        }
    };

} // std

#endif /* __BL_OSIMPLPLATFORMCOMMON_H_ */
