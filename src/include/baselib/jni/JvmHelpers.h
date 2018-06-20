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

#ifndef __BL_JNI_JVMHELPERS_H_
#define __BL_JNI_JVMHELPERS_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        template
        <
            typename E = void
        >
        class JvmHelpersT
        {
            BL_DECLARE_STATIC( JvmHelpersT )

        public:

            static std::string buildClassPath(
                SAA_in              const std::string&                  jarBasePath,
                SAA_in              const std::string&                  jarFileName
                )
            {
                const fs::path basePath = fs::normalize( jarBasePath );
                const auto jarPath = basePath / jarFileName;
                const auto dependenciesPath = basePath / "lib";

                BL_CHK(
                    false,
                    fs::exists( jarPath ),
                    BL_MSG()
                        << "Cannot find required JAR file "
                        << fs::normalizePathParameterForPrint( jarPath )
                    );

                MessageBuffer buffer;
                buffer << jarPath.string();

                for( fs::directory_iterator i( dependenciesPath ), end; i != end; ++i )
                {
                    const auto path = ( i -> path() ).string();

                    if( str::ends_with( path, ".jar" ) )
                    {
                        buffer
                            << ( os::onWindows() ? ';' : ':' )
                            << path;
                    }
                }

                return buffer.text();
            }
        };

        typedef JvmHelpersT<> JvmHelpers;

    } // jni

} // bl

#endif /* __BL_JNI_JVMHELPERS_H_ */
