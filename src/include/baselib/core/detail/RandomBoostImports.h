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

#ifndef __BL_RANDOMBOOSTIMPORTS_H_
#define __BL_RANDOMBOOSTIMPORTS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/uuid/seed_rng.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

namespace bl
{
    namespace random
    {
        /*
         * Import the Mersenne Twister generator from boost as we
         * need to use Boost seed() implementation to seed our RNGs.
         *
         * We cannot use the corresponding std classes as std::random_device cannot
         * reliably seed any RNG. Its implementation isn't guaranteed to be robust and in
         * some confirmed cases it always results in the same seed value being returned
         * (e.g. due to reliance on malfunctioning /dev/urandom) causing duplicated UUIDs.
         *
         */

        using boost::mt19937;
        using boost::uuids::detail::seed;
        using boost::random::uniform_int_distribution;

    } // random

} // bl

#endif /* __BL_RANDOMBOOSTIMPORTS_H_ */
