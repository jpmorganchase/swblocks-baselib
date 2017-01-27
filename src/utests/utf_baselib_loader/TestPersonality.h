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

#include <baselib/loader/Personality.h>
#include <baselib/loader/Manifest.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/UtfDirectoryFixture.h>
#include <utests/baselib/Utf.h>

#include <set>

struct PersonalityTestFixture
{
    const utest::TestDirectory                  m_dir;

    const bl::fs::path                          m_plugin1;
    const bl::fs::path                          m_plugin2;
    const bl::fs::path                          m_plugin3;

    bl::om::ObjPtr< bl::loader::Manifest >     m_mf1;
    bl::om::ObjPtr< bl::loader::Manifest >     m_mf2;
    bl::om::ObjPtr< bl::loader::Manifest >     m_mf3;

    PersonalityTestFixture()
        :
        m_plugin1( m_dir.testFile( "plugin1" ) ),
        m_plugin2( m_dir.testFile( "plugin2" ) ),
        m_plugin3( m_dir.testFile( "plugin3" ) )
    {
        using namespace bl;
        using namespace bl::loader;

        /*
         * Create dummy plug-in libraries
         */

        os::fopen( m_plugin1, "w" );
        os::fopen( m_plugin2, "w" );
        os::fopen( m_plugin3, "w" );

        /*
         * Common plug-in properties
         */

        const std::size_t versionMajor = 1;
        const std::size_t versionMinor = 0;
        const std::size_t versionPatch = 0;

        const auto platform = Platform::get( "os", "arch", "toolchain" );
        const auto cppCompatId = uuids::create();

        /*
         * Plug-in 1
         */

        const auto m1ServerId = uuids::create();
        const auto m1PluginClsId = uuids::create();

        std::set< om::clsid_t > m1ClsIds;
        m1ClsIds.insert( m1PluginClsId );

        m_mf1 = Manifest::createInstance(
            m1ServerId,
            versionMajor,
            versionMinor,
            versionPatch,
            std::move( m1ClsIds ),
            m1PluginClsId,
            "plug-in 1 name",
            "plug-in 1 description",
            true /* isClient */,
            false /* isServer */,
            om::copy( platform ),
            cppCompatId
            );

        /*
         * Plug-in 2
         */

        const auto m2ServerId = uuids::create();
        const auto m2PluginClsId = uuids::create();

        std::set< om::clsid_t > m2ClsIds;
        m2ClsIds.insert( m2PluginClsId );

        m_mf2 = Manifest::createInstance(
            m2ServerId,
            versionMajor,
            versionMinor,
            versionPatch,
            std::move( m2ClsIds ),
            m2PluginClsId,
            "plug-in 2 name",
            "plug-in 2 description",
            false /* isClient */,
            true /* isServer */,
            om::copy( platform ),
            cppCompatId
            );

        /*
         * Plug-in 3
         */

        const auto m3ServerId = uuids::create();
        const auto m3PluginClsId = uuids::create();

        std::set< om::clsid_t > m3ClsIds;
        m3ClsIds.insert( m3PluginClsId );

        m_mf3 = Manifest::createInstance(
            m3ServerId,
            versionMajor,
            versionMinor,
            versionPatch,
            std::move( m3ClsIds ),
            m3PluginClsId,
            "plug-in 3 name",
            "plug-in 3 description",
            true /* isClient */,
            true /* isServer */,
            om::copy( platform ),
            cppCompatId
            );

        /*
         * Create plug-in manifests
         */

        ManifestFactory::writeForBinary( om::copy( m_mf1 ), cpp::copy( m_plugin1 ) );
        ManifestFactory::writeForBinary( om::copy( m_mf2 ), cpp::copy( m_plugin2 ) );
        ManifestFactory::writeForBinary( om::copy( m_mf3 ), cpp::copy( m_plugin3 ) );
    }
};

UTF_FIXTURE_TEST_CASE( TestPersonalityFileSystem, PersonalityTestFixture )
{
    using namespace utest;
    using namespace bl;
    using namespace bl::loader;

    /*
     * Test client and server plug-in accessors
     */

    const auto personality = PersonalityFileSystem::createInstance< Personality >( m_dir.path() );

    const auto& serverPlugins = personality -> getServerPlugins();
    const auto& clientPlugins = personality -> getClientPlugins();

    UTF_CHECK_EQUAL( 2U, clientPlugins.size() );
    UTF_CHECK_EQUAL( 2U, serverPlugins.size() );

    {
        const auto entry = clientPlugins.find( m_plugin1 );
        UTF_CHECK_EQUAL( true, entry != clientPlugins.end() );
        UTF_CHECK_EQUAL( m_mf1 -> serverId(), entry -> second -> serverId() );
    }

    {
        const auto entry = clientPlugins.find( m_plugin3 );
        UTF_CHECK_EQUAL( true, entry != clientPlugins.end() );
        UTF_CHECK_EQUAL( m_mf3 -> serverId(), entry -> second -> serverId() );
    }

    {
        const auto entry = serverPlugins.find( m_plugin2 );
        UTF_CHECK_EQUAL( true, entry != serverPlugins.end() );
        UTF_CHECK_EQUAL( m_mf2 -> serverId(), entry -> second -> serverId() );
    }

    {
        const auto entry = serverPlugins.find( m_plugin3 );
        UTF_CHECK_EQUAL( true, entry != serverPlugins.end() );
        UTF_CHECK_EQUAL( m_mf3 -> serverId(), entry -> second -> serverId() );
    }
}
