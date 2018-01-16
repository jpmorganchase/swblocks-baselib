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

#ifndef __BL_REFCOUNTEDBASE_H_
#define __BL_REFCOUNTEDBASE_H_

#include <baselib/core/BaseIncludes.h>

#include <type_traits>
#include <atomic>
#include <utility>

namespace bl
{
    /**
     * @brief class RefCountedBase
     */
    template
    <
        bool isMultiThreaded = true
    >
    class RefCountedBase
    {
        BL_CTR_COPY_DELETE( RefCountedBase )
        BL_NO_POLYMORPHIC_BASE( RefCountedBase )

    protected:

        typedef typename std::conditional< isMultiThreaded, std::atomic< long >, long >::type
            counter_t;

        counter_t m_refs;

        RefCountedBase()
            :
            m_refs( 1L )
        {
        }
    };

} // bl

#endif /* __BL_REFCOUNTEDBASE_H_ */

