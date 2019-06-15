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

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/numeric/conversion/cast.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/BaseIncludes.h>

#include <cmath>

namespace bl
{
    namespace detail
    {
        template
        <
            bool checkIfItFits
        >
        class NumberCoerceHelper;

        template
        <
        >
        class NumberCoerceHelper< false >
        {
        public:

            template
            <
                typename T,
                typename U
            >
            static T safeCoerceTo(
                 SAA_in             const U                                 value,
                 SAA_in             const cpp::void_callback_t&             ehCallback
                )
            {
                BL_UNUSED( ehCallback );

                static_assert( ( sizeof( T ) >= sizeof( U ) ), "sizeof T must be greater or equal to sizeof U" );

                BL_ASSERT( static_cast< U >( static_cast< T >( value ) ) == value );

                return static_cast< T >( value );
            }
        };

        template
        <
        >
        class NumberCoerceHelper< true >
        {
        public:

            template
            <
                typename T,
                typename U
            >
            static T safeCoerceTo(
                 SAA_in             const U                                 value,
                 SAA_in             const cpp::void_callback_t&             ehCallback
                )
            {
                static_assert( sizeof( T ) <= sizeof( U ), "sizeof T must be less or equal to sizeof U" );

                if( value > static_cast< U >( std::numeric_limits< T >::max() ) )
                {
                    if( ehCallback )
                    {
                        ehCallback();
                    }
                    else
                    {
                        /*
                         * The cast to std::uint64_t below (in the message) is to make sure characters are
                         * serialized as numbers
                         */

                        BL_THROW(
                            NumberCoerceException(),
                            BL_MSG()
                                << "Cannot coerce number "
                                << static_cast< std::uint64_t >( value )
                                << " into a smaller numeric type of size "
                                << sizeof( T )
                                << " (in bytes) which is also "
                                << ( std::is_signed< T >::value ? "signed" : "unsigned" )
                                << " and can hold a maximum value of "
                                << static_cast< std::uint64_t >( std::numeric_limits< T >::max() )
                            );
                    }
                }
                else
                {
                    BL_ASSERT( static_cast< U >( static_cast< T >( value ) ) == value );
                }

                return static_cast< T >( value );
            }
        };

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
        using boost::numeric_cast;
        using boost::numeric::bad_numeric_cast;
        using boost::numeric::positive_overflow;
        using boost::numeric::negative_overflow;

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
        inline bool floatingPointEqual(
            SAA_in              const T                                 v1,
            SAA_in              const T                                 v2,
            SAA_in              const T                                 epsilon = getDefaultEpsilon< T >()
            ) NOEXCEPT
        {
            return std::fabs( v1 - v2 ) < epsilon;
        }

        template
        <
            typename T,
            typename U
        >
        inline T safeCoerceTo(
             SAA_in             const U                                 value,
             SAA_in_opt         const cpp::void_callback_t&             ehCallback = cpp::void_callback_t()
            )
        {
            static_assert(
                std::is_integral< T >::value && ! std::is_same< bool, T >::value,
                "T must be of numeric type"
                );

            static_assert(
                std::is_integral< U >::value && ! std::is_same< bool, U >::value,
                "U must be of numeric type"
                );

            typedef detail::NumberCoerceHelper
            <
                /* checkIfItFits */
                ( sizeof( T ) < sizeof( U ) ) ||
                ( sizeof( T ) == sizeof( U ) && std::is_signed< T >::value && ! std::is_signed< U >::value )
            >
            coerce_helpet_t;

            return coerce_helpet_t::template safeCoerceTo< T, U >( value, ehCallback );
        }

    } // numbers

} // bl

#endif /* __BL_NUMBERUTILS_H_ */
