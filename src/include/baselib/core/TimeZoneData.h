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

#ifndef __BL_TIMEZONEDATA_H_
#define __BL_TIMEZONEDATA_H_

#include <baselib/core/FsUtils.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/OS.h>

#include <unordered_map>

#if defined( _WIN32 )
#include <Winbase.h>
#else
#include <ctime>
#endif

namespace bl
{
    namespace time
    {
        /**
         * @brief class TimeZoneData
         */

        template
        <
            typename E = void
        >
        class TimeZoneDataT FINAL
        {
            BL_DECLARE_STATIC( TimeZoneDataT )

        private:

            static const char*                                          g_timeZoneData [];
            static const char*                                          g_windows2OlsonData [];

            static std::unordered_map< std::string, std::string >       g_timeZonePriorityMap;
            static std::unordered_map< std::string, std::string >       g_timeZoneDataMap;
            static std::unordered_map< std::string, std::string >       g_windows2OlsonDataMap;

            static const std::string                                    g_lastWeekOfMonth;

            static bool                                                 g_initialized;

        public:

            static void init()
            {
                if( g_initialized )
                {
                    return;
                }

                g_timeZonePriorityMap.clear();
                g_timeZoneDataMap.clear();
                g_windows2OlsonDataMap.clear();

                loadTimeZonePriorityMap();

                const char** timeZoneData = g_timeZoneData;

                for( std::size_t i = 0U; timeZoneData[ i ] != nullptr; ++i )
                {
                    std::vector< std::string > timeZoneDataFields;
                    str::split( timeZoneDataFields, timeZoneData[ i ], str::is_equal_to( ',' ) );

                    const std::string olsonName( timeZoneDataFields[ 0 ] );

                    BL_ASSERT( ! str::icontains( olsonName, " " ) );

                    g_timeZoneDataMap[ olsonName ] = timeZoneData[ i ];

                    const auto olsonNameWithSpaces = str::replace_all_copy( olsonName, "_", " " );

                    if( olsonNameWithSpaces != olsonName )
                    {
                        g_timeZoneDataMap[ olsonNameWithSpaces ] = timeZoneData[ i ];
                    }

                    if( g_timeZoneDataMap.find( timeZoneDataFields[ 1 ] ) == g_timeZoneDataMap.end() )
                    {
                        const auto priorityMapIterator = g_timeZonePriorityMap.find( timeZoneDataFields[ 1 ] );

                        if(
                            priorityMapIterator != g_timeZonePriorityMap.end() &&
                            olsonName != priorityMapIterator -> second
                            )
                        {
                            continue;
                        }

                        g_timeZoneDataMap[ timeZoneDataFields[ 1 ] ] = timeZoneData[ i ];

                        /*
                         * If DST ABBR is defined then create a key for that as well.
                         */

                        if( ! timeZoneDataFields[ 3 ].empty() )
                        {
                            g_timeZoneDataMap[ timeZoneDataFields[ 3 ] ] = timeZoneData[ i ];
                        }
                    }
                }

                for( std::size_t i = 0U; g_windows2OlsonData[ i ] != nullptr; ++i )
                {
                    std::vector< std::string > fields;
                    str::split( fields, g_windows2OlsonData[ i ], str::is_equal_to( ',' ) );

                    BL_ASSERT( fields.size() == 2 );
                    BL_ASSERT( ! fields[ 0 ].empty() );
                    BL_ASSERT( ! fields[ 1 ].empty() );

                    g_windows2OlsonDataMap.emplace( std::move( fields[ 0 ] ), std::move( fields[ 1 ] ) );
                }

                g_initialized = true;
            }

            static void loadTimeZonePriorityMap()
            {
                /*
                 * Few cities use the same timezone STD abbrevation.
                 * For such cities we need to prioritize which cities to use
                 * based on where servers are located. For others we can just
                 * pick the first entry.
                 */

                g_timeZonePriorityMap[ "CET" ] = "Europe/Paris";
                g_timeZonePriorityMap[ "CST" ] = "America/Chicago";
                g_timeZonePriorityMap[ "GMT" ] = "Europe/London";
                g_timeZonePriorityMap[ "UTC" ] = "Etc/GMT";
                g_timeZonePriorityMap[ "EST" ] = "America/New_York";
                g_timeZonePriorityMap[ "GST" ] = "Asia/Dubai";
                g_timeZonePriorityMap[ "IST" ] = "Asia/Calcutta";
                g_timeZonePriorityMap[ "MST" ] = "America/Denver";
                g_timeZonePriorityMap[ "PST" ] = "America/Los_Angeles";
            }

            static const std::unordered_map< std::string, std::string >& getTimeZoneDataMap()
            {
                return g_timeZoneDataMap;
            }

            static std::vector< std::string > getTimeZoneDataFields( SAA_in const std::string& timeZone )
            {
                const auto timeZoneMapIterator = g_timeZoneDataMap.find( timeZone );

                BL_CHK_T_USER_FRIENDLY(
                    true,
                    timeZoneMapIterator == g_timeZoneDataMap.end(),
                    UnexpectedException(),
                    BL_MSG()
                        << "Unable to find timezone '"
                        << timeZone
                        << "' in database."
                    );

                std::vector< std::string > timeZoneDataFields;

                str::split( timeZoneDataFields, timeZoneMapIterator -> second, str::is_equal_to( ',' ) );

                return timeZoneDataFields;
            }

            static std::string getTimeZoneOffset( SAA_in const std::string& timeZone )
            {
                const auto timeZoneDataFields = getTimeZoneDataFields( timeZone );

                /*
                 * If timezone does not use Daylight saving then just return GMTOffset
                 */

                if ( timeZoneDataFields[ 3 ].empty() )
                {
                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        timeZoneDataFields.size() >= 6,
                        UnexpectedException(),
                        BL_MSG()
                            << "Insufficient information found for timezone '"
                            << timeZoneDataFields[ 1 ]
                            << "' in order to derive timezone offset."
                        );

                    return timeZoneDataFields[ 1 ] + timeZoneDataFields [ 5 ];
                }
                else
                {
                    /*
                     * timeZoneOffset example - "CST-06:00:00CDT+01:00:00,M3.2.0/02:00:00,M11.1.0/02:00:00"
                     */

                    BL_CHK_T_USER_FRIENDLY(
                        false,
                        timeZoneDataFields.size() == 11,
                        UnexpectedException(),
                        BL_MSG()
                          << "Insufficient information found for timezone '"
                          << timeZoneDataFields[ 1 ]
                          << "' in order to derive timezone offset."
                        );

                    cpp::SafeOutputStringStream timeZoneOffset;

                    timeZoneOffset << timeZoneDataFields[ 1 ] << timeZoneDataFields [ 5 ] << timeZoneDataFields[ 3 ]
                                   << timeZoneDataFields[ 6 ] << ",M" << getDSTStartEndRule( timeZoneDataFields [ 7 ] )
                                   << "/" << timeZoneDataFields [ 8 ] << ",M" + getDSTStartEndRule( timeZoneDataFields [ 9 ] )
                                   << "/" << timeZoneDataFields [ 10 ];

                    return timeZoneOffset.str();
                }
            }

            static bool validateTimeZone( SAA_in const std::string& timeZone )
            {
                return g_timeZoneDataMap.find( timeZone ) != g_timeZoneDataMap.end();
            }

            static std::string getDSTStartEndRule( SAA_in const std::string& dstRule )
            {
                /*
                 * Example - IN: -1;5;4
                 *           Week( -1 = Last week of month ); Day of week; Month
                 *
                 *           OUT: 4.5.5
                 *           Month; Week( 5 is always considered last week of any month ); Day
                 */

                std::vector< std::string > dstRuleData;

                str::split( dstRuleData, dstRule, str::is_equal_to( ';' ) );

                BL_CHK_T_USER_FRIENDLY(
                    false,
                    dstRuleData.size() >= 3,
                    UnexpectedException(),
                    BL_MSG()
                      << "Insufficient information found in DST rule '"
                      << dstRule
                      << "'"
                    );

                const auto& week = dstRuleData[ 0 ] == "-1" ? g_lastWeekOfMonth : dstRuleData[ 0 ];

                return dstRuleData[ 2 ] + '.' + week + '.' + dstRuleData[ 1 ];
            }

            static const std::string& getOlsonFromWindows( SAA_in const std::string& windowsTimeZoneName )
            {
                const auto i = g_windows2OlsonDataMap.find( windowsTimeZoneName );

                return i == g_windows2OlsonDataMap.end() ? str::empty() : i -> second;
            }

            static std::string getDefaultTimeZone()
            {
                #if defined( _WIN32 )

                ::TIME_ZONE_INFORMATION tzi;

                if( TIME_ZONE_ID_INVALID == ::GetTimeZoneInformation( &tzi ) )
                {
                    BL_THROW_EC(
                        eh::error_code( ( int )::GetLastError(), eh::system_category() ),
                        BL_MSG()
                            << "Cannot obtain time zone information"
                        );
                }

                cpp::wstring_convert_t conv;

                auto olsonTimeZone = getOlsonFromWindows( conv.to_bytes( tzi.StandardName ) );

                if( olsonTimeZone.empty() )
                {
                    olsonTimeZone = getOlsonFromWindows( conv.to_bytes( tzi.DaylightName ) );
                }

                if( ! olsonTimeZone.empty() )
                {
                    return olsonTimeZone;
                }

                #else // defined( _WIN32 )

                /*
                 * Check TZ environment variable
                 */

                const auto tzEnvVar = os::tryGetEnvironmentVariable( "TZ" );

                if( tzEnvVar )
                {
                    if( validateTimeZone( *tzEnvVar ) )
                    {
                        return *tzEnvVar;
                    }
                    else
                    {
                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "Unable to determine machine time zone from TZ env variable: "
                                << *tzEnvVar
                                << ", it should be either in Olson or abbreviated form"
                          );
                    }
                }

                std::string fileName;

                /*
                 * Check /etc/timezone which should be present for Ubuntu
                 */

                if( fs::path_exists( "/etc/timezone" ) )
                {
                    fs::SafeInputFileStreamWrapper file( "/etc/timezone" );
                    auto& is = file.stream();

                    std::string timeZone;

                    if( std::getline( is, timeZone ) )
                    {
                        if( validateTimeZone( timeZone ) )
                        {
                            return timeZone;
                        }
                        else
                        {
                            BL_LOG(
                                Logging::warning(),
                                BL_MSG()
                                    << "Unable to determine machine time zone from '/etc/timezone' file: "
                                    << timeZone
                                    << ", it should be either in Olson or abbreviated form"
                              );
                        }
                    }
                }

                /*
                 * Check /etc/sysconfig/clock which should be present for RedHat
                 */

                if( fs::path_exists( "/etc/sysconfig/clock" ) )
                {
                    fs::SafeInputFileStreamWrapper file( "/etc/sysconfig/clock" );
                    auto& is = file.stream();

                    if( is )
                    {
                        std::string line;

                        while( std::getline( is, line ) )
                        {
                            str::trim( line );

                            if( line.find( "ZONE" ) == 0 )
                            {
                                std::vector< std::string > zoneDefFields;
                                str::split( zoneDefFields, line, str::is_equal_to( '"' ) );

                                if( validateTimeZone( zoneDefFields[ 1 ] ) )
                                {
                                    return zoneDefFields[ 1 ];
                                }
                                else
                                {
                                    BL_LOG(
                                        Logging::warning(),
                                        BL_MSG()
                                            << "Unable to determine machine time zone from '/etc/sysconfig/clock' file: "
                                            << zoneDefFields[ 1 ]
                                            << ", it should be either in Olson or abbreviated form"
                                      );
                                }

                                break;
                            }
                        }
                    }
                }

                /*
                 * Fallback to localtime_r call which should return time zone abbreviation
                 */

                auto tm = time::to_tm( time::second_clock::universal_time() );

                auto t = ::mktime( &tm );

                struct tm tms;

                ::localtime_r( &t, &tms );

                if( validateTimeZone( tms.tm_zone ) )
                {
                    return tms.tm_zone;
                }

                #endif // defined( _WIN32 )

                BL_THROW_USER(
                    BL_MSG()
                        << "Cannot determine local time zone"
                    );
            }
        };

        template
        <
            typename E
        >
        std::unordered_map< std::string, std::string >
        TimeZoneDataT< E >::g_timeZonePriorityMap;

        template
        <
            typename E
        >
        std::unordered_map< std::string, std::string >
        TimeZoneDataT< E >::g_timeZoneDataMap;

        template
        <
            typename E
        >
        std::unordered_map< std::string, std::string >
        TimeZoneDataT< E >::g_windows2OlsonDataMap;

        template
        <
            typename E
        >
        const std::string
        TimeZoneDataT< E >::g_lastWeekOfMonth( "5" );

        template
        <
            typename E
        >
        bool TimeZoneDataT< E >::g_initialized( false );

        template
        <
            typename E
        >
        const char*
        TimeZoneDataT< E >::g_timeZoneData[] =
        {
            /*
             * America/Bahia and America/Santa_Isabel were not originally present in Boost 1.53 list of time zones.
             * They were added manually to match relevant Windows time zones.
             */

            /*
             * The artificial UTC timezone has been added to support Linux as some installations
             * default to it. Its Olson name is Etc/GMT to match the Windows conversion table.
             */

            /*
             * Olson time zone name cannot contain spaces, use only '_' character as words separator!!!
             * The map population procedure adds the row with original key as well as another one
             * with spaces instead of '_'.
             */

            //"ID,STD ABBR,STD NAME,DST ABBR,DST NAME,GMT offset,DST adjustment,DST Start Date rule,Start time,DST End date rule,End time"
            "Africa/Abidjan,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Accra,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Addis_Ababa,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Algiers,CET,CET,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Asmera,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Bamako,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Bangui,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Banjul,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Bissau,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Blantyre,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Brazzaville,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Bujumbura,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Cairo,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;5;4,+00:00:00,-1;5;9,+00:00:00",
            "Africa/Casablanca,WET,WET,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Ceuta,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Africa/Conakry,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Dakar,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Dar_es_Salaam,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Djibouti,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Douala,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/El_Aaiun,WET,WET,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Freetown,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Gaborone,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Harare,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Johannesburg,SAST,SAST,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Kampala,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Khartoum,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Kigali,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Kinshasa,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Lagos,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Libreville,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Lome,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Luanda,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Lubumbashi,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Lusaka,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Malabo,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Maputo,CAT,CAT,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Maseru,SAST,SAST,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Mbabane,SAST,SAST,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Mogadishu,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Monrovia,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Nairobi,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Ndjamena,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Niamey,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Nouakchott,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Ouagadougou,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Porto-Novo,WAT,WAT,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Sao_Tome,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Timbuktu,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Tripoli,EET,EET,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Tunis,CET,CET,,,+01:00:00,+00:00:00,,,,+00:00:00",
            "Africa/Windhoek,WAT,WAT,WAST,WAST,+01:00:00,+01:00:00,1;0;9,+02:00:00,1;0;4,+02:00:00",
            "America/Adak,HAST,HAST,HADT,HADT,-10:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Anchorage,AKST,AKST,AKDT,AKDT,-09:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Anguilla,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Antigua,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Araguaina,BRT,BRT,BRST,BRST,-03:00:00,+01:00:00,2;0;10,+00:00:00,3;0;2,+00:00:00",
            "America/Aruba,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Asuncion,PYT,PYT,PYST,PYST,-04:00:00,+01:00:00,1;0;10,+00:00:00,1;0;3,+00:00:00",
            "America/Bahia,BRT,BRT,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Barbados,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Belem,BRT,BRT,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Belize,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Boa_Vista,AMT,AMT,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Bogota,COT,COT,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Boise,MST,MST,MDT,MDT,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Buenos_Aires,ART,ART,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Cambridge_Bay,MST,MST,MDT,MDT,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Cancun,CST,CST,CDT,CDT,-06:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Caracas,VET,VET,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Catamarca,ART,ART,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Cayenne,GFT,GFT,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Cayman,EST,EST,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Chicago,CST,Central Standard Time,CDT,Central Daylight Time,-06:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Chihuahua,MST,MST,MDT,MDT,-07:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Cordoba,ART,ART,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Costa_Rica,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Cuiaba,AMT,AMT,AMST,AMST,-04:00:00,+01:00:00,2;0;10,+00:00:00,3;0;2,+00:00:00",
            "America/Curacao,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Danmarkshavn,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "America/Dawson,PST,PST,PDT,PDT,-08:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Dawson_Creek,MST,MST,,,-07:00:00,+00:00:00,,,,+00:00:00",
            "America/Denver,MST,Mountain Standard Time,MDT,Mountain Daylight Time,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Detroit,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Dominica,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Edmonton,MST,MST,MDT,MDT,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Eirunepe,ACT,ACT,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/El_Salvador,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Fortaleza,BRT,BRT,BRST,BRST,-03:00:00,+01:00:00,2;0;10,+00:00:00,3;0;2,+00:00:00",
            "America/Glace_Bay,AST,AST,ADT,ADT,-04:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Godthab,WGT,WGT,WGST,WGST,-03:00:00,+01:00:00,-1;6;3,+22:00:00,-1;6;10,+23:00:00",
            "America/Goose_Bay,AST,AST,ADT,ADT,-04:00:00,+01:00:00,1;0;4,+00:01:00,-1;0;10,+00:01:00",
            "America/Grand_Turk,EST,EST,EDT,EDT,-05:00:00,+01:00:00,1;0;4,+00:00:00,-1;0;10,+00:00:00",
            "America/Grenada,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Guadeloupe,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Guatemala,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Guayaquil,ECT,ECT,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Guyana,GYT,GYT,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Halifax,AST,AST,ADT,ADT,-04:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Havana,CST,CST,CDT,CDT,-05:00:00,+01:00:00,1;0;4,+00:00:00,-1;0;10,+01:00:00",
            "America/Hermosillo,MST,MST,,,-07:00:00,+00:00:00,,,,+00:00:00",
            "America/Indiana/Indianapolis,EST,EST,,,-05:00:00,+00:00:00,2;0;3,,1;0;11,+00:00:00",
            "America/Indiana/Knox,EST,EST,,,-05:00:00,+00:00:00,2;0;3,,1;0;11,+00:00:00",
            "America/Indiana/Marengo,EST,EST,,,-05:00:00,+00:00:00,2;0;3,,1;0;11,+00:00:00",
            "America/Indiana/Vevay,EST,EST,,,-05:00:00,+00:00:00,2;0;3,,1;0;11,+00:00:00",
            "America/Indianapolis,EST,EST,,,-05:00:00,+00:00:00,2;0;3,,1;0;11,+00:00:00",
            "America/Inuvik,MST,MST,MDT,MDT,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Iqaluit,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Jamaica,EST,EST,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Jujuy,ART,ART,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Juneau,AKST,AKST,AKDT,AKDT,-09:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Kentucky/Louisville,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Kentucky/Monticello,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/La_Paz,BOT,BOT,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Lima,PET,PET,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Los_Angeles,PST,Pacific Standard Time,PDT,Pacific Daylight Time,-08:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Louisville,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Maceio,BRT,BRT,BRST,BRST,-03:00:00,+01:00:00,2;0;10,+00:00:00,3;0;2,+00:00:00",
            "America/Managua,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Manaus,AMT,AMT,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Martinique,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Mazatlan,MST,MST,MDT,MDT,-07:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Mendoza,ART,ART,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Menominee,CST,CST,CDT,CDT,-06:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Merida,CST,CST,CDT,CDT,-06:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Mexico_City,CST,CST,CDT,CDT,-06:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Miquelon,PMST,PMST,PMDT,PMDT,-03:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Monterrey,CST,CST,CDT,CDT,-06:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Montevideo,UYT,UYT,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Montreal,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Montserrat,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Nassau,EST,EST,EDT,EDT,-05:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/New_York,EST,Eastern Standard Time,EDT,Eastern Daylight Time,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Nipigon,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Nome,AKST,AKST,AKDT,AKDT,-09:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Noronha,FNT,FNT,,,-02:00:00,+00:00:00,,,,+00:00:00",
            "America/North_Dakota/Center,CST,CST,CDT,CDT,-06:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Panama,EST,EST,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Pangnirtung,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Paramaribo,SRT,SRT,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Phoenix,MST,Mountain Standard Time,,,-07:00:00,+00:00:00,,,,+00:00:00",
            "America/Port-au-Prince,EST,EST,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Port_of_Spain,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Porto_Velho,AMT,AMT,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Puerto_Rico,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Rainy_River,CST,CST,CDT,CDT,-06:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Rankin_Inlet,CST,CST,CDT,CDT,-06:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Recife,BRT,BRT,BRST,BRST,-03:00:00,+01:00:00,2;0;10,+00:00:00,3;0;2,+00:00:00",
            "America/Regina,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Rio_Branco,ACT,ACT,,,-05:00:00,+00:00:00,,,,+00:00:00",
            "America/Rosario,ART,ART,,,-03:00:00,+00:00:00,,,,+00:00:00",
            "America/Santa_Isabel,PST,PST,PDT,PDT,-08:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Santiago,CLT,CLT,CLST,CLST,-04:00:00,+01:00:00,2;0;10,+00:00:00,2;0;3,+00:00:00",
            "America/Santo_Domingo,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Sao_Paulo,BRT,BRT,BRST,BRST,-03:00:00,+01:00:00,2;0;10,+00:00:00,3;0;2,+00:00:00",
            "America/Scoresbysund,EGT,EGT,EGST,EGST,-01:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+01:00:00",
            "America/Shiprock,MST,MST,MDT,MDT,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/St_Johns,NST,NST,NDT,NDT,-03:30:00,+01:00:00,1;0;4,+00:01:00,-1;0;10,+00:01:00",
            "America/St_Kitts,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/St_Lucia,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/St_Thomas,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/St_Vincent,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Swift_Current,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Tegucigalpa,CST,CST,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "America/Thule,AST,AST,,,-04:00:00,+00:00:00,2;0;3,,1;0;11,+00:00:00",
            "America/Thunder_Bay,EST,EST,EDT,EDT,-05:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Tijuana,PST,PST,PDT,PDT,-08:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "America/Tortola,AST,AST,,,-04:00:00,+00:00:00,,,,+00:00:00",
            "America/Vancouver,PST,PST,PDT,PDT,-08:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Whitehorse,PST,PST,PDT,PDT,-08:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Winnipeg,CST,CST,CDT,CDT,-06:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+03:00:00",
            "America/Yakutat,AKST,AKST,AKDT,AKDT,-09:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "America/Yellowknife,MST,MST,MDT,MDT,-07:00:00,+01:00:00,2;0;3,+02:00:00,1;0;11,+02:00:00",
            "Antarctica/Casey,WST,WST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Antarctica/Davis,DAVT,DAVT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Antarctica/DumontDUrville,DDUT,DDUT,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Antarctica/Mawson,MAWT,MAWT,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Antarctica/McMurdo,NZST,NZST,NZDT,NZDT,+12:00:00,+01:00:00,1;0;10,+02:00:00,3;0;3,+03:00:00",
            "Antarctica/Palmer,CLT,CLT,CLST,CLST,-04:00:00,+01:00:00,2;0;10,+00:00:00,2;0;3,+00:00:00",
            "Antarctica/South_Pole,NZST,NZST,NZDT,NZDT,+12:00:00,+01:00:00,1;0;10,+02:00:00,3;0;3,+03:00:00",
            "Antarctica/Syowa,SYOT,SYOT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Antarctica/Vostok,VOST,VOST,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Arctic/Longyearbyen,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Aden,AST,AST,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Almaty,ALMT,ALMT,ALMST,ALMST,+06:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+00:00:00",
            "Asia/Amman,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;4;3,+00:00:00,-1;4;9,+01:00:00",
            "Asia/Anadyr,ANAT,ANAT,ANAST,ANAST,+12:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Aqtau,AQTT,AQTT,AQTST,AQTST,+04:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+00:00:00",
            "Asia/Aqtobe,AQTT,AQTT,AQTST,AQTST,+05:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+00:00:00",
            "Asia/Ashgabat,TMT,TMT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Baghdad,AST,AST,ADT,ADT,+03:00:00,+01:00:00,1;0;4,+03:00:00,1;0;10,+04:00:00",
            "Asia/Bahrain,AST,AST,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Baku,AZT,AZT,AZST,AZST,+04:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+01:00:00",
            "Asia/Bangkok,ICT,ICT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Beirut,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+00:00:00",
            "Asia/Bishkek,KGT,KGT,KGST,KGST,+05:00:00,+01:00:00,-1;0;3,+02:30:00,-1;0;10,+02:30:00",
            "Asia/Brunei,BNT,BNT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Calcutta,IST,IST,,,+05:30:00,+00:00:00,,,,+00:00:00",
            "Asia/Choibalsan,CHOT,CHOT,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Chongqing,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Colombo,LKT,LKT,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Damascus,EET,EET,EEST,EEST,+02:00:00,+01:00:00,1;0;4,+00:00:00,1;0;10,+00:00:00",
            "Asia/Dhaka,BDT,BDT,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Dili,TPT,TPT,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Dubai,GST,GST,,,+04:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Dushanbe,TJT,TJT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Gaza,EET,EET,EEST,EEST,+02:00:00,+01:00:00,3;5;4,+00:00:00,3;5;10,+00:00:00",
            "Asia/Harbin,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Hong_Kong,HKT,HKT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Hovd,HOVT,HOVT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Irkutsk,IRKT,IRKT,IRKST,IRKST,+08:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Istanbul,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Asia/Jakarta,WIT,WIT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Jayapura,EIT,EIT,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Jerusalem,IST,IST,IDT,IDT,+02:00:00,+01:00:00,1;0;4,+01:00:00,1;0;10,+01:00:00",
            "Asia/Kabul,AFT,AFT,,,+04:30:00,+00:00:00,,,,+00:00:00",
            "Asia/Kamchatka,PETT,PETT,PETST,PETST,+12:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Karachi,PKT,PKT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Kashgar,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Katmandu,NPT,NPT,,,+05:45:00,+00:00:00,,,,+00:00:00",
            "Asia/Krasnoyarsk,KRAT,KRAT,KRAST,KRAST,+07:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Kuala_Lumpur,MYT,MYT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Kuching,MYT,MYT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Kuwait,AST,AST,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Macao,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Macau,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Magadan,MAGT,MAGT,MAGST,MAGST,+11:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Makassar,CIT,CIT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Manila,PHT,PHT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Muscat,GST,GST,,,+04:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Nicosia,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Asia/Novosibirsk,NOVT,NOVT,NOVST,NOVST,+06:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Omsk,OMST,OMST,OMSST,OMSST,+06:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Oral,WST,WST,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Phnom_Penh,ICT,ICT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Pontianak,WIT,WIT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Pyongyang,KST,KST,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Qyzylorda,KST,KST,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Qatar,AST,AST,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Rangoon,MMT,MMT,,,+06:30:00,+00:00:00,,,,+00:00:00",
            "Asia/Riyadh,AST,AST,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Saigon,ICT,ICT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Sakhalin,SAKT,SAKT,SAKST,SAKST,+10:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Samarkand,UZT,UZT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Seoul,KST,KST,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Shanghai,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Singapore,SGT,SGT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Taipei,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Tashkent,UZT,UZT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Tbilisi,GET,GET,GEST,GEST,+04:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+00:00:00",
            "Asia/Tehran,IRT,IRT,,,+03:30:00,+00:00:00,,,,+00:00:00",
            "Asia/Thimphu,BTT,BTT,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Tokyo,JST,JST,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Ujung_Pandang,CIT,CIT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Ulaanbaatar,ULAT,ULAT,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Urumqi,CST,CST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Vientiane,ICT,ICT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Asia/Vladivostok,VLAT,VLAT,VLAST,VLAST,+10:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Yakutsk,YAKT,YAKT,YAKST,YAKST,+09:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Yekaterinburg,YEKT,YEKT,YEKST,YEKST,+05:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Asia/Yerevan,AMT,AMT,AMST,AMST,+04:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Atlantic/Azores,AZOT,AZOT,AZOST,AZOST,-01:00:00,+01:00:00,-1;0;3,+00:00:00,-1;0;10,+01:00:00",
            "Atlantic/Bermuda,AST,AST,ADT,ADT,-04:00:00,+01:00:00,1;0;4,+02:00:00,-1;0;10,+02:00:00",
            "Atlantic/Canary,WET,WET,WEST,WEST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Atlantic/Cape_Verde,CVT,CVT,,,-01:00:00,+00:00:00,,,,+00:00:00",
            "Atlantic/Faeroe,WET,WET,WEST,WEST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Atlantic/Jan_Mayen,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Atlantic/Madeira,WET,WET,WEST,WEST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Atlantic/Reykjavik,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Atlantic/South_Georgia,GST,GST,,,-02:00:00,+00:00:00,,,,+00:00:00",
            "Atlantic/St_Helena,GMT,GMT,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Atlantic/Stanley,FKT,FKT,FKST,FKST,-04:00:00,+01:00:00,1;0;9,+02:00:00,3;0;4,+02:00:00",
            "Australia/Adelaide,CST,CST,CST,CST,+09:30:00,+01:00:00,1;0;10,+02:00:00,1;0;4,+03:00:00",
            "Australia/Brisbane,EST,EST,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Australia/Broken_Hill,CST,CST,CST,CST,+09:30:00,+01:00:00,1;0;10,+02:00:00,1;0;4,+03:00:00",
            "Australia/Darwin,CST,CST,,,+09:30:00,+00:00:00,,,,+00:00:00",
            "Australia/Eucla,CWST,CWST,,,+08:45:00,+00:00:00,,,,+00:00:00",
            "Australia/Hobart,EST,EST,EST,EST,+10:00:00,+01:00:00,1;0;10,+02:00:00,1;0;4,+03:00:00",
            "Australia/Lindeman,EST,EST,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Australia/Lord_Howe,LHST,LHST,LHST,LHST,+10:30:00,+00:30:00,1;0;10,+02:00:00,1;0;4,+02:30:00",
            "Australia/Melbourne,EST,EST,EST,EST,+10:00:00,+01:00:00,1;0;10,+02:00:00,1;0;4,+03:00:00",
            "Australia/Perth,AWST,AWST,,,+08:00:00,+00:00:00,,,,+00:00:00",
            "Australia/Sydney,AEST,AEST,AEDT,AEDT,+10:00:00,+01:00:00,1;0;10,+02:00:00,1;0;4,+03:00:00",
            "Etc/GMT,UTC,UTC,,,+00:00:00,+00:00:00,,,,+00:00:00",
            "Europe/Amsterdam,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Andorra,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Athens,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Belfast,GMT,GMT,BST,BST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Europe/Belgrade,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Berlin,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Bratislava,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Brussels,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Bucharest,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Budapest,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Chisinau,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Copenhagen,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Dublin,GMT,GMT,IST,IST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Europe/Gibraltar,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Helsinki,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Istanbul,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Kaliningrad,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Kiev,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Lisbon,WET,WET,WEST,WEST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Europe/Ljubljana,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/London,GMT,GMT,BST,BST,+00:00:00,+01:00:00,-1;0;3,+01:00:00,-1;0;10,+02:00:00",
            "Europe/Luxembourg,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Madrid,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Malta,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Minsk,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Monaco,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Moscow,MSK,MSK,MSD,MSD,+03:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Nicosia,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Oslo,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Paris,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Prague,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Riga,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Rome,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Samara,SAMT,SAMT,SAMST,SAMST,+04:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/San_Marino,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Sarajevo,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Simferopol,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Skopje,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Sofia,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Stockholm,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Tallinn,EET,EET,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Europe/Tirane,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Uzhgorod,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Vaduz,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Vatican,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Vienna,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Vilnius,EET,EET,,,+02:00:00,+00:00:00,,,,+00:00:00",
            "Europe/Warsaw,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Zagreb,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Europe/Zaporozhye,EET,EET,EEST,EEST,+02:00:00,+01:00:00,-1;0;3,+03:00:00,-1;0;10,+04:00:00",
            "Europe/Zurich,CET,CET,CEST,CEST,+01:00:00,+01:00:00,-1;0;3,+02:00:00,-1;0;10,+03:00:00",
            "Indian/Antananarivo,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Chagos,IOT,IOT,,,+06:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Christmas,CXT,CXT,,,+07:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Cocos,CCT,CCT,,,+06:30:00,+00:00:00,,,,+00:00:00",
            "Indian/Comoro,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Kerguelen,TFT,TFT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Mahe,SCT,SCT,,,+04:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Maldives,MVT,MVT,,,+05:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Mauritius,MUT,MUT,,,+04:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Mayotte,EAT,EAT,,,+03:00:00,+00:00:00,,,,+00:00:00",
            "Indian/Reunion,RET,RET,,,+04:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Apia,WST,WST,,,-11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Auckland,NZST,NZST,NZDT,NZDT,+12:00:00,+01:00:00,1;0;10,+02:00:00,3;0;3,+03:00:00",
            "Pacific/Chatham,CHAST,CHAST,CHADT,CHADT,+12:45:00,+01:00:00,1;0;10,+02:45:00,3;0;3,+03:45:00",
            "Pacific/Easter,EAST,EAST,EASST,EASST,-06:00:00,+01:00:00,2;6;10,+22:00:00,2;6;3,+22:00:00",
            "Pacific/Efate,VUT,VUT,,,+11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Enderbury,PHOT,PHOT,,,+13:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Fakaofo,TKT,TKT,,,-10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Fiji,FJT,FJT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Funafuti,TVT,TVT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Galapagos,GALT,GALT,,,-06:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Gambier,GAMT,GAMT,,,-09:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Guadalcanal,SBT,SBT,,,+11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Guam,ChST,ChST,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Honolulu,HST,HST,,,-10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Johnston,HST,HST,,,-10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Kiritimati,LINT,LINT,,,+14:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Kosrae,KOST,KOST,,,+11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Kwajalein,MHT,MHT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Majuro,MHT,MHT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Marquesas,MART,MART,,,-09:30:00,+00:00:00,,,,+00:00:00",
            "Pacific/Midway,SST,SST,,,-11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Nauru,NRT,NRT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Niue,NUT,NUT,,,-11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Norfolk,NFT,NFT,,,+11:30:00,+00:00:00,,,,+00:00:00",
            "Pacific/Noumea,NCT,NCT,,,+11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Pago_Pago,SST,SST,,,-11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Palau,PWT,PWT,,,+09:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Pitcairn,PST,PST,,,-08:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Ponape,PONT,PONT,,,+11:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Port_Moresby,PGT,PGT,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Rarotonga,CKT,CKT,,,-10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Saipan,ChST,ChST,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Tahiti,TAHT,TAHT,,,-10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Tarawa,GILT,GILT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Tongatapu,TOT,TOT,,,+13:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Truk,TRUT,TRUT,,,+10:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Wake,WAKT,WAKT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Wallis,WFT,WFT,,,+12:00:00,+00:00:00,,,,+00:00:00",
            "Pacific/Yap,YAPT,YAPT,,,+10:00:00,+00:00:00,,,,+00:00:00",
            nullptr
        };

        template
        <
            typename E
        >
        const char*
        TimeZoneDataT< E >::g_windows2OlsonData[] =
        {
            "Afghanistan Standard Time,Asia/Kabul",
            "Alaskan Standard Time,America/Anchorage",
            "Arabian Standard Time,Asia/Dubai",
            "Arabic Standard Time,Asia/Baghdad",
            "Arab Standard Time,Asia/Riyadh",
            "Argentina Standard Time,America/Buenos_Aires",
            "Atlantic Standard Time,America/Halifax",
            "AUS Central Standard Time,Australia/Darwin",
            "AUS Eastern Standard Time,Australia/Sydney",
            "Azerbaijan Standard Time,Asia/Baku",
            "Azores Standard Time,Atlantic/Azores",
            "Bahia Standard Time,America/Bahia",
            "Bangladesh Standard Time,Asia/Dhaka",
            "Canada Central Standard Time,America/Regina",
            "Cape Verde Standard Time,Atlantic/Cape_Verde",
            "Caucasus Standard Time,Asia/Yerevan",
            "Cen. Australia Standard Time,Australia/Adelaide",
            "Central America Standard Time,America/Guatemala",
            "Central Asia Standard Time,Asia/Almaty",
            "Central Brazilian Standard Time,America/Cuiaba",
            "Central European Standard Time,Europe/Warsaw",
            "Central Europe Standard Time,Europe/Budapest",
            "Central Pacific Standard Time,Pacific/Guadalcanal",
            "Central Standard Time,America/Chicago",
            "Central Standard Time (Mexico),America/Mexico_City",
            "China Standard Time,Asia/Shanghai",
            "Dateline Standard Time,Etc/GMT+12",
            "E. Africa Standard Time,Africa/Nairobi",
            "Eastern Standard Time,America/New_York",
            "E. Australia Standard Time,Australia/Brisbane",
            "E. Europe Standard Time,Europe/Bucharest",
            "Egypt Standard Time,Africa/Cairo",
            "Ekaterinburg Standard Time,Asia/Yekaterinburg",
            "E. South America Standard Time,America/Sao_Paulo",
            "Fiji Standard Time,Pacific/Fiji",
            "FLE Standard Time,Europe/Kiev",
            "Georgian Standard Time,Asia/Tbilisi",
            "GMT Standard Time,Europe/London",
            "Greenland Standard Time,America/Godthab",
            "Greenwich Standard Time,Atlantic/Reykjavik",
            "GTB Standard Time,Europe/Bucharest",
            "Hawaiian Standard Time,Pacific/Honolulu",
            "India Standard Time,Asia/Calcutta",
            "Iran Standard Time,Asia/Tehran",
            "Israel Standard Time,Asia/Jerusalem",
            "Jordan Standard Time,Asia/Amman",
            "Kaliningrad Standard Time,Europe/Kaliningrad",
            "Kamchatka Standard Time,Asia/Kamchatka",
            "Korea Standard Time,Asia/Seoul",
            "Libya Standard Time,Africa/Tripoli",
            "Magadan Standard Time,Asia/Magadan",
            "Malay Peninsula Standard Time,Asia/Singapore",
            "Mauritius Standard Time,Indian/Mauritius",
            "Mid-Atlantic Standard Time,Atlantic/South_Georgia",
            "Middle East Standard Time,Asia/Beirut",
            "Montevideo Standard Time,America/Montevideo",
            "Morocco Standard Time,Africa/Casablanca",
            "Mountain Standard Time,America/Denver",
            "Mountain Standard Time (Mexico),America/Chihuahua",
            "Myanmar Standard Time,Asia/Rangoon",
            "Namibia Standard Time,Africa/Windhoek",
            "N. Central Asia Standard Time,Asia/Novosibirsk",
            "Nepal Standard Time,Asia/Katmandu",
            "Newfoundland Standard Time,America/St_Johns",
            "New Zealand Standard Time,Pacific/Auckland",
            "North Asia East Standard Time,Asia/Irkutsk",
            "North Asia Standard Time,Asia/Krasnoyarsk",
            "Pacific SA Standard Time,America/Santiago",
            "Pacific Standard Time,America/Los_Angeles",
            "Pacific Standard Time (Mexico),America/Santa_Isabel",
            "Pakistan Standard Time,Asia/Karachi",
            "Paraguay Standard Time,America/Asuncion",
            "Romance Standard Time,Europe/Paris",
            "Russian Standard Time,Europe/Moscow",
            "SA Eastern Standard Time,America/Cayenne",
            "Samoa Standard Time,Pacific/Apia",
            "SA Pacific Standard Time,America/Bogota",
            "SA Western Standard Time,America/La_Paz",
            "SE Asia Standard Time,Asia/Bangkok",
            "South Africa Standard Time,Africa/Johannesburg",
            "Sri Lanka Standard Time,Asia/Colombo",
            "Syria Standard Time,Asia/Damascus",
            "Taipei Standard Time,Asia/Taipei",
            "Tasmania Standard Time,Australia/Hobart",
            "Tokyo Standard Time,Asia/Tokyo",
            "Tonga Standard Time,Pacific/Tongatapu",
            "Turkey Standard Time,Europe/Istanbul",
            "Ulaanbaatar Standard Time,Asia/Ulaanbaatar",
            "US Eastern Standard Time,America/Indianapolis",
            "US Mountain Standard Time,America/Phoenix",
            "UTC-02,Etc/GMT+2",
            "UTC-11,Etc/GMT+11",
            "UTC+12,Etc/GMT-12",
            "UTC,Etc/GMT",
            "Venezuela Standard Time,America/Caracas",
            "Vladivostok Standard Time,Asia/Vladivostok",
            "W. Australia Standard Time,Australia/Perth",
            "W. Central Africa Standard Time,Africa/Lagos",
            "West Asia Standard Time,Asia/Tashkent",
            "West Pacific Standard Time,Pacific/Port_Moresby",
            "W. Europe Standard Time,Europe/Berlin",
            "Yakutsk Standard Time,Asia/Yakutsk",
            nullptr
        };

        typedef TimeZoneDataT<> TimeZoneData;

    } // time

} // bl

#endif /* __BL_TIMEZONEDATA_H_ */
