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
#include <utests/baselib/UtfBaseLibCommon.h>
#include <utests/baselib/MachineGlobalTestLock.h>

#include <baselib/core/Singleton.h>
#include <baselib/core/Watchdog.h>
#include <baselib/core/BaseIncludes.h>

UTF_AUTO_TEST_CASE( TestWatchdogSetupMonitor )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 1 ) /* checkingInterval */ );

    const auto testMonitor = watchdog.setupMonitor( "testMonitor" );

    /*
     * Repeated call to setupMonitor with the same name should return
     * the same index
     */

    UTF_CHECK_EQUAL( testMonitor, watchdog.setupMonitor( "testMonitor" ) );

    watchdog.extendExpiration( "testMonitor", bl::time::seconds( 1U ) );

    watchdog.setupMonitor( "testMonitor" );

    /*
     * setupMonitor must not change the value of the expiration
     */

    UTF_CHECK( ! watchdog.expiringMonitors( bl::time::seconds( 10U ) ).empty() );
}

UTF_AUTO_TEST_CASE( TestWatchdogExceedMaxMonitors1 )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 1 ) /* checkingInterval */ );

    for( std::size_t i = 0U; i < Watchdog::MAX_MONITORS; ++i )
    {
        watchdog.setupMonitor( "testMonitor" + std::to_string( i ));
    }

    /*
     * By now, we have exhausted all the slots for the monitors
     */

    UTF_CHECK_THROW( watchdog.setupMonitor( "extra" ), bl::UnexpectedException );
}

UTF_AUTO_TEST_CASE( TestWatchdogExceedMaxMonitors2 )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 1 ) /* checkingInterval */ );

    for( std::size_t i = 0U; i < Watchdog::MAX_MONITORS; ++i )
    {
        watchdog.extendExpiration( "testMonitor" + std::to_string( i ), bl::time::milliseconds( 1U ) );
    }

    /*
     * By now, we have exhausted all the slots for the monitors
     */

    UTF_CHECK_THROW( watchdog.extendExpiration( "extra", bl::time::milliseconds( 1U ) ), bl::UnexpectedException );
}

UTF_AUTO_TEST_CASE( TestWatchdogOneIteration )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 1 ) /* checkingInterval */ );

    UTF_CHECK_EQUAL( 1U, watchdog.run( 1U ) );
}

UTF_AUTO_TEST_CASE( TestWatchdogNoUpdates )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 1U ) /* checkingInterval */ );

    watchdog.extendExpiration( "testMonitor", bl::time::milliseconds( 1U ) );

    /* With timeout 1ms and checking interval 1ms, the timeout should be triggered in 1-2 iterations */

    UTF_CHECK_EQUAL( 1U, watchdog.run( 100U ) );
}

UTF_AUTO_TEST_CASE( TestWatchdogSomeUpdates )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 100U ) /* checkingInterval */ );

    watchdog.extendExpiration( "testMonitor", bl::time::milliseconds( 100U ) );

    bl::tasks::scheduleAndExecuteInParallel(
        [ &watchdog ]( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
        {
            eq -> push_back(
                [ &watchdog ]() -> void
                {
                    bl::os::sleep( bl::time::milliseconds( 100U ) );
                    watchdog.extendExpiration( "testMonitor", bl::time::milliseconds( 100U ) );
                }
                );

            eq -> push_back(
                [ &watchdog ]() -> void
                {
                    /* Expect this to be terminated due to timeout */

                    UTF_CHECK( watchdog.run( 5U ) < 5U );
                }
                );
        }
        );
}

UTF_AUTO_TEST_CASE( TestWatchdogNoTermination )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 100U ) /* checkingInterval */ );

    watchdog.extendExpiration( "testMonitor", bl::time::milliseconds( 200U ) );

    bl::tasks::scheduleAndExecuteInParallel(
        [ &watchdog ]( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
        {
            eq -> push_back(
                [ &watchdog ]() -> void
                {
                    /*
                     * Perform 10 extentions to make sure that the watchdog
                     * will not terminate us in 5 iterations
                     */

                    for( std::size_t i = 0U; i < 10; ++i )
                    {
                        bl::os::sleep( bl::time::milliseconds( 100U ) );

                        /*
                         * We extend with an overlap, so there is no situation when
                         * the expiration is not extended
                         */
                        watchdog.extendExpiration( "testMonitor", bl::time::milliseconds( 200U ) );
                    }
                }
                );

            eq -> push_back(
                [ &watchdog ]() -> void
                {
                    /* Value 5 (maxIterations) is only returned if no termination was triggered */

                    UTF_CHECK_EQUAL( 5U, watchdog.run( 5U ) );
                }
                );
        }
        );
}

UTF_AUTO_TEST_CASE( TestWatchdogExpring )
{
    using bl::Watchdog;

    Watchdog watchdog( bl::time::milliseconds( 1 ) /* checkingInterval */ );

    watchdog.extendExpiration( "testMonitor1", bl::time::milliseconds( 10U ) );
    watchdog.extendExpiration( "testMonitor2", bl::time::milliseconds( 10U ) );
    watchdog.extendExpiration( "testMonitor3", bl::time::milliseconds( 100000U ) );

    auto expiring = watchdog.expiringMonitors( bl::time::milliseconds( 100U ) );

    /*
     * We only expect testMonitor1 and testMonitor2 to appear in the expiring list
     */

    UTF_CHECK_EQUAL( 2U, expiring.size() );

    std::sort( expiring.begin(), expiring.end() );

    UTF_CHECK_EQUAL( "testMonitor1", expiring[ 0 ] );
    UTF_CHECK_EQUAL( "testMonitor2", expiring[ 1 ] );
}
