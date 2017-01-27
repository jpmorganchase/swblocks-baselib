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

#ifndef __BL_NUMBERUTILS_H_
#define __BL_NUMBERUTILS_H_

#include <baselib/core/BaseIncludes.h>

#include <cmath>

namespace bl
{
    namespace detail
    {
        /**
         * @brief class NumberUtils
         */
        template
        <
            typename E = void
        >
        class NumberUtilsT
        {
        public:

        };

        typedef NumberUtilsT<> NumberUtils;

    } // detail

    namespace numbers
    {
        template
        <
            typename T
        >
        inline CONSTEXPR T getDefaultEpsilon() NOEXCEPT
        {
            /*
             * Nothing fancy right now; just some value that is small enough.
             *
             * We also return it as float, so we don't have to specialize this
             * for double vs. float (the compiler will do the conversion)
             */

            return 0.000001f;
        }

        template
        <
            typename T
        >
        inline bool floatingPointEqual( SAA_in const T v1, const T v2, const T epsilon = getDefaultEpsilon< T >() ) NOEXCEPT
        {
            return std::fabs( v1 - v2 ) < epsilon;
        }

    } // numbers

} // bl

#endif /* __BL_NUMBERUTILS_H_ */
