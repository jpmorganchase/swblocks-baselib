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

#ifndef __BL_OS_SPECIFIC_WINDOWSSHELLSHORTCUT_H_
#define __BL_OS_SPECIFIC_WINDOWSSHELLSHORTCUT_H_

#if ! defined( _WIN32 )
#error This file can only be included when compiling on Windows platform
#endif

#ifndef VOID
#define VOID void
#endif // VOID

/*
 * The above check and define of VOID is necessary as apparently shlobj.h doesn't include
 * the right windows headers where this is defined and produces an error claiming that VOID
 * is not defined anywhere
 */

#include <shlobj.h>

/*
 * Above include will also pull rpcdce.h which defines macro uuid_t
 * Undef this macro to avoid conflict with bl::uuid_t type
 */

#undef uuid_t

#include <baselib/core/specific/ComUtils.h>
#include <baselib/core/StringUtils.h>

namespace bl
{
    namespace os
    {
        namespace specific
        {
            template
            <
                typename E = void
            >
            class WindowsShellShortcutT
            {
                BL_DECLARE_STATIC( WindowsShellShortcutT )

                BL_TYPEDEF_COM_POINTER( IShellLinkW,   shell_link_ptr_t );
                BL_TYPEDEF_COM_POINTER( IPersistFile,  persist_file_ptr_t );

            public:

                struct Info
                {
                    std::string                                     m_description;
                    fs::path                                        m_targetPath;
                    std::string                                     m_targetArguments;
                    fs::path                                        m_targetWorkingDir;
                    fs::path                                        m_iconPath;
                    cpp::ScalarTypeIniter< int >                    m_iconIndex;
                };

                static void createShortcut(
                    SAA_in      const fs::path&                     shortcutPath,
                    SAA_in      const Info&                         info
                    )
                {
                    BL_CHK(
                        false,
                        fs::path_exists( info.m_targetPath ),
                        BL_MSG()
                            << "Target path "
                            << info.m_targetPath
                            << " does not exist for shell shortcut "
                            << shortcutPath
                        );

                    shell_link_ptr_t link;

                    BL_CHK_HR(
                        CoCreateInstance(
                            CLSID_ShellLink,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IShellLinkW,
                            ( LPVOID* ) &link
                            ) );

                    const auto shortTargetPath =
                        fs::detail::WinLfnUtils::chk2RemovePrefix( cpp::copy( info.m_targetPath ) );

                    cpp::wstring_convert_t conv;

                    BL_CHK_HR( link -> SetPath( conv.from_bytes( shortTargetPath.string() ).c_str() ) );

                    if( ! info.m_description.empty() )
                    {
                        BL_CHK_HR( link -> SetDescription( conv.from_bytes( info.m_description ).c_str() ) );
                    }

                    if( ! info.m_targetArguments.empty() )
                    {
                        BL_CHK_HR( link -> SetArguments( conv.from_bytes( info.m_targetArguments ).c_str() ) );
                    }

                    if( ! info.m_targetWorkingDir.empty() )
                    {
                        if( ! fs::path_exists( info.m_targetWorkingDir ) )
                        {
                            /*
                             * This isn't necessarily a failure in case shell variable like
                             * "%APPDATA%" is used as part of the path, just log here
                             */

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Target working directory  "
                                    << info.m_targetWorkingDir
                                    << " does not exist for shell shortcut "
                                    << shortcutPath
                                );
                        }

                        const auto shortWorkingDir =
                            fs::detail::WinLfnUtils::chk2RemovePrefix( cpp::copy( info.m_targetWorkingDir ) );

                        BL_CHK_HR( link -> SetWorkingDirectory ( conv.from_bytes( shortWorkingDir.string() ).c_str() ) );
                    }

                    if( ! info.m_iconPath.string().empty() )
                    {
                        if( ! fs::path_exists( info.m_iconPath ) )
                        {
                            /*
                             * This isn't necessarily a failure in case shell variable like
                             * "%APPDATA%" is used as part of the path, just log here
                             */

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Icon path "
                                    << info.m_iconPath
                                    << " does not exist for shell shortcut "
                                    << shortcutPath
                                );
                        }

                        const auto shortIconPath =
                            fs::detail::WinLfnUtils::chk2RemovePrefix( cpp::copy( info.m_iconPath ) );

                        BL_CHK_HR( link -> SetIconLocation( conv.from_bytes( shortIconPath.string() ).c_str(), info.m_iconIndex ) );
                    }

                    fs::safeCreateDirectory( shortcutPath.parent_path() );

                    persist_file_ptr_t persistFile;

                    BL_CHK_HR( link -> QueryInterface( IID_IPersistFile, ( LPVOID* ) &persistFile) );

                    BL_CHK_HR( persistFile -> Save( conv.from_bytes( shortcutPath.string() ).c_str(), true /* fRemember */ ) );

                    SHChangeNotify(
                        SHCNE_CREATE,
                        SHCNF_PATH | SHCNF_FLUSHNOWAIT,
                        conv.from_bytes( shortcutPath.string() ).c_str(),
                        NULL
                        );
                }

                static void deleteShortcut( SAA_in const fs::path& shortcutPath )
                {
                    if( fs::path_exists( shortcutPath ) )
                    {
                        fs::safeRemove( shortcutPath );

                        cpp::wstring_convert_t conv;

                        SHChangeNotify(
                            SHCNE_DELETE,
                            SHCNF_PATH | SHCNF_FLUSHNOWAIT,
                            conv.from_bytes( shortcutPath.string() ).c_str(),
                            NULL
                            );
                    }
                }

                static Info queryShortcut( SAA_in const fs::path& shortcutPath )
                {
                    BL_CHK_USER_FRIENDLY(
                        false,
                        fs::path_exists( shortcutPath ),
                        BL_MSG()
                            << "Shortcut "
                            << shortcutPath
                            << " does not exist"
                        );

                    shell_link_ptr_t link;

                    BL_CHK_HR(
                        CoCreateInstance(
                            CLSID_ShellLink,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IShellLinkW,
                            ( LPVOID* ) &link
                            ) );

                    persist_file_ptr_t persistFile;

                    BL_CHK_HR( link -> QueryInterface( IID_IPersistFile, ( LPVOID* ) &persistFile) );

                    cpp::wstring_convert_t conv;

                    BL_CHK_HR( persistFile -> Load( conv.from_bytes( shortcutPath.string() ).c_str(), STGM_READ) );

                    Info info;

                    wchar_t buffer[ MAX_PATH ];

                    BL_CHK_HR( link -> GetDescription( buffer, BL_ARRAY_SIZE( buffer ) ) );

                    info.m_description = conv.to_bytes( buffer );

                    BL_CHK_HR( link -> GetPath( buffer, BL_ARRAY_SIZE( buffer ), nullptr /* WIN32_FIND_DATA */, SLGP_RAWPATH ) );

                    info.m_targetPath = conv.to_bytes( buffer );

                    BL_CHK_HR( link -> GetArguments( buffer, BL_ARRAY_SIZE( buffer ) ) );

                    info.m_targetArguments = conv.to_bytes( buffer );

                    BL_CHK_HR( link -> GetWorkingDirectory( buffer, BL_ARRAY_SIZE( buffer ) ) );

                    info.m_targetWorkingDir = conv.to_bytes( buffer );

                    BL_CHK_HR( link -> GetIconLocation( buffer, BL_ARRAY_SIZE( buffer ), &info.m_iconIndex.lvalue() ) );

                    info.m_iconPath = conv.to_bytes( buffer );

                    return info;
                }
            };

            typedef WindowsShellShortcutT<> WindowsShellShortcut;

        } // specific

    } // os

} // bl

#endif /* __BL_OS_SPECIFIC_WINDOWSSHELLSHORTCUT_H_ */
