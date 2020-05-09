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

#ifndef __BL_OSIMPLWINDOWS_H_
#define __BL_OSIMPLWINDOWS_H_

#if ! defined( _WIN32 )
#error This file can only be included when compiling on Windows platform
#endif

#include <baselib/core/detail/OSImplPlatformCommon.h>
#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/CPP.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/Uuid.h>

/*
 * This header must be included before we include Boost ASIO
 * headers, so we can override the error handling if we need to
 */

#include <baselib/core/detail/BoostAsioErrorCallback.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/asio/detail/winsock_init.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <Windows.h>
#include <Lmcons.h>
#include <tlhelp32.h>
#include <winioctl.h>
#include <ntsecapi.h>
#include <security.h>
#include <aclapi.h>
#include <VersionHelpers.h>
#include <Userenv.h>

/*
 * Below macro definitions are pasted from ntstatus.h as we need them but
 * if ntstatus.h is included directly then we get macro re-definition
 * warnings in winnt.h as it defines some of the STATUS_* codes but not all.
 * Few workarounds mentioned online didn't help.
 */

#define STATUS_SUCCESS                   ( ( NTSTATUS )0x00000000L )
#define STATUS_ACCESS_DENIED             ( ( NTSTATUS )0xC0000022L )

/*
 * The above include will also pull rpcdce.h which defines macro uuid_t
 * Undefine this macro to avoid conflicts with bl::uuid_t type
 */

#undef uuid_t

#include <utility>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <codecvt>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <crtdbg.h>

#pragma comment( lib, "advapi32.lib" )
#pragma comment( lib, "Secur32.lib" )
#pragma comment( lib, "Userenv.lib" )

#define BL_CHK_BOOL_WINAPI_ERROR_MESSAGE( expression, errorMessage ) \
    BL_CHK_T( \
        FALSE, \
        ( expression ), \
        createException( #expression /* locationOrAPI */ ), \
        errorMessage \
        ) \

#define BL_CHK_BOOL_WINAPI( expression ) \
    BL_CHK_BOOL_WINAPI_ERROR_MESSAGE( expression, BL_SYSTEM_ERROR_DEFAULT_MSG )

#define BL_CHK_BOOL_WINAPI_USER_FRIENDLY( expression, errorMessage ) \
    BL_CHK_T_USER_FRIENDLY( \
        FALSE, \
        ( expression ), \
        createException( #expression /* locationOrAPI */ ), \
        errorMessage \
        ) \

namespace bl
{
    namespace os
    {
        namespace detail
        {
            /**
             * @brief release credentials handle helper
             */

            template
            <
                typename E = void
            >
            class CredHandleDeleterT FINAL
            {
            public:

                void operator()( CredHandle* credentials ) const NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    if( credentials )
                    {
                        BL_CHK(
                            SEC_E_OK == ::FreeCredentialsHandle( credentials ),
                            false,
                            BL_MSG()
                                << "FreeCredentialsHandle() failed"
                            );
                    }

                    BL_NOEXCEPT_END()
                }
            };

            typedef CredHandleDeleterT<> CredHandleDeleter;
            typedef cpp::SafeUniquePtr< CredHandle, bl::os::detail::CredHandleDeleter > cred_handle_ptr_t;

            /**
             * @brief release security context handle helper
             */

            template
            <
                typename E = void
            >
            class ContextHandleDeleterT FINAL
            {
            public:

                void operator()( CtxtHandle* context ) const NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    if( context )
                    {
                        BL_CHK(
                            SEC_E_OK == ::DeleteSecurityContext( context ),
                            false,
                            BL_MSG()
                                << "DeleteSecurityContext() failed"
                            );
                    }

                    BL_NOEXCEPT_END()
                }
            };

            typedef ContextHandleDeleterT<> ContextHandleDeleter;
            typedef cpp::SafeUniquePtr< CtxtHandle, os::detail::ContextHandleDeleter > ctxt_handle_ptr_t;

            /**
             * @brief release security token buffer helper
             */

            template
            <
                typename E = void
            >
            class SecurityBufferDeleterT FINAL
            {
            public:

                void operator()( SecBuffer* buffer ) const NOEXCEPT
                {
                    if( buffer )
                    {
                        BL_NOEXCEPT_BEGIN()

                        if( buffer -> pvBuffer )
                        {
                            BL_CHK(
                                SEC_E_OK == ::FreeContextBuffer( buffer -> pvBuffer ),
                                false,
                                BL_MSG()
                                    << "FreeContextBuffer() failed"
                                );
                            buffer -> pvBuffer = NULL;
                        }

                        BL_NOEXCEPT_END()
                    }
                }
            };

            typedef SecurityBufferDeleterT<> SecurityBufferDeleter;
            typedef cpp::SafeUniquePtr< SecBuffer, os::detail::SecurityBufferDeleter > sec_buf_ptr_t;

            /**
             * @brief release registry key handle helper
             */

            template
            <
                typename E = void
            >
            class RegistryKeyHandleDeleterT FINAL
            {
            public:

                void operator()( HKEY hkey )
                {
                    BL_NOEXCEPT_BEGIN()

                    if( hkey )
                    {
                        BL_CHK(
                            ERROR_SUCCESS == ::RegCloseKey( hkey ),
                            false,
                            BL_MSG()
                                << "RegCloseKey() failed"
                            );
                    }

                    BL_NOEXCEPT_END()
                }
            };

            typedef RegistryKeyHandleDeleterT<> RegistryKeyHandleDeleter;
            typedef cpp::SafeUniquePtr< HKEY__, RegistryKeyHandleDeleter > reg_key_handle_ptr_t;


            /**
             * @brief class GlobalProcessInit (for Windows platform)
             */

            template
            <
                typename E = void
            >
            class GlobalProcessInitT FINAL : protected GlobalProcessInitBase
            {
                BL_NO_COPY_OR_MOVE( GlobalProcessInitT )

            protected:

                typedef GlobalProcessInitT< E >                         this_type;

                _invalid_parameter_handler                              m_oldCrtHandler;
                const boost::asio::detail::winsock_init<>               m_winsockInit;

                SAA_noreturn
                static void crtInvalidParameterHandler(
                    SAA_in          const wchar_t*                      expression,
                    SAA_in          const wchar_t*                      function,
                    SAA_in          const wchar_t*                      file,
                    SAA_in          const unsigned int                  line,
                    SAA_in          uintptr_t                           pReserved
                    )
                {
                    if( function || file || line || expression )
                    {
                        BL_STDIO_TEXT(
                            {
                                std::wcerr
                                    << "ERROR: Invalid parameter detected in function "
                                    << ( function ? function : L"(unknown)" )
                                    << ", file: "
                                    << ( file ? file : L"(unknown)" )
                                    << ", line: "
                                    << line
                                    << std::endl
                                    << "ERROR: Expression which failed: '"
                                    << ( expression ? expression : L"(unknown)" )
                                    << "'"
                                    << std::endl;

                                std::fflush( stderr );
                            }
                            );
                    }

                    BL_RIP_MSG( "A CRT function was called with an invalid parameter" );
                }

            public:

                GlobalProcessInitT()
                {
                    m_oldCrtHandler = ::_set_invalid_parameter_handler( &this_type::crtInvalidParameterHandler );

                    idempotentGlobalProcessInitHelpers();
                }

                ~GlobalProcessInitT() NOEXCEPT
                {
                    ::_set_invalid_parameter_handler( m_oldCrtHandler );
                }

                static void idempotentGlobalProcessInitHelpers()
                {
                    idempotentGlobalProcessInitBaseHelpers();

                    /*
                     * Note: these need to be called only if we link to the debug CRT, which normally
                     * we don't, but still it is good to have them to make the code usable and safe
                     * if we decide to do this in the future
                     */

                    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG );
                    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
                    _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG );
                    _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
                    _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG );
                    _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );

                    /*
                     * setting output where the C runtime will write error messages to stderr.
                     * In this way, assertions are not delivered to a message box
                     */

                    BL_CHK_ERRNO_NM( false, -1 != _set_error_mode( _OUT_TO_STDERR ) );
                }
            };

            typedef GlobalProcessInitT<> GlobalProcessInit;

            /**
             * @brief class OSImpl (for Windows platform)
             */

            template
            <
                typename E = void
            >
            class OSImplT : public OSImplBase
            {
                BL_DECLARE_STATIC( OSImplT )

            private:

                static const std::wstring                       g_noParsePrefix;
                static const std::wstring                       g_lfnPrefix;
                static const std::wstring                       g_windowManagerDomain;

                static const std::map< fs::perms, DWORD >&      g_permissionsMap;

                static AbstractPriority                         g_currentAbstractPriority;

                static bool                                     g_backupEnabled;
                static bool                                     g_restoreEnabled;

                /*
                 * Import the REPARSE_DATA_BUFFER from MSDN since it is not part of the
                 * platform headers:
                 *
                 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff552012(v=vs.85).aspx
                 */

                typedef struct _REPARSE_DATA_BUFFER
                {
                    ULONG ReparseTag;
                    USHORT ReparseDataLength;
                    USHORT Reserved;
                    union
                    {
                        struct
                        {
                            USHORT SubstituteNameOffset;
                            USHORT SubstituteNameLength;
                            USHORT PrintNameOffset;
                            USHORT PrintNameLength;
                            ULONG Flags;
                            WCHAR PathBuffer[ 1 ];
                        }
                        SymbolicLinkReparseBuffer;

                        struct
                        {
                            USHORT SubstituteNameOffset;
                            USHORT SubstituteNameLength;
                            USHORT PrintNameOffset;
                            USHORT PrintNameLength;
                            WCHAR PathBuffer[ 1 ];
                        }
                        MountPointReparseBuffer;

                        struct
                        {
                            UCHAR DataBuffer[ 1 ];
                        }
                        GenericReparseBuffer;
                    };
                }
                REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

                enum
                {
                    /*
                     * The first 3 members in the struct above are the reparse header
                     */

                    REPARSE_BUFFER_HEADER_SIZE =
                        FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer ),
                };

                enum
                {
                    /*
                     * The reparse buffer header size plus the header size
                     * of MountPointReparseBuffer which is 4 USHORTs
                     */

                    REPARSE_MOUNT_POINT_TOTAL_HEADER_SIZE =
                        FIELD_OFFSET( REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer ),

                    REPARSE_MOUNT_POINT_SUB_HEADER_SIZE =
                        REPARSE_MOUNT_POINT_TOTAL_HEADER_SIZE -
                        REPARSE_BUFFER_HEADER_SIZE,
                };

                enum
                {
                    /*
                     * The initial static buffer size should be enough to hold the header
                     * plus two paths (the substitute name and the print name) in bytes
                     * bounded by MAX_PATH length
                     */

                    REPARSE_BUFFER_SIZE_DEFAULT =
                        sizeof( REPARSE_DATA_BUFFER ) + ( 4 * ( MAX_PATH + sizeof( WCHAR ) ) ),
                };

                class EncapsulatedProcessJobHandle FINAL
                {
                    BL_NO_COPY( EncapsulatedProcessJobHandle )

                private:

                    HANDLE      m_processHandle;
                    HANDLE      m_jobHandle;
                    bool        m_terminateProcessOnDestruction;

                public:

                    EncapsulatedProcessJobHandle( SAA_in bool terminateProcessOnDestruction )
                        :
                        m_processHandle( NULL ),
                        m_jobHandle( NULL ),
                        m_terminateProcessOnDestruction( terminateProcessOnDestruction )
                    {
                    }

                    ~EncapsulatedProcessJobHandle()
                    {
                        BL_NOEXCEPT_BEGIN()

                        if( m_processHandle )
                        {
                            if( m_terminateProcessOnDestruction )
                            {
                                ::TerminateProcess( m_processHandle, 1 /* exitCode */ );
                            }

                            ::CloseHandle( m_processHandle );
                        }

                        if( m_jobHandle )
                        {
                            ::CloseHandle( m_jobHandle );
                        }

                        BL_NOEXCEPT_END()
                    }

                    void setProcessHandle( SAA_in const HANDLE processHandle ) NOEXCEPT
                    {
                        BL_ASSERT( ! m_processHandle );

                        m_processHandle = processHandle;
                    }

                    HANDLE getProcessHandle() const NOEXCEPT
                    {
                        return m_processHandle;
                    }

                    void setJobHandle( SAA_in const HANDLE jobHandle ) NOEXCEPT
                    {
                        BL_ASSERT( ! m_jobHandle );

                        m_jobHandle = jobHandle;
                    }

                    void resetJobHandle() NOEXCEPT
                    {
                        if( m_jobHandle )
                        {
                            ::CloseHandle( m_jobHandle );

                            m_jobHandle = NULL;
                        }
                    }

                    HANDLE getJobHandle() const NOEXCEPT
                    {
                        return m_jobHandle;
                    }

                    bool tryTimedAwaitTermination(
                        SAA_out_opt int*                        exitCode = nullptr,
                        SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
                        )
                    {
                        static_assert( TIMEOUT_INFINITE < 0, "TIMEOUT_INFINITE must be negative" );

                        const auto rc = ::WaitForSingleObject(
                            m_processHandle,
                            timeoutMs < 0 ? INFINITE : ( DWORD ) timeoutMs
                            );

                        if( WAIT_OBJECT_0 == rc )
                        {
                            if( exitCode )
                            {
                                DWORD ec;

                                BL_CHK_BOOL_WINAPI( ::GetExitCodeProcess( m_processHandle, &ec ) );

                                BL_CHK(
                                    false,
                                    STILL_ACTIVE != rc,
                                    BL_MSG()
                                        << "Process is not expected to be active"
                                    );

                                *exitCode = ( int ) ec;
                            }

                            return true;
                        }

                        BL_CHK_T(
                            false,
                            rc != WAIT_FAILED,
                            createException( "WaitForSingleObject" /* locationOrAPI */ ),
                            BL_MSG()
                                << "WaitForSingleObject failed for a process handle"
                            );

                        /*
                         * WAIT_TIMEOUT should be the only possible return value since
                         * WAIT_ABANDONED cannot be returned on a process handle
                         */

                        BL_ASSERT( WAIT_TIMEOUT == rc );

                        return false;
                    }
                };

                template
                <
                    typename T
                >
                struct RCB
                {
                    typedef cpp::function
                        <
                            T
                            (
                                SAA_in                              const eh::error_code&               ec,
                                SAA_in_bcount_opt( sizeInBytes )    const REPARSE_DATA_BUFFER*          buffer,
                                SAA_in_opt                          const DWORD                         sizeInBytes
                            )
                        >
                        type;
                };

                static HMODULE getModule( SAA_in const library_handle_t handle ) NOEXCEPT
                {
                    return reinterpret_cast< HMODULE >( handle );
                }

                template
                <
                    typename T
                >
                static EncapsulatedProcessJobHandle* getHandle( SAA_in T* handle ) NOEXCEPT
                {
                    return reinterpret_cast< EncapsulatedProcessJobHandle* >( handle );
                }

                static eh::error_code createSystemErrorCode( SAA_in const int errorCode )
                {
                    return eh::error_code( errorCode, eh::system_category() );
                }

                static SystemException createException(
                    SAA_in_opt      const std::string&      locationOrAPI,
                    SAA_in_opt      const std::string&      fileName = str::empty()
                    )
                {
                    const auto code = createSystemErrorCode( ( int )::GetLastError() );

                    auto exception = SystemException::create( code, locationOrAPI );

                    if( ! fileName.empty() )
                    {
                        exception << eh::errinfo_file_name( fileName );
                    }

                    return exception;
                }

                static ::errno_t putenvWrapper(
                    SAA_in          const char*                                 name,
                    SAA_in          const char*                                 value,
                    SAA_in          const bool                                  checkValue
                    ) NOEXCEPT
                {
                    /*
                     * Environment variable name must be valid and must not contain '=' (Windows doesn't enforce the latter)
                     */

                    if( ! name || ::strchr( name, '=' ) )
                    {
                        return errno = EINVAL;
                    }

                    /*
                     * Disallow empty values because setting an environment variable to an empty string
                     * actually removes it from the environment
                     */

                    if( checkValue && ( ! value || ! *value ) )
                    {
                        return errno = EINVAL;
                    }

                    return ::_putenv_s( name, value );
                }

                /*
                 * These magic constants used in the following two functions
                 * are per following Microsoft KB article:
                 *
                 * http://support.microsoft.com/kb/167296
                 */

                static std::time_t FILETIME2time_t( SAA_in const FILETIME& fileTime )
                {
                    LONGLONG time = ( static_cast< LONGLONG >( fileTime.dwHighDateTime ) << 32 );

                    time += fileTime.dwLowDateTime;
                    time -= 116444736000000000LL;
                    time /= 10000000;

                    return static_cast< std::time_t >( time );
                }

                static void time_t2FILETIME( SAA_in const std::time_t time, SAA_out FILETIME& fileTime )
                {
                    LONGLONG tmp = time;

                    tmp *= 10000000;
                    tmp += 116444736000000000LL;

                    fileTime.dwLowDateTime = static_cast< DWORD >( tmp );
                    fileTime.dwHighDateTime = static_cast< DWORD >( tmp >> 32 );
                }

                static void terminateProcessTree( SAA_in const process_handle_t process )
                {
                    auto handle = getHandle( process );

                    BL_ASSERT( handle -> getJobHandle() != NULL );

                    BL_CHK_BOOL_WINAPI( ::TerminateJobObject( handle -> getJobHandle(), 1 /* exit code */ ) );
                }

                static const std::map< fs::perms, DWORD >& initPermissionsMap()
                {
                    using fs::perms;

                    static std::map< perms, DWORD > g_permissionsMapLocal;

                    g_permissionsMapLocal[ perms::owner_read ]                       = GENERIC_READ;
                    g_permissionsMapLocal[ perms::owner_write ]                      = GENERIC_WRITE;
                    g_permissionsMapLocal[ perms::owner_exe ]                        = GENERIC_EXECUTE;
                    g_permissionsMapLocal[ perms::owner_read  | perms::owner_write ] = GENERIC_READ | GENERIC_WRITE;
                    g_permissionsMapLocal[ perms::owner_read  | perms::owner_exe ]   = GENERIC_READ | GENERIC_EXECUTE;
                    g_permissionsMapLocal[ perms::owner_write | perms::owner_exe ]   = GENERIC_WRITE | GENERIC_EXECUTE;
                    g_permissionsMapLocal[ perms::owner_all ]                        = GENERIC_ALL;
                    g_permissionsMapLocal[ perms::group_read ]                       = GENERIC_READ;
                    g_permissionsMapLocal[ perms::group_write ]                      = GENERIC_WRITE;
                    g_permissionsMapLocal[ perms::group_exe ]                        = GENERIC_EXECUTE;
                    g_permissionsMapLocal[ perms::group_read  | perms::group_write ] = GENERIC_READ | GENERIC_WRITE;
                    g_permissionsMapLocal[ perms::group_read  | perms::group_exe ]   = GENERIC_READ | GENERIC_EXECUTE;
                    g_permissionsMapLocal[ perms::group_write | perms::group_exe ]   = GENERIC_WRITE | GENERIC_EXECUTE;
                    g_permissionsMapLocal[ perms::group_all ]                        = GENERIC_ALL;

                    return g_permissionsMapLocal;
                }

            public:

                typedef OSImplT< E > this_type;

                template
                <
                    typename T
                >
                static void closeHandle( SAA_in T* handle ) NOEXCEPT
                {
                    BL_VERIFY( ::CloseHandle( reinterpret_cast< HANDLE >( handle ) ) );
                }

                class CloseHandleDeleter
                {
                public:

                    template
                    <
                        typename T
                    >
                    void operator()( SAA_inout T* handle ) const NOEXCEPT
                    {
                        this_type::closeHandle( handle );
                    }
                };

                template
                <
                    typename T
                >
                static void localFree( SAA_in T* hlocal ) NOEXCEPT
                {
                    BL_VERIFY( NULL == ::LocalFree( reinterpret_cast< HLOCAL >( hlocal ) ) );
                }

                class LocalFreeDeleter
                {
                public:

                    template
                    <
                        typename T
                    >
                    void operator()( SAA_in T* handle ) const NOEXCEPT
                    {
                        this_type::localFree( handle );
                    }
                };

                template
                <
                    typename T
                >
                static void deleteHandle( SAA_in T* handle ) NOEXCEPT
                {
                    delete getHandle( handle );
                }

                class DeleteHandleDeleter
                {
                public:

                    template
                    <
                        typename T
                    >
                    void operator()( SAA_inout T* handle ) const NOEXCEPT
                    {
                        this_type::deleteHandle( handle );
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

                        BL_VERIFY( 0 == ::_close( fd ) );
                    }
                };

                typedef cpp::SafeUniquePtr< typename std::remove_pointer< process_handle_t >::type, DeleteHandleDeleter > process_ref;
                typedef cpp::SafeUniquePtr< typename std::remove_pointer< generic_handle_t >::type, CloseHandleDeleter > handle_ref;
                typedef cpp::SafeUniquePtr< typename std::remove_pointer< generic_handle_t >::type, LocalFreeDeleter > hlocal_ref;
                typedef std::pair< handle_ref, handle_ref > stdio_pipe_t;
                typedef cpp::UniqueHandle< int, CloseFileDescriptorDeleter > fd_ref;

                /*****************************************************
                 * Pipe creation and others
                 */

                static HANDLE getOSFileHandle( SAA_in const stdio_file_ptr& fileptr )
                {
                    const int fd = ::_fileno( fileptr.get() );

                    BL_ASSERT( fd );
                    BL_ASSERT( -1 != fd );

                    const auto osfHandle = ( HANDLE )( ::_get_osfhandle( fd ) );

                    BL_ASSERT( osfHandle );
                    BL_ASSERT( INVALID_HANDLE_VALUE != osfHandle );

                    return osfHandle;
                }

                static stdio_pipe_t createPipe( SAA_in const bool writeInheritable )
                {
                    stdio_pipe_t pipe;

                    HANDLE hReadPipe = NULL;
                    HANDLE hWritePipe = NULL;

                    SECURITY_ATTRIBUTES sa;

                    /*
                     * Pipe handles must be inheritable, so they can be
                     * passed to another process
                     */

                    sa.nLength = sizeof( sa );
                    sa.lpSecurityDescriptor = NULL;
                    sa.bInheritHandle = TRUE;

                    BL_CHK_BOOL_WINAPI( ::CreatePipe( &hReadPipe, &hWritePipe, &sa, 0U /* nSize */ ) );

                    pipe.first = handle_ref::attach( hReadPipe );
                    pipe.second = handle_ref::attach( hWritePipe );

                    BL_ASSERT( hReadPipe );
                    BL_ASSERT( hWritePipe );

                    {
                        /*
                         * Make one of the handles to be non-inheritable
                         */

                        BL_CHK_BOOL_WINAPI(
                            ::SetHandleInformation(
                                writeInheritable ? hReadPipe : hWritePipe                       /* hObject */,
                                HANDLE_FLAG_INHERIT                                             /* dwMask */,
                                0U                                                              /* dwFlags */
                                )
                            );
                    }

                    return pipe;
                }

                static stdio_file_ptr convert2StdioFile(
                    SAA_inout           handle_ref&                             handle,
                    SAA_in              const bool                              readOnly
                    )
                {
                    fd_ref fd;

                    BL_CHK_ERRNO(
                        -1,
                        ( fd = ::_open_osfhandle( ( intptr_t ) handle.get(), _O_TEXT | ( readOnly ? _O_RDONLY : 0 ) ) ),
                        BL_MSG()
                            << "Cannot convert OS file handle to file descriptor"
                        );

                    const auto origHandle = handle.get();
                    BL_UNUSED( origHandle );

                    if( fd )
                    {
                        /*
                         * The OS handle is now owned by the CRT file descriptor
                         */

                        handle.release();
                    }

                    auto result = convert2StdioFile( fd, readOnly );

                    BL_ASSERT( origHandle == getOSFileHandle( result ) );

                    return result;
                }

                static stdio_file_ptr convert2StdioFile(
                    SAA_inout           fd_ref&                                 fd,
                    SAA_in              const bool                              readOnly
                    )
                {
                    stdio_file_ptr result;

                    BL_CHK_ERRNO(
                        NULL,
                        ( result = stdio_file_ptr::attach( ::_fdopen( fd.get(), readOnly ? "r" : "w" ) ) ),
                        BL_MSG()
                            << "Cannot open file stream"
                        );

                    /*
                     * The file descriptor is now owned by the file stream object
                     */

                    fd.release();

                    return result;
                }

                /*****************************************************
                 * Shared library support
                 */

                static library_handle_t loadLibrary( SAA_in const std::string& name )
                {
                    std::wstring wname( name.begin(), name.end() );

                    const auto h = ::LoadLibraryExW( wname.c_str(), NULL, 0 );

                    BL_CHK_T(
                        false,
                        !!h,
                        createException( "LoadLibraryEx" /* locationOrAPI */, name ),
                        BL_MSG()
                            << "Shared library '"
                            << name
                            << "' cannot be loaded"
                        );

                    return reinterpret_cast< library_handle_t >( h );
                }

                static void freeLibrary( SAA_in const library_handle_t handle )
                {
                    BL_ASSERT( handle );

                    BL_VERIFY( TRUE == ::FreeLibrary( getModule( handle ) ) );
                }

                class FreeLibraryDeleter
                {
                public:

                    void operator()( SAA_inout library_handle_t handle )
                    {
                        this_type::freeLibrary( handle );
                    }
                };

                typedef cpp::SafeUniquePtr< typename std::remove_pointer< library_handle_t >::type, FreeLibraryDeleter > library_ref;

                static void* getProcAddress( SAA_in const library_handle_t handle, const std::string& name )
                {
                    BL_ASSERT( handle );

                    const auto pfn = ::GetProcAddress( getModule( handle ), name.c_str() );

                    BL_CHK_T(
                        false,
                        !!pfn,
                        createException( "GetProcAddress" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot find address of '"
                            << name
                            << "' function in a shared library"
                        );

                    return pfn;
                }

                static std::string getCurrentExecutablePath()
                {
                    WCHAR path[ MAX_PATH ];

                    /*
                     * TODO: This will silently fail with GetLastError=ERROR_INSUFFICIENT_BUFFER
                     * if the current path is too long
                     */

                    BL_CHK_BOOL_WINAPI( ::GetModuleFileNameW( NULL, path, MAX_PATH ) );

                    cpp::wstring_convert_t conv;

                    return conv.to_bytes( path );
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
                        0 == putenvWrapper( name.c_str(), value.c_str(), true /* checkValue */ ),
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
                        0 == putenvWrapper( name.c_str(), "", false /* checkValue */ ),
                        BL_MSG()
                            << "Cannot unset environment variable '"
                            << name
                            << "'"
                        );
                }

                /*****************************************************
                 * FILE* to i/o stream support
                 */

                /*
                 * Windows file stream implementations (std::ifstream / std::ofstream) have non-standard
                 * constructors from FILE* which were used prior version 3 of the devenv environment
                 *
                 * For version 3 of the devenv environment and onwards we will be using the platform
                 * agnostic implementation based on Boost.IOStreams
                 */

#if !defined( BL_DEVENV_VERSION ) || BL_DEVENV_VERSION < 3
                typedef std::ifstream                                       stdio_istream_t;
                typedef std::ofstream                                       stdio_ostream_t;
#else
                typedef stdio_istream_char_t                                stdio_istream_t;
                typedef stdio_ostream_char_t                                stdio_ostream_t;
#endif

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

                    return unwrappedHandle -> tryTimedAwaitTermination( exitCode, timeoutMs );
                }

                static int tryAwaitTermination(
                    SAA_in      const process_handle_t      handle,
                    SAA_in_opt  const int                   timeoutMs = TIMEOUT_INFINITE
                    )
                {
                    int exitCode = 1;

                    auto* unwrappedHandle = getHandle( handle );

                    if( ! unwrappedHandle -> tryTimedAwaitTermination( &exitCode, timeoutMs ) )
                    {
                        ::SetLastError( ERROR_OPERATION_IN_PROGRESS );

                        BL_THROW(
                            createException( "WaitForSingleObject" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Process did not terminate within specified timeout interval ("
                                << timeoutMs
                                << "ms)"
                            );
                    }

                    return exitCode;
                }

                static void terminateProcess(
                    SAA_in      const process_handle_t      handle,
                    SAA_in_opt  const bool                  force = true,
                    SAA_in_opt  const bool                  includeSubprocesses = false
                    )
                {
                    BL_UNUSED( force );

                    if( includeSubprocesses && getHandle( handle ) -> getJobHandle() )
                    {
                        terminateProcessTree( handle );
                    }
                    else
                    {
                         BL_CHK_BOOL_WINAPI(
                            ::TerminateProcess( getHandle( handle ) -> getProcessHandle(), 1 /* exitCode */ )
                            );
                    }
                }

                static void sendProcessStopEvent(
                    SAA_in      const process_handle_t      handle,
                    SAA_in      const bool                  includeSubprocesses
                    )
                {
                    ( void ) terminateProcess( handle, true /* force */, includeSubprocesses );
                }

                static void sendSignal(
                    SAA_in      const process_handle_t      handle,
                    SAA_in      const int                   signal,
                    SAA_in      const bool                  includeSubprocesses
                    )
                {
                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "sendSignal is not implemented on Windows"
                        );
                }

                static process_handle_t createProcess(
                    SAA_in          const std::string&                          userName,
                    SAA_in          const std::string&                          commandLine,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        userName.empty(),
                        NotSupportedException(),
                        BL_MSG()
                            << "Creating processes under a different user name is not supported on Windows"
                        );

                    return createProcess(
                        commandLine,
                        flags,
                        callbackIos,
                        callbackFile
                        );
                }

                static process_handle_t createProcess(
                    SAA_in          const std::string&                          userName,
                    SAA_in          const std::vector< std::string >&           commandArguments,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        userName.empty(),
                        NotSupportedException(),
                        BL_MSG()
                            << "Creating processes under a different user name is not supported on Windows"
                        );

                    return createProcess(
                        commandArguments,
                        flags,
                        callbackIos,
                        callbackFile
                        );
                }

                static process_handle_t createProcess(
                    SAA_in          const std::string&                          commandLine,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    BL_CHK_ARG( ! commandLine.empty(), commandLine );

                    PROCESS_INFORMATION pi;
                    STARTUPINFOW si;
                    ZeroMemory( &pi, sizeof( pi ) );
                    ZeroMemory( &si, sizeof( si ) );
                    si.cb = sizeof( si );

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
                        outPipe = createPipe( true /* writeInheritable */ );
                        outFilePtr = convert2StdioFile( outPipe.first, true /* readOnly */ );

                        if( callbackIos )
                        {
                            out = fileptr2istream( outFilePtr.get() );
                            outPtr = out.get();
                        }

                        si.hStdOutput = outPipe.second.get();
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

                            si.hStdError = outPipe.second.get();
                        }
                        else
                        {
                            errPipe = createPipe( true /* writeInheritable */ );
                            errFilePtr = convert2StdioFile( errPipe.first, true /* readOnly */ );

                            if( callbackIos )
                            {
                                err = fileptr2istream( errFilePtr.get() );
                                errPtr = err.get();
                            }

                            si.hStdError = errPipe.second.get();
                        }
                    }

                    if( flagsRedirect & ProcessCreateFlags::RedirectStdin )
                    {
                        inPipe = createPipe( false /* writeInheritable */ );

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

                        si.hStdInput = inPipe.first.get();
                    }

                    const bool stdioRedirected = ( si.hStdInput || si.hStdOutput || si.hStdError );

                    if( stdioRedirected )
                    {
                        si.dwFlags |= STARTF_USESTDHANDLES;
                    }

                    std::vector< WCHAR > cmdline( commandLine.size() + 1, L'\0' );
                    std::copy( commandLine.begin(), commandLine.end(), cmdline.begin() );

                    auto processJobHandle = cpp::SafeUniquePtr< EncapsulatedProcessJobHandle >::attach(
                        new EncapsulatedProcessJobHandle( ! detachProcess /* terminateProcessOnDestruction */ )
                        );

                    DWORD creationFlags = CREATE_NO_WINDOW;

                    auto assignNewJob = false;

                    BOOL isInJob;

                    BL_CHK_BOOL_WINAPI(
                        ::IsProcessInJob( ::GetCurrentProcess(), NULL /* JobHandle */, &isInJob )
                        );

                    /*
                     * The isInJob check above is needed because according to MSDN a process that is created as
                     * part of existing job can't be assigned to a new job for Windows version less than Windows 8
                     * (AssignProcessToJobObject() fails with ERROR_ACCESS_DENIED).
                     *
                     * If the current process is part of a job object then we must check if the job object
                     * it is part of was created with JOB_OBJECT_LIMIT_BREAKAWAY_OK and only in that case we
                     * can pass CREATE_BREAKAWAY_FROM_JOB flag to the CreateProcess call
                     *
                     * If the current process is not part of a job object then we create a new job object with
                     * JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flags and assign it to
                     * our current process before we attempt the CreateProcess call. In this case we also pass
                     * CREATE_SUSPENDED flag to the CreateProcess call, so we can assign the newly created job
                     * object to the child process before we let it execute
                     */

                    if( detachProcess )
                    {
                        if( isInJob )
                        {
                            JOBOBJECT_BASIC_LIMIT_INFORMATION info = { 0 };

                            BL_CHK_BOOL_WINAPI(
                                ::QueryInformationJobObject(
                                    NULL /* hJob */,
                                    JobObjectBasicLimitInformation,
                                    &info,
                                    sizeof( info ),
                                    NULL /* lpReturnLength */
                                    )
                                );

                            if( info.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK )
                            {
                                creationFlags |= CREATE_BREAKAWAY_FROM_JOB;
                            }
                        }
                    }
                    else
                    {
                        assignNewJob = ! isInJob;

                        if( assignNewJob )
                        {
                            const auto jobName = uuids::uuid2string( uuids::create() );
                            const auto jobHandle = ::CreateJobObject( NULL, jobName.c_str() );

                            if( jobHandle == NULL )
                            {
                                BL_THROW_EC(
                                    createSystemErrorCode( ( int )::GetLastError() ),
                                    BL_MSG()
                                        << "Cannot create a job object"
                                    );
                            }

                            processJobHandle -> setJobHandle( jobHandle );

                            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };

                            jeli.BasicLimitInformation.LimitFlags =
                                JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

                            BL_CHK_BOOL_WINAPI(
                                ::SetInformationJobObject(
                                    jobHandle,
                                    JobObjectExtendedLimitInformation,
                                    &jeli,
                                    sizeof( jeli )
                                    )
                                );

                            creationFlags |= CREATE_SUSPENDED;
                        }
                    }

                    BL_CHK_BOOL_WINAPI_ERROR_MESSAGE(
                        ::CreateProcessW(
                            NULL                                            /* lpApplicationName */,
                            &cmdline[ 0 ]                                   /* lpCommandLine */,
                            NULL                                            /* lpProcessAttributes */,
                            NULL                                            /* lpThreadAttributes */,
                            stdioRedirected ? TRUE : FALSE                  /* bInheritHandles */,
                            creationFlags                                   /* dwCreationFlags */,
                            NULL                                            /* lpEnvironment */,
                            NULL                                            /* lpCurrentDirectory */,
                            &si                                             /* lpStartupInfo */,
                            &pi                                             /* lpProcessInformation */
                            ),
                        BL_MSG()
                            << "Cannot create process with command line '"
                            << commandLine
                            << "'"
                        );

                    processJobHandle -> setProcessHandle( pi.hProcess );

                    if( assignNewJob )
                    {
                        const auto thread = handle_ref::attach( reinterpret_cast< generic_handle_t >( pi.hThread ) );

                        if( ! ::AssignProcessToJobObject( processJobHandle -> getJobHandle(), processJobHandle -> getProcessHandle() ) )
                        {
                            const auto lastError = ::GetLastError();

                            BL_LOG(
                                Logging::warning(),
                                BL_MSG()
                                    << "Cannot assign process to a job object, "
                                    << "error code: "
                                    << lastError
                                    << ", command line '"
                                    << commandLine
                                    << "'"
                                );

                            processJobHandle -> resetJobHandle();
                        }

                        BL_CHK_T(
                            ( DWORD ) -1,
                            ::ResumeThread( pi.hThread ),
                            createException( "ResumeThread" /* locationOrAPI */ ),
                            "Cannot un-suspend process (resume thread)"
                            );
                    }

                    auto process = process_ref::attach( reinterpret_cast< process_handle_t >( processJobHandle.release() ) );

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

                static process_handle_t createProcess(
                    SAA_in          const std::vector< std::string >&           commandArguments,
                    SAA_in_opt      const ProcessCreateFlags                    flags = ProcessCreateFlags::NoRedirect,
                    SAA_in_opt      const process_redirect_callback_ios_t&      callbackIos = process_redirect_callback_ios_t(),
                    SAA_in_opt      const process_redirect_callback_file_t&     callbackFile = process_redirect_callback_file_t()
                    )
                {
                    cpp::SafeOutputStringStream commandLine;
                    bool first = true;

                    for( auto arg : commandArguments )
                    {
                        if( ! first )
                        {
                            commandLine << ' ';
                        }

                        if( cpp::contains( arg, ' ' ) )
                        {
                            str::replace_all( arg, "\\", "\\\\" );
                            str::replace_all( arg, "\"", "\\\"" );

                            commandLine << '"' << arg << '"';
                        }
                        else
                        {
                            commandLine << arg;
                        }

                        first = false;
                    }

                    return createProcess( commandLine.str(), flags, callbackIos, callbackFile );
                }

                static std::uint64_t getPid( SAA_in_opt const process_ref& process )
                {
                    if( process )
                    {
                        const auto pid = ::GetProcessId( getHandle( process.get() ) -> getProcessHandle() );

                        BL_CHK_T(
                            0U,
                            pid,
                            createException( "GetProcessId" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Cannot obtain process id from a process handle"
                            );

                        return static_cast< std::uint64_t >( pid );
                    }
                    else
                    {
                        return static_cast< std::uint64_t >( ::GetCurrentProcessId() );
                    }
                }

                /*****************************************************
                 * File I/O support
                 */

                static stdio_file_ptr fopen( SAA_in const fs::path& path, SAA_in const char* mode )
                {
                    stdio_file_ptr  file;

                    std::string     smode( mode );
                    std::wstring    wmode( smode.begin(), smode.end() );

                    /*
                     * On Windows we want to use _wfopen to make sure we don't
                     * go through the non-UNICODE APIs
                     */

                    BL_CHK_ERRNO_USER_FRIENDLY(
                        false,
                        !! ( file = os::stdio_file_ptr::attach( ::_wfopen( path.native().c_str(), wmode.c_str() ) ) ),
                        BL_MSG()
                            << "Cannot open or create file "
                            << fs::normalizePathParameterForPrint( path )
                        );

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
                        0 == ::_fseeki64( fileptr.get(), ( std::int64_t ) offset, origin )
                        );
                }

                static std::uint64_t ftell(
                    SAA_in          const stdio_file_ptr&               fileptr
                    )
                {
                    const auto pos = ::_ftelli64( fileptr.get() );
                    BL_CHK_ERRNO_NM( -1L, pos );

                    return pos;
                }

                static void updateFileAttributes(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const FileAttributes                attributes,
                    SAA_in          const bool                          remove
                    )
                {
                    BL_CHK(
                        false,
                        attributes && ( attributes == ( FileAttributesMask & attributes ) ),
                        BL_MSG()
                            << "Invalid file attributes "
                            << attributes
                        );

                    const auto pwzFileName = path.native().c_str();

                    const DWORD currentAttributes = ::GetFileAttributesW( pwzFileName );

                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        INVALID_FILE_ATTRIBUTES != currentAttributes,
                        createException( "GetFileAttributesW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot obtain file attributes for "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    DWORD newAttributes = currentAttributes;

                    if( FileAttributeHidden & attributes )
                    {
                        newAttributes = remove ?
                            ( newAttributes & ( ~FILE_ATTRIBUTE_HIDDEN ) ) :
                            ( newAttributes | FILE_ATTRIBUTE_HIDDEN );
                    }

                    if( newAttributes != currentAttributes )
                    {
                        BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                            ::SetFileAttributesW( pwzFileName, newAttributes ),
                            BL_MSG()
                                << "Cannot set file attributes for "
                                << fs::normalizePathParameterForPrint( path )
                            );
                    }
                }

                static FileAttributes getFileAttributes( SAA_in const fs::path& path )
                {
                    const auto pwzFileName = path.native().c_str();

                    const DWORD nativeAttributes = ::GetFileAttributesW( pwzFileName );

                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        INVALID_FILE_ATTRIBUTES != nativeAttributes,
                        createException( "GetFileAttributesW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot obtain file attributes for "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    FileAttributes attributes = FileAttributeNone;

                    if( nativeAttributes & FILE_ATTRIBUTE_HIDDEN )
                    {
                        attributes = static_cast< FileAttributes >( attributes | FileAttributeHidden );
                    }

                    return attributes;
                }

                static std::time_t getFileCreateTime( SAA_in const fs::path& path )
                {
                    WIN32_FILE_ATTRIBUTE_DATA fa = { 0 };

                    const auto pwzFileName = path.native().c_str();

                    BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                        ::GetFileAttributesExW( pwzFileName, GetFileExInfoStandard, ( LPVOID ) &fa ),
                        BL_MSG()
                            << "Cannot obtain file attributes for "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    return FILETIME2time_t( fa.ftCreationTime );
                }

                static void setFileCreateTime(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const std::time_t&                  newTime
                    )
                {
                    FILETIME fileTime;
                    time_t2FILETIME( newTime, fileTime );

                    const auto pwzFileName = path.native().c_str();

                    const auto rawHandle = ::CreateFileW(
                            pwzFileName,
                            FILE_WRITE_ATTRIBUTES,                                      /* dwDesiredAccess */
                            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,     /* dwShareMode */
                            NULL,                                                       /* lpSecurityAttributes */
                            OPEN_EXISTING                                               /* dwCreationDisposition */,
                            FILE_FLAG_BACKUP_SEMANTICS,                                 /* dwFlagsAndAttributes */
                            NULL                                                        /* hTemplateFile */
                            );

                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        INVALID_HANDLE_VALUE != rawHandle,
                        createException( "CreateFileW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot open file "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    const auto handle = handle_ref::attach( rawHandle );

                    BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                        ::SetFileTime(
                            handle.get(),
                            &fileTime       /* lpCreationTime */,
                            NULL            /* lpLastAccessTime */,
                            NULL            /* lpLastWriteTime */
                            ),
                        BL_MSG()
                            << "Cannot update file create time for "
                            << fs::normalizePathParameterForPrint( path )
                        );
                }

                static bool getConsoleSize(
                    SAA_out         int&                                columns,
                    SAA_out         int&                                rows
                    )
                {
                    const auto handle = getConsoleHandle();

                    if( handle == NULL )
                    {
                        columns = rows = -1;

                        return false;
                    }

                    CONSOLE_SCREEN_BUFFER_INFO csbi;

                    if( ! ::GetConsoleScreenBufferInfo( handle, &csbi ) )
                    {
                        /*
                         * The handle may refer to a console as well as a character device (NUL, CON).
                         * We are unable to determine the exact handle type beforehand so we
                         * silently ignore invalid handle errors. Other errors are reported.
                         */

                        BL_CHK_T(
                            false,
                            ::GetLastError() == ERROR_INVALID_HANDLE,
                            createException( "GetConsoleScreenBufferInfo" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Cannot get console screen buffer info"
                            );

                        columns = rows = -1;

                        return false;
                    }

                    /*
                     * Return the visible window size, not the entire console buffer size (typically having 300 rows)
                     */

                    columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                    rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

                    return true;
                }

                /**
                 * @brief Return console handle or NULL if standard input/output is not using the console.
                 * @note Do not close the returned handle. You don't own it.
                 */

                static HANDLE getConsoleHandle( SAA_in_opt const bool getInput = false )
                {
                    const auto handle = ::GetStdHandle( getInput ? STD_INPUT_HANDLE : STD_OUTPUT_HANDLE );

                    if( handle != INVALID_HANDLE_VALUE && handle != NULL )
                    {
                        /*
                         * The handle may also refer to a file or a pipe if standard input/output has been
                         * redirected. Check the file type to make sure that it is not one of these.
                         */

                        const auto fileType = ::GetFileType( handle );

                        if( fileType == FILE_TYPE_CHAR )
                        {
                            /*
                             * The handle is either a console, printer, serial port, NUL(!) or CON(!).
                             * Console API calls may still fail with this handle.
                             */

                            return handle;
                        }
                    }

                    return NULL;
                }

                static std::string getUserName()
                {
                    if( OSImplBase::tryGetEnvironmentVariable( "BL_ANALYSIS_TESTING" ) )
                    {
                        /*
                         * On Windows we just use USERNAME environment variable
                         * because GetUserNameW and GetUserNameExW trigger weird
                         * app verifier failures when page heap is turned of with
                         * the unaligned flag
                         *
                         * At some point when we have more time to investigate
                         * this external problem and find a better workaround
                         * we can switch back to the official GetUserNameW /
                         * GetUserNameExW APIs
                         */

                        return OSImplBase::getEnvironmentVariable( "USERNAME" );
                    }

                    WCHAR buffer[ UNLEN + 1 ];
                    DWORD bufLen = BL_ARRAY_SIZE( buffer );

                    buffer[ 0 ] = 0;

                    BL_CHK_BOOL_WINAPI( ::GetUserNameW( buffer, &bufLen ) );

                    /*
                     * GetUserNameW should zero terminate the buffer, but
                     * being paranoid about this is never wrong
                     */

                    buffer[ BL_ARRAY_SIZE( buffer ) - 1 ] = 0;

                    cpp::wstring_convert_t conv;

                    return conv.to_bytes( buffer );
                }

                static auto getLoggedInUserNames() -> std::vector< std::string >
                {
                    ULONG count;
                    PLUID sessionsPtr;

                    auto status = ::LsaEnumerateLogonSessions( &count, &sessionsPtr );

                    BL_CHK(
                        false,
                        status == STATUS_SUCCESS,
                        BL_MSG()
                            << "LsaEnumerateLogonSessions failed; NTSTATUS="
                            << status
                        );

                    BL_SCOPE_EXIT( ::LsaFreeReturnBuffer( sessionsPtr ); );

                    /*
                     * LsaGetLogonSessionData may return multiple sessions
                     * for the same user hence using set to avoid duplicates
                     */

                    std::unordered_set< std::string > names;

                    cpp::wstring_convert_t conv;

                    for( ULONG i = 0; i < count; ++i )
                    {
                        PSECURITY_LOGON_SESSION_DATA dataPtr;

                        status = ::LsaGetLogonSessionData( sessionsPtr + i, &dataPtr );

                        if( status == STATUS_ACCESS_DENIED )
                        {
                            /*
                             * It is expected that when running the process as non-admin some system sessions
                             * (like Desktop Windows Manager's "DWM-1" account) cannot be read - ignoring these
                             */

                            continue;
                        }
                        else
                        {
                            BL_CHK(
                                false,
                                status == STATUS_SUCCESS,
                                BL_MSG()
                                    << "LsaGetLogonSessionData failed; NTSTATUS="
                                    << status
                                );
                        }

                        BL_SCOPE_EXIT( ::LsaFreeReturnBuffer( dataPtr ); );

                        /*
                         * Consider only interactive logons (e.g. ignore Network) and
                         * skip Window Manager account ("DWM-*") which is always
                         * logged-in when hardware accelerated desktop is enabled
                         */

                        const auto isInteractiveLogon =
                            SECURITY_LOGON_TYPE::Interactive == dataPtr -> LogonType ||
                            SECURITY_LOGON_TYPE::RemoteInteractive == dataPtr -> LogonType;

                        if (
                            isInteractiveLogon &&
                            g_windowManagerDomain != dataPtr -> LogonDomain.Buffer
                            )
                        {
                            names.emplace( conv.to_bytes( dataPtr -> UserName.Buffer ) );
                        }
                    }

                    std::vector< std::string > result;

                    std::move( names.begin(), names.end(), std::back_inserter( result ) );

                    return result;
                }

                static bool isClientOS()
                {
                    return ! ::IsWindowsServer();
                }

                static std::string getFileOwner( SAA_in const std::string& filename )
                {
                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "getFileOwner is not implemented on Windows"
                        );
                }

                static void enablePrivilege( SAA_in const std::wstring& privilegeName )
                {
                    handle_ref token;
                    TOKEN_PRIVILEGES tp;

                    HANDLE tokenHandle;

                    BL_CHK_BOOL_WINAPI(
                        ::OpenProcessToken( ::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle )
                        );

                    token = handle_ref::attach( tokenHandle );

                    BL_CHK_BOOL_WINAPI(
                        ::LookupPrivilegeValueW(
                            NULL,
                            privilegeName.c_str(),
                            &tp.Privileges[ 0 ].Luid
                            )
                        );

                    tp.PrivilegeCount = 1U;
                    tp.Privileges[ 0 ].Attributes = SE_PRIVILEGE_ENABLED;

                    BL_CHK_BOOL_WINAPI(
                        ::AdjustTokenPrivileges( token.get(), FALSE, &tp, sizeof( TOKEN_PRIVILEGES ), NULL, NULL )
                        );
                }

                static void addBackupRestorePrivilege( SAA_in const bool enableRestore = false )
                {
                    if( enableRestore && g_restoreEnabled )
                    {
                        return;
                    }

                    if( false == enableRestore && g_backupEnabled )
                    {
                        return;
                    }

                    enablePrivilege( enableRestore ? L"SeRestorePrivilege" : L"SeBackupPrivilege" );

                    if( enableRestore )
                    {
                        g_restoreEnabled = true;
                    }
                    else
                    {
                        g_backupEnabled = true;
                    }
                }

                static handle_ref openDirectory(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const bool                          writeAccess = false
                    )
                {
                    addBackupRestorePrivilege( writeAccess );

                    const DWORD access = writeAccess ? ( GENERIC_READ | GENERIC_WRITE ) : GENERIC_READ;

                    const HANDLE rawHandle = ::CreateFileW(
                        path.native().c_str(),
                        access                                                          /* dwDesiredAccess */,
                        0U                                                              /* dwShareMode - no sharing */,
                        NULL                                                            /* lpSecurityAttributes */,
                        OPEN_EXISTING                                                   /* dwCreationDisposition */,
                        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS       /* dwFlagsAndAttributes */,
                        NULL                                                            /* hTemplateFile */
                        );

                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        INVALID_HANDLE_VALUE != rawHandle,
                        createException( "CreateFileW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot open directory "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    return handle_ref::attach( rawHandle );
                }

                template
                <
                    typename T
                >
                static T getAndProcessReparseBuffer(
                    SAA_in          const handle_ref&                   device,
                    SAA_in          const typename RCB< T >::type&      cb
                    )
                {
                    BYTE staticBuffer[ REPARSE_BUFFER_SIZE_DEFAULT ];
                    cpp::SafeUniquePtr< BYTE[] > dynamicBuffer;
                    DWORD bytesReturned;
                    eh::error_code ec;

                    BYTE* buffer = staticBuffer;
                    DWORD bufferSizeInBytes = BL_ARRAY_SIZE( staticBuffer );

                    for( ;; )
                    {
                        const auto win32Bool = ::DeviceIoControl(
                            device.get()                                                    /* hDevice */,
                            FSCTL_GET_REPARSE_POINT                                         /* dwIoControlCode */,
                            NULL                                                            /* lpInBuffer */,
                            0U                                                              /* nInBufferSize */,
                            buffer                                                          /* lpOutBuffer */,
                            bufferSizeInBytes                                               /* nOutBufferSize */,
                            &bytesReturned                                                  /* lpBytesReturned */,
                            NULL                                                            /* lpOverlapped */
                            );

                        if( win32Bool )
                        {
                            break;
                        }

                        if(
                            ERROR_INSUFFICIENT_BUFFER == ::GetLastError() ||
                            ERROR_MORE_DATA == ::GetLastError()
                            )
                        {
                            /*
                             * Insufficient buffer case
                             *
                             * Some drivers will return ERROR_INSUFFICIENT_BUFFER and
                             * zero bytesReturned and some will return partial data
                             * with non-zero bytesReturned and GLE = ERROR_MORE_DATA
                             */

                            bufferSizeInBytes *= 2;
                            dynamicBuffer.reset( new BYTE[ bufferSizeInBytes ] );
                            buffer = dynamicBuffer.get();

                            continue;
                        }

                        /*
                         * If we're here we have a real error that should be reported
                         * to the callback
                         */

                        ec.assign( ( int )::GetLastError(), eh::system_category() );

                        break;
                    }

                    if( ec )
                    {
                        /*
                         * If we have an error the buffer will be NULL and size will be zero
                         */

                        buffer = NULL;
                        bufferSizeInBytes = 0U;
                    }
                    else
                    {
                        BL_ASSERT( buffer );

                        BL_CHK(
                            false,
                            bytesReturned >= REPARSE_BUFFER_HEADER_SIZE,
                            BL_MSG()
                                << "DeviceIoControl returned invalid data"
                            );
                    }

                    return cb(
                        ec,
                        reinterpret_cast< const REPARSE_DATA_BUFFER* >( buffer ),
                        bytesReturned
                        );
                }

                static bool validateJunction(
                    SAA_in_bcount_opt( sizeInBytes )    const REPARSE_DATA_BUFFER*          buffer,
                    SAA_in_opt                          const DWORD                         sizeInBytes
                    )
                {
                    if(
                        NULL == buffer ||
                        sizeInBytes < REPARSE_MOUNT_POINT_TOTAL_HEADER_SIZE ||
                        IO_REPARSE_TAG_MOUNT_POINT != buffer -> ReparseTag
                        )
                    {
                        return false;
                    }

                    /*
                     * This is appears to be a mount point
                     *
                     * Calculate the expected reparse data length in bytes and
                     * validate the rest of the structure, including the total
                     * size passed in (sizeInBytes)
                     *
                     * The two wide chars added are for the zero terminators
                     */

                    const DWORD expectedPathsLengthInBytes =
                        ( 2 * sizeof( WCHAR ) ) +
                        buffer -> MountPointReparseBuffer.SubstituteNameLength +
                        buffer -> MountPointReparseBuffer.PrintNameLength;

                    const DWORD expectedDataLengthInBytes =
                        REPARSE_MOUNT_POINT_SUB_HEADER_SIZE + expectedPathsLengthInBytes;

                    if( buffer -> ReparseDataLength != expectedDataLengthInBytes )
                    {
                        return false;
                    }

                    if( sizeInBytes != ( REPARSE_BUFFER_HEADER_SIZE + expectedDataLengthInBytes ) )
                    {
                        return false;
                    }

                    if(
                        ( buffer -> MountPointReparseBuffer.SubstituteNameLength % 2 ) ||
                        ( buffer -> MountPointReparseBuffer.PrintNameLength % 2 )
                        )
                    {
                        /*
                         * The paths are wide chars and their lengths are in bytes, so they
                         * must be even numbers
                         */

                        return false;
                    }

                    /*
                     * Verify that all offsets are bounded inside the data buffer
                     */

                    const DWORD substituteNameEndOffset =
                        buffer -> MountPointReparseBuffer.SubstituteNameOffset +
                        buffer -> MountPointReparseBuffer.SubstituteNameLength +
                        sizeof( WCHAR );

                    const DWORD printNameEndOffset =
                        buffer -> MountPointReparseBuffer.PrintNameOffset +
                        buffer -> MountPointReparseBuffer.PrintNameLength +
                        sizeof( WCHAR );

                    const DWORD maxOffset = sizeInBytes - REPARSE_MOUNT_POINT_TOTAL_HEADER_SIZE;

                    if(
                        buffer -> MountPointReparseBuffer.SubstituteNameOffset > maxOffset ||
                        substituteNameEndOffset > maxOffset ||
                        buffer -> MountPointReparseBuffer.PrintNameOffset > maxOffset ||
                        printNameEndOffset > maxOffset
                        )
                    {
                        return false;
                    }

                    return true;
                }

                static void chkJunction(
                    SAA_in          const eh::error_code&               ec,
                    SAA_in          const fs::path&                     junction
                    )
                {
                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Path "
                            << junction
                            << " is not a valid junction"
                        );
                }

                static void createJunction(
                    SAA_in          const fs::path&             to,
                    SAA_in          const fs::path&             junction
                    )
                {
                    const auto dir = openDirectory( junction, true /* writeAccess */ );

                    auto targetClean = to.native();

                    if( 0U == targetClean.find( g_lfnPrefix ) )
                    {
                        targetClean.erase( targetClean.begin(), targetClean.begin() + g_lfnPrefix.size() );

                        if( targetClean.back() != L'\\' )
                        {
                            /*
                             * The substitute name must end in backslash, but we will
                             * strip the backslash in the print name
                             */

                            targetClean += L"\\";
                        }
                    }

                    const DWORD substituteNameLengthInBytes =
                        ( DWORD ) ( ( g_noParsePrefix.size() + targetClean.size() ) * sizeof( WCHAR ) );

                    /*
                     * When we copy the print name later we'll delete the training slash char
                     * so we subtract - 1 when we calculate the print name length
                     */

                    const DWORD printNameLengthInBytes = ( DWORD ) ( ( targetClean.size() - 1 ) * sizeof( WCHAR ) );

                    /*
                     * The paths length is the two paths length plus two wide chars for the
                     * zero terminator characters
                     */

                    const DWORD pathsLengthInBytes =
                        ( 2 * sizeof( WCHAR ) ) +
                        substituteNameLengthInBytes +
                        printNameLengthInBytes;

                    /*
                     * ReparseDataLength is expected to be REPARSE_MOUNT_POINT_SUB_HEADER_SIZE plus
                     * the size of the paths calculated above
                     *
                     * The total buffer size needed (bufferSizeInBytes) is ReparseDataLength plus
                     * the reparse data structure headers size
                     */

                    const DWORD dataLengthInBytes = REPARSE_MOUNT_POINT_SUB_HEADER_SIZE + pathsLengthInBytes;

                    const DWORD bufferSizeInBytes = REPARSE_BUFFER_HEADER_SIZE + dataLengthInBytes;

                    /*
                     * Allocate the buffer and initialize the header
                     */

                    BYTE staticBuffer[ REPARSE_BUFFER_SIZE_DEFAULT ];
                    cpp::SafeUniquePtr< BYTE[] > dynamicBuffer;
                    BYTE* buffer;

                    if( bufferSizeInBytes <= BL_ARRAY_SIZE( staticBuffer ) )
                    {
                        buffer = staticBuffer;
                    }
                    else
                    {
                        dynamicBuffer.reset( new BYTE[ bufferSizeInBytes ] );
                        buffer = dynamicBuffer.get();
                    }

                    ::memset( buffer, 0, bufferSizeInBytes );

                    auto& dataIn = *reinterpret_cast< REPARSE_DATA_BUFFER* >( buffer );

                    dataIn.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
                    dataIn.ReparseDataLength = ( USHORT ) dataLengthInBytes;

                    dataIn.MountPointReparseBuffer.SubstituteNameOffset = 0U;
                    dataIn.MountPointReparseBuffer.SubstituteNameLength = ( USHORT ) substituteNameLengthInBytes;
                    dataIn.MountPointReparseBuffer.PrintNameOffset = ( USHORT ) ( substituteNameLengthInBytes + sizeof( WCHAR ) );
                    dataIn.MountPointReparseBuffer.PrintNameLength = ( USHORT ) printNameLengthInBytes;

                    /*
                     * Copy the paths now
                     */

                    BYTE* psz = reinterpret_cast< BYTE* >( dataIn.MountPointReparseBuffer.PathBuffer );
                    std::size_t bytesToCopy;

                    bytesToCopy = sizeof( WCHAR ) * g_noParsePrefix.size();
                    std::memcpy( psz, g_noParsePrefix.c_str(), bytesToCopy );
                    psz += bytesToCopy;

                    bytesToCopy = sizeof( WCHAR ) * ( targetClean.size() + 1 );
                    std::memcpy( psz, targetClean.c_str(), bytesToCopy );
                    psz += bytesToCopy;

                    /*
                     * Before we copy the print path we erase the trailing slash character
                     */

                    targetClean.erase( targetClean.end() - 1 );

                    bytesToCopy = sizeof( WCHAR ) * ( targetClean.size() + 1 );
                    std::memcpy( psz, targetClean.c_str(), bytesToCopy );
                    psz += bytesToCopy;

                    BL_ASSERT( psz == ( buffer + bufferSizeInBytes ) );

                    DWORD bytesReturnedDummy;

                    BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                        ::DeviceIoControl(
                            dir.get()                                                       /* hDevice */,
                            FSCTL_SET_REPARSE_POINT                                         /* dwIoControlCode */,
                            buffer                                                          /* lpInBuffer */,
                            bufferSizeInBytes                                               /* nInBufferSize */,
                            NULL                                                            /* lpOutBuffer */,
                            0U                                                              /* nOutBufferSize */,
                            &bytesReturnedDummy                                             /* lpBytesReturned */,
                            NULL                                                            /* lpOverlapped */
                            ),
                        BL_MSG()
                                << "Cannot create junction "
                                << junction
                                << " pointing to "
                                << to
                        );
                }

                static void deleteJunction( SAA_in const fs::path& junction )
                {
                    const auto dir = openDirectory( junction, true /* writeAccess */ );

                    REPARSE_DATA_BUFFER dataIn;

                    ::memset( &dataIn, 0, sizeof( dataIn ) );

                    dataIn.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

                    DWORD bytesReturnedDummy;

                    BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                        ::DeviceIoControl(
                            dir.get()                                                       /* hDevice */,
                            FSCTL_DELETE_REPARSE_POINT                                      /* dwIoControlCode */,
                            &dataIn                                                         /* lpInBuffer */,
                            REPARSE_BUFFER_HEADER_SIZE                                      /* nInBufferSize */,
                            NULL                                                            /* lpOutBuffer */,
                            0U                                                              /* nOutBufferSize */,
                            &bytesReturnedDummy                                             /* lpBytesReturned */,
                            NULL                                                            /* lpOverlapped */
                            ),
                        BL_MSG()
                            << "Cannot delete junction "
                            << junction
                        );
                }

                static bool isJunction( SAA_in const fs::path& path )
                {
                    const auto dir = openDirectory( path, false /* writeAccess */ );

                    return getAndProcessReparseBuffer< bool >(
                        dir,
                        [ & ]
                        (
                            SAA_in                              const eh::error_code&               ec,
                            SAA_in_bcount_opt( sizeInBytes )    const REPARSE_DATA_BUFFER*          buffer,
                            SAA_in_opt                          const DWORD                         sizeInBytes
                        )
                        -> bool
                        {
                            if(
                                ec &&
                                ec.category() == eh::system_category() &&
                                ec.value() == ERROR_NOT_A_REPARSE_POINT
                                )
                            {
                                return false;
                            }

                            BL_CHK_EC_NM( ec );

                            BL_ASSERT( buffer );
                            BL_ASSERT( sizeInBytes >= REPARSE_BUFFER_HEADER_SIZE );

                            /*
                             * If there is no error code the buffer and buffer size must be
                             * valid
                             */

                            SAA_assume( buffer );
                            SAA_assume( sizeInBytes >= REPARSE_BUFFER_HEADER_SIZE );

                            return ( IO_REPARSE_TAG_MOUNT_POINT == buffer -> ReparseTag );
                        }
                        );
                }

                static fs::path getJunctionTarget( SAA_in const fs::path& junction )
                {
                    const auto dir = openDirectory( junction, false /* writeAccess */ );

                    return getAndProcessReparseBuffer< fs::path >(
                        dir,
                        [ & ]
                        (
                            SAA_in                              const eh::error_code&               ec,
                            SAA_in_bcount_opt( sizeInBytes )    const REPARSE_DATA_BUFFER*          buffer,
                            SAA_in_opt                          const DWORD                         sizeInBytes
                        )
                        -> fs::path
                        {
                            chkJunction( ec, junction );

                            if( ! validateJunction( buffer, sizeInBytes ) )
                            {
                                chkJunction(
                                    eh::errc::make_error_code( eh::errc::invalid_argument ),
                                    junction
                                    );
                            }

                            const WCHAR* targetChars =
                                reinterpret_cast< const WCHAR* >(
                                    reinterpret_cast< const BYTE* >( buffer ) +
                                    REPARSE_MOUNT_POINT_TOTAL_HEADER_SIZE +
                                    buffer -> MountPointReparseBuffer.SubstituteNameOffset
                                    );

                            const size_t targetLength =
                                buffer -> MountPointReparseBuffer.SubstituteNameLength / 2;

                            std::wstring targetPath( targetChars, targetChars + targetLength );

                            /*
                             * The substitute name must start with the expected junction prefix
                             * check for that and then erase it
                             */

                            if( 0U != targetPath.find( g_noParsePrefix ) )
                            {
                                chkJunction(
                                    eh::errc::make_error_code( eh::errc::invalid_argument ),
                                    junction
                                    );
                            }

                            targetPath.erase( targetPath.begin(), targetPath.begin() + g_noParsePrefix.length() );

                            if( targetPath.size() && targetPath.back() == L'\\' )
                            {
                                /*
                                 * Check to remove the trailing slash if any
                                 */

                                targetPath.erase( targetPath.end() - 1 );
                            }

                            return fs::path( targetPath );
                        }
                        );
                }

                static void createHardlink(
                    SAA_in          const fs::path&             to,
                    SAA_in          const fs::path&             hardlink
                    )
                {
                    BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                        ::CreateHardLinkW(
                            hardlink.native().c_str(),          /* lpFileName */
                            to.native().c_str(),                /* lpExistingFileName */
                            NULL                                /* lpSecurityAttributes */
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
                    const auto pwzFileName = path.native().c_str();

                    const auto rawHandle = ::CreateFileW(
                        pwzFileName,
                        0,                                                          /* dwDesiredAccess */
                        FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,     /* dwShareMode */
                        NULL,                                                       /* lpSecurityAttributes */
                        OPEN_EXISTING                                               /* dwCreationDisposition */,
                        FILE_FLAG_BACKUP_SEMANTICS,                                 /* dwFlagsAndAttributes */
                        NULL                                                        /* hTemplateFile */
                        );

                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        INVALID_HANDLE_VALUE != rawHandle,
                        createException( "CreateFileW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot open file "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    const auto handle = handle_ref::attach( rawHandle );

                    BY_HANDLE_FILE_INFORMATION fileInfo;

                    ::memset( &fileInfo, 0, sizeof( fileInfo ) );

                    BL_CHK_BOOL_WINAPI_USER_FRIENDLY(
                        ::GetFileInformationByHandle( handle.get(), &fileInfo ),
                        BL_MSG()
                            << "Cannot get file information for "
                            << fs::normalizePathParameterForPrint( path )
                        );

                    return fileInfo.nNumberOfLinks;
                }

                static fs::path getUsersDirectory()
                {
                    WCHAR staticBuffer[ MAX_PATH ];

                    DWORD lpcchSize = BL_ARRAY_SIZE( staticBuffer );

                    const auto wasSuccessful =
                        ::GetProfilesDirectoryW(
                            reinterpret_cast< LPWSTR >( staticBuffer )      /* lpProfilesDir */,
                            &lpcchSize                                      /* lpcchSize */
                            );

                    cpp::wstring_convert_t conv;

                    if( wasSuccessful )
                    {
                        return fs::path( std::string( conv.to_bytes( staticBuffer ) ) );
                    }

                    BL_CHK_T(
                        false,
                        ::GetLastError() == ERROR_INSUFFICIENT_BUFFER,
                        createException( "GetProfilesDirectoryW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Unable to obtain profiles directory"
                        );

                    cpp::SafeUniquePtr< WCHAR[] > dynamicBuffer;

                    dynamicBuffer.reset( new WCHAR[ lpcchSize ] );

                    BL_CHK_BOOL_WINAPI(
                        ::GetProfilesDirectoryW(
                            reinterpret_cast< LPWSTR >( dynamicBuffer.get() )       /* lpProfilesDir */,
                            &lpcchSize                                              /* lpcchSize */
                            )
                        );

                    return fs::path( std::string( conv.to_bytes( dynamicBuffer.get() ) ) );
                }

                /**
                 * @brief Disable console input echo
                 */

                class DisableConsoleEcho FINAL
                {
                    BL_NO_COPY_OR_MOVE( DisableConsoleEcho )

                    cpp::ScalarTypeIniter< HANDLE >                 m_consoleHandle;
                    cpp::ScalarTypeIniter< DWORD >                  m_mode;
                    cpp::ScalarTypeIniter< bool >                   m_echoDisabled;

                public:

                    DisableConsoleEcho()
                    {
                        m_consoleHandle = getConsoleHandle( true /* getInput */ );

                        if( m_consoleHandle != NULL )
                        {
                            if( ! ::GetConsoleMode( m_consoleHandle, &m_mode.lvalue() ) )
                            {
                                /*
                                 * The handle may refer to a console as well as a character device (NUL, CON).
                                 * We are unable to determine the exact handle type beforehand so we
                                 * silently ignore invalid handle errors. Other errors are reported.
                                 */

                                BL_CHK_T(
                                    false,
                                    ::GetLastError() == ERROR_INVALID_HANDLE,
                                    createException( "GetConsoleMode" /* locationOrAPI */ ),
                                    BL_MSG()
                                        << "Cannot obtain console mode settings"
                                    );
                            }
                            else
                            {
                                /*
                                 * Disable input echo
                                 */

                                BL_CHK_BOOL_WINAPI( ::SetConsoleMode( m_consoleHandle, m_mode & ~ENABLE_ECHO_INPUT ) );

                                m_echoDisabled = true;
                            }
                        }
                    }

                    ~DisableConsoleEcho() NOEXCEPT
                    {
                        if( m_echoDisabled )
                        {
                            ::SetConsoleMode( m_consoleHandle, m_mode );
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
                 */

                class RobustNamedMutex
                {
                    BL_NO_COPY_OR_MOVE( RobustNamedMutex )

                private:

                    handle_ref m_lock;

                public:

                    typedef ipc::scoped_lock< RobustNamedMutex > Guard;

                    RobustNamedMutex( SAA_in const std::string& name )
                    {
                        std::wstring wname( name.begin(), name.end() );

                        const auto rawHandle =
                            ::CreateMutexW( NULL /* security attributes */, FALSE /* take ownership */, wname.c_str() );

                        BL_CHK_T(
                            false,
                            NULL != rawHandle,
                            createException( "CreateMutexW" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Cannot create a named mutex '"
                                << name
                                << "'"
                            );

                        m_lock = handle_ref::attach( rawHandle );
                    }

                    void lock()
                    {
                        const auto wait = ::WaitForSingleObject( m_lock.get(), INFINITE );

                        if( WAIT_OBJECT_0 == wait || WAIT_ABANDONED == wait )
                        {
                            /*
                             * The wait was successful or the mutex was abandoned
                             *
                             * If the mutex was abandoned now we're the new owner
                             *
                             * This is generally an OK situation because the process
                             * may have crashed or terminated abruptly for some other
                             * reason
                             */

                            return;
                        }

                        BL_CHK_T(
                            false,
                            WAIT_FAILED == wait,
                            createException( "WaitForSingleObject" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Waiting on a named mutex failed"
                            );

                        /*
                         * This should never happen since we pass INFINITE to the
                         * wait call
                         */

                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Waiting on a named mutex timed out"
                            );
                    }

                    void unlock()
                    {
                        BL_CHK_BOOL_WINAPI( ::ReleaseMutex( m_lock.get() ) );
                    }
                };

                /*****************************************************************************
                 * Set priority for CPU, IO & Memory based on whether the process
                 * requires normal vs. background priority
                 */

                static void setMemoryPagingPriority( SAA_in const AbstractPriority priority )
                {
                    enum ProcessInfoClass : ULONG
                    {
                        ProcessPagePriorityClass            = 39,
                    };

                    enum ProcessPagePriority : ULONG
                    {
                        ProcessPagePriorityLow              = 2,
                        ProcessPagePriorityNormal           = 5,
                    };

                    /*
                     * Note, on Windows the 'Greedy' option will be the same as 'Normal'
                     */

                    const auto pagePriority =
                        priority == AbstractPriority::Background ?
                            ProcessPagePriorityLow :
                            ProcessPagePriorityNormal;

                    setProcessInformationWrapper( ProcessPagePriorityClass, pagePriority );
                }

                static void setIoPriority( SAA_in const AbstractPriority priority )
                {
                    enum ProcessInfoClass : ULONG
                    {
                        ProcessIoPriorityClass              = 33,
                    };

                    enum ProcessIoPriority : ULONG
                    {
                        ProcessIoPriorityLow                = 1,
                        ProcessIoPriorityNormal             = 2,
                    };

                    /*
                     * Note, on Windows the 'Greedy' option will be the same as 'Normal'
                     */

                    const auto ioPriority =
                        priority == AbstractPriority::Background ?
                            ProcessIoPriorityLow :
                            ProcessIoPriorityNormal;

                    setProcessInformationWrapper( ProcessIoPriorityClass, ioPriority );
                }

                static void setProcessInformationWrapper(
                    SAA_in          const ULONG                         processInfoType,
                    SAA_in          const ULONG                         processInfoValue
                    )
                {
                    /*
                     * This functionality while available in both Vista and Win7 is not exposed there
                     * as public Win32 API, but it was only exposed in Win8:
                     * http://msdn.microsoft.com/en-us/library/windows/desktop/hh448389(v=vs.85).aspx
                     *
                     * Therefore we need to call the low level Nt* API directly to make our code to
                     * work for all platforms, including Vista and Win7
                     */

                    typedef
                    LONG
                    WINAPI
                    NtSetInformationProcessType(
                        HANDLE              ProcessHandle,
                        ULONG               ProcessInformationClass,
                        PVOID               ProcessInformation,
                        ULONG               ProcessInformationLength
                        );

                    typedef NtSetInformationProcessType* PfnNtSetInformationProcess;

                    const auto ntdll = ::GetModuleHandleW( L"ntdll.dll" );

                    BL_CHK_T(
                        false,
                        !!ntdll,
                        createException( "GetModuleHandleW" /* locationOrAPI */ ),
                        BL_MSG()
                            << "Cannot obtain module handle for library 'ntdll.dll'"
                        );

                    const auto pfn = reinterpret_cast< PfnNtSetInformationProcess >(
                        getProcAddress( reinterpret_cast< library_handle_t >( ntdll ), "NtSetInformationProcess" )
                        );

                    const auto status = pfn(
                            ::GetCurrentProcess(),
                            processInfoType,
                            const_cast< ULONG* >( &processInfoValue ),
                            sizeof( processInfoValue )
                        );

                    BL_CHK(
                        false,
                        status == STATUS_SUCCESS,
                        BL_MSG()
                            << "NtSetInformationProcess failed; NTSTATUS="
                            << status
                            << "; processInfoType="
                            << processInfoType
                            << "; processInfoValue="
                            << processInfoValue
                    );
                }

                static void setAbstractPriority( SAA_in const AbstractPriority priority )
                {
                    setMemoryPagingPriority( priority );

                    /*
                     * Add a scope guard to make sure we restore the original priority
                     * in case of failure
                     */

                    auto g = BL_SCOPE_GUARD(
                        {
                            try
                            {
                                setMemoryPagingPriority( g_currentAbstractPriority );
                            }
                            catch( std::exception& )
                            {
                                /*
                                 * This lambda has to be nothrow, but there is not much to do
                                 * if this fails
                                 */
                            }
                        }
                        );

                    switch( priority )
                    {
                        default:
                            BL_ASSERT( false );
                            break;

                        case AbstractPriority::Greedy:
                        case AbstractPriority::Normal:
                            {
                                /*
                                 * Note, on Windows the 'Greedy' option will be the same as 'Normal'
                                 */

                                const auto bWin32 = ::SetPriorityClass( ::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END );

                                BL_CHK_T(
                                    false,
                                    bWin32 || ERROR_PROCESS_MODE_NOT_BACKGROUND == ::GetLastError(),
                                    createException( "SetPriorityClass" /* locationOrAPI */ ),
                                    BL_MSG()
                                        << "Cannot set the process priority to PROCESS_MODE_BACKGROUND_END"
                                    );

                                /*
                                 * If the parent process was having background priority set then
                                 * PROCESS_MODE_BACKGROUND_END will not restore child process CPU & IO priority
                                 * back to Normal but just to the original priority when the child process started
                                 * Check for this case and enforce the priority change
                                 */

                                const auto priority = ::GetPriorityClass( ::GetCurrentProcess() );

                                BL_CHK_T(
                                    0U,
                                    priority,
                                    createException( "GetPriorityClass" /* locationOrAPI */ ),
                                    BL_MSG()
                                        << "Cannot get the process priority"
                                    );

                                if( priority != NORMAL_PRIORITY_CLASS )
                                {
                                    BL_CHK_BOOL_WINAPI( ::SetPriorityClass( ::GetCurrentProcess(), NORMAL_PRIORITY_CLASS ) );

                                    setIoPriority( os::AbstractPriority::Normal );
                                }
                            }
                            break;

                        case AbstractPriority::Background:
                            {
                                const auto bWin32 =
                                    ::SetPriorityClass( ::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN );

                                BL_CHK_T(
                                    false,
                                    bWin32 || ERROR_PROCESS_MODE_ALREADY_BACKGROUND == ::GetLastError(),
                                    createException( "SetPriorityClass" /* locationOrAPI */ ),
                                    BL_MSG()
                                        << "Cannot set the process priority to background mode"
                                    );
                            }
                            break;
                    }

                    g.dismiss();

                    g_currentAbstractPriority = priority;
                }

                static void daemonize()
                {
                    /*
                     * No-op on Windows
                     */
                }

                static std::string tryGetUserDomain()
                {
                    const auto dnsDomain = tryGetEnvironmentVariable( "USERDNSDOMAIN" );

                    if( dnsDomain )
                    {
                        return *dnsDomain;
                    }

                    const auto computerName = tryGetEnvironmentVariable( "COMPUTERNAME" );

                    const auto userDomain =  tryGetEnvironmentVariable( "USERDNSDOMAIN" );

                    if(
                        ! userDomain ||
                        userDomain -> empty() ||
                        ( computerName && ( *computerName == *userDomain ) )
                        )
                    {
                        return std::string();
                    }

                    return *userDomain;
                }

                /*****************************************************************************
                 * Create a Kerberos token for authenticating the user on the provided server
                 * and the requested user token will be used to automatically log the user on
                 * other servers requiring authentication
                 */

                static std::string getSPNEGOtoken(
                    SAA_in          const std::string&                  server,
                    SAA_in          const std::string&                  domain
                    )
                {
                    std::string                     result;
                    ::TimeStamp                     lifetime;
                    ::CredHandle                    credentialsStorage;
                    ::CtxtHandle                    contextStorage;
                    ::SecBuffer                     bufferLocal;
                    ::SecBufferDesc                 securityDescriptor;
                    ::ULONG                         contextAttributes;

                    auto status = ::AcquireCredentialsHandleW(
                        NULL,
                        NEGOSSP_NAME_W,
                        SECPKG_CRED_OUTBOUND,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        &credentialsStorage,
                        &lifetime
                        );

                    BL_CHK_T(
                        false,
                        SEC_E_OK == status,
                        createException( "getSPNEGOtoken" /* locationOrAPI */ ),
                        BL_MSG()
                            << "The call to AcquireCredentialsHandleW() failed"
                        );

                    const auto credentials = cred_handle_ptr_t::attach( &credentialsStorage );

                    {
                        cpp::wstring_convert_t convertor;
                        const auto target = convertor.from_bytes( "HTTP/" + server + "@" + domain );

                        bufferLocal.cbBuffer = 0;
                        bufferLocal.BufferType = SECBUFFER_TOKEN;
                        bufferLocal.pvBuffer = NULL;

                        securityDescriptor.ulVersion = 0;
                        securityDescriptor.cBuffers = 1;
                        securityDescriptor.pBuffers = &bufferLocal;

                        status = ::InitializeSecurityContextW(
                            credentials.get(),
                            NULL,
                            const_cast< SEC_WCHAR * >( target.c_str() ),
                            ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY,
                            0,
                            SECURITY_NETWORK_DREP,
                            NULL,
                            0,
                            &contextStorage,
                            &securityDescriptor,
                            &contextAttributes,
                            &lifetime
                            );

                        const bool success =
                            (
                                SEC_E_OK == status                      ||
                                SEC_I_COMPLETE_AND_CONTINUE == status   ||
                                SEC_I_COMPLETE_NEEDED == status         ||
                                SEC_I_CONTINUE_NEEDED == status         ||
                                SEC_I_INCOMPLETE_CREDENTIALS == status
                            );

                        BL_CHK_T(
                            false,
                            success,
                            createException( "getSPNEGOtoken" /* locationOrAPI */ ),
                            BL_MSG()
                                << "The call to InitializeSecurityContextW() failed"
                            );

                        const auto context = ctxt_handle_ptr_t::attach( &contextStorage );
                        const auto buffer = sec_buf_ptr_t::attach( &bufferLocal );

                        result.reserve( bufferLocal.cbBuffer );

                        for( std::size_t i = 0; i < bufferLocal.cbBuffer; i++ )
                        {
                            const auto ch = ( static_cast< unsigned char* >( bufferLocal.pvBuffer ) )[ i ];

                            result.push_back( ch );
                        }
                    }

                    return result;
                }

                static bool isUserAdministrator()
                {
                    /*
                     * TODO: return true if current user has local admin rights
                     *
                     * See also:
                     * http://msdn.microsoft.com/en-gb/library/windows/desktop/aa376389%28v=vs.85%29.aspx
                     * http://support.microsoft.com/kb/118626
                     */

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "isUserAdministrator is not implemented on Windows yet"
                        );
                }

                static bool isUserInteractive()
                {
                    HWINSTA stationHandle = nullptr;

                    BL_CHK_BOOL_WINAPI( stationHandle = ::GetProcessWindowStation() );

                    USEROBJECTFLAGS userObjectFlags = { 0 };

                    BL_CHK_BOOL_WINAPI(
                        ::GetUserObjectInformation(
                            stationHandle                   /* hObj */,
                            UOI_FLAGS                       /* nIndex */,
                            &userObjectFlags                /* pvInfo */,
                            sizeof( USEROBJECTFLAGS )       /* nLength */ ,
                            nullptr                         /* lpnLengthNeeded */
                            )
                        );

                    return userObjectFlags.dwFlags & WSF_VISIBLE;
                }

                static int getSessionId()
                {
                    DWORD sessionId;

                    BL_CHK_BOOL_WINAPI( ::ProcessIdToSessionId( ::GetCurrentProcessId(), &sessionId ) );

                    return ( int ) sessionId;
                }

                static void copyDirectoryPermissions(
                    SAA_in          const fs::path&                 srcDirPath,
                    SAA_in          const fs::path&                 dstDirPath
                    )
                {
                    addBackupRestorePrivilege( true /* enableRestore */ );
                    enablePrivilege( L"SeSecurityPrivilege" );

                    SECURITY_INFORMATION securityInfo =
                        OWNER_SECURITY_INFORMATION |
                        GROUP_SECURITY_INFORMATION |
                        DACL_SECURITY_INFORMATION |
                        SACL_SECURITY_INFORMATION;

                    PSID psidOwner = NULL;
                    PSID psidGroup = NULL;
                    PACL pDacl = NULL;
                    PACL pSacl = NULL;
                    PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;

                    const auto& srcDir = srcDirPath.native();
                    std::wstring wsrcDir( srcDir.begin(), srcDir.end() );

                    DWORD rc =
                        ::GetNamedSecurityInfoW(
                            wsrcDir.c_str(),                /* pObjectName */
                            SE_FILE_OBJECT,                 /* ObjectType */
                            securityInfo,                   /* SecurityInfo */
                            &psidOwner,                     /* ppsidOwner */
                            &psidGroup,                     /* ppsidGroup */
                            &pDacl,                         /* ppDacl */
                            &pSacl,                         /* ppSacl */
                            &pSecurityDescriptor            /* ppSecurityDescriptor */
                           );

                    if( rc != 0 )
                    {
                        BL_THROW_EC(
                            createSystemErrorCode( rc ),
                            BL_MSG()
                                << "GetNamedSecurityInfoW returned error for path "
                                << fs::normalizePathParameterForPrint( srcDirPath )
                            );
                    }

                    const auto hlocal = hlocal_ref::attach( pSecurityDescriptor );

                    const auto& dstDir = dstDirPath.native();
                    std::wstring wdstDir( dstDir.begin(), dstDir.end() );

                    securityInfo |=
                        PROTECTED_DACL_SECURITY_INFORMATION |
                        PROTECTED_SACL_SECURITY_INFORMATION;

                    rc = ::SetNamedSecurityInfoW(
                            const_cast< LPWSTR >( wdstDir.c_str() ),                /* pObjectName */
                            SE_FILE_OBJECT,                                         /* ObjectType */
                            securityInfo,                                           /* SecurityInfo */
                            psidOwner,                                              /* ppsidOwner */
                            psidGroup,                                              /* ppsidGroup */
                            pDacl,                                                  /* ppDacl */
                            pSacl                                                   /* ppSacl */
                           );

                    if( rc != 0 )
                    {
                        BL_THROW_EC(
                            createSystemErrorCode( rc ),
                            BL_MSG()
                                << "SetNamedSecurityInfoW returned error for path "
                                << fs::normalizePathParameterForPrint( dstDirPath )
                            );
                    }
                }

                static void setLinuxPathPermissions(
                    SAA_in      const fs::path&             path,
                    SAA_in      const fs::perms             permissions
                    )
                {
                    BL_UNUSED( path );
                    BL_UNUSED( permissions );

                    BL_THROW(
                        NotSupportedException(),
                        "setLinuxPathPermissions is implemented only on Linux"
                        );
                }

                static void setWindowsPathPermissions(
                    SAA_in      const fs::path&             path,
                    SAA_in      const fs::perms             ownerPermissions,
                    SAA_in      const std::string&          groupName,
                    SAA_in      const fs::perms             groupPermissions
                    )
                {
                    const auto pos = g_permissionsMap.find( ownerPermissions );

                    if( pos == g_permissionsMap.end() )
                    {
                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Invalid owner permissions: "
                                << ownerPermissions
                            );
                    }

                    const DWORD ownerAccessPermissions = pos -> second;

                    DWORD groupAccessPermissions = 0;

                    if( ! groupName.empty() )
                    {
                        const auto pos = g_permissionsMap.find( groupPermissions );

                        if( pos == g_permissionsMap.end() )
                        {
                            BL_THROW(
                                UnexpectedException(),
                                BL_MSG()
                                    << "Invalid group permissions: "
                                    << groupPermissions
                                );

                        }

                        groupAccessPermissions = pos -> second;
                    }

                    PSID psidOwner = NULL;
                    PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;

                    auto pathPreferred = path;
                    pathPreferred.make_preferred();

                    const auto name = pathPreferred.string();
                    std::wstring wname( name.begin(), name.end() );

                    DWORD rc =
                        ::GetNamedSecurityInfoW(
                            wname.c_str(),                  /* pObjectName */
                            SE_FILE_OBJECT,                 /* ObjectType */
                            OWNER_SECURITY_INFORMATION,     /* SecurityInfo */
                            &psidOwner,                     /* ppsidOwner */
                            NULL,                           /* ppsidGroup */
                            NULL,                           /* ppDacl */
                            NULL,                           /* ppSacl */
                            &pSecurityDescriptor            /* ppSecurityDescriptor */
                           );

                    if( rc != 0 )
                    {
                        BL_THROW_EC(
                            createSystemErrorCode( rc ),
                            BL_MSG()
                                << "GetNamedSecurityInfoW returned error for path "
                                << fs::normalizePathParameterForPrint( path )
                            );
                    }

                    const auto hlocalSecurityDescriptor = hlocal_ref::attach( pSecurityDescriptor );

                    /*
                     * After calling ::GetNamedSecurityInfoW() to get the owner SID,
                     * we fill 2 EXPLICIT_ACCESS structures for the owner and the group
                     * and set the new path permissions with ::SetNamedSecurityInfoW()
                     */

                    EXPLICIT_ACCESS ea[ 2 ]; /* owner and group */
                    ::memset( ea, 0, sizeof( ea ) );

                    ea[ 0 ].grfAccessPermissions = ownerAccessPermissions;
                    ea[ 0 ].grfAccessMode = SET_ACCESS;
                    ea[ 0 ].grfInheritance = NO_INHERITANCE;
                    ea[ 0 ].Trustee.TrusteeForm = TRUSTEE_IS_SID;
                    ea[ 0 ].Trustee.ptstrName = (LPTSTR) psidOwner;

                    ULONG eaCount = 1;

                    if( ! groupName.empty() )
                    {
                        ea[ 1 ].grfAccessPermissions = groupAccessPermissions;
                        ea[ 1 ].grfAccessMode = SET_ACCESS;
                        ea[ 1 ].grfInheritance = NO_INHERITANCE;
                        ea[ 1 ].Trustee.TrusteeForm = TRUSTEE_IS_NAME;
                        ea[ 1 ].Trustee.ptstrName = (LPTSTR) groupName.c_str();

                        ++eaCount;
                    }

                    PACL pPacl = NULL;
                    rc = ::SetEntriesInAcl(
                        eaCount,        /* cCountOfExplicitEntries */
                        ea,             /* pListOfExplicitEntries */
                        NULL,           /* OldAcl */
                        &pPacl          /* NewAcl */
                        );

                    if( rc != 0 )
                    {
                        BL_THROW_EC(
                            createSystemErrorCode( rc ),
                            BL_MSG()
                                << "SetEntriesInAcl returned error for path "
                                << fs::normalizePathParameterForPrint( path )
                            );
                    }

                    const auto hlocalPacl = hlocal_ref::attach( pPacl );

                    SECURITY_INFORMATION securityInfo =
                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION;

                    rc = ::SetNamedSecurityInfoW(
                            const_cast< LPWSTR >( wname.c_str() ),                  /* pObjectName */
                            SE_FILE_OBJECT,                                         /* ObjectType */
                            securityInfo,                                           /* SecurityInfo */
                            NULL,                                                   /* ppsidOwner */
                            NULL,                                                   /* ppsidGroup */
                            pPacl,                                                  /* ppDacl */
                            NULL                                                    /* ppSacl */
                           );

                    if( rc != 0 )
                    {
                        BL_THROW_EC(
                            createSystemErrorCode( rc ),
                            BL_MSG()
                                << "SetNamedSecurityInfoW returned error for path "
                                << fs::normalizePathParameterForPrint( path )
                            );
                    }
                }

                static void sendDebugMessage( SAA_in const std::string& message ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    /*
                     * Limit the message length to 1023 characters (including ...<CR><LF>)
                     */

                    const std::size_t maxLength = 1023 - 5;

                    std::string buffer;

                    if( message.length() > maxLength )
                    {
                        buffer.reserve( maxLength + 5 );
                        buffer.assign( message, 0, maxLength );
                        buffer.append( "...\r\n" );
                    }
                    else
                    {
                        buffer.reserve( message.length() + 2 );
                        buffer.assign( message );
                        buffer.append( "\r\n" );
                    }

                    ::OutputDebugStringA( buffer.c_str() );

                    BL_NOEXCEPT_END()
                }

                static os::string_ptr tryGetRegistryValue(
                    SAA_in         const std::string&                       key,
                    SAA_in         const std::string&                       name,
                    SAA_in_opt     const bool                               currentUser = false
                    )
                {
                    HKEY regKeyHandle;
                    WCHAR buffer[ 1024 ];
                    DWORD size = sizeof( buffer );

                    const auto locationCode = currentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

                    const auto location = HKEY_CURRENT_USER ? "HKEY_CURRENT_USER" : "HKEY_LOCAL_MACHINE";

                    const auto openKeyErrorCode =
                        RegOpenKeyExW(
                            locationCode                                      /* hKey */,
                            std::wstring( key.begin(), key.end() ).c_str()    /* lpSubKey */,
                            0                                                 /* ulOptions */,
                            KEY_QUERY_VALUE                                   /* samDesired */,
                            &regKeyHandle                                     /* phkResult */
                            );

                    const auto handlePtr = reg_key_handle_ptr_t::attach( regKeyHandle );

                    if( openKeyErrorCode == ERROR_FILE_NOT_FOUND )
                    {
                        return nullptr;
                    }
                    else
                    {
                        BL_CHK_T_USER_FRIENDLY(
                            true,
                            openKeyErrorCode != ERROR_SUCCESS,
                            createException( "RegOpenKeyExW" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Cannot open registry key "
                                << location
                                << "\\"
                                << key
                        );
                    }

                    const auto regValueErrorCode =
                        RegGetValueW(
                            regKeyHandle                                       /* hKey */,
                            NULL,                                              /* lpSubKey */
                            std::wstring( name.begin(), name.end() ).c_str()   /* lpValue */,
                            RRF_RT_REG_SZ                                      /* dwFlags */,
                            NULL                                               /* pdwType */,
                            ( BYTE* )buffer                                    /* pvData */,
                            &size                                              /* pcbData */
                            );

                    if( regValueErrorCode == ERROR_FILE_NOT_FOUND )
                    {
                        return nullptr;
                    }
                    else
                    {
                        BL_CHK_T_USER_FRIENDLY(
                            true,
                            regValueErrorCode != ERROR_SUCCESS,
                            createException( "RegGetValueW" /* locationOrAPI */ ),
                            BL_MSG()
                                << "Cannot open registry value "
                                << location
                                << "\\"
                                << key
                                << "\\"
                                << name
                            );
                    }

                    cpp::wstring_convert_t conv;

                    return string_ptr::attach( new std::string( conv.to_bytes( buffer ) ) );
                }

                static std::string getRegistryValue(
                    SAA_in            const std::string&                      key,
                    SAA_in            const std::string&                      value,
                    SAA_in_opt        const bool                              currentUser = false
                    )
                {
                    const auto regValue = tryGetRegistryValue( key, value, currentUser );

                    BL_CHK_USER_FRIENDLY(
                        nullptr,
                        regValue.get(),
                        BL_MSG()
                            << "Registry value "
                            << str::quoteString( value )
                            << " was not found in key "
                            << str::quoteString( key )
                        );

                    return *regValue;
                }

                static bool createNewFile( SAA_in const fs::path& path )
                {
                    const auto pwzFileName = path.native().c_str();

                    const auto fileHandle = ::CreateFileW(
                            pwzFileName,
                            FILE_WRITE_ATTRIBUTES,                                      /* dwDesiredAccess */
                            0U                                                          /* dwShareMode - no sharing */,
                            NULL,                                                       /* lpSecurityAttributes */
                            CREATE_NEW                                                  /* dwCreationDisposition */,
                            FILE_FLAG_BACKUP_SEMANTICS,                                 /* dwFlagsAndAttributes */
                            NULL                                                        /* hTemplateFile */
                            );

                    if( INVALID_HANDLE_VALUE == fileHandle )
                    {
                        return false;
                    }
                    else
                    {
                        ::CloseHandle( fileHandle );
                        return true;
                    }
                }

                static bool isFileInUseError( SAA_in const eh::error_code& ec )
                {
                    if( ec.category() == eh::system_category() )
                    {
                        switch( ec.value() )
                        {
                            case ERROR_ACCESS_DENIED:
                            case ERROR_SHARING_VIOLATION:
                            case ERROR_LOCK_VIOLATION:

                                return true;
                        }
                    }

                    return false;
                }

                static eh::error_code tryRemoveFileOnReboot( SAA_in const fs::path& path )
                {
                    const auto success = ::MoveFileExW(
                        path.native().c_str()           /* lpExistingFileName */,
                        NULL                            /* lpNewFileName, NULL means delete the file */,
                        MOVEFILE_DELAY_UNTIL_REBOOT     /* dwFlags, usage of this flag requires admin rights */
                        );

                    return success ? eh::error_code() : createSystemErrorCode( ( int )::GetLastError() );
                }

                static stdio_file_ptr getDuplicatedFileDescriptorAsFilePtr(
                    SAA_in      const int           fd,
                    SAA_in      const bool          readOnly
                    )
                {
                    int newfd = -1;

                    BL_CHK_ERRNO(
                        -1,
                        ( newfd = ::_dup( fd ) ),
                        BL_MSG()
                            << "Cannot duplicate file descriptor: "
                            << fd
                        );

                    auto newfdRef = fd_ref( newfd );

                    return convert2StdioFile( newfdRef, readOnly );
                }
            };

            BL_DEFINE_STATIC_MEMBER( OSImplT, const std::wstring, g_noParsePrefix )         = L"\\??\\";
            BL_DEFINE_STATIC_MEMBER( OSImplT, const std::wstring, g_lfnPrefix )             = L"\\\\?\\";
            BL_DEFINE_STATIC_MEMBER( OSImplT, const std::wstring, g_windowManagerDomain )   = L"Window Manager";
            BL_DEFINE_STATIC_MEMBER( OSImplT, AbstractPriority, g_currentAbstractPriority ) = AbstractPriority::Normal;

            BL_DEFINE_STATIC_MEMBER( OSImplT, bool, g_backupEnabled )  = false;
            BL_DEFINE_STATIC_MEMBER( OSImplT, bool, g_restoreEnabled ) = false;

            template
            <
                typename E
            >
            const std::map< fs::perms, DWORD >&
            OSImplT< E >::g_permissionsMap = OSImplT< E >::initPermissionsMap();

            typedef OSImplT<> OSImpl;

        } // detail

    } // os

} // bl

#endif /* __BL_OSIMPLWINDOWS_H_ */
