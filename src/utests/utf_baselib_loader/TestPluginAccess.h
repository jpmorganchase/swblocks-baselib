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

#include <baselib/loader/PluginAccess.h>
#include <baselib/loader/Manifest.h>
#include <baselib/loader/Platform.h>

#include <baselib/core/OS.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>

#include <unordered_map>

UTF_AUTO_TEST_CASE( TestPluginAccess )
{
    using namespace bl;
    using namespace bl::loader;

    /*
     * Fields not used by PluginAccess and not required for test purposes -
     * these can be shared by all Manifest objects
     */

    const std::size_t versionMajor = 1;
    const std::size_t versionMinor = 0;
    const std::size_t versionPatch = 0;

    const std::string pluginName( "name" );
    const std::string pluginDesc( "description" );

    const auto isClient = false;
    const auto isServer = true;

    const auto platform = Platform::get( "os", "arch", "toolchain" );
    const auto cppCompatId = uuids::create();

    /*
     * Create test data for plug-in 1
     */

    const auto m1ServerId = uuids::create();

    const auto m1ClsId1 = uuids::create();
    const auto m1ClsId2 = uuids::create();
    const auto m1ClsId3 = uuids::create();

    std::set< om::clsid_t > m1ClsIds;
    m1ClsIds.insert( m1ClsId1 );
    m1ClsIds.insert( m1ClsId2 );
    m1ClsIds.insert( m1ClsId3 );

    const om::clsid_t m1PluginClsId( m1ClsId1 );

    auto m1 = Manifest::createInstance(
        m1ServerId,
        versionMajor,
        versionMinor,
        versionPatch,
        std::move( m1ClsIds ),
        m1PluginClsId,
        cpp::copy( pluginName ),
        cpp::copy( pluginDesc ),
        isClient,
        isServer,
        om::copy( platform ),
        cppCompatId
        );

    const auto p1Path = fs::path( "plugin1" );

    /*
     * Create test data for plug-in 2
     */

    const auto m2ServerId = uuids::create();

    const auto m2ClsId1 = uuids::create();
    const auto m2ClsId2 = uuids::create();
    const auto m2ClsId3 = uuids::create();

    std::set< om::clsid_t > m2ClsIds;
    m2ClsIds.insert( m2ClsId1 );
    m2ClsIds.insert( m2ClsId2 );
    m2ClsIds.insert( m2ClsId3 );

    const om::clsid_t m2PluginClsId( m2ClsId1 );

    auto m2 = Manifest::createInstance(
        m2ServerId,
        versionMajor,
        versionMinor,
        versionPatch,
        std::move( m2ClsIds ),
        m2PluginClsId,
        cpp::copy( pluginName ),
        cpp::copy( pluginDesc ),
        isClient,
        isServer,
        om::copy( platform ),
        cppCompatId
        );

    const auto p2Path = fs::path( "plugin2" );

    /*
     * Create path -> manifest map
     */

    std::unordered_map< fs::path, om::ObjPtr< Manifest > > plugins;
    plugins.emplace( p1Path, std::move( m1 ) );
    plugins.emplace( p2Path, std::move( m2 ) );

    const auto pluginAccess = PluginAccess::createInstance( plugins );

    /*
     * Test getServerId
     */

    UTF_CHECK_EQUAL( m1ServerId, pluginAccess -> getServerId( m1ClsId1 ) );
    UTF_CHECK_EQUAL( m1ServerId, pluginAccess -> getServerId( m1ClsId2 ) );
    UTF_CHECK_EQUAL( m1ServerId, pluginAccess -> getServerId( m1ClsId3 ) );

    UTF_CHECK_EQUAL( m2ServerId, pluginAccess -> getServerId( m2ClsId1 ) );
    UTF_CHECK_EQUAL( m2ServerId, pluginAccess -> getServerId( m2ClsId2 ) );
    UTF_CHECK_EQUAL( m2ServerId, pluginAccess -> getServerId( m2ClsId3 ) );

    /*
     * Test getPluginClassId
     */

    UTF_CHECK_EQUAL( m1PluginClsId, pluginAccess -> getPluginClassId( m1ServerId ) );

    UTF_CHECK_EQUAL( m2PluginClsId, pluginAccess -> getPluginClassId( m2ServerId ) );

    /*
     * Test getLibrary
     */

    UTF_CHECK_EQUAL( p1Path, pluginAccess -> getLibrary( m1ServerId ) );

    UTF_CHECK_EQUAL( p2Path, pluginAccess -> getLibrary( m2ServerId ) );

    /*
     * Test getPluginClassIds
     */

    std::vector< om::clsid_t > clsids;
    pluginAccess -> getPluginClassIds( clsids );

    UTF_CHECK_EQUAL( 2U, clsids.size() );
    UTF_CHECK_EQUAL( true, std::find( clsids.begin(), clsids.end(), m1PluginClsId ) != clsids.end() );
    UTF_CHECK_EQUAL( true, std::find( clsids.begin(), clsids.end(), m2PluginClsId ) != clsids.end() );

    /*
     * Test error cases
     */

    const auto unknownUuid = uuids::create();

    UTF_CHECK_THROW( pluginAccess -> getServerId( unknownUuid ), ClassNotFoundException );
    UTF_CHECK_THROW( pluginAccess -> getPluginClassId( unknownUuid ), ClassNotFoundException );
    UTF_CHECK_THROW( pluginAccess -> getLibrary( unknownUuid ), ClassNotFoundException );
}
