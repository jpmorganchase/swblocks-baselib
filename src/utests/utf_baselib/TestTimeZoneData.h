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

#include <utests/baselib/Utf.h>

#include <baselib/core/ErrorHandling.h>
#include <baselib/core/TimeZoneData.h>
#include <baselib/core/TimeUtils.h>

static void logTestName( SAA_in const std::string& name )
{
    BL_LOG_MULTILINE(
        bl::Logging::info(),
        BL_MSG()
            << "\nTimezone data validation - "
            << name
            << " **********\n"
        );
}

UTF_AUTO_TEST_CASE( TestTimeZoneData )
{
    using namespace bl;
    using namespace bl::time;

    TimeZoneData::init();

    logTestName( "Empty Timezone check" );

    UTF_CHECK_EQUAL( TimeZoneData::validateTimeZone( "" ), false );

    logTestName( "Invalid Timezone check" );

    UTF_CHECK_EQUAL( TimeZoneData::validateTimeZone( "INVALID" ), false );

    logTestName( "Valid Timezone check" );

    UTF_CHECK_EQUAL( TimeZoneData::validateTimeZone( "IST" ), true );

    logTestName( "Invalid Timezone offset check" );

    UTF_CHECK_THROW( TimeZoneData::getTimeZoneOffset( "" ), UnexpectedException );

    logTestName( "Timezone offset check - Daylight saving timezone" );

    UTF_CHECK_EQUAL( TimeZoneData::getTimeZoneOffset( "GMT" ), "GMT+00:00:00BST+01:00:00,M3.5.0/+01:00:00,M10.5.0/+02:00:00" );

    UTF_CHECK_EQUAL( TimeZoneData::getTimeZoneOffset( "UTC" ), "UTC+00:00:00" );

    const auto fieldsGmt = TimeZoneData::getTimeZoneDataFields( "GMT" );
    UTF_CHECK( fieldsGmt.size() > 0 );

    const auto fieldsUtc = TimeZoneData::getTimeZoneDataFields( "UTC" );
    UTF_CHECK( fieldsUtc.size() > 0 );

    UTF_CHECK_EQUAL( fieldsGmt.size(), fieldsUtc.size() );

    logTestName( "Timezone offset check - Non-Daylight saving timezone" );

    UTF_CHECK_EQUAL( TimeZoneData::getTimeZoneOffset( "IST" ), "IST+05:30:00" );
}

UTF_AUTO_TEST_CASE( TestISOTimeFormat )
{
    using namespace bl::time;

    const char timeDesignator = 'T';
    const char dateSeparator = '-';
    const char timeSeparator = ':';
    const char microSecondSeparator = '.';

    /*
     * YYYY-MM-DDThh:mm:ss.uuuuuu-hh:mm
     * OR
     * YYYY-MM-DDThh:mm:ss-hh:mm
     */

    std::string utcTimeStr1( "20020131T235959.000159" );

    ptime utcTime( from_iso_string( utcTimeStr1 ) );

    auto localTime = getLocalTimeISO( utcTime );

    bl::str::regex regex( regexLocalTimeISO() );
    bl::str::smatch results;
    UTF_CHECK( bl::str::regex_match( localTime, results, regex ) );

    UTF_REQUIRE( localTime.size() == 32U );

    UTF_CHECK( localTime.at( 4 ) == dateSeparator );
    UTF_CHECK( localTime.at( 7 ) == dateSeparator );

    UTF_CHECK( localTime.at( 10 ) == timeDesignator );

    UTF_CHECK( localTime.at( 13 ) == timeSeparator );
    UTF_CHECK( localTime.at( 16 ) == timeSeparator );

    UTF_CHECK( localTime.at( 19 ) == microSecondSeparator );

    UTF_CHECK( localTime.at( 29 ) == timeSeparator );

    ptime invalidUtcTime;

    UTF_CHECK( getLocalTimeISO( invalidUtcTime ).empty() );

    std::string utcTimeStr2( "20020131T235959" );
    std::string utcTimeStr3( "20020131T235959.145" );
    std::string utcTimeStr4( "20020131T235959.000000" );

    ptime utcTime2( from_iso_string( utcTimeStr2 ) );
    ptime utcTime3( from_iso_string( utcTimeStr3 ) );
    ptime utcTime4( from_iso_string( utcTimeStr4 ) );

    localTime = getLocalTimeISO( utcTime2 );
    UTF_CHECK( localTime.size() == 25U );

    localTime = getLocalTimeISO( utcTime3 );
    UTF_CHECK( localTime.size() == 32U );

    localTime = getLocalTimeISO( utcTime4 );
    UTF_CHECK( localTime.size() == 25U );

    if( test::UtfArgsParser::isClient() )
    {
        utest::TestUtils::measureRuntime(
            "Generating 1 thousand local timestamps",
            [ & ]() -> void
            {
                for( std::size_t i = 0U; i < 1000U; ++i )
                {
                    getCurrentLocalTimeISO();
                }
            }
            );

        utest::TestUtils::measureRuntime(
            "Generating 10 thousand local timestamps",
            [ & ]() -> void
            {
                for( std::size_t i = 0U; i < 10000U; ++i )
                {
                    getCurrentLocalTimeISO();
                }
            }
            );

        utest::TestUtils::measureRuntime(
            "Generating 1 million local timestamps",
            [ & ]() -> void
            {
                for( std::size_t i = 0U; i < 1000000U; ++i )
                {
                    getCurrentLocalTimeISO();
                }
            }
            );
    }
}
