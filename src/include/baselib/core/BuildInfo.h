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

#ifndef __BL_BUILDINFO_H_
#define __BL_BUILDINFO_H_

#include <baselib/core/PreprocessorUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    template
    <
        typename E = void
    >
    class BuildInfoT
    {
        BL_DECLARE_ABSTRACT( BuildInfoT )

    public:

        static const std::string                                            platform;
        static const std::string                                            arch;
        static const std::string                                            variant;
        static const std::string                                            os;
        static const std::string                                            toolchain;
    };

    BL_DEFINE_STATIC_CONST_STRING( BuildInfoT, platform )                   = BL_PP_STRINGIZE( BL_BUILD_PLATFORM );
    BL_DEFINE_STATIC_CONST_STRING( BuildInfoT, arch )                       = BL_PP_STRINGIZE( BL_BUILD_ARCH );
    BL_DEFINE_STATIC_CONST_STRING( BuildInfoT, variant )                    = BL_PP_STRINGIZE( BL_BUILD_VARIANT );
    BL_DEFINE_STATIC_CONST_STRING( BuildInfoT, os )                         = BL_PP_STRINGIZE( BL_BUILD_OS );
    BL_DEFINE_STATIC_CONST_STRING( BuildInfoT, toolchain )                  = BL_PP_STRINGIZE( BL_BUILD_TOOLCHAIN );

    typedef BuildInfoT<> BuildInfo;

} // bl

#endif /* __BL_BUILDINFO_H_ */
