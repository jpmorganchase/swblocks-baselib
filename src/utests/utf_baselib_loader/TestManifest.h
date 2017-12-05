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

#include <baselib/loader/Manifest.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/JsonUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/UtfDirectoryFixture.h>
#include <utests/baselib/UtfPluginFixture.h>
#include <utests/baselib/TestUtils.h>
#include <utests/baselib/Utf.h>

struct ManifestFixture
{
    utest::TestDirectory m_dir;
    utest::TestPlugin m_plugin;

    ManifestFixture()
        :
        m_dir( true /* ignoreErrors */ ),
        m_plugin()
    {
    }
};

UTF_FIXTURE_TEST_CASE( TestManifestRead, ManifestFixture )
{
    using namespace bl;
    using namespace bl::loader;
    using namespace utest;

    auto platform = Platform::get( "windows", "x64", "msvc12" );
    const auto manifest = ManifestFactory::create( m_plugin.path(), std::move( platform ) );

    UTF_CHECK_EQUAL( uuids::uuid2string( manifest -> serverId() ), "8e213524-8c75-4622-8273-6a5eeaa26250" );
    UTF_CHECK_EQUAL( manifest -> versionMajor(), 1U );
    UTF_CHECK_EQUAL( manifest -> versionMinor(), 2U );

    const auto clsids = manifest -> classIds();

    UTF_CHECK_EQUAL( clsids.size(), 2U );
    UTF_CHECK( clsids.find( uuids::string2uuid( "3fd48332-db97-42bd-9cf6-f6a332895d92" ) ) != clsids.end() );
    UTF_CHECK( clsids.find( uuids::string2uuid( "c76d0949-92a2-4925-8c9f-890f05484474" ) ) != clsids.end() );

    UTF_CHECK_EQUAL( uuids::uuid2string( manifest -> pluginClassId() ), "3fd48332-db97-42bd-9cf6-f6a332895d92" );
    UTF_CHECK_EQUAL( manifest -> pluginName(), "calculator" );
    UTF_CHECK_EQUAL( manifest -> pluginDescription(), "Simple calculator plug-in" );
    UTF_CHECK_EQUAL( manifest -> isClientPlugin(), false );
    UTF_CHECK_EQUAL( manifest -> isServerPlugin(), true );

    UTF_CHECK_EQUAL( manifest -> platform() -> os(), "windows" );
    UTF_CHECK_EQUAL( manifest -> platform() -> architecture(), "x64" );
    UTF_CHECK_EQUAL( manifest -> platform() -> toolchain(), "msvc12" );
}

UTF_FIXTURE_TEST_CASE( TestManifestWrite, ManifestFixture )
{
    using namespace bl;
    using namespace bl::loader;
    using namespace utest;

    auto output = ( m_dir.path() / "plugin.mf" ).string();
    auto platform = Platform::get( "linux", "x64", "gcc48" );
    auto manifest = ManifestFactory::create( m_plugin.path(), std::move( platform ) );

    ManifestFactory::write( std::move( manifest ), std::move( output ) );

    json::Value value;

    {
        bl::fs::SafeInputFileStreamWrapper inputFile( output );
        auto& is = inputFile.stream();

        value = json::readFromStream( is );
    }

    auto json = value.get_obj();

    UTF_CHECK_EQUAL( json[ "manifestVersion" ].get_int(), 1 );
    UTF_CHECK_EQUAL( json[ "serverId" ].get_str(), "8e213524-8c75-4622-8273-6a5eeaa26250" );
    UTF_CHECK_EQUAL( json[ "versionMajor" ].get_int(), 1 );
    UTF_CHECK_EQUAL( json[ "versionMinor" ].get_int(), 2 );

    const auto clsids = json[ "classIds" ].get_array();

    UTF_CHECK_EQUAL( clsids.size(), 2U );
    UTF_CHECK( std::find( clsids.begin(), clsids.end(), "3fd48332-db97-42bd-9cf6-f6a332895d92" ) != clsids.end() );
    UTF_CHECK( std::find( clsids.begin(), clsids.end(), "c76d0949-92a2-4925-8c9f-890f05484474" ) != clsids.end() );

    UTF_CHECK_EQUAL( json[ "pluginClassId" ].get_str(), "3fd48332-db97-42bd-9cf6-f6a332895d92" );
    UTF_CHECK_EQUAL( json[ "pluginName" ].get_str(), std::string( "calculator" ) );
    UTF_CHECK_EQUAL( json[ "pluginDescription" ].get_str(), "Simple calculator plug-in" );
    UTF_CHECK_EQUAL( json[ "isClientPlugin" ].get_bool(), false );
    UTF_CHECK_EQUAL( json[ "isServerPlugin" ].get_bool(), true );

    UTF_CHECK_EQUAL( json[ "os" ].get_str(), "linux" );
    UTF_CHECK_EQUAL( json[ "architecture" ].get_str(), "x64" );
    UTF_CHECK_EQUAL( json[ "toolchain" ].get_str(), "gcc48" );
}

UTF_AUTO_TEST_CASE( TestToolchainMatch )
{
    const auto platform = bl::loader::Platform::get( "windows", "x64", "vc12-release" );

    UTF_CHECK_EQUAL( platform -> toolchain(), "vc12-release" );

    UTF_CHECK_EQUAL( platform -> name(), "windows_x64_vc12-release" );
    UTF_CHECK_EQUAL( platform -> name( true /* ignoreCompilerId */ ), "windows_x64_release" );

    UTF_CHECK( platform -> matchesToolchain( "vc12-release", false /* ignoreCompilerId */ ) );
    UTF_CHECK( ! platform -> matchesToolchain( "vc13-release", false /* ignoreCompilerId */ ) );
    UTF_CHECK( platform -> matchesToolchain( "vc13-release", true /* ignoreCompilerId */ ) );
    UTF_CHECK( ! platform -> matchesToolchain( "vc12-debug", true /* ignoreCompilerId */ ) );
}
