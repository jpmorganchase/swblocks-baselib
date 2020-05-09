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
#include <boost/version.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/seed_seq.hpp>
#include <boost/random/random_device.hpp>
#if ( ( BOOST_VERSION / 100 ) < 1072 )
#include <boost/uuid/seed_rng.hpp>
#endif
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <climits>

namespace bl
{
    namespace random
    {
        /*
         * Import the Mersenne Twister generator from boost as we
         * need to use Boost implementation to seed our RNGs.
         *
         * On some platforms we cannot use the corresponding std::random_device as it cannot
         * reliably seed any RNG. Its implementation isn't guaranteed to be robust and in
         * some confirmed cases it always results in the same seed value being returned
         * (e.g. due to reliance on malfunctioning /dev/urandom) causing duplicated UUIDs.
         *
         * On some older version of boost we can use a custom entropy function to generate
         * seed sequence (boost::uuids::detail::seed) and on the newer versions we simply
         * use boost::random::random_device
         */

        using boost::mt19937;
        using boost::random::uniform_int_distribution;
        using boost::random::seed_seq;
        using boost::random::random_device;

        #if ( ( BOOST_VERSION / 100 ) < 1072 )
        using boost::uuids::detail::seed;
        #else
        /*
         * As per the following SO post:
         * https://codereview.stackexchange.com/questions/109260/seed-stdmt19937-from-stdrandom-device
         */
        template
        <
            typename UniformRandomNumberGenerator
        >
        inline void seed( UniformRandomNumberGenerator& urng )
        {
            const std::size_t urngStateSizeInBits =
                UniformRandomNumberGenerator::state_size * UniformRandomNumberGenerator::word_size;

            random_device randomSource;
            unsigned data[ ( urngStateSizeInBits - 1 ) / sizeof( unsigned ) + 1 ];

            std::generate( std::begin( data ), std::end( data ), std::ref( randomSource ) );
            seed_seq seedBytes( std::begin( data ), std::end( data ) );
            urng.seed( seedBytes );
        }
        #endif

    } // random

} // bl

#endif /* __BL_RANDOMBOOSTIMPORTS_H_ */
