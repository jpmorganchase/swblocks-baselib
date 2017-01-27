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

#ifndef __BL_DATETIMEVALIDATIONUTILS_H_
#define __BL_DATETIMEVALIDATIONUTILS_H_

#include <baselib/core/StringUtils.h>
#include <baselib/core/Utils.h>

namespace bl
{
    namespace time
    {
        /**
         * @brief class DateTimeValidationUtils
         */

        template
        <
            typename E = void
        >
        class DateTimeValidationUtilsT FINAL
        {
            BL_DECLARE_STATIC( DateTimeValidationUtilsT )

        private:

            static const std::vector< std::string > g_dayOfWeekList;

        public:

            static void validateTime( SAA_in const std::string& timePart )
            {
                std::vector< std::string > timeValue;

                str::split( timeValue, timePart, bl::str::is_equal_to( ':' ) );

                if( timeValue.size() != 3 || timePart.length() != 8 )
                {
                    throwInvalidTimeError( timePart );
                }

                /*
                 * Upper limits of - HH  MM  SS
                 */

                const static std::size_t upperLimit[ 3 ] = { 23, 59, 59 };

                for( std::size_t i = 0; i < 3; ++i )
                {
                    try
                    {
                        if(
                            timeValue[ i ].empty() ||
                            utils::lexical_cast< std::size_t >( timeValue[ i ] ) > upperLimit[ i ] ||
                            timeValue[ i ].size() != 2U
                            )
                        {
                            throwInvalidTimeError( timePart );
                        }
                    }
                    catch( utils::bad_lexical_cast& )
                    {
                        throwInvalidTimeError( timePart );
                    }
                }
            }

            static time::ptime getDateTime( SAA_in const std::string& dateTime )
            {
                using namespace bl::time;

                auto facet = cpp::SafeUniquePtr< time_input_facet >::attach(
                    new time_input_facet( "%Y%m%d %H:%M:%S" )
                    );

                ptime time;
                cpp::SafeInputStringStream stream( dateTime );
                stream.imbue( std::locale( std::locale(), facet.release() ) );
                stream >> time;

                return time;
            }

            static time::date getDate( SAA_in const std::string& dateString )
            {
                using namespace bl::time;

                auto facet = cpp::SafeUniquePtr< date_input_facet >::attach(
                    new date_input_facet( "%Y%m%d" )
                    );

                date d;

                cpp::SafeInputStringStream stream( dateString );
                stream.imbue( std::locale( std::locale(), facet.release() ) );
                stream >> d;

                return d;
            }

            SAA_noreturn
            static void throwInvalidTimeError( SAA_in const std::string& timePart )
            {
                BL_THROW_USER(
                    BL_MSG()
                        << "Value '"
                        << timePart
                        << "' is out of range or not in valid time format. Should be in 'HH:MM:SS' format."
                    );
            }

            static void throwIfInvalidDateTime( SAA_in const bool condition, SAA_in const std::string& propertyValue )
            {
                BL_CHK_USER(
                    true,
                    condition,
                    BL_MSG()
                        << "Value '"
                        << propertyValue
                        << "' is out of range or not in valid datetime format. Should be in 'YYYYMMDD' or 'YYYYMMDD HH:MM:SS' format."
                    );
            }

            static void validateStartEndRange( SAA_in const std::string& propertyValue )
            {
                std::vector< std::string > dateTime;

                str::split( dateTime, propertyValue, bl::str::is_equal_to( ' ' ) );

                throwIfInvalidDateTime( dateTime[ 0 ].length() != 8, propertyValue );

                const auto time = getDateTime( propertyValue );

                throwIfInvalidDateTime( time.is_not_a_date_time(), propertyValue );

                /*
                 * ptime automatically converts time to valid format.
                 * Validate the time part separately.
                 */

                if( dateTime.size() >= 2 )
                {
                    validateTime( dateTime[ 1 ] );
                }
            }

            static void validateDayOfWeek( SAA_in const std::string& propertyValue )
            {
                std::vector< std::string > userDayOfWeek;

                str::split( userDayOfWeek, propertyValue, bl::str::is_equal_to( ',' ) );

                return validateDayOfWeek( userDayOfWeek );
            }

            static void validateDayOfWeek( SAA_in const std::vector< std::string >& days )
            {
                for( const auto& udw : days )
                {
                    bool foundDayOfWeek = false;

                    for( const auto& dw : g_dayOfWeekList )
                    {
                        if( str::iequals( udw, dw ) )
                        {
                            foundDayOfWeek = true;
                            break;
                        }
                    }

                    BL_CHK_USER(
                        false,
                        foundDayOfWeek,
                        BL_MSG()
                            << "Invalid day of week found '"
                            << str::to_lower_copy( udw )
                            << "'. Allowed values: "
                            << str::joinQuoteFormatted< std::string >( g_dayOfWeekList )
                        );
                }
            }

            static const std::string& getDayOfWeek( SAA_in const date& date )
            {
                return g_dayOfWeekList.at( date.day_of_week() );
            }

            static const std::string& getDayOfWeek( SAA_in const local_date_time& localDateTime )
            {
                return getDayOfWeek( localDateTime.local_time().date() );
            }
        };

        BL_DEFINE_STATIC_MEMBER( DateTimeValidationUtilsT, const std::vector< std::string >, g_dayOfWeekList ) =
        {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };

        typedef DateTimeValidationUtilsT<> DateTimeValidationUtils;

    } // time

} // bl

#endif /* __BL_DATETIMEVALIDATIONUTILS_H_ */
