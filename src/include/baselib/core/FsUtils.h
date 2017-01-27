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

#ifndef __BL_FSUTILS_H_
#define __BL_FSUTILS_H_

#include <baselib/core/OS.h>
#include <baselib/core/PathUtils.h>
#include <baselib/core/Logging.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

#include <stack>
#include <locale>

namespace bl
{
    namespace fs
    {
        namespace detail
        {
            /**
             * @brief class FsUtils - filesystem utility code
             */

            template
            <
                typename E = void
            >
            class FsUtilsT
            {
            protected:

                typedef cpp::function < void ( SAA_inout eh::error_code& ec ) >         fs_callback_t;
                typedef FsUtilsT< E >                                                   this_type;

                enum
                {
                    DEFAULT_TIMEOUT_IN_MILLISECONDS = 100,
                };

                static std::size_t getRetryCount()
                {
                    return os::onWindows() ? 20U : 1U;
                }

                static eh::error_code trySafeFileOperation( SAA_in const fs_callback_t& cb )
                {
                    eh::error_code ec;

                    /*
                     * On Windows sometimes ::CreateDirectory, or other file APIs will fail
                     * with 'access denied' error when the directory / file exists (instead
                     * of ERROR_ALREADY_EXISTS)
                     *
                     * This probably happens due to anti-virus software or other such, but
                     * we have to handle this case and unfortunately using retry seems to
                     * be the only practical option
                     */

                    const auto retryCount = getRetryCount();

                    for( std::size_t i = 0; i < retryCount; ++i )
                    {
                        ec.clear();

                        cb( ec );

                        if( ! ec )
                        {
                            return ec;
                        }

                        os::sleep( time::milliseconds( DEFAULT_TIMEOUT_IN_MILLISECONDS ) );
                    }

                    return ec;
                }

                static void ensurePathExists( SAA_in const fs::path& path )
                {
                    BL_CHK_USER_FRIENDLY(
                        false,
                        fs::path_exists( path ),
                        BL_MSG()
                            << "Path "
                            << normalizePathParameterForPrint( path )
                            << " does not exist"
                        );
                }

                static void ensurePathDoesNotExist( SAA_in const fs::path& path )
                {
                    BL_CHK_USER_FRIENDLY(
                        true,
                        fs::path_exists( path ),
                        BL_MSG()
                            << "Path "
                            << normalizePathParameterForPrint( path )
                            << " already exists"
                        );
                }

                static void ensureDirectoryAndNotJunction( SAA_in const fs::path& path )
                {
                    ensurePathExists( path );

                    BL_CHK_USER_FRIENDLY(
                        false,
                        false == isDirectoryJunction( path ) && fs::is_directory( path ),
                        BL_MSG()
                            << "Path "
                            << normalizePathParameterForPrint( path )
                            << " must be a valid directory that is not a junction point"
                        );
                }

                static void ensureDirectoryJunction( SAA_in const fs::path& path )
                {
                    BL_CHK_USER_FRIENDLY(
                        false,
                        isDirectoryJunction( path ),
                        BL_MSG()
                            << "Path "
                            << normalizePathParameterForPrint( path )
                            << " must be a valid directory junction"
                        );
                }

                SAA_noreturn
                static void reportUnexpectedFileType(
                    SAA_in          const path&                 path,
                    SAA_in          const file_type             type
                    )
                {
                    /*
                     * The passed in file is one of the types we don't support, namely
                     * (all declared in boost/filesystem/operations.hpp):
                     *
                     * status_error
                     * status_unknown
                     * file_not_found
                     * block_file
                     * character_file
                     * fifo_file
                     * type_unknown
                     * _detail_directory_symlink (private/internal type)
                     *
                     * symlink_file (on Windows only, since we don't support symlinks on Windows)
                     * reparse_file (on UNIX and on Windows if not a junction)
                     */

                    BL_THROW_USER_FRIENDLY(
                        UnexpectedException(),
                        BL_MSG()
                            << "File "
                            << normalizePathParameterForPrint( path )
                            << " is of unexpected type "
                            << type
                        );
                }

                static eh::error_code trySafeRemoveInternal( SAA_in const path& path )
                {
                    return trySafeFileOperation(
                        cpp::bind( &fs::unsafe::remove, cpp::cref( path ), _1 )
                        );
                }

                static void chkDeleteInternal(
                    SAA_in          const path&                 path,
                    SAA_in          const eh::error_code&       ec
                    )
                {
                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not delete "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static void getFileCreateTimeEC(
                    SAA_in          const fs::path&             path,
                    SAA_in          std::time_t&                time,
                    SAA_inout       eh::error_code&             ec
                    )
                {
                    try
                    {
                        time = os::getFileCreateTime( path );
                        ec.clear();
                    }
                    catch( eh::system_error& e )
                    {
                        ec = e.code();
                    }
                }

                static void setFileCreateTimeEC(
                    SAA_in          const fs::path&             path,
                    SAA_in          const std::time_t&          newTime,
                    SAA_inout       eh::error_code&             ec
                    )
                {
                    try
                    {
                        os::setFileCreateTime( path, newTime );
                        ec.clear();
                    }
                    catch( eh::system_error& e )
                    {
                        ec = e.code();
                    }
                }

                static void updateFileAttributesEC(
                    SAA_in          const fs::path&             path,
                    SAA_in          const os::FileAttributes    attributes,
                    SAA_in          const bool                  remove,
                    SAA_inout       eh::error_code&             ec
                    )
                {
                    try
                    {
                        os::unsafe::updateFileAttributes( path, attributes, remove );
                        ec.clear();
                    }
                    catch( eh::system_error& e )
                    {
                        ec = e.code();
                    }
                }

                static void getFileAttributesEC(
                    SAA_in          const fs::path&             path,
                    SAA_inout       os::FileAttributes&         attributes,
                    SAA_inout       eh::error_code&             ec
                    )
                {
                    try
                    {
                        attributes = os::unsafe::getFileAttributes( path );
                        ec.clear();
                    }
                    catch( eh::system_error& e )
                    {
                        attributes = os::FileAttributes::FileAttributeNone;
                        ec = e.code();
                    }
                }

            public:

                static std::time_t safeGetFileCreateTime( SAA_in const fs::path& path )
                {
                    std::time_t time = 0;

                    const auto ec = trySafeFileOperation(
                        cpp::bind( &this_type::getFileCreateTimeEC, cpp::cref( path ), cpp::ref( time ), _1 )
                        );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not obtain file create time for "
                            << normalizePathParameterForPrint( path )
                        );

                    return time;
                }

                static void safeSetFileCreateTime(
                    SAA_in          const fs::path&             path,
                    SAA_in          const std::time_t&          newTime
                    )
                {
                    const auto ec = trySafeFileOperation(
                        cpp::bind( &this_type::setFileCreateTimeEC, cpp::cref( path ), cpp::cref( newTime ), _1 )
                        );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not set file create time for "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static void safeUpdateFileAttributes(
                    SAA_in          const fs::path&             path,
                    SAA_in          const os::FileAttributes    attributes,
                    SAA_in          const bool                  remove = false
                    )
                {
                    const auto ec = trySafeFileOperation(
                        cpp::bind( &this_type::updateFileAttributesEC, cpp::cref( path ), attributes, remove, _1 )
                        );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not update file attributes for "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static os::FileAttributes safeGetFileAttributes( SAA_in const fs::path& path )
                {
                    auto attributes = os::FileAttributes::FileAttributeNone;

                    const auto ec = trySafeFileOperation(
                        cpp::bind( &this_type::getFileAttributesEC, cpp::cref( path ), cpp::ref( attributes ), _1 )
                        );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not obtain file attributes for "
                            << normalizePathParameterForPrint( path )
                        );

                    return attributes;
                }

                static void safeRename(
                    SAA_in          const path&                 oldPath,
                    SAA_in          const path&                 newPath
                    )
                {
                    const auto ec = trySafeRename( oldPath, newPath );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not rename "
                            << normalizePathParameterForPrint( oldPath )
                            << " to "
                            << normalizePathParameterForPrint( newPath )
                        );
                }

                static eh::error_code trySafeRename(
                    SAA_in          const path&                 oldPath,
                    SAA_in          const path&                 newPath
                    )
                {
                    return trySafeFileOperation(
                        cpp::bind( &fs::unsafe::rename, cpp::cref( oldPath ), cpp::cref( newPath ), _1 )
                        );
                }

                static void safeRemove( SAA_in const path& path )
                {
                    chkDeleteInternal( path, trySafeRemove( path ) );
                }

                static eh::error_code trySafeRemove( SAA_in const path& path )
                {
                    if( os::onWindows() && isDirectoryJunction( path ) )
                    {
                        try
                        {
                            deleteDirectoryJunction( path );
                            return eh::error_code();
                        }
                        catch( eh::system_error& e )
                        {
                            return e.code();
                        }
                    }

                    return trySafeRemoveInternal( path );
                }

                static void safeRemoveAll(
                    SAA_in          const path&                                 path,
                    SAA_in_opt      const cpp::void_callback_t&                 checkCancelCallback = cpp::void_callback_t(),
                    SAA_in_opt      const bool                                  removeFileInUseOnReboot = false
                    )
                {
                    const auto ec = trySafeRemoveAll( path, checkCancelCallback, removeFileInUseOnReboot );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not delete recursively "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static eh::error_code trySafeRemoveAll(
                    SAA_in          const path&                                 path,
                    SAA_in_opt      const cpp::void_callback_t&                 checkCancelCallback = cpp::void_callback_t(),
                    SAA_in_opt      const bool                                  removeFileInUseOnReboot = false
                    )
                {
                    /*
                     * Just do simple recursive implementation
                     *
                     * This recursive implementation is not ideal and should be
                     * converted to non-recursive at some point
                     */

                    eh::error_code ec;

                    const auto status = fs::symlink_status( path, ec );

                    if( ec )
                    {
                        return ec;
                    }

                    const auto type = status.type();

                    switch( type )
                    {
                        default:
                            {
                                reportUnexpectedFileType( path, type );
                            }
                            break;

                        case regular_file:
                            break;

                        case reparse_file:
                            {
                                if( false == os::onWindows() || false == isDirectoryJunction( path ) )
                                {
                                    /*
                                     * This is unsupported reparse file (we only support directory junctions
                                     * on Windows) or the OS is UNIX where such type (reparse_file) is invalid
                                     */

                                    reportUnexpectedFileType( path, type );
                                }
                            }
                            break;

                        case symlink_file:
                            {
                                if( os::onWindows() && ! isDirectoryJunction( path ) )
                                {
                                    /*
                                     * We don't support real symlinks on Windows (only directory junctions)
                                     *
                                     * Older versions of Boost.Filesystem were returning reparse_file for
                                     * directory junctions while newer versions are now returning
                                     * symlink_file, so we need to handle both
                                     */

                                    reportUnexpectedFileType( path, type );
                                }
                            }
                            break;

                        case directory_file:
                            {
                                if( checkCancelCallback )
                                {
                                    checkCancelCallback();
                                }

                                directory_iterator i( path, ec ), end;

                                if( ec )
                                {
                                    return ec;
                                }

                                for( ; i != end; ++i )
                                {
                                    ec = trySafeRemoveAll( i -> path(), checkCancelCallback, removeFileInUseOnReboot );

                                    if( ec )
                                    {
                                        return ec;
                                    }
                                }
                            }
                            break;
                    }

                    ec = trySafeRemove( path );

                    if(
                        ec &&
                        removeFileInUseOnReboot &&
                        os::onWindows() &&
                        os::isFileInUseError( ec )
                        )
                    {
                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Requesting removal of file in use on OS reboot: "
                                << path
                            );

                        const auto ec2 = os::tryRemoveFileOnReboot( path );

                        if( ec2 )
                        {
                            BL_LOG(
                                Logging::warning(),
                                BL_MSG()
                                    << "Failed request to remove file in use on OS reboot: "
                                    << path
                                    << "; error message: "
                                    << str::quoteString( ec2.message() )
                                    << "; error code: "
                                    << ec2.value()
                                );
                        }
                    }

                    return ec;
                }

                static void safeRemoveIfExists( SAA_in const path& path )
                {
                    if( path_exists( path ) )
                    {
                        safeRemove( path );
                    }
                }

                static eh::error_code trySafeRemoveIfExists( SAA_in const path& path )
                {
                    if( path_exists( path ) )
                    {
                        return trySafeRemove( path );
                    }

                    return eh::error_code();
                }

                static void safeRemoveAllIfExists( SAA_in const path& path )
                {
                    if( path_exists( path ) )
                    {
                        safeRemoveAll( path );
                    }
                }

                static eh::error_code trySafeRemoveAllIfExists( SAA_in const path& path )
                {
                    if( path_exists( path ) )
                    {
                        return trySafeRemoveAll( path );
                    }

                    return eh::error_code();
                }

                static bool safeDeletePathNothrow( SAA_in const fs::path& path ) NOEXCEPT
                {
                    BL_WARN_NOEXCEPT_BEGIN()

                    if( path_exists( path ) )
                    {
                        const auto ec = trySafeRemoveAll( path );

                        if( ec )
                        {
                            BL_LOG_MULTILINE(
                                Logging::warning(),
                                BL_MSG()
                                    << "Could not delete path "
                                    << normalizePathParameterForPrint( path )
                                    << " due to the following reason: '"
                                    << ec.message()
                                    << "' ; error code value: "
                                    << ec.value()
                                );

                            return false;
                        }
                    }

                    BL_WARN_NOEXCEPT_END( "safeDeletePathNothrow" )

                    return true;
                }

                static fs::path makeHidden( SAA_in const fs::path& path )
                {
                    BL_CHK_USER_FRIENDLY(
                        false,
                        fs::exists( path ),
                        BL_MSG()
                            << "Path "
                            << normalizePathParameterForPrint( path )
                            << " is expected to exist"
                        );

                    const auto fileName = path.filename();
                    const auto fileNameAsString = fileName.string();

                    BL_CHK_USER_FRIENDLY(
                        false,
                        fileNameAsString != "." && fileNameAsString != "..",
                        BL_MSG()
                            << "Attempts to modify '.' or '..' path is not allowed"
                        );

                    fs::path newPath;

                    if( 0U != fileNameAsString.find( "." ) )
                    {
                        /*
                         * File is not marked as hidden by Linux convention; let's rename it
                         */

                        fs::path newName( "." );
                        newName += fileName;

                        newPath = path.parent_path() / newName;
                        safeRename( path, newPath );
                    }
                    else
                    {
                        newPath = path;
                    }

                    safeUpdateFileAttributes( newPath, os::FileAttributeHidden );

                    return newPath;
                }

                static eh::error_code trySafeCreateDirectory( SAA_in const fs::path& path )
                {
                    return trySafeFileOperation(
                        cpp::bind( &fs::unsafe::create_directory, cpp::cref( path ), _1 )
                        );
                }

                static void safeCreateDirectory( SAA_in const fs::path& path )
                {
                    const auto ec = trySafeCreateDirectory( path );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not create directory "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static eh::error_code tryCreateTempDir( SAA_inout fs::path& path )
                {
                    eh::error_code ec;

                    const auto tmpDir = temp_directory_path( ec );

                    if( ! ec )
                    {
                        path = tmpDir / fs::path( "bl-temp-dir-" + uuids::uuid2string( uuids::create() ) );

                        ec = trySafeCreateDirectory( path );
                    }

                    return ec;
                }

                static void createTempDir( SAA_inout fs::path& path )
                {
                    const auto ec = tryCreateTempDir( path );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not create a temporary directory "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static eh::error_code trySafeMkdirs( SAA_in const fs::path& path )
                {
                    return trySafeFileOperation(
                        cpp::bind( &fs::unsafe::create_directories, cpp::cref( path ), _1 )
                        );
                }

                static void safeMkdirs( SAA_in const fs::path& path )
                {
                    const auto ec = trySafeMkdirs( path );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not create directory "
                            << normalizePathParameterForPrint( path )
                        );
                }

                static void imbueUtf8Paths()
                {
                    const std::locale locDefault;
                    const std::locale locUtf8Cvt( locDefault, new fs::utf8_codecvt_facet );

                    fs::path::imbue( locUtf8Cvt );
                }

                static fs::path normalize(
                    SAA_in          const fs::path&             path,
                    SAA_in          const fs::path&             base = fs::current_path()
                    )
                {
                    if( os::onUNIX() )
                    {
                        if( path.string().find( "\\\\" ) != std::string::npos )
                        {
                            BL_THROW_USER_FRIENDLY(
                                ArgumentException(),
                                BL_MSG()
                                    << "Path "
                                    << path
                                    << " contains an unsupported character sequence on Unix"
                                );
                        }
                    }

                    const auto absPath = absolute( path, base );

                    auto it = absPath.begin();
                    auto result = * it++;

                    /*
                     * resolves ".." and "." in the path
                     */

                    for( ; it != absPath.end(); ++it )
                    {
                        if( *it == ".." )
                        {
                            result = result.parent_path();
                        }
                        else if( *it != "." )
                        {
                            result /= *it;
                        }
                    }

                    /*
                     * converting the path to the preferred native format, on
                     * Windows slashes (which are also legal) are replaced with
                     * backslashes. The operation is a no-op on Unix.
                     */

                    return result.make_preferred();
                }

                static void createDirectoryJunction(
                    SAA_in          const fs::path&             to,
                    SAA_in          const fs::path&             junction
                    )
                {
                    ensureDirectoryAndNotJunction( to );
                    ensurePathDoesNotExist( junction );

                    if( os::onWindows() )
                    {
                        safeCreateDirectory( junction );

                        auto g = BL_SCOPE_GUARD(
                            {
                                safeDeletePathNothrow( junction );
                            }
                            );

                        os::createJunction( to, junction );

                        g.dismiss();
                    }
                    else
                    {
                        fs::create_symlink( to, junction );
                    }
                }

                static void deleteDirectoryJunction( SAA_in const fs::path& junction )
                {
                    ensureDirectoryJunction( junction );

                    if( os::onWindows() )
                    {
                        os::deleteJunction( junction );
                    }

                    chkDeleteInternal( junction, trySafeRemoveInternal( junction ) );
                }

                static void deleteDirectoryJunctionIfExists( SAA_in const fs::path& junction )
                {
                    if( path_exists( junction ) )
                    {
                        deleteDirectoryJunction( junction );
                    }
                }

                static bool isDirectoryJunction( SAA_in const fs::path& path )
                {
                    ensurePathExists( path );

                    const auto status = fs::symlink_status( path );

                    if( os::onWindows() )
                    {
                        return
                        (
                            ( reparse_file == status.type() || symlink_file == status.type() ) &&
                            os::isJunction( path )
                        );
                    }

                    if( symlink_file != status.type() )
                    {
                        return false;
                    }

                    if( fs::exists( path ) )
                    {
                        return fs::is_directory( fs::read_symlink( path ) );
                    }

                    /*
                     * This is a broken symlink case; we don't have much choice, but to
                     * assume it was a directory when it existed
                     */

                    return true;
                }

                static fs::path getDirectoryJunctionTarget( SAA_in const fs::path& junction )
                {
                    ensureDirectoryJunction( junction );

                    return os::onWindows() ?
                        os::getJunctionTarget( junction ) : fs::read_symlink( junction );
                }

                static fs::space_info diskSpace( SAA_in const fs::path& entry )
                {
                    eh::error_code ec;

                    const auto spaceInfo = fs::space( entry, ec );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Could not check disk space using "
                            << normalizePathParameterForPrint( entry )
                        );

                    return spaceInfo;
                }

                static std::string normalizePathCliParameter( SAA_in const std::string& path )
                {
                    /*
                     * On the server side the paths need to be absolute and normalized -
                     * i.e. relative paths and paths which refer to .. must be resolved
                     * on the CLI side
                     */

                    if( path.empty() )
                    {
                        return std::string();
                    }

                    auto pathAbsolute = normalize( path, fs::current_path() );

                    return fs::detail::WinLfnUtils::chk2RemovePrefix( std::move( pathAbsolute ) ).string();
                }

                static void copyDirectoryWithContents(
                    SAA_in      const fs::path&             sourceDir,
                    SAA_in      const fs::path&             targetDir
                    )
                {
                    ensurePathExists( sourceDir );
                    ensurePathDoesNotExist( targetDir );

                    auto g = BL_SCOPE_GUARD(
                        {
                            safeDeletePathNothrow( targetDir );
                        }
                        );

                    fs::copy_directory( sourceDir, targetDir );

                    for( fs::recursive_directory_iterator i( sourceDir ), end; i != end; ++i )
                    {
                        const auto sourcePath = i -> path();

                        auto sourcePathStr = sourcePath.relative_path().string();

                        str::replace_first(
                            sourcePathStr,
                            sourceDir.relative_path().string(),
                            str::empty()
                            );

                        fs::copy(
                            sourcePath,
                            targetDir / fs::path( sourcePathStr ).relative_path()
                            );
                    }

                    g.dismiss();
                }
            };

            typedef FsUtilsT<> FsUtils;

            /*
            * @brief SafeFileStreamWrapper is a base class to be derived from by safe input/output file stream wrappers
            */

            template
            <
                typename STREAM
            >
            class SafeFileStreamWrapper
            {
                BL_NO_COPY( SafeFileStreamWrapper )

            private:

                os::stdio_file_ptr                          m_filePtr;
                cpp::SafeUniquePtr< STREAM >                m_fileStream;

            protected:

                SafeFileStreamWrapper(
                    SAA_in      const fs::path&             path,
                    SAA_in      const char*                 openMode
                    )
                    :
                    m_filePtr( os::fopen( path, openMode ) ),
                    m_fileStream( os::fileptr2stream< STREAM >( m_filePtr.get() ) )
                {
                }

                SafeFileStreamWrapper( SafeFileStreamWrapper&& other ) NOEXCEPT
                    :
                    m_filePtr( std::move( other.m_filePtr ) ),
                    m_fileStream( std::move( other.m_fileStream ) )
                {
                }

                SafeFileStreamWrapper& operator=( SafeFileStreamWrapper&& other ) NOEXCEPT
                {
                    m_filePtr = std::move( other.m_filePtr );
                    m_fileStream = std::move( other.m_fileStream );
                    return *this;
                }

                ~SafeFileStreamWrapper() NOEXCEPT
                {
                    m_fileStream.reset();
                    m_filePtr.reset();
                }

            public:

                STREAM& stream() const NOEXCEPT
                {
                    BL_ASSERT( m_fileStream );
                    return *m_fileStream;
                }
            };

        } // detail

        inline std::time_t safeGetFileCreateTime( SAA_in const fs::path& path )
        {
            return detail::FsUtils::safeGetFileCreateTime( path );
        }

        inline void safeSetFileCreateTime(
            SAA_in          const fs::path&             path,
            SAA_in          const std::time_t&          newTime
            )
        {
            detail::FsUtils::safeSetFileCreateTime( path, newTime );
        }

        inline void safeUpdateFileAttributes(
            SAA_in          const fs::path&             path,
            SAA_in          const os::FileAttributes    attributes,
            SAA_in          const bool                  remove = false
            )
        {
            detail::FsUtils::safeUpdateFileAttributes( path, attributes, remove );
        }

        inline os::FileAttributes safeGetFileAttributes( SAA_in const fs::path& path )
        {
            return detail::FsUtils::safeGetFileAttributes( path );
        }

        inline void safeRename(
            SAA_in          const path&                 oldPath,
            SAA_in          const path&                 newPath
            )
        {
            detail::FsUtils::safeRename( oldPath, newPath );
        }

        inline eh::error_code trySafeRename(
            SAA_in          const path&                 oldPath,
            SAA_in          const path&                 newPath
            )
        {
            return detail::FsUtils::trySafeRename( oldPath, newPath );
        }

        inline void safeRemove( SAA_in const path& path )
        {
            detail::FsUtils::safeRemove( path );
        }

        inline eh::error_code trySafeRemove( SAA_in const path& path )
        {
            return detail::FsUtils::trySafeRemove( path );
        }

        inline void safeRemoveAll(
            SAA_in          const path&                                 path,
            SAA_in_opt      const cpp::void_callback_t&                 checkCancelCallback = cpp::void_callback_t(),
            SAA_in_opt      const bool                                  removeFileInUseOnReboot = false
            )
        {
            detail::FsUtils::safeRemoveAll( path, checkCancelCallback, removeFileInUseOnReboot );
        }

        inline eh::error_code trySafeRemoveAll( SAA_in const path& path )
        {
            return detail::FsUtils::trySafeRemoveAll( path );
        }

        inline void safeRemoveIfExists( SAA_in const path& path )
        {
            detail::FsUtils::safeRemoveIfExists( path );
        }

        inline eh::error_code trySafeRemoveIfExists( SAA_in const path& path )
        {
            return detail::FsUtils::trySafeRemoveIfExists( path );
        }

        inline void safeRemoveAllIfExists( SAA_in const path& path )
        {
            detail::FsUtils::safeRemoveAllIfExists( path );
        }

        inline eh::error_code trySafeRemoveAllIfExists( SAA_in const path& path )
        {
            return detail::FsUtils::trySafeRemoveAllIfExists( path );
        }

        inline fs::path makeHidden( SAA_in const fs::path& path )
        {
            return detail::FsUtils::makeHidden( path );
        }

        inline eh::error_code trySafeCreateDirectory( SAA_in const fs::path& path )
        {
            return detail::FsUtils::trySafeCreateDirectory( path );
        }

        inline void safeCreateDirectory( SAA_in const fs::path& path )
        {
            detail::FsUtils::safeCreateDirectory( path );
        }

        inline eh::error_code tryCreateTempDir( SAA_inout fs::path& path )
        {
            return detail::FsUtils::tryCreateTempDir( path );
        }

        inline void createTempDir( SAA_inout fs::path& path )
        {
            detail::FsUtils::createTempDir( path );
        }

        inline eh::error_code trySafeMkdirs( SAA_in const fs::path& path )
        {
            return detail::FsUtils::trySafeMkdirs( path );
        }

        inline void safeMkdirs( SAA_in const fs::path& path )
        {
            detail::FsUtils::safeMkdirs( path );
        }

        inline bool safeDeletePathNothrow( SAA_in const fs::path& path ) NOEXCEPT
        {
            return detail::FsUtils::safeDeletePathNothrow( path );
        }

        inline void renameToMakeVisibleAs(
            SAA_in                  const fs::path&                         tmpDir,
            SAA_in                  const fs::path&                         targetDir
            )
        {
            /*
             * Just create the parent directory if not present, remove the hidden attribute
             * (if present) and then rename into the target directory
             */

            BL_CHK_USER_FRIENDLY(
                false,
                false == fs::exists( targetDir ) && targetDir.is_absolute() && targetDir.has_parent_path(),
                BL_MSG()
                    << "The target directory "
                    << normalizePathParameterForPrint( targetDir )
                    << " should be absolute, it should not exist and must have a valid parent path"
                );

            safeMkdirs( targetDir.parent_path() );
            safeUpdateFileAttributes( tmpDir, os::FileAttributeHidden, true /* remove */ );
            safeRename( tmpDir, targetDir );
        }

        inline void imbueUtf8Paths()
        {
            detail::FsUtils::imbueUtf8Paths();
        }

        inline fs::path normalize(
            SAA_in          const fs::path&             path,
            SAA_in          const fs::path&             base = fs::current_path()
            )
        {
            return detail::FsUtils::normalize( path, base );
        }

        inline void createDirectoryJunction(
            SAA_in          const fs::path&             to,
            SAA_in          const fs::path&             junction
            )
        {
            detail::FsUtils::createDirectoryJunction( to, junction );
        }

        inline void deleteDirectoryJunction( SAA_in const fs::path& junction )
        {
            detail::FsUtils::deleteDirectoryJunction( junction );
        }

        inline void deleteDirectoryJunctionIfExists( SAA_in const fs::path& junction )
        {
            detail::FsUtils::deleteDirectoryJunctionIfExists( junction );
        }

        inline bool isDirectoryJunction( SAA_in const fs::path& path )
        {
            return detail::FsUtils::isDirectoryJunction( path );
        }

        inline fs::path getDirectoryJunctionTarget( SAA_in const fs::path& junction )
        {
            return detail::FsUtils::getDirectoryJunctionTarget( junction );
        }

        inline fs::space_info diskSpace( SAA_in const fs::path& entry )
        {
            return detail::FsUtils::diskSpace( entry );
        }

        inline std::string normalizePathCliParameter( SAA_in const std::string& path )
        {
            return detail::FsUtils::normalizePathCliParameter( path );
        }

        inline void copyDirectoryWithContents(
            SAA_in          const fs::path&             sourceDir,
            SAA_in          const fs::path&             targetDir
            )
        {
            detail::FsUtils::copyDirectoryWithContents( sourceDir, targetDir );
        }

        namespace unsafe
        {
            inline fs::path makeHidden( SAA_in const fs::path& path )
            {
                return detail::FsUtils::makeHidden( path );
            }
        }

        /**
         * @brief class TmpDir
         */

        template
        <
            typename E = void
        >
        class TmpDirT
        {
        private:

            fs::path m_tmpDir;

        public:

            TmpDirT( SAA_in_opt const fs::path& rootTemp = fs::path() )
            {
                if( rootTemp.empty() )
                {
                    createTempDir( m_tmpDir );
                }
                else
                {
                    auto tmpDir = rootTemp / fs::path( "bl-temp-dir-" + uuids::uuid2string( uuids::create() ) );
                    safeCreateDirectory( tmpDir );
                    m_tmpDir.swap( tmpDir );
                }

                m_tmpDir = makeHidden( m_tmpDir );
            }

            ~TmpDirT() NOEXCEPT
            {
                safeDeletePathNothrow( m_tmpDir );
            }

            const fs::path& path() const NOEXCEPT
            {
                return m_tmpDir;
            }
        };

        typedef TmpDirT<> TmpDir;

        /*
         * @brief Safe input file stream wrapper
         * The input file stream wrapper is based on os::fopen and os::fileptr2istream
         */

        template
        <
            typename E = void
        >
        class SafeInputFileStreamWrapperT : public detail::SafeFileStreamWrapper< std::istream >
        {
            typedef detail::SafeFileStreamWrapper< std::istream >              base_type;

            BL_CTR_COPY_DEFAULT_T( SafeInputFileStreamWrapperT, SAA_in_opt SafeInputFileStreamWrapperT&&, base_type, public, NOEXCEPT )
            BL_NO_COPY( SafeInputFileStreamWrapperT )

        public:

            SafeInputFileStreamWrapperT( SAA_in const fs::path& path )
                :
                detail::SafeFileStreamWrapper< std::istream >( path, "rb" )
            {
            }
        };

        typedef SafeInputFileStreamWrapperT<>                       SafeInputFileStreamWrapper;
        typedef cpp::SafeUniquePtr< SafeInputFileStreamWrapper >    SafeInputFileStreamWrapperPtr;

         /*
         * @brief Safe output file stream wrapper
         * The output file stream wrapper is based on os::fopen and os::fileptr2ostream
         */

        template
        <
            typename E = void
        >
        class SafeOutputFileStreamWrapperT : public detail::SafeFileStreamWrapper< std::ostream >
        {
            typedef detail::SafeFileStreamWrapper< std::ostream >              base_type;

            BL_CTR_COPY_DEFAULT_T( SafeOutputFileStreamWrapperT, SAA_in_opt SafeOutputFileStreamWrapperT&&, base_type, public, NOEXCEPT )
            BL_NO_COPY( SafeOutputFileStreamWrapperT )

        public:

            enum class OpenMode
            {
                TRUNCATE,
                APPEND
            };

            SafeOutputFileStreamWrapperT(
                SAA_in          const fs::path&         path,
                SAA_in_opt      OpenMode                openMode = OpenMode::TRUNCATE
                )
                :
                detail::SafeFileStreamWrapper< std::ostream >( path, openMode == OpenMode::APPEND ? "ab" : "wb" )
            {
                BL_ASSERT( openMode == OpenMode::TRUNCATE || openMode == OpenMode::APPEND );
            }
        };

        typedef SafeOutputFileStreamWrapperT<>                      SafeOutputFileStreamWrapper;
        typedef cpp::SafeUniquePtr< SafeOutputFileStreamWrapper >   SafeOutputFileStreamWrapperPtr;

        inline bool createLockFile( SAA_in const fs::path& path )
        {
            if ( os::createNewFile( path ) )
            {
                SafeOutputFileStreamWrapper file( path );
                auto& os = file.stream();

                os
                    << os::getPid()
                    << '\n';

                return true;
            }
            else
            {
                return false;
            }
        }

    } // fs

} // bl

#endif /* __BL_FSUTILS_H_ */
