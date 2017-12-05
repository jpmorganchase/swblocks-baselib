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

#ifndef __BL_UUIDBOOSTIMPORTS_H_
#define __BL_UUIDBOOSTIMPORTS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#if defined(_WIN32)

/*
 * uuid_t macro pulled from rpcdce.h shouldn't be defined at this point to avoid conflict with bl::uuid_t
 * Report an error in case we didn't #undef it
 */

#if defined( uuid_t )
#error "uuid_t shouldn't be defined as macro when compiling UuidBoostImports.h file"
#endif // defined( uuid_t )

#endif // defined(_WIN32)

namespace bl
{
    namespace uuids
    {
        using boost::uuids::uuid;
        using boost::uuids::random_generator;
        using boost::uuids::nil_uuid;

    } // uuids

    typedef uuids::uuid uuid_t;

} // bl

namespace std
{
    /*
     * Provide specialization of hash function in the std namespace
     * for uuid_t. This will allow us to use uuid_t as key in the
     * new C++11 std containers (e.g. std::unordered_map)
     */

    template
    <
    >
    struct hash< bl::uuid_t >
    {
        std::size_t operator()( const bl::uuid_t& uuid ) const
        {
            boost::hash< bl::uuid_t > hasher;
            return hasher( uuid );
        }
    };

} // std

#endif /* __BL_UUIDBOOSTIMPORTS_H_ */
