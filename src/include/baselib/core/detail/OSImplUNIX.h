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

#ifndef __BL_OSIMPLLINUX_H_
#define __BL_OSIMPLLINUX_H_

#if defined( _WIN32 )
#error This file can only be included when compiling on Linux platform
#endif

#include <baselib/core/detail/OSImplPlatformCommon.h>
#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/Checksum.h>
#include <baselib/core/CPP.h>
#include <baselib/core/StringUtils.h>

#include <cstring>
#include <cerrno>
#include <cstdint>
#include <type_traits>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/resource.h>

#ifdef __linux__

/*
 * This is for ::prctl API which is only available on Linux
 */

#include <sys/prctl.h>

#endif

#if !defined( BL_DEVENV_VERSION ) || BL_DEVENV_VERSION < 3

/*
 * stdio_filebuf.h is a GNU GCC extension, so only can include it and use it
 * if we are compiling with GCC
 *
 * Since in BL_DEVENV_VERSION 3 and above we are adding support for Darwin
 * with Clang and the libc++ standard library this file will not be present
 * and in this case we are going to use local implementations based on
 * Boost I/O streams
 *
 * For BL_DEVENV_VERSION < 3 we will keep the original GNU based filebuf
 * implementation for backward compatibility reasons
 */

#include <ext/stdio_filebuf.h>

#endif

#include <sys/syscall.h>

/*
 * For System V semaphores
 */

#include <sys/ipc.h>
#include <sys/sem.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

#if _FILE_OFFSET_BITS != 64
#error Large file support (LFS) on Linux must be enabled; please, define _FILE_OFFSET_BITS=64
#endif

#ifndef NZERO
/*
 * Default process priority
 * This is workaround for Clang
 */
#define NZERO       20
#endif

namespace bl
{
    namespace os
    {
        namespace detail
        {
            /**
             * @brief class GlobalProcessInit (for UNIX platforms)
             */

            template
            <
                typename E = void
            >
            class GlobalProcessInitT FINAL : protected GlobalProcessInitBase
            {
                BL_NO_COPY_OR_MOVE( GlobalProcessInitT )

            public:

                GlobalProcessInitT()
                {
                    idempotentGlobalProcessInitHelpers();
                }

                ~GlobalProcessInitT() NOEXCEPT
                {
                }

                static void idempotentGlobalProcessInitHelpers()
                {
                    idempotentGlobalProcessInitBaseHelpers();
                }
            };

            typedef GlobalProcessInitT<> GlobalProcessInit;

            /**
             * @brief class OSImpl (for UNIX platforms)
             */

            template
            <
                typename E = void
            >
            class OSImplT : public OSImplBase
            {
                BL_DECLARE_STATIC( OSImplT )

            private:

                enum
                {
                    GET_PASSWD_BUFFER_LENGTH = 512
                };

                static const char*                  g_procSelfExeSymlink;

                class EncapsulatedPidHandle FINAL
                {
                    BL_NO_COPY( EncapsulatedPidHandle )

                private:

                    mutex               m_lock;
                    pid_t               m_pid;
                    int                 m_exitCode;
                    bool                m_terminateOnDestruction;

                    enum : long
                    {
                        PID_WAIT_SLEEP_IN_MILLISECONDS = 200,
                    };

                public:

                    void sendSignal(
                        SAA_in    const int     signal,
                        SAA_in    const bool    includeSubprocesses
                        )
                    {
                        const auto rc = ::kill(
                            includeSubprocesses ? -m_pid : m_pid,
                            signal
                            );

                        if( rc == -1 && errno == ESRCH )
                        {
                            /*
                             * ESRCH means 'No process or process group found for the specified pid'
                             * The process has already exited so just return here
                             */

                            return;
                        }

                        BL_CHK_T(
                            false,
                            rc == 0,
                            createException( "kill", errno ),
                            BL_MSG()
                                << "Failed to send signal to process "
                                << ( includeSubprocesses ? "group " : "" )
                                << "with pid "
                                << m_pid
                            );
                    }

                    EncapsulatedPidHandle( SAA_in_opt bool terminateOnDestruction = true )
                        :
                        m_pid( 0 ),
                        m_exitCode( 0 ),
                        m_terminateOnDestruction( terminateOnDestruction )
                    {
                    }

                    EncapsulatedPidHandle(
                        SAA_in      const pid_t pid,
                        SAA_in_opt  bool terminateOnDestruction = true
                        )
                        :
                        m_pid( pid ),
                        m_exitCode( 0 ),
                        m_terminateOnDestruction( terminateOnDestruction )
                    {
                    }

                    ~EncapsulatedPidHandle() NOEXCEPT
                    {
                        BL_NOEXCEPT_BEGIN()

                        if( tryTimedAwaitTermination( 2000 /* timeoutMs */ ) )
                        {
                            return;
                        }

                        if( m_terminateOnDestruction )
                        {
                            terminateProcess( false /* force */, true /* includeSubprocesses */ );

                            if( ! tryTimedAwaitTermination( 2000 /* timeoutMs */ ) )
                            {
                                terminateProcess( true /* force */, true /* includeSubprocesses */ );
                            }
                        }

                        BL_NOEXCEPT_END()
                    }

                    void setPid( SAA_in const pid_t pid ) NOEXCEPT
                    {
                        BL_ASSERT( 0 == m_pid );

                        m_pid = pid;
                    }

                    pid_t getPid() const NOEXCEPT
                    {
                        return m_pid;
                    }

                    int getExitCode() const NOEXCEPT
                    {
                        return m_exitCode;
                    }

                    bool tryTimedAwaitTermination( SAA_in_opt const int timeoutMs = TIMEOUT_INFINITE )
                    {
                        BL_MUTEX_GUARD( m_lock );

                        if( 0 == m_pid )
                        {
                            /*
                             * The process has already terminated and has been
                             * waited on
                             */

                            return true;
                        }

                        int status;
                        bool isExitStatusValid = false;
                        pid_t pid;

                        errno = 0;

                        /*
                         * WCONTINUED and WUNTRACED mean that we request status for processes
                         * which have been stopped or continued
                         */

                        const int options = ( WCONTINUED | WUNTRACED );

                        /*
                         * Returns true if we should continue calling waitpid and false if not
                         */

                        const auto cbHandleWaitPidCall = [ & ]( SAA_in const bool allowTimeout ) -> bool
                        {
                            if( pid == m_pid )
                            {
                                /*
                                 * Only if pid == m_pid the status parameter is valid
                                 */

                                const auto exited =
                                    ( WIFEXITED( status ) || WIFSIGNALED( status ) );

                                if( exited )
                                {
                                    isExitStatusValid = true;
                                    return false;
                                }

                                /*
                                 * This is an odd case where the child has not exited, but
                                 * we got status - e.g. the process was stopped or continued
                                 * (WIFSTOPPED or WIFCONTINUED)
                                 *
                                 * In either case we should continue waiting on it because
                                 * we only care when it was terminated
                                 */

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "waitpid returned WIFSTOPPED || WIFCONTINUED for pid "
                                        << pid
                                    );

                                 BL_ASSERT( WIFSTOPPED( status ) || WIFCONTINUED( status ) );
                                 return true;
                            }
                            else
                            {
                                /*
                                 * In this case status is undefined and we have the following cases:
                                 *
                                 * -- pid == -1 : signal was delivered to the calling process (EINTR)
                                 *    or some other error has occurred (e.g. there is no existing
                                 *    child process that has not been waited for - ECHILD)
                                 *
                                 * -- pid == 0 : status is not available for the specified pid
                                 *    this can only happen if we pass WNOHANG which we don't, so this
                                 *    error should be treated similarly to ECHILD
                                 */

                                if( pid == -1 )
                                {
                                    if( errno == EINTR )
                                    {
                                        /*
                                         * This is the case where the wait terminated due to a signal
                                         * being delivered to the calling process
                                         *
                                         * In this case we should simply continue waiting
                                         */

                                        BL_LOG(
                                            Logging::debug(),
                                            BL_MSG()
                                                << "waitpid returned -1 with errno equal to EINTR"
                                            );

                                        return true;
                                    }

                                    BL_THROW(
                                        createException( "waitpid", errno ),
                                        BL_MSG()
                                            << "Error while waiting on pid "
                                            << m_pid
                                        );
                                }

                                /*
                                 * If we are here the only valid case is when pid == 0
                                 * which means that status is not available and in this
                                 * case we return false if allowTimeout is true or fail
                                 * if allowTimeout is false since in this case we don't
                                 * expect a timeout
                                 */

                                if( pid == 0 && allowTimeout )
                                {
                                    return false;
                                }

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "waitpid returned "
                                        << pid
                                        << " while waiting on "
                                        << m_pid
                                        << "; errno is "
                                        << errno
                                    );

                                BL_THROW(
                                    UnexpectedException(),
                                    BL_MSG()
                                        << "Unexpected error while waiting on pid "
                                        << m_pid
                                        << "; waitpid return value is "
                                        << pid
                                        << " and errno is "
                                        << errno
                                    );
                            }

                            return false;

                        }; // cbHandleWaitPidCall lambda

                        if( TIMEOUT_INFINITE == timeoutMs )
                        {
                            /*
                             * The wait until the process terminates case
                             */

                            for( ;; )
                            {
                                pid = ::waitpid( m_pid, &status, options );

                                if( cbHandleWaitPidCall( false /* allowTimeout */ ) )
                                {
                                    continue;
                                }

                                /*
                                 * We should only get here when the process exits and
                                 * in this case the status should be valid
                                 */

                                BL_ASSERT( isExitStatusValid );
                                break;
                            }
                        }
                        else
                        {
                            /*
                             * This is the case where timeout was specified and we need to use
                             * WNOHANG option to ask waitpid to return immediately
                             */

                            const auto until =
                                time::microsec_clock::universal_time() + time::milliseconds( timeoutMs );

                            BL_LOG(
                                Logging::trace(),
                                BL_MSG()
                                    << "tryTimedAwaitTermination() starts waiting for pid "
                                    << m_pid
                                    << " with a timeout "
                                    << timeoutMs
                                    << "(ms); until="
                                    << until
                                );

                            for( ;; )
                            {
                                pid = ::waitpid( m_pid, &status, options | WNOHANG );

                                if( cbHandleWaitPidCall( true /* allowTimeout */ ) )
                                {
                                    continue;
                                }

                                if( isExitStatusValid )
                                {
                                    /*
                                     * The child process has exited within the specified timeout
                                     */

                                    break;
                                }

                                if( time::microsec_clock::universal_time() >= until )
                                {
                                    /*
                                     * The timeout has expired, but the process did not exit
                                     */

                                    BL_LOG(
                                        Logging::trace(),
                                        BL_MSG()
                                            << "tryTimedAwaitTermination() exited for pid "
                                            << m_pid
                                            << " with a timeout"
                                        );

                                    break;
                                }

                                /*
                                 * We are within the timeout, but the process is still running
                                 * Sleep for a while and then try again
                                 */

                                sleep( time::milliseconds( PID_WAIT_SLEEP_IN_MILLISECONDS ) );
                            }
                        }

                        if( isExitStatusValid )
                        {
                            if( WIFEXITED( status ) )
                            {
                                m_exitCode = WEXITSTATUS( status );
                            }
                            else if( WIFSIGNALED( status ) )
                            {
                                m_exitCode = ( 0 - WTERMSIG( status ) );
                            }
                            else
                            {
                                isExitStatusValid = false;
                            }
                        }

                        if( isExitStatusValid )
                        {
                            m_pid = 0;
                        }
                        else if( TIMEOUT_INFINITE == timeoutMs )
                        {
                            /*
                             * This is a weird case where we have requested to wait indefinitely
                             * but we didn't obtain the status
                             */

                            BL_THROW(
                                UnexpectedException(),
                                BL_MSG()
                                    << "Unexpected error while waiting on pid "
                                    << m_pid
                                );
                        }

                        /*
                         * Return only if the exit status was valid (in which
                         * case m_pid will be set to zero)
                         */

                        return ( 0 == m_pid );
                    }

                    void terminateProcess(
                        SAA_in    const bool    force,
                        SAA_in    const bool    includeSubprocesses
                        )
                    {
                        BL_MUTEX_GUARD( m_lock );

                        if( 0 == m_pid )
                        {
                            /*
                             * The process has already terminated and has been
                             * waited on
                             */

                            return;
                        }

                        sendSignal( force ? SIGKILL : SIGTERM, includeSubprocesses );
                    }

                    void sendProcessStopEvent( SAA_in const bool includeSubprocesses )
                    {
                        BL_MUTEX_GUARD( m_lock );

                        BL_CHK(
                            false,
                            0 != m_pid,
                            BL_MSG()
                                << "Attempting to send stop signal to a process which has been terminated already"
                            );

                        sendSignal( SIGINT, includeSubprocesses );
                    }
                };

                class CloseFileDescriptorDeleter
                {
                public:

                    int get_null() const NOEXCEPT
                    {
                        return -1;
                    }

                    void operator()( SAA_in const int fd ) const NOEXCEPT
                    {
                        BL_ASSERT( fd );
                        BL_ASSERT( -1 != fd );

                        BL_VERIFY( 0 == ::close( fd ) );
                    }
                };

                /*
                 * See BL_DEVENV_VERSION < 3 comment above for more details
                 */

#if !defined( BL_DEVENV_VERSION ) || BL_DEVENV_VERSION < 3
                template
                <
                    typename BASE,
                    std::ios_base::openmode mode
                >
                class StdioStream : public BASE
                {
                    BL_NO_COPY_OR_MOVE( StdioStream )

                    __gnu_cxx::stdio_filebuf< char > m_filebuf;
                public:

                    StdioStream( SAA_inout std::FILE* fileptr )
                        :
                        m_filebuf( fileptr, mode )
                    {
                        BASE::rdbuf( &m_filebuf );
                    }
                };

                typedef StdioStream< std::istream, std::ios_base::in >      stdio_istream_t;
                typedef StdioStream< std::ostream, std::ios_base::out >     stdio_ostream_t;
#else
                typedef stdio_istream_char_t                                stdio_istream_t;
                typedef stdio_ostream_char_t                                stdio_ostream_t;
#endif

                typedef cpp::UniqueHandle< int, CloseFileDescriptorDeleter > fd_ref;
                typedef std::pair< fd_ref, fd_ref > stdio_pipe_t;

                static void* getModule( SAA_in const library_handle_t handle ) NOEXCEPT
                {
                    return reinterpret_cast< void* >( handle );
                }

                static EncapsulatedPidHandle* getHandle( SAA_in const process_handle_t handle ) NOEXCEPT
                {
                    return reinterpret_cast< EncapsulatedPidHandle* >( handle );
                }

                static eh::error_code createSystemErrorCode( SAA_in const int errorCode )
                {
                    /*
                     * The errorCode is assumed to be errno if non-zero thus use eh::generic_category
                     * error category; if zero then we throw ENOTSUP
                     */

                    return eh::error_code( errorCode ? errorCode : ENOTSUP, eh::generic_category() );
                }

                static SystemException createException(
                    SAA_in_opt      const std::string&      locationOrAPI,
                    SAA_in_opt      const int               errorCode = 0,
                    SAA_in_opt      const std::string&      fileName = str::empty()
                    )
                {
                    const auto code = createSystemErrorCode( errorCode );

                    auto exception = SystemException::create( code, locationOrAPI );

                    if( ! fileName.empty() )
                    {
                        exception << eh::errinfo_file_name( fileName );
                    }

                    return exception;
                }

                static int setenvWrapper(
                    SAA_in          const char*                                 name,
                    SAA_in          const char*                                 value
                    ) NOEXCEPT
                {
                    /*
                     * Even though POSIX systems allow environment variables without a value (FOO=),
                     * we deliberately fail in such cases because we want to enforce consistent behaviour
                     */

                    if( ! value || ! *value )
                    {
                        return errno = EINVAL;
                    }

                    return ::setenv( name, value, 1 /* overwrite */ );
                }

                static bool tryMakeFileDescriptorPrivate( SAA_in const int fd )
                {
                    const int flags = ::fcntl( fd, F_GETFD );

                    if( flags < 0 )
                    {
                        return false;
                    }

                    return 0 == ::fcntl( fd, F_SETFD, flags | FD_CLOEXEC );
                }

                static void makeFileDescriptorPrivate( SAA_in const int fd )
                {
                    BL_CHK_ERRNO(
                        false,
                        tryMakeFileDescriptorPrivate( fd ),
                        BL_MSG()
                            << "Cannot set the file descriptor close-on-exec flag"
                        );
                }

                static stdio_pipe_t createPipe()
                {
                    stdio_pipe_t pipe;

                    int fds[ 2 ];

                    BL_CHK_ERRNO(
                        -1,
                        ::pipe( fds ),
                        BL_MSG()
                            << "Cannot create a pipe"
                        );

                    pipe.first.reset( fds[ 0 ] );
                    pipe.second.reset( fds[ 1 ] );

                    return pipe;
                }

                static stdio_file_ptr convert2StdioFile(
                    SAA_inout           fd_ref&                                 fd,
                    SAA_in              const bool                              readOnly
                    )
                {
                    stdio_file_ptr result;

                    BL_CHK_ERRNO(
                        nullptr,
                        ( result = stdio_file_ptr::attach( ::fdopen( fd.get(), readOnly ? "r" : "w" ) ) ),
                        BL_MSG()
                            << "Cannot open file stream"
                        );

                    BL_ASSERT( fd == ::fileno( result.get() ) );

                    /*
                     * The file descriptor is now owned by the file stream object
                     */

                    fd.release();

                    return result;
                }

            public:

                typedef OSImplT< E > this_type;

                /*****************************************************
                 * Shared library support
                 */

                static library_handle_t loadLibrary( SAA_in const std::string& name )
                {
                    ::dlerror();
                    const auto handle = ::dlopen( name.c_str(), RTLD_NOW );

                    BL_CHK_T(
                        true,
                        handle == nullptr,
                        createException( "dlopen", errno, name ),
                        BL_MSG()
                            << "Shared library '"
                            << name
                            << "' cannot be loaded: "
                            << ::dlerror()
                        );

                    return reinterpret_cast< library_handle_t >( handle );
                }

                static void freeLibrary( SAA_in const library_handle_t handle )
                {
                    BL_ASSERT( handle );
                    ::dlerror();
                    BL_VERIFY( 0 == ::dlclose( getModule( handle ) ) );
                }

                static void* getProcAddress(
                    SAA_in      const library_handle_t      handle,
                    SAA_in      const std::string&          name
                    )
                {
                    BL_ASSERT( handle );

                    ::dlerror();
                    const auto pfn = ::dlsym( getModule( handle ), name.c_str() );

                    /*
                     * Note: nullptr is actually a valid symbol resolution in some cases,
                     * so test dlerror instead
                     */

                    const auto err = ::dlerror();

                    BL_CHK_T(
                        false,
                        err == nullptr,
                        createException( "dlsym", errno ),
                        BL_MSG()
                            << "Cannot find address of '"
                            << name
                            << "' function in a shared library: "
                            << err
                        );

                    return pfn;
                }

                static std::string getCurrentExecutablePath()
                {
#ifdef __APPLE__
                    /*
                     * Based on the following posts:
                     *
                     * https://astojanov.wordpress.com/2011/11/16/mac-os-x-resolve-absolute-path-using-process-pid
                     * http://stackoverflow.com/questions/799679/programatically-retrieving-the-absolute-path-of-an-os-x-command-line-app/1024933#1024933
                     */

                    char buffer[ PROC_PIDPATHINFO_MAXSIZE ];

                    BL_CHK_T(
                        false,
                        ::proc_pidpath( ::getpid(), buffer, sizeof( buffer ) ) > 0,
                        createException( "proc_pidpath", errno ),
                        BL_MSG()
                            << "Cannot obtain current executable path"
                        );

                    return std::string( buffer );
#else
                    ssize_t rc = INT_MAX;
                    int size = 0;
                    const int increase = 1024;

                    cpp::SafeUniquePtr< char[] > path;

                    while( rc >= size )
                    {
                        /*
                         * Increase the buffer size until it is large enough to fit the
                         * path written by readlink.
                         *
                         * We cannot check the path length ahead of time using lstat
                         * because the /proc filesystem on Linux is not POSIX-compliant
                         * (the resulting st_size field is incorrect).
                         *
                         * We cannot use PATH_MAX because POSIX allows paths to be larger
                         * than this value.
                         */

                        size += increase;
                        path = cpp::SafeUniquePtr< char[] >::attach( new char[ size ] );

                        rc = ::readlink( g_procSelfExeSymlink, path.get(), size );

                        BL_CHK_T(
                            true,
                            rc == -1,
                            createException( "readlink", errno ),
                            BL_MSG()
                                << "Cannot obtain current executable path"
                            );
                    }

                    path.get()[ rc ] = '\0';

                    return std::string( path.get() );
#endif
                }

                /*****************************************************
                 * Environment variable support
                 */

                static void setEnvironmentVariable(
                    SAA_in          const std::string&                          name,
                    SAA_in          const std::string&                          value
                    )
                {
                    BL_MUTEX_GUARD( g_envLock );

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        false,
                        0 == setenvWrapper( name.c_str(), value.c_str() ),
                        BL_MSG()
                            << "Cannot set environment variable '"
                            << name
                            << "' to '"
                            << value
                            << "'"
                        );
                }

                static void unsetEnvironmentVariable( SAA_in const std::string& name )
                {
                    BL_MUTEX_GUARD( g_envLock );

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        false,
                        0 == ::unsetenv( name.c_str() ),
                        BL_MSG()
                            << "Cannot unset environment variable '"
                            << name
                            << "'"
                        );
                }

                /*****************************************************
                 * FILE* to i/o stream support
                 */

                static auto fileptr2istream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< std::istream >
                {
                    cpp::SafeUniquePtr< std::istream > result;

                    result.reset( new stdio_istream_t( fileptr ) );

                    /*
                     * For std::istream we only want to set std::ios::badbit
                     * because the fail bit could be set if you try to read
                     * past the end of file, which is not uncommon in user code
                     * (e.g. if you try to call std::getline)
                     */

                    result -> exceptions( std::ios::badbit );

                    return result;
                }

                static auto fileptr2ostream( SAA_inout std::FILE* fileptr ) -> cpp::SafeUniquePtr< std::ostream >
                {
                    cpp::SafeUniquePtr< std::ostream > result;

                    result.reset( new stdio_ostream_t( fileptr ) );

                    /*
                     * For std::ostream we want to set both std::ios::badbit
                     * and std::ios::failbit because if we fail to write this
                     * is an error which must be reported to the user
                     */

                    result -> exceptions( std::ios::badbit | std::ios::failbit );

                    return result;
                }

                /*****************************************************
                 * Process creation and termination support
                 */

                static bool tryTimedAwaitTermination(
                    SAA_in      const process_handle_t      handle,
                    SAA_out_opt int*                        exitCode = nullptr,
                    SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
                    )
                {
                    auto* unwrappedHandle = getHandle( handle );

                    if( unwrappedHandle -> tryTimedAwaitTermination( timeoutMs ) )
                    {
                        if( exitCode )
                        {
                            *exitCode = unwrappedHandle -> getExitCode();
                        }

                        return true;
                    }

                    return false;
                }

                static int tryAwaitTermination(
                    SAA_in      const process_handle_t      handle,
                    SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
                    )
                {
                    auto* unwrappedHandle = getHandle( handle );

                    if( ! unwrappedHandle -> tryTimedAwaitTermination( timeoutMs ) )
                    {
                        BL_THROW(
                            createException( "waitpid", EINPROGRESS ),
                            BL_MSG()
                                << "Process did not terminate within specified timeout interval ("
                                << timeoutMs
                                << "ms)"
                            );
                    }

                    return unwrappedHandle -> getExitCode();
                }

                static void terminateProcess(
                    SAA_in      const process_handle_t      handle,
                    SAA_in_opt  const bool                  force = true,
                    SAA_in_opt  const bool                  includeSubprocesses = false
                    )
                {
                    getHandle( handle ) -> terminateProcess( force, includeSubprocesses );
                }

                static void sendProcessStopEvent(
                    SAA_in      const process_handle_t      handle,
                    SAA_in      const bool                  includeSubprocesses
                    )
                {
                    getHandle( handle ) -> sendProcessStopEvent( includeSubprocesses );
                }

                static void sendSignal(
                    SAA_in      const process_handle_t      handle,
                    SAA_in      const int                   signal,
                    SAA_in      const bool                  includeSubprocesses
                    )
                {
                    getHandle( handle ) -> sendSignal( signal, includeSubprocesses );
                }

                static process_handle_t createProcess(
                    SAA_in          const std::string&                          commandLine,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    return createProcess( str::empty(), commandLine, flags, callbackIos, callbackFile );
                }

                static process_handle_t createProcess(
                    SAA_in          const std::vector< std::string >&           commandArguments,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    return createProcess( str::empty(), commandArguments, flags, callbackIos, callbackFile );
                }

                static process_handle_t createProcess(
                    SAA_in          const std::string&                          userName,
                    SAA_in          const std::string&                          commandLine,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    return createProcess( userName, splitCommandLineArguments( commandLine ), flags, callbackIos, callbackFile );
                }

                static std::vector< std::string > splitCommandLineArguments( SAA_in const std::string& commandLine )
                {
                    std::vector< std::string > args;

                    try
                    {
                        str::escaped_list_separator< char > els( "\\", " ", "\"\'");
                        str::tokenizer< str::escaped_list_separator< char > > tokens( commandLine, els );

                        for( const auto arg : tokens )
                        {
                            args.emplace_back( arg );
                        }
                    }
                    catch( str::escaped_list_error& e )
                    {
                        BL_THROW(
                            ArgumentException()
                                << eh::errinfo_string_value( commandLine ),
                            BL_MSG()
                                << "Invalid command line: "
                                << e.what()
                            );
                    }

                    return args;
                }

                static process_handle_t createProcess(
                    SAA_in          const std::string&                          userName,
                    SAA_in          const std::vector< std::string >&           commandArguments,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    BL_CHK_ARG( ! commandArguments.empty(), commandArguments );

                    /*
                     * Note: any local objects created here must be cleaned up manually
                     * in the Linux case because we replace the process with execvp - see
                     * comment in the child part of the 'if' statement after the fork call
                     */

                    stdio_pipe_t outPipe;
                    stdio_pipe_t errPipe;
                    stdio_pipe_t inPipe;

                    stdio_file_ptr outFilePtr;
                    stdio_file_ptr errFilePtr;
                    stdio_file_ptr inFilePtr;

                    cpp::SafeUniquePtr< std::istream > out;
                    cpp::SafeUniquePtr< std::istream > err;
                    cpp::SafeUniquePtr< std::ostream > in;

                    std::istream* outPtr = nullptr;
                    std::istream* errPtr = nullptr;
                    std::ostream* inPtr = nullptr;

                    const auto cbCloseObjects = [ & ]() -> void
                    {
                        /*
                         * Close the objects in the right order
                         */

                        out.reset();
                        err.reset();
                        in.reset();

                        outFilePtr.reset();
                        errFilePtr.reset();
                        inFilePtr.reset();

                        outPipe.first.reset();
                        outPipe.second.reset();
                        errPipe.first.reset();
                        errPipe.second.reset();
                        inPipe.first.reset();
                        inPipe.second.reset();
                    };

                    BL_SCOPE_EXIT(
                        {
                            cbCloseObjects();
                        }
                        );

                    const auto waitToFinish = ( flags & ProcessCreateFlags::WaitToFinish ) != 0;

                    const auto flagsRedirect = flags & ( ~( ProcessCreateFlags::WaitToFinish | ProcessCreateFlags::DetachProcess ) );

                    const auto detachProcess = ( flags & ProcessCreateFlags::DetachProcess ) != 0;

                    detail::OSImplBase::validateProcessRedirectFlags( flagsRedirect, callbackIos, callbackFile );

                    if( flagsRedirect & ProcessCreateFlags::RedirectStdout )
                    {
                        outPipe = createPipe();
                        outFilePtr = convert2StdioFile( outPipe.first, true /* readOnly */ );

                        if( callbackIos )
                        {
                            out = fileptr2istream( outFilePtr.get() );
                            outPtr = out.get();
                        }
                    }

                    if( flagsRedirect & ProcessCreateFlags::RedirectStderr )
                    {
                        if( flagsRedirect & ProcessCreateFlags::MergeStdoutAndStderr )
                        {
                            /*
                             * This will be ensured by validateProcessRedirectFlags ensuring
                             * that ProcessCreateFlags::RedirectStdout flag is set
                             */

                            BL_ASSERT( outPipe.second.get() );
                            BL_ASSERT( out );
                        }
                        else
                        {
                            errPipe = createPipe();
                            errFilePtr = convert2StdioFile( errPipe.first, true /* readOnly */ );

                            if( callbackIos )
                            {
                                err = fileptr2istream( errFilePtr.get() );
                                errPtr = err.get();
                            }
                        }
                    }

                    if( flagsRedirect & ProcessCreateFlags::RedirectStdin )
                    {
                        inPipe = createPipe();

                        if( flagsRedirect & ProcessCreateFlags::CloseStdin )
                        {
                            inPipe.second.reset();
                        }
                        else
                        {
                            inFilePtr = convert2StdioFile( inPipe.second, false /* readOnly */ );

                            if( callbackIos )
                            {
                                in = fileptr2ostream( inFilePtr.get() );
                                inPtr = in.get();
                            }
                        }
                    }

                    auto tokens = transformCommandLineArguments( userName, commandArguments );

                    BL_LOG(
                        Logging::trace(),
                        BL_MSG()
                            << "createProcess: "
                            << str::joinQuoteFormatted( tokens, " ", " " )
                        );

                    /*
                     * Drop path if present in lead argument
                     */

                    const std::string path = tokens[ 0 ];

                    const auto pos = path.rfind( '/' );
                    if( pos != std::string::npos )
                    {
                        tokens[ 0 ] = path.substr( pos + 1 );
                    }

                    /*
                     * Build a char* array of command line arguments (the last element must be NULL)
                     */

                    std::vector< char* > argv( tokens.size() + 1, nullptr );
                    for( std::size_t i = 0U; i < tokens.size(); ++i )
                    {
                        argv[ i ] = const_cast< char* >( tokens[ i ].c_str() );
                    }

                    cpp::SafeUniquePtr< EncapsulatedPidHandle > pidHandle;
                    pidHandle.reset( new EncapsulatedPidHandle( ! detachProcess /* terminateOnDestruction */ ) );

                    /*
                     * Get the user and group IDs before we fork to avoid forking and then having a problem getting these values.
                     */

                    auto userID = ( ::uid_t ) -1;
                    auto groupID = ( ::gid_t ) -1;

                    if( ! userName.empty() )
                    {
                        getUserCredentials( userName, userID, groupID );

                        BL_CHK_T_USER_FRIENDLY(
                            false,
                            isUserAdministrator(),
                            UnexpectedException()
                                << eh::errinfo_string_value( userName ),
                            BL_MSG()
                                << "Only root can execute processes under a different user account"
                            );
                    }

                    /*
                     * Create a parent-child communication pipe so the child can inform the parent
                     * that a call to execve() has failed
                     */

                    stdio_pipe_t parentChildCommPipe = createPipe();

                    const auto pid = ::fork();

                    BL_CHK_T(
                        -1,
                        pid,
                        createException( "fork", errno ),
                        BL_MSG()
                            << "Fork failed, unable to create a new process"
                        );

                    if( pid == 0 )
                    {
                        /*
                         * Child Process
                         */

                        /*
                         * Apparently the only safe thing to do after ::fork() API is
                         * to call exec and replace the process completely or call _exit
                         *
                         * If an error occurs in the child process code after ::fork
                         * call, but before we replace the process with exec then we
                         * must call _exit and terminate the process immediately
                         *
                         * See the following web page for one of the reasons why:
                         *
                         * http://programmers.stackexchange.com/questions/206963/which-child-process-will-inherit-threads-of-parent-process
                         */

                        try
                        {
                            {
                                /*
                                 * Since we are going to replace the child process with execvp
                                 * which we are never going to return from, the first thing we
                                 * must do in the child process is create a scope to clean up
                                 * appropriately the inherited handles and anything else in the
                                 * local scope which needs to be cleaned up
                                 */

                                BL_SCOPE_EXIT(
                                    {
                                        cbCloseObjects();
                                    }
                                    );

                                /*
                                 * Override the standard I/O descriptors before we do
                                 * anything else
                                 */

                                if( flagsRedirect & ProcessCreateFlags::RedirectStdout )
                                {
                                    BL_ASSERT( outPipe.second.get() );

                                    BL_CHK_ERRNO(
                                        -1,
                                        ::dup2( outPipe.second.get(), STDOUT_FILENO ),
                                        BL_MSG()
                                            << "Cannot replace STDOUT file descriptor"
                                        );
                                }

                                if( flagsRedirect & ProcessCreateFlags::RedirectStderr )
                                {
                                    const int fderr =
                                        errPipe.second ?
                                        errPipe.second.get() :
                                        outPipe.second.get();

                                    BL_ASSERT( fderr );

                                    BL_CHK_ERRNO(
                                        -1,
                                        ::dup2( fderr, STDERR_FILENO ),
                                        BL_MSG()
                                            << "Cannot replace STDERR file descriptor"
                                        );
                                }

                                if( flagsRedirect & ProcessCreateFlags::RedirectStdin )
                                {
                                    BL_ASSERT( inPipe.first.get() );

                                    BL_CHK_ERRNO(
                                        -1,
                                        ::dup2( inPipe.first.get(), STDIN_FILENO ),
                                        BL_MSG()
                                            << "Cannot replace STDIN file descriptor"
                                        );
                                }
                            }

                            /*
                             * In child, divorce from parent session
                             */

                            BL_CHK_ERRNO(
                                -1,
                                ::setsid(),
                                BL_MSG()
                                    << "Setsid failed, unable to create a new process"
                                );
#ifdef __linux__
                            /*
                             * The ::prctl API is only available on Linux and according to this stack overflow thread it is
                             * not easy to implement equivalent functionality on non-Linux POSIX compatible platform
                             *
                             * We ill figure out how to deal with this later as it is not essential functinality
                             *
                             * http://stackoverflow.com/questions/284325/how-to-make-child-process-die-after-parent-exits/17589555#17589555
                             */

                            if( ! detachProcess )
                            {
                                /*
                                 * As suggested in 'http://stackoverflow.com/questions/284325/how-to-make-child-process-die-after-parent-exits'
                                 * prctl will make sure that the signal SIGKILL will be sent to the child process when the parent dies.
                                 *
                                 * 'man 2 prctl'
                                 * PR_SET_PDEATHSIG (since Linux 2.1.57)
                                 * Set the parent process death signal of the calling process to arg2 (either a signal value in the range 1..maxsig, or 0 to clear).
                                 * This is the signal that the calling process will get when its parent dies. This value is cleared for the child of a fork(2)
                                 */

                                BL_CHK_ERRNO(
                                    -1,
                                    ::prctl( PR_SET_PDEATHSIG, SIGKILL ),
                                    BL_MSG()
                                        << "prctl failed, unable to force the parent lifetime to the child process"
                                    );
                            }
#endif

                            /*
                             * For detached processes, we need to close all file descriptors.
                             *
                             * For the rest, we leave stdin, stdout, and stderr open.
                             *
                             * http://www.gnu.org/software/libc/manual/html_node/Descriptors-and-Streams.html
                             */

                            const int fileDescriptorThreshold = detachProcess ? -1 : STDERR_FILENO;

                            /*
                             * HACK: mark all open file handles as close-on-exec to hide them from the child process
                             */

                            for( int fd = ::getdtablesize(); --fd > fileDescriptorThreshold; )
                            {
                                tryMakeFileDescriptorPrivate( fd );
                            }

                            parentChildCommPipe.first.reset();
                            makeFileDescriptorPrivate( parentChildCommPipe.second.get() );

                            const auto commFilePtr =
                                convert2StdioFile( parentChildCommPipe.second, false /* readOnly */ );
                            const auto commStream = fileptr2ostream( commFilePtr.get() );

                            /*
                             * Execute the new process
                             */

                            const auto execResult = ::execvp( path.c_str(), &argv[ 0 ] );

                            if( -1 == execResult )
                            {
                                *commStream << errno;

                                BL_THROW(
                                    createException( "exec", errno, path ),
                                    BL_MSG()
                                        << "exec failed"
                                    );
                            }
                        }
                        catch( std::exception& e )
                        {
                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "\nUnhandled exception was encountered in child process:\n"
                                    << eh::diagnostic_information( e )
                                );
                        }
                        catch( ... )
                        {
                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "Unexpected error occurred in child process"
                                );
                        }

                        /*
                         * If we are here then apparently only safe thing to do
                         * is to call _exit - see comment at the beginning of
                         * the try block
                         *
                         * The top handler after fork always returns 1 exit code
                         * to indicate an error
                         *
                         * Note that 1 can be a genuine exit code returned by the
                         * process itself and we can't distinguish this case from
                         * errors which have occurred post fork and before exec
                         *
                         * In the cases where the caller wants to handle these
                         * errors it should redirect the output and parse it
                         *
                         * There is no better way to handle these errors on Linux
                         * and we can't really communicate the error efficiently
                         * to the calling process without inventing some
                         * sophisticated mechanism to transport the exception info
                         * with pipes, etc, but then the question is how do you
                         * handle errors which occur while trying to do this
                         * sophisticated error handling
                         */

                        ::_exit( 1 );
                    }

                    /*
                     * Parent Process
                     */

                    pidHandle -> setPid( pid );

                    {
                        parentChildCommPipe.second.reset();

                        const auto g = BL_SCOPE_GUARD( parentChildCommPipe.first.reset(); );

                        const auto commFilePtr =
                            convert2StdioFile( parentChildCommPipe.first, true /* readOnly */ );
                        const auto commStream = fileptr2istream( commFilePtr.get() );

                        std::string childErrorCode;

                        std::getline( *commStream, childErrorCode );

                        if( ! childErrorCode.empty() )
                        {
                            const int errorCode = std::atoi( childErrorCode.c_str() );

                            BL_THROW(
                                createException( "createProcess" /* locationOrAPI */, errorCode, path ),
                                BL_MSG()
                                    << "Cannot create process with command line '"
                                    << str::join( commandArguments, " " )
                                    << "'"
                                );
                        }
                    }

                    auto process = process_ref::attach( reinterpret_cast< process_handle_t >( pidHandle.release() ) );

                    if( flagsRedirect )
                    {
                        /*
                         * Callback not being nullptr is ensured by validateProcessRedirectFlags
                         *
                         * Before we call the callback we can now close the handles we passed
                         * for redirection, so when the process exits on the other side it will
                         * actually close the pipe
                         */

                        BL_ASSERT( callbackIos || callbackFile );

                        outPipe.second.reset();
                        errPipe.second.reset();
                        inPipe.first.reset();

                        if( callbackIos )
                        {
                            callbackIos( process.get(), outPtr, errPtr, inPtr );
                        }
                        else
                        {
                            callbackFile( process.get(), outFilePtr.get(), errFilePtr.get(), inFilePtr.get() );
                        }

                        cbCloseObjects();
                    }

                    if( waitToFinish )
                    {
                        const int exitCode = tryAwaitTermination( process.get() );
                        BL_UNUSED( exitCode );
                    }

                    return process.release();
                }

                static std::vector< std::string > transformCommandLineArguments(
                    SAA_in          const std::string&                          userName,
                    SAA_in          const std::vector< std::string >&           commandArguments
                    )
                {
                    std::vector< std::string > args;

                    if( ! userName.empty() )
                    {
                        /*
                         * Create the process as "/bin/su --command <commandArguments> <userName>"
                         *
                         * Do not use the "--login" option because it removes most environment variables
                         */

                        const std::string space( " " );

                        args.reserve( 4 );

                        args.emplace_back( "/bin/su" );
                        args.emplace_back( "--command" );
                        args.emplace_back(
                            str::joinFormatted< std::string >( commandArguments, space, space, &shellEscapeFormatter )
                            );
                        args.emplace_back( userName );
                    }
                    else
                    {
                        args = commandArguments;
                    }

                    return args;
                }

                static void shellEscapeFormatter(
                    SAA_inout       std::ostream&                               str,
                    SAA_in          const std::string&                          value
                    )
                {
                    for( const char c : value )
                    {
                        switch( c )
                        {
                            /*
                             * Quote all shell metacharacters
                             */

                            case ' ':
                            case '\t':
                            case '\'':
                            case '\"':
                            case '\\':
                            case '|':
                            case '&':
                            case ';':
                            case '(':
                            case ')':
                            case '<':
                            case '>':
                                str << '\\';
                                break;
                        }

                        str << c;
                    }
                }

                template
                <
                    typename T
                >
                static void closeHandle( SAA_in T* handle )
                {
                    delete getHandle( handle );
                }

                class FreeLibraryDeleter
                {
                public:

                    void operator()( SAA_inout library_handle_t handle )
                    {
                        this_type::freeLibrary( handle );
                    }
                };

                class CloseHandleDeleter
                {
                public:

                    template
                    <
                        typename T
                    >
                    void operator()( SAA_inout T* handle )
                    {
                        this_type::closeHandle( handle );
                    }
                };

                typedef cpp::SafeUniquePtr< typename std::remove_pointer< library_handle_t >::type, FreeLibraryDeleter > library_ref;
                typedef cpp::SafeUniquePtr< typename std::remove_pointer< process_handle_t >::type, CloseHandleDeleter > process_ref;

                static std::uint64_t getPid( SAA_in_opt const process_ref& process )
                {
                    if( process )
                    {
                        return getHandle( process.get() ) -> getPid();
                    }
                    else
                    {
                        return static_cast< std::uint64_t >( ::getpid() );
                    }
                }

                /*****************************************************
                 * File I/O support
                 */

                static stdio_file_ptr fopen(
                    SAA_in      const fs::path&                         path,
                    SAA_in      const char*                             mode
                    )
                {
                    stdio_file_ptr file;

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        false,
                        !! ( file = os::stdio_file_ptr::attach( std::fopen( path.c_str(), mode ) ) ),
                        BL_MSG()
                            << "Cannot open or create file "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    makeFileDescriptorPrivate( ::fileno( file.get() ) );

                    return file;
                }

                static void fseek(
                    SAA_in          const stdio_file_ptr&               fileptr,
                    SAA_in          const std::uint64_t                 offset,
                    SAA_in          const int                           origin
                    )
                {
                    detail::stdioChkOffset( offset );

                    BL_CHK_ERRNO_NM(
                        false,
                        0 == std::fseek( fileptr.get(), ( std::int64_t ) offset, origin )
                        );
                }

                static std::uint64_t ftell(
                    SAA_in          const stdio_file_ptr&               fileptr
                    )
                {
                    const auto pos = std::ftell( fileptr.get() );
                    BL_CHK_ERRNO_NM( -1L, pos );

                    return pos;
                }

                static void updateFileAttributes(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const FileAttributes                attributes,
                    SAA_in          const bool                          remove
                    )
                {
                    BL_UNUSED( path );
                    BL_UNUSED( attributes );
                    BL_UNUSED( remove );

                    /*
                     * The current set of file attributes are not supported on Linux
                     * so this API just does nothing
                     */
                }

                static FileAttributes getFileAttributes( SAA_in const fs::path& path )
                {
                    BL_UNUSED( path );

                    /*
                     * The current set of file attributes are not supported on Linux
                     * so this API returns no attributes
                     */

                    return FileAttributeNone;
                }

                static std::time_t getFileCreateTime( SAA_in const fs::path& path )
                {
                    BL_UNUSED( path );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "getFileCreateTime() is not supported on this platform"
                        );

                    return 0;
                }

                static void setFileCreateTime(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const std::time_t&                  newTime
                    )
                {
                    BL_UNUSED( path );
                    BL_UNUSED( newTime );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "setFileCreateTime() is not supported on this platform"
                        );
                }

                static bool getConsoleSize(
                    SAA_out         int&                                columns,
                    SAA_out         int&                                rows
                    )
                {
                    ::winsize size;

                    ::memset( &size, 0, sizeof( size ) );

                    const auto rc = ::ioctl( STDOUT_FILENO, TIOCGWINSZ, &size );

                    if( rc != 0 )
                    {
                        /*
                         * Standard output can be redirected (errno would be set to ENOTTY (>file) or EINVAL (|pipe)),
                         * the stdout file descriptor can be closed or it can be in an indeterminate state.
                         * All of this is more or less expected behaviour across different *nix systems.
                         */

                        columns = rows = -1;

                        return false;
                    }

                    columns = size.ws_col;
                    rows = size.ws_row;

                    /*
                     * Some broken systems may return 0 columns and 0 rows even if the ioctl succeeds.
                     */

                    return columns > 0 && rows > 0;
                }

                static std::string getUserName()
                {
                    ::passwd pw;
                    ::passwd *result = nullptr;
                    char buffer[ GET_PASSWD_BUFFER_LENGTH ];

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        -1,
                        ::getpwuid_r(
                            ::geteuid(),
                            &pw,
                            buffer,
                            BL_ARRAY_SIZE( buffer ),
                            &result
                            ),
                        BL_MSG()
                            << "Cannot obtain user login name"
                        );

                    BL_CHK_USER_FRIENDLY(
                        nullptr,
                        result,
                        BL_MSG()
                            << "User login name not found"
                        );

                    return pw.pw_name;
                }

                static auto getLoggedInUserNames() -> std::vector< std::string >
                {
                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "getLoggedInUserNames is not implemented on Linux"
                        );
                }

                static bool isClientOS()
                {
                    return false;
                }

                static void getUserCredentials(
                    SAA_in          const std::string&                  userName,
                    SAA_out         ::uid_t&                            userId,
                    SAA_out         ::gid_t&                            groupId
                    )
                {
                    ::passwd pw;
                    ::passwd *result = nullptr;
                    char buffer[ GET_PASSWD_BUFFER_LENGTH ];

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        -1,
                        ::getpwnam_r(
                            userName.c_str(),
                            &pw,
                            buffer,
                            BL_ARRAY_SIZE( buffer ),
                            &result
                            ),
                        BL_MSG()
                            << "Cannot obtain password entry for user '"
                            << userName
                            << "'"
                        );

                    BL_CHK_USER_FRIENDLY(
                        nullptr,
                        result,
                        BL_MSG()
                            << "User '"
                            << userName
                            << "' not found"
                        );

                    userId = pw.pw_uid;
                    groupId = pw.pw_gid;
                }

                static void getFileStatus(
                    SAA_in    const std::string&    filename,
                    SAA_in    struct ::stat*        fileStatus
                    )
                {
                    BL_CHK_ERRNO_USER_FRIENDLY(
                        -1,
                        ::stat( filename.c_str(), fileStatus ),
                        BL_MSG()
                            << "Cannot get status for file "
                            << fs::path( filename )
                        );
                }

                static std::string getFileOwner( SAA_in const std::string& filename )
                {
                    struct ::stat fileStatus;

                    getFileStatus( filename, &fileStatus );

                    ::passwd pw;
                    ::passwd *result = nullptr;
                    char buffer[ GET_PASSWD_BUFFER_LENGTH ];

                    BL_CHK_ERRNO(
                        -1,
                        ::getpwuid_r(
                            fileStatus.st_uid,
                            &pw,
                            buffer,
                            BL_ARRAY_SIZE( buffer ),
                            &result
                            ),
                        BL_MSG()
                            << "Cannot get user name for UID "
                            << fileStatus.st_uid
                        );

                    BL_CHK(
                        nullptr,
                        result,
                        BL_MSG()
                            << "User name for UID "
                            << fileStatus.st_uid
                            << " not found"
                        );

                    return pw.pw_name;
                }

                static void createJunction(
                    SAA_in          const fs::path&             to,
                    SAA_in          const fs::path&             junction
                    )
                {
                    BL_UNUSED( to );
                    BL_UNUSED( junction );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "junctions are not supported on this platform"
                        );
                }

                static void deleteJunction( SAA_in const fs::path& junction )
                {
                    BL_UNUSED( junction );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "junctions are not supported on this platform"
                        );
                }

                static bool isJunction( SAA_in const fs::path& path )
                {
                    BL_UNUSED( path );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "junctions are not supported on this platform"
                        );

                    return false;
                }

                static fs::path getJunctionTarget( SAA_in const fs::path& junction )
                {
                    BL_UNUSED( junction );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "junctions are not supported on this platform"
                        );

                    return junction;
                }

                static void createHardlink(
                    SAA_in          const fs::path&             to,
                    SAA_in          const fs::path&             hardlink
                    )
                {
                    BL_CHK_ERRNO_USER_FRIENDLY(
                        -1,
                        ::link(
                            to.c_str(),                     /* old path */
                            hardlink.c_str()                /* new path */
                            ),
                        BL_MSG()
                            << "Cannot create hard-link "
                            << fs::normalizePathParameterForPrint( hardlink )
                            << " pointing to "
                            << fs::normalizePathParameterForPrint( to )
                        );
                }

                static std::size_t getHardlinkCount( SAA_in const fs::path& path )
                {
                    struct ::stat fileInfo;

                    ::memset( &fileInfo, 0, sizeof( fileInfo ) );

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        -1,
                        ::lstat( path.c_str(), &fileInfo ),
                        BL_MSG()
                            << "Cannot get file information for "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    return fileInfo.st_nlink;
                }

                static fs::path getUsersDirectory()
                {
                    return fs::path( getEnvironmentVariable( "HOME" ) ).parent_path();
                }

                /**
                 * @brief Disable console input echo
                 */

                class DisableConsoleEcho FINAL
                {
                    BL_NO_COPY_OR_MOVE( DisableConsoleEcho )

                    ::termios                                       m_termios;
                    cpp::ScalarTypeIniter< bool >                   m_echoDisabled;

                public:

                    DisableConsoleEcho()
                    {
                        const auto rc = ::tcgetattr( STDIN_FILENO, &m_termios );

                        if( rc != 0 )
                        {
                            /*
                             * Standard input can be redirected and errno would be set to ENOTTY (file) or EINVAL (pipe).
                             * This is expected behaviour. Do not report an error in these cases.
                             */

                            BL_CHK_T(
                                false,
                                errno == ENOTTY || errno == EINVAL,
                                createException( "tcgetattr", errno ),
                                BL_MSG()
                                    << "Cannot get terminal IO state"
                                );
                        }
                        else
                        {
                            /*
                             * Disable local echo
                             */

                            ::termios newTermios = m_termios;
                            newTermios.c_lflag &= ~ECHO;

                            BL_CHK_T(
                                false,
                                0 == ::tcsetattr( STDIN_FILENO, TCSANOW, &newTermios ),
                                createException( "tcsetattr", errno ),
                                BL_MSG()
                                    << "Cannot set terminal IO state"
                                );

                            m_echoDisabled = true;
                        }
                    }

                    ~DisableConsoleEcho() NOEXCEPT
                    {
                        if( m_echoDisabled )
                        {
                            ::tcsetattr( STDIN_FILENO, TCSANOW, &m_termios );
                        }
                    }

                    bool isEchoDisabled() const NOEXCEPT
                    {
                        return m_echoDisabled;
                    }
                };

                /**
                 * @brief RobustNamedMutex class - a robust named mutex for out of process
                 * synchronization
                 *
                 * When a process terminates abruptly due to crash or other reason and
                 * orphans the mutex the other processes which are waiting on it will not
                 * hang indefinitely
                 *
                 * The UNIX version implemented here is based on System V semaphores
                 *
                 * Note that System V semaphores have initialization issues as well as
                 * the removal issue, but they're at least working unlike the POSIX
                 * mutexes; at some point this code will be implemented to something
                 * more sophisticated along the lines of the implementation in ACE which
                 * tries to fix the initialization races as well as remove the semaphore
                 * when the last process exits (unless it crashes)
                 */

                class RobustNamedMutex
                {
                    BL_NO_COPY_OR_MOVE( RobustNamedMutex )

                private:

                    const int m_semaphoreId;

                    static void chkOp(
                        SAA_in      const bool                          cond,
                        SAA_in      const std::string&                  locationOrAPI
                        )
                    {
                        BL_CHK_T(
                            false,
                            cond,
                            createException( locationOrAPI, errno ),
                            BL_MSG()
                                << "System V semaphore API has failed"
                            );
                    }

                    static void semOp(
                        SAA_in      const int                           id,
                        SAA_in      const int                           value,
                        SAA_in      const bool                          noUndo = false
                        )
                    {
                        BL_ASSERT( value );

                        ::sembuf op_op = { 0, 0, 0 };

                        op_op.sem_flg = ( noUndo ? 0 : SEM_UNDO );
                        op_op.sem_op = value;

                        chkOp( 0 == ::semop( id, &op_op, 1 ), "semop" );
                    }

                    static int semOpenOrCreate(
                        SAA_in      const std::string&                  name,
                        SAA_in      const int                           permissions
                        )
                    {
                        BL_ASSERT( name.size() );

                        /*
                         * We use checksum as good enough method to generate keys
                         */

                        cs::crc_32_type crcc;
                        crcc.process_bytes( name.c_str(), name.size() );
                        const auto checksum = crcc.checksum();

                        const ::key_t key =
                            static_cast< decltype( checksum ) >( -1 ) == checksum
                            ? 1234U /* some default */
                            : checksum;

                        int semid = ::semget(
                            key,
                            1,
                            ( permissions & 0x01FF ) | IPC_CREAT | IPC_EXCL
                            );

                        if( -1 != semid )
                        {
                            semOp( semid, 1, true /* noUndo */ );
                        }
                        else if( errno == EEXIST )
                        {
                            /*
                             * The semaphore already exists - just try to open it
                             */

                            semid = ::semget( key, 0, 0 );
                        }

                        chkOp( semid >= 0, "semget" );

                        return semid;
                    }

                    static int defaultPermissions()
                    {
                        return ( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
                    }

                public:

                    typedef ipc::scoped_lock< RobustNamedMutex > Guard;

                    RobustNamedMutex( SAA_in const std::string& name )
                        :
                        m_semaphoreId( semOpenOrCreate( name, defaultPermissions() ) )
                    {
                    }

                    void lock()
                    {
                        semOp( m_semaphoreId, -1 );
                    }

                    void unlock()
                    {
                        semOp( m_semaphoreId, 1 );
                    }
                };

                /*****************************************************************************
                 * Set priority for CPU, IO & Memory based on whether the process
                 * requires normal vs. background priority
                 */

                /*
                 * These enum values are hard-coded here to avoid including <linux/ioprio.h>
                 * which is not normally present on most Linux installs
                 *
                 * More information for the values can be found on the following link:
                 * http://man7.org/linux/man-pages/man2/ioprio_set.2.html
                 */

                enum
                {
                    CPU_PRIORITY_HIGHEST                = ( 0 - NZERO ),
                    CPU_PRIORITY_NORMAL                 = 0,
                    CPU_PRIORITY_LOWEST                 = NZERO - 1,
                };

                enum
                {
                    IO_PRIORITY_WHO_PROCESS             = 1,        /* who is process id or thread id */
                };

                enum
                {
                    IO_PRIORITY_WHO_CURRENT_THREAD      = 0,        /* who is the current thread */
                };

                enum
                {
                    IO_PRIORITY_CLASS_RT                = 1,        /* real time class */
                    IO_PRIORITY_CLASS_BE                = 2,        /* best effort class */
                    IO_PRIORITY_CLASS_IDLE              = 3,        /* idle class (always last on the queue) */
                };

                enum
                {
                    IO_PRIORITY_HIGHEST                 = 0,
                    IO_PRIORITY_NORMAL                  = 4,
                    IO_PRIORITY_LOWEST                  = 7,
                };

                enum
                {
                    IO_PRIORITY_CLASS_SHIFT             = 13,
                };

                static int ioPriorityValue(
                    SAA_in      const int                           priorityClass,
                    SAA_in      const int                           priorityValue
                    )
                {
                    return ( ( priorityClass << IO_PRIORITY_CLASS_SHIFT ) | priorityValue );
                }

#ifdef __APPLE__
                static int getPriorityValue( SAA_in const AbstractPriority priority )
                {
                    /*
                     * More details for the I/O priority mappings can be found here:
                     *
                     * https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/setiopolicy_np.3.html
                     */

                    int priorityValue;

                    switch( priority )
                    {
                        default:
                            BL_ASSERT( false );
                            priorityValue = IOPOL_STANDARD;
                            break;

                        case AbstractPriority::Normal:
                            priorityValue = IOPOL_STANDARD;
                            break;

                        case AbstractPriority::Background:
                            priorityValue = IOPOL_UTILITY;
                            break;

                        case AbstractPriority::Greedy:
                            priorityValue = IOPOL_IMPORTANT;
                            break;
                    }

                    return priorityValue;
                }
#else
                static int getPriorityValue( SAA_in const AbstractPriority priority )
                {
                    /*
                     * For background we should either use best effort class with lowest
                     * priority - i.e.:
                     * ioPriorityValue( IO_PRIORITY_CLASS_BE, IO_PRIORITY_LOWEST )
                     * or the idle class - i.e.:
                     * ioPriorityValue( IO_PRIORITY_CLASS_IDLE, 0 )
                     *
                     * For now we will use the best effort class with lowest priority
                     * for background
                     */

                    int priorityValue;

                    switch( priority )
                    {
                        default:
                            BL_ASSERT( false );
                            priorityValue = ioPriorityValue( IO_PRIORITY_CLASS_BE /* best effort class */, IO_PRIORITY_NORMAL );
                            break;

                        case AbstractPriority::Normal:
                            priorityValue = ioPriorityValue( IO_PRIORITY_CLASS_BE /* best effort class */, IO_PRIORITY_NORMAL );
                            break;

                        case AbstractPriority::Background:
                            priorityValue = ioPriorityValue( IO_PRIORITY_CLASS_BE /* best effort class */, IO_PRIORITY_LOWEST );
                            break;

                        case AbstractPriority::Greedy:
                            priorityValue = ioPriorityValue( IO_PRIORITY_CLASS_BE /* best effort class */, IO_PRIORITY_HIGHEST );
                            break;
                    }

                    return priorityValue;
                }
#endif

                static void setIoPriority(
                    SAA_in      const int                           who,
                    SAA_in      const AbstractPriority              priority
                    )
                {
#ifdef __APPLE__
                    BL_UNUSED( who );

                    BL_CHK_ERRNO(
                        -1,
                        ::setiopolicy_np(
                            IOPOL_TYPE_DISK                 /* iotype */,
                            IOPOL_SCOPE_PROCESS             /* ioscope */,
                            getPriorityValue( priority )    /* I/O policy / priority */
                            ),
                        BL_MSG()
                            << "Cannot set abstract I/O priority to "
                            << ( int ) priority
                            << " for the current process [0=Normal;1=Background;2=Greedy]"
                        );
#else
                    BL_CHK_ERRNO(
                        -1,
                        ::syscall(
                            SYS_ioprio_set,
                            IO_PRIORITY_WHO_PROCESS,
                            who,
                            getPriorityValue( priority )
                            ),
                        BL_MSG()
                            << "Cannot set abstract I/O priority to "
                            << ( int ) priority
                            << " for the current "
                            << ( who ? "thread" : "process [0=Normal;1=Background;2=Greedy]" )
                        );
#endif
                }

                static void setAbstractPriority( SAA_in_opt const AbstractPriority priority )
                {
                    int cpuPriority;

                    switch( priority )
                    {
                        default:
                            BL_ASSERT( false );
                            cpuPriority = CPU_PRIORITY_NORMAL;
                            break;

                        case AbstractPriority::Normal:
                            cpuPriority = CPU_PRIORITY_NORMAL;
                            break;

                        case AbstractPriority::Background:
                            cpuPriority = CPU_PRIORITY_LOWEST;
                            break;

                        case AbstractPriority::Greedy:
                            cpuPriority = CPU_PRIORITY_HIGHEST;
                            break;
                    }

                    /*
                     * Set the process and I/O priority
                     */

                    BL_CHK_ERRNO(
                        -1,
                        ::setpriority( PRIO_PROCESS, ::getpid(), cpuPriority ),
                        BL_MSG()
                            << "Cannot set priority value of "
                            << cpuPriority
                            << " for the current process"
                        );

                    /*
                     * Set the I/O priority at both process level and for the
                     * current thread as well (in case it is in different I/O
                     * context)
                     */

                    setIoPriority( ::getpid(), priority );
                    setIoPriority( IO_PRIORITY_WHO_CURRENT_THREAD, priority );
                }

                /**
                 * @brief Turn the process into a daemon - a background service process detached from the console
                 *
                 * @see http://0pointer.de/public/systemd-man/daemon.html
                 */

                static void daemonize()
                {
#ifdef __APPLE__
                    /*
                     *  To suppress locally the following error on Darwin:
                     *
                     * error: 'daemon' is deprecated: first deprecated in OS X 10.5 - Use posix_spawn APIs instead.
                     * [-Werror,-Wdeprecated-declarations]
                     */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
                    BL_CHK_ERRNO_NM(
                        false,
                        0 == ::daemon( 0 /* nochdir */, 0 /* noclose */ )
                        );
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
                }

                static std::string tryGetUserDomain()
                {
                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "tryGetUserDomain() is not implemented on Linux"
                        );
                }

                static std::string getSPNEGOtoken(
                    SAA_in          const std::string&                  /* server */,
                    SAA_in          const std::string&                  /* domain */
                    )
                {
                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "getSPNEGOtoken() is not implemented on Linux"
                        );
                }

                static bool isUserAdministrator() NOEXCEPT
                {
                    return ::geteuid() == 0 /* root */;
                }

                static int getSessionId()
                {
                    int sessionId;

                    BL_CHK_ERRNO_NM(
                        -1,
                        sessionId = ::getsid( 0 )
                        );

                    return sessionId;
                }

                static void copyDirectoryPermissions(
                    SAA_in          const fs::path&                 srcDirPath,
                    SAA_in          const fs::path&                 dstDirPath
                    )
                {
                    const auto status = fs::status( srcDirPath );

                    fs::permissions( dstDirPath, status.permissions() );

                    struct stat srcStat;
                    int rc = ::stat( srcDirPath.string().c_str(), &srcStat );

                    BL_CHK_ERRNO(
                        false,
                        rc == 0,
                        BL_MSG()
                            << "Cannot get stat info for path "
                            << fs::normalizePathParameterForPrint( srcDirPath )
                        );

                    rc = ::chown( dstDirPath.string().c_str(), srcStat.st_uid, srcStat.st_gid );

                    BL_CHK_ERRNO(
                        false,
                        rc == 0,
                        BL_MSG()
                            << "Cannot change ownership for path "
                            << fs::normalizePathParameterForPrint( dstDirPath )
                        );
                }

                static void setLinuxPathPermissions(
                    SAA_in      const fs::path&             path,
                    SAA_in      const fs::perms             permissions
                    )
                {
                    const auto status = fs::status( path );

                    if( status.permissions() != permissions )
                    {
                        fs::permissions( path, permissions );
                    }
                }

                static void setWindowsPathPermissions(
                    SAA_in      const fs::path&             path,
                    SAA_in      const fs::perms             ownerPermissions,
                    SAA_in_opt  const std::string&          groupName,
                    SAA_in_opt  const fs::perms             groupPermissions
                    )
                {
                    BL_UNUSED( path );
                    BL_UNUSED( ownerPermissions );
                    BL_UNUSED( groupName );
                    BL_UNUSED( groupPermissions );

                    BL_THROW(
                        NotSupportedException(),
                        "setWindowsFilePermissions is implemented only on Windows"
                        );
                }

                static void sendDebugMessage( SAA_in const std::string& message ) NOEXCEPT
                {
                    BL_UNUSED( message );

                    /*
                     * NOP
                     */
                }

                static os::string_ptr tryGetRegistryValue(
                    SAA_in         const std::string             key,
                    SAA_in         const std::string             name,
                    SAA_in_opt     const bool                    currentUser = false
                    )
                {
                    BL_UNUSED( key );
                    BL_UNUSED( name );
                    BL_UNUSED( currentUser );

                    BL_THROW(
                        NotSupportedException(),
                        "tryGetRegistryValue is implemented only on Windows"
                        );
                }

                static std::string getRegistryValue(
                    SAA_in         const std::string             key,
                    SAA_in         const std::string             name,
                    SAA_in_opt     const bool                    currentUser = false
                    )
                {
                    BL_UNUSED( key );
                    BL_UNUSED( name );
                    BL_UNUSED( currentUser );

                    BL_THROW(
                        NotSupportedException(),
                        "getRegistryValue is implemented only on Windows"
                        );
                }

                static bool createNewFile( SAA_in const fs::path& path )
                {
                    int fd = ::open(
                        path.string().c_str(),
                        O_WRONLY | O_CREAT | O_EXCL,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
                        );

                     if( fd == -1 )
                     {
                        return false;
                     }
                     else
                     {
                        ::close( fd );
                        return true;
                     }
                }

                static bool isFileInUseError( SAA_in const eh::error_code& ec )
                {
                    BL_UNUSED( ec );

                    BL_THROW(
                        NotSupportedException(),
                        "isFileInUseError() is not implemented on Linux"
                        );
                }

                static eh::error_code tryRemoveFileOnReboot( SAA_in const fs::path& path )
                {
                    BL_UNUSED( path );

                    BL_THROW(
                        NotSupportedException(),
                        "tryRemoveFileOnReboot() is not implemented on Linux"
                        );
                }
            };

            BL_DEFINE_STATIC_MEMBER( OSImplT, const char*, g_procSelfExeSymlink ) = "/proc/self/exe";

            typedef OSImplT<> OSImpl;

        } // detail

    } // os

} // bl

#endif /* __BL_OSIMPLLINUX_H_ */
