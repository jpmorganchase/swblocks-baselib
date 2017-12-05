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

#ifndef __BL_TIMEUTILS_H_
#define __BL_TIMEUTILS_H_

#include <baselib/core/detail/TimeBoostImports.h>
#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>

namespace bl
{
    /**
     * @brief class TimeUtils
     */
    template
    <
        typename E = void
    >
    class TimeUtilsT
    {
        BL_DECLARE_STATIC( TimeUtilsT )

    private:

        static const time::ptime                                        g_epoch;
        static const std::string                                        g_regexLocalTimeISO;

    public:

        static std::string isoToSimpleString( SAA_in const std::string& isoString )
        {
            return time::to_simple_string( time::from_iso_string( isoString ) );
        }

        static auto epoch() NOEXCEPT -> const time::ptime&
        {
            return g_epoch;
        }

        static auto regexLocalTimeISO() NOEXCEPT -> const std::string&
        {
            return g_regexLocalTimeISO;
        }
    };

    BL_DEFINE_STATIC_MEMBER( TimeUtilsT, const time::ptime, g_epoch )( time::date( 1970, 1, 1 ) );

    /*
     * Timestamp example in local time ISO format: 2015-02-20T16:10:14.766537-05:00
     * The validation enforced with the regex is approximate, but not perfect of course
     */

    BL_DEFINE_STATIC_CONST_STRING( TimeUtilsT, g_regexLocalTimeISO ) =
        "\\d{4}-[01]\\d-[0-3]\\dT[0-2]\\d:[0-5]\\d:[0-5]\\d\\.\\d+[+-][0-2]\\d:[0-5]\\d";

    typedef TimeUtilsT<> TimeUtils;

    namespace time
    {
        inline auto epoch() NOEXCEPT -> const time::ptime&
        {
            return TimeUtils::epoch();
        }

        inline auto durationSinceEpochUtc() NOEXCEPT -> time::time_duration
        {
            return time::second_clock::universal_time() - epoch();
        }

        inline auto durationSinceEpochUtcInSeconds() NOEXCEPT -> time::time_duration::sec_type
        {
            return durationSinceEpochUtc().total_seconds();
        }

        inline std::string createISOTimestamp()
        {
            return time::to_iso_string( time::second_clock::universal_time() );
        }

        inline std::string getLocalTimeISO( SAA_in const time::ptime utcTime )
        {
            if( utcTime.is_special() )
            {
                return std::string();
            }

            const auto localTime = c_local_adjustor< ptime >::utc_to_local( utcTime );
            const auto timeZoneOffset = localTime - utcTime;
            const auto fillChar = '0';

            cpp::SafeOutputStringStream out;

            out
                << to_iso_extended_string( localTime )
                << ( timeZoneOffset.is_negative() ? '-' : '+' )
                << std::setw( 2 )
                << std::setfill( fillChar )
                << std::abs( timeZoneOffset.hours() )
                << ':'
                << std::setw( 2 )
                << std::setfill( fillChar )
                << std::abs( timeZoneOffset.minutes() );

            return out.str();
        }

        inline std::string getCurrentLocalTimeISO()
        {
            return getLocalTimeISO( microsec_clock::universal_time() /* utcTime */ );
        }

        inline auto regexLocalTimeISO() NOEXCEPT -> const std::string&
        {
            return TimeUtils::regexLocalTimeISO();
        }
    }

} // bl

#endif /* __BL_TIMEUTILS_H_ */

