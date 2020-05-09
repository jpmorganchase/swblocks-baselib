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

#ifndef __BL_TIMEBOOSTIMPORTS_H_
#define __BL_TIMEBOOSTIMPORTS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/thread/thread_time.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

namespace bl
{
    namespace time
    {
        using boost::system_time;

        using boost::posix_time::ptime;
        using boost::posix_time::time_duration;
        using boost::posix_time::second_clock;
        using boost::posix_time::microsec_clock;

        /*
         * Special values for time/duration related types
         */

        using boost::posix_time::pos_infin;
        using boost::posix_time::neg_infin;
        using boost::posix_time::not_a_date_time;
        using boost::posix_time::min_date_time;
        using boost::posix_time::max_date_time;

        using boost::posix_time::to_simple_string;
        using boost::posix_time::to_iso_string;
        using boost::posix_time::to_iso_extended_string;
        using boost::posix_time::to_tm;

        using boost::posix_time::time_from_string;
        using boost::posix_time::duration_from_string;
        using boost::posix_time::from_iso_string;
        using boost::posix_time::from_time_t;
        using boost::posix_time::time_facet;
        using boost::posix_time::time_input_facet;
        using boost::local_time::time_zone_ptr;
        using boost::local_time::local_date_time;
        using boost::local_time::posix_time_zone;
        using boost::local_time::local_time_facet;

        using boost::date_time::c_local_adjustor;

        using boost::gregorian::date;
        using boost::gregorian::date_input_facet;
        using boost::gregorian::days;
        using boost::gregorian::day_clock;
        using boost::gregorian::from_string;
        using boost::gregorian::from_undelimited_string;
        using boost::gregorian::to_iso_string;
        using boost::gregorian::to_simple_string;

        /*
         * Standard duration units to construct time
         */

        namespace internal
        {
            using boost::posix_time::hours;
            using boost::posix_time::minutes;
            using boost::posix_time::seconds;
            using boost::posix_time::milliseconds;
            using boost::posix_time::microseconds;

        } // internal

        template
        <
            typename T
        >
        inline time_duration hours( const T value )
        {
            return internal::hours( static_cast< long >( value ) );
        }

        template
        <
            typename T
        >
        inline time_duration minutes( const T value )
        {
            return internal::minutes( static_cast< long >( value ) );
        }

        template
        <
            typename T
        >
        inline time_duration seconds( const T value )
        {
            return internal::seconds( static_cast< long >( value ) );
        }

        template
        <
            typename T
        >
        inline time_duration milliseconds( const T value )
        {
            return internal::milliseconds( static_cast< std::int64_t >( value ) );
        }

        template
        <
            typename T
        >
        inline time_duration microseconds( const T value )
        {
            return internal::microseconds( static_cast< std::int64_t >( value ) );
        }

    } // time

} // bl

#endif /* __BL_TIMEBOOSTIMPORTS_H_ */
