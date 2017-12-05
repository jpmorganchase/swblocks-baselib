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

#ifndef __BL_PATHUTILS_H_
#define __BL_PATHUTILS_H_

#include <baselib/core/OS.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace fs
    {
        namespace detail
        {
            /**
             * @brief class PathUtils - extensions to boost filesystem
             */

            template
            <
                typename E = void
            >
            class PathUtilsT
            {
            private:

                static const fs::path                                   g_dotPath;
                static const std::string                                g_dot;
                static const std::string                                g_twoDots;

            public:

                static void ensureAbsolute( SAA_in const fs::path& path )
                {
                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        path.is_absolute(),
                        ArgumentException(),
                        BL_MSG()
                            << "Path "
                            << path
                            << " must be absolute"
                        );
                }

                static void ensureNotEmpty( SAA_in const fs::path& path )
                {
                    BL_CHK_T_USER_FRIENDLY(
                        true,
                        path.empty(),
                        ArgumentException(),
                        BL_MSG()
                            << "Path "
                            << path
                            << " cannot be empty"
                        );
                }

                /**
                 * @brief Helper to obtain relative path of a full path since such function doesn't
                 * exist in boost filesystem (open issue: https://svn.boost.org/trac/boost/ticket/1976)
                 *
                 * Note: Both input paths are assumed to be absolute (it will not work with relative
                 * paths) and generally canonicalized if necessary on the client side. The function
                 * does not resolve symlinks and / or tries to do canonicalization. Also note that
                 * the root is expected to be not empty otherwise the test is nonsensical.
                 *
                 * @return true if 'root' is a strict prefix path of of 'path' and false otherwise
                 */

                static bool getRelativePath(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const fs::path&                     root,
                    SAA_inout       fs::path&                           relPath,
                    SAA_in_opt      bool                                allowNonStrictRoot = false
                    )
                {
                    ensureAbsolute( path );
                    ensureAbsolute( root );
                    ensureNotEmpty( root );

                    bool isRootStrictPrefix = true;

                    auto ip = path.begin();
                    auto ir = root.begin();

                    while( *ip == *ir && ip != path.end() && ir != root.end() )
                    {
                        ++ip;
                        ++ir;
                    }

                    /*
                     * Filter out the trailing slashes if (any). According to the boost filesystem
                     * docs for path iterators (link below) the path iterator will be dot for any
                     * trailing slashes.
                     *
                     * http://www.boost.org/doc/libs/1_53_0/libs/filesystem/doc/reference.html#path-iterators
                     */

                    while( ir != root.end() && g_dotPath == *ir )
                    {
                        ++ir;
                    }

                    if( ir != root.end() )
                    {
                        /*
                         * 'root' is not a strict prefix of path
                         */
                        isRootStrictPrefix = false;

                        if( ! allowNonStrictRoot )
                        {
                            return false;
                        }
                    }

                    /*
                     * just construct the relative path and return isRootStrictPrefix
                     */

                    fs::path rp;

                    while( ir != root.end() )
                    {
                       rp /= g_twoDots;
                       ++ir;
                    }

                    while( ip != path.end() )
                    {
                        rp /= *ip;
                        ++ip;
                    }

                    rp.swap( relPath );

                    return isRootStrictPrefix;
                }

                template
                <
                    typename Predicate
                >
                static bool isValidDirectoryName(
                    SAA_in          const std::string&                  name,
                    SAA_in          const Predicate&                    isInvalidChar,
                    SAA_in          const bool                          allowCurrentAndParentDir
                    )
                {
                    /*
                     * NTFS is more restrictive than typical Linux file systems hence doing the below check
                     * Boost portable_name() cannot be used because it is too restrictive, e.g. does not allow '+'
                     * Boost windows_name() considers '?' and '*' valid hence not using it as well
                     */

                    if( name.empty() || std::any_of( name.begin(), name.end(), isInvalidChar ) )
                    {
                        return false;
                    }

                    if ( ! allowCurrentAndParentDir )
                    {
                        return g_dot != name && g_twoDots != name;
                    }

                    return true;
                }

                template
                <
                    typename Predicate
                >
                static bool isValidPathName(
                    SAA_in          const fs::path&                     path,
                    SAA_in          const Predicate&                    isInvalidChar,
                    SAA_in          const bool                          allowCurrentAndParentDir,
                    SAA_in          const bool                          allowAbsolutePath
                    )
                {
                    if( path.empty() )
                    {
                        return false;
                    }

                    if( allowAbsolutePath )
                    {
                        for( const auto& dir : path.relative_path() )
                        {
                            if( ! isValidDirectoryName( dir.string(), isInvalidChar, allowCurrentAndParentDir ) )
                            {
                                return false;
                            }
                        }
                    }
                    else
                    {
                        for( const auto& dir : path )
                        {
                            if( ! isValidDirectoryName( dir.string(), isInvalidChar, allowCurrentAndParentDir ) )
                            {
                                return false;
                            }
                        }
                    }

                    return true;
                }
            };

            BL_DEFINE_STATIC_MEMBER( PathUtilsT, const fs::path, g_dotPath )    = ".";
            BL_DEFINE_STATIC_CONST_STRING( PathUtilsT, g_dot )                  = ".";
            BL_DEFINE_STATIC_CONST_STRING( PathUtilsT, g_twoDots )              = "..";

            typedef PathUtilsT<> PathUtils;

        } // detail

        inline void ensureAbsolute( SAA_in const fs::path& path )
        {
            detail::PathUtils::ensureAbsolute( path );
        }

        inline void ensureNotEmpty( SAA_in const fs::path& path )
        {
            detail::PathUtils::ensureNotEmpty( path );
        }

        inline bool getRelativePath(
            SAA_in          const fs::path&                     path,
            SAA_in          const fs::path&                     root,
            SAA_inout       fs::path&                           relPath,
            SAA_in_opt      bool                                allowNonStrictRoot = false
            )
        {
            return detail::PathUtils::getRelativePath( path, root, relPath, allowNonStrictRoot );
        }

        /**
         * @brief Check if a file or directory name (not the whole file path)
         * is valid on all OS architectures
         */

        inline bool isPortableDirectoryName(
            SAA_in          const std::string&                  name,
            SAA_in_opt      const bool                          allowCurrentAndParentDir = false
            )
        {
            return detail::PathUtils::isValidDirectoryName(
                name,
                str::is_any_of( os::getNonPortableFileNameCharacters() ),
                allowCurrentAndParentDir
                );
        }

        /**
         * @brief Check if a file or directory name (not the whole file path)
         * is valid on the current OS architecture
         */

        inline bool isValidDirectoryName(
            SAA_in          const std::string&                  name,
            SAA_in_opt      const bool                          allowCurrentAndParentDir = false
            )
        {
            return detail::PathUtils::isValidDirectoryName(
                name,
                str::is_any_of( os::getInvalidFileNameCharacters() ),
                allowCurrentAndParentDir
                );
        }

        /**
         * @brief Checks if the path is relative and contains only portable characters.
         *
         * @note Absolute paths (/foo/bar, C:\foo\bar) are considered non-portable because
         * the first element is either "/" or "C:", both of which contain invalid characters
         * and both are system-specific.
         */

        inline bool isPortablePath(
            SAA_in          const std::string&                  path,
            SAA_in_opt      const bool                          allowCurrentAndParentDir = false
            )
        {
            return
                detail::PathUtils::isValidDirectoryName(
                    path,
                    str::is_any_of( os::getNonPortablePathNameCharacters() ),
                    true /* allowCurrentAndParentDir */
                    )
                &&
                detail::PathUtils::isValidPathName(
                    path,
                    str::is_any_of( os::getNonPortableFileNameCharacters() ),
                    allowCurrentAndParentDir,
                    false /* allowAbsolutePath */
                    );
        }

        /**
         * @brief Checks if the path contains only valid characters.
         *
         * @note Absolute paths must be allowed explicitly.
         */

        inline bool isValidPath(
            SAA_in          const std::string&                  path,
            SAA_in_opt      const bool                          allowCurrentAndParentDir = false,
            SAA_in_opt      const bool                          allowAbsolutePath = false
            )
        {
            return detail::PathUtils::isValidPathName(
                path,
                str::is_any_of( os::getInvalidFileNameCharacters() ),
                allowCurrentAndParentDir,
                allowAbsolutePath
                );
        }

    } // fs

} // bl

#endif /* __BL_PATHUTILS_H_ */
