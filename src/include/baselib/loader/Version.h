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

#ifndef __BL_LOADER_VERSION_H_
#define __BL_LOADER_VERSION_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace loader
    {
        /**
         * @brief class Version
         */
        template
        <
            typename E = void
        >
        class VersionT
        {
        public:

            static std::string toString(
                SAA_in      const std::size_t           majorVersion,
                SAA_in      const std::size_t           minorVersion,
                SAA_in      const std::size_t           patchVersion
                )
            {
                cpp::SafeOutputStringStream version;

                version
                    << majorVersion
                    << "."
                    << minorVersion
                    << "."
                    << patchVersion;

                return version.str();
            }

            static void fromString(
                SAA_in      const std::string&          versionStr,
                SAA_out     std::size_t&                majorVersion,
                SAA_out     std::size_t&                minorVersion,
                SAA_out     std::size_t&                patchVersion
                )
            {
                unsigned short major, minor, patch;

                const auto rc = ::sscanf(
                    versionStr.c_str(),
                    "%hu.%hu.%hu",
                    &major,
                    &minor,
                    &patch
                    );

                BL_CHK_T(
                    true,
                    rc != 3,
                    ArgumentException(),
                    BL_MSG()
                        << "Invalid version number '"
                        << versionStr
                        << "', expected format <major>.<minor>.<patch>"
                    );

                majorVersion = major;
                minorVersion = minor;
                patchVersion = patch;
            }
        };

        typedef VersionT<> Version;

    } // loader

} // bl

#endif /* __BL_LOADER_VERSION_H_ */
