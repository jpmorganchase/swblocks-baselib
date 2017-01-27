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

#include <utf/plugins/calculator/Calculator.h>

#include <baselib/loader/Manifest.h>
#include <baselib/loader/Platform.h>
#include <baselib/loader/PluginAccess.h>
#include <baselib/loader/Resolver.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/LoggerUtils.h>
#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfDirectoryFixture.h>
#include <utests/baselib/UtfPluginFixture.h>
#include <utests/baselib/TestUtils.h>

#include <unordered_map>

struct ResolverFixture
{
    typedef ResolverFixture this_type;

    const utest::TestDirectory      m_dir;
    const utest::TestPlugin         m_plugin;

    ResolverFixture()
        :
        m_dir( true /* ignoreErrors */ ),
        m_plugin()
    {
    }
};

UTF_AUTO_TEST_CASE( TestResolver_CheckFailure )
{
    using namespace bl;
    using namespace bl::loader;

    UTF_CHECK_EQUAL( 0L, om::outstandingObjectRefs() );

    {
        auto manifest = Manifest::createInstance(
            uuids::create() /* serverid */,
            1 /* versionMajor */,
            0 /* versionMinor */,
            0 /* versionPatch */,
            std::set< om::clsid_t >() /* clsids */,
            uuids::nil() /* pluginClassId */,
            "name",
            "description",
            false /* isClient */,
            true /* isServer */,
            Platform::get( "os", "arch", "toolchain" ),
            uuids::create() /* cppCompatibilityId */
            );

        std::unordered_map< fs::path, om::ObjPtr< Manifest > > plugins;
        plugins.emplace( fs::path( "plugin" ), std::move( manifest ) );

        const auto resolver = ResolverImplDefault::createInstance< om::Resolver >(
            PluginAccess::createInstance( plugins )
            );

        /*
         * Verify failure when server cannot be found
         */

        const auto unknownClassId = uuids::create();
        const auto unknownServerId = uuids::create();

        {
            Logging::LineLoggerPusher pushLineLogger( &utest::errorToDebugLineLogger );

            om::serverid_t serverid;
            const auto rc = resolver -> resolveServer( unknownClassId, serverid );

            UTF_CHECK( rc != 0 );
        }

        /*
         * Verify failure when requested server cannot / has not be loaded
         */

        {
            Logging::LineLoggerPusher pushLineLogger( &utest::errorToDebugLineLogger );

            om::objref_t factoryRef;
            const auto rc = resolver -> getFactory( unknownServerId, factoryRef, true );

            UTF_CHECK( rc != 0 );
        }

        {
            Logging::LineLoggerPusher pushLineLogger( &utest::errorToDebugLineLogger );

            om::objref_t factoryRef;
            const auto rc = resolver -> getFactory( unknownServerId, factoryRef, false );

            UTF_CHECK( rc != 0 );
        }
    }

    UTF_CHECK_EQUAL( 0L, om::outstandingObjectRefs() );
}

UTF_FIXTURE_TEST_CASE( TestResolver_CheckSuccess, ResolverFixture )
{
    using namespace bl;
    using namespace bl::loader;

    UTF_CHECK_EQUAL( 0L, om::outstandingObjectRefs() );

    {
        /*
         * Create manifest and plug-in access for calculator plug-in built for fixture
         */

        ManifestFactory::createAndWriteForLibrary(
            m_plugin.path(),
            Platform::get( "windows", "x64", "msvc12" )
            );

        std::vector< std::string > plugins;
        plugins.push_back( m_plugin.path() );

        const auto resolver = ResolverImplDefault::createInstance< om::Resolver >(
            PluginAccess::createInstance( plugins )
            );

        /*
         * Actual calculator plug-in info
         */

        const auto iid = iids::Calculator();
        const auto clsid = clsids::CalculatorObj();
        const auto serverid = uuids::string2uuid( "8e213524-8c75-4622-8273-6a5eeaa26250" );

        /*
         * Verify successful server resolution
         */

        {
            om::serverid_t serveridOut;
            const auto rc = resolver -> resolveServer( clsid, serveridOut );

            UTF_CHECK_EQUAL( rc, 0 );
            UTF_CHECK_EQUAL( serverid, serveridOut );
        }

        /*
         * Verify first-time factory look-up fails if not previously loaded
         */

        {
            Logging::LineLoggerPusher pushLineLogger( &utest::errorToDebugLineLogger );

            om::objref_t factoryRefOut;
            const auto rc = resolver -> getFactory( serverid, factoryRefOut, true );

            UTF_CHECK( rc != 0 );
        }

        /*
         * Verify successful factory resolution
         */

        {
            om::objref_t factoryRefOut;
            const auto rc = resolver -> getFactory( serverid, factoryRefOut, false );

            UTF_CHECK( rc == 0 );
        }

        /*
         * Verify subsequent factory look-up is successful when previously loaded
         */

        {
            om::objref_t factoryRefOut;
            const auto rc = resolver -> getFactory( serverid, factoryRefOut, true );

            UTF_CHECK( rc == 0 );

            /*
             * Test execution of code from plug-in binary
             */

            const auto factory = om::wrap< om::Factory >( factoryRefOut );
            const auto instance = factory -> createInstance( clsid, iid );
            const auto calc = om::wrap< utest::plugins::Calculator >( instance );

            UTF_CHECK_EQUAL( 5, calc -> add( 2, 3 ) );
            UTF_CHECK_EQUAL( 3, calc -> subtract( 7, 4 ) );
        }
    }

    UTF_CHECK_EQUAL( 0L, om::outstandingObjectRefs() );
}
