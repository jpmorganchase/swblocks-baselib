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

#ifndef __BL_RANDOM_H_
#define __BL_RANDOM_H_

#include <baselib/core/BaseIncludes.h>
#include <baselib/core/detail/RandomBoostImports.h>

namespace bl
{
    namespace random
    {
        namespace detail
        {
            /**
             * @brief class Random
             */

            template
            <
                typename E = void
            >
            class RandomUtilsT
            {
            public:

                template
                <
                    typename T
                >
                static T getUniformRandomUnsignedValue( SAA_in const T maxValue )
                {
                    random::uniform_int_distribution< T > dist( 0, maxValue );

                    return dist( tlsData().random.urng() );
                }

                static void getRandomBytes(
                    SAA_out_bcount( bufferSize )        void*                                   buffer,
                    SAA_in                              const std::size_t                       bufferSize
                    )
                {
                    BL_ASSERT( buffer );
                    BL_ASSERT( bufferSize );

                    random::uniform_int_distribution< unsigned short > dist(
                        std::numeric_limits< unsigned char >::min(),
                        std::numeric_limits< unsigned char >::max()
                        );

                    unsigned char* p = ( unsigned char* ) buffer;

                    auto& urng = tlsData().random.urng();

                    for( std::size_t i = 0U; i < bufferSize; ++i )
                    {
                        p[ i ] = static_cast< unsigned char >( dist( urng ) );
                    }
                }
            };

            typedef RandomUtilsT<> RandomUtils;

        } // detail

        template
        <
            typename T
        >
        T getUniformRandomUnsignedValue( SAA_in const T maxValue )
        {
            return detail::RandomUtils::getUniformRandomUnsignedValue< T >( maxValue );
        }

        static void getRandomBytes(
            SAA_out_bcount( bufferSize )        void*                                   buffer,
            SAA_in                              const std::size_t                       bufferSize
            )
        {
            detail::RandomUtils::getRandomBytes( buffer, bufferSize );
        }

    } // random

} // bl

#endif /* __BL_RANDOM_H_ */
