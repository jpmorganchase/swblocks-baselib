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

#include <baselib/loader/Cookie.h>

#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/UtfDirectoryFixture.h>
#include <utests/baselib/Utf.h>

UTF_AUTO_TEST_CASE( TestCookie )
{
    using namespace bl;
    using namespace bl::loader;
    using namespace utest;

    utest::TestDirectory dir;

    const auto file = dir.testFile( "cookie" );

    /*
     * Check exception thrown when cookie does not exist
     */

    UTF_CHECK_THROW( CookieFactory::getCookie( file, false ), UnexpectedException );

    /*
     * Check cookie can be created from file
     */

    {
        bl::fs::SafeOutputFileStreamWrapper outputFile( file );
        auto& os = outputFile.stream();

        os << "4f082035-e301-4cce-94f0-68f1c99f9223" << std::endl;
    }

    const auto cookie = CookieFactory::getCookie( file, false );
    UTF_CHECK_EQUAL( cookie -> getId(), uuids::string2uuid( "4f082035-e301-4cce-94f0-68f1c99f9223" ) );

    /*
     * Check new cookie creation
     */

    const auto newFile = dir.testFile( "new_cookie" );
    CookieFactory::getCookie( newFile, true );

    UTF_CHECK( fs::exists( newFile ) );
}
