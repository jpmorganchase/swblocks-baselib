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

#include <baselib/core/OS.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>


/************************************************************************
 * Abstract process priority tests
 *
 * NOTE:
 * on Unix, it is possible to call nice/setpriority and lower a process'
 * priority, but one can't raise it back to what it was at process creation
 * unless its owner has superuser privileges.
 *
 * for this reason, the test below has a side effect on the process (i.e., it
 * lowers its priority) and, hence, we must make it a standalone test.
 *
 */

UTF_AUTO_TEST_CASE( BaseLib_AbstractPriorityTests )
{
    using namespace bl;

    /*
     * Call the API twice to ensure it is idempotent (i.e. if the abstract priority is
     * already set to the requested value the call should be NOP)
     */

    os::setAbstractPriority( os::AbstractPriority::Background );
    os::setAbstractPriority( os::AbstractPriority::Background );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Abstract priority expected to be set to 'Background'"
        );

    if( test::UtfArgsParser::isClient() )
    {
        os::sleep( time::seconds( 30 ) );
    }

    bool ok = true;

    /*
     * Call the API twice to ensure it is idempotent (i.e. if the abstract priority is
     * already set to the requested value the call should be NOP)
     */

    ok = ok && os::trySetAbstractPriority( os::AbstractPriority::Normal );
    ok = ok && os::trySetAbstractPriority( os::AbstractPriority::Normal );

    if( ! ok )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Ignoring expected failures from os::trySetAbstractPriority( ... )"
            );
    }

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Abstract priority expected to be set to 'Normal'"
        );

    if( test::UtfArgsParser::isClient() )
    {
        os::sleep( time::seconds( 30 ) );
    }

    ok = true;

    /*
     * Call the API twice to ensure it is idempotent (i.e. if the abstract priority is
     * already set to the requested value the call should be NOP)
     */

    ok = ok && os::trySetAbstractPriority( os::AbstractPriority::Greedy );
    ok = ok && os::trySetAbstractPriority( os::AbstractPriority::Greedy );

    if( ! ok )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Ignoring expected failures from os::trySetAbstractPriority( ... )"
            );
    }

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Abstract priority expected to be set to 'Greedy'"
        );

    if( test::UtfArgsParser::isClient() )
    {
        os::sleep( time::seconds( 30 ) );
    }
}
