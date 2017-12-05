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

#ifndef __BL_POOLALLOCATORDEFAULT_H_
#define __BL_POOLALLOCATORDEFAULT_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    /**
     * @brief class PoolAllocatorDefault - default pool allocator based on
     * new[]/delete[] with throw semantics
     */
    template
    <
        typename E = void
    >
    class PoolAllocatorDefaultT
    {
    public:

        typedef std::size_t                 size_type;
        typedef std::ptrdiff_t              difference_type;

        static char* malloc( SAA_in const size_type sizeInBytes  )
        {
            return new char[ sizeInBytes ];
        }

        static void free( const char* block )
        {
            delete[] block;
        }
    };

    typedef PoolAllocatorDefaultT<> PoolAllocatorDefault;

} // bl

#endif /* __BL_POOLALLOCATORDEFAULT_H_ */
