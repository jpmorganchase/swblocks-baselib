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

#ifndef __BL_OSIMPL_H_
#define __BL_OSIMPL_H_

#if defined(_WIN32)
#include <baselib/core/detail/OSImplWindows.h>
#else // defined(_WIN32)
#include <baselib/core/detail/OSImplUNIX.h>
#endif // defined(_WIN32)

#include <baselib/core/detail/OSBoostImports.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace os
    {
        namespace detail
        {
            /**
             * @brief class OS
             */

            typedef OSImpl OS;

        } // detail

    } // os

} // bl

#endif /* __BL_OSIMPL_H_ */
