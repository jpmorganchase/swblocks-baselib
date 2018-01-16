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

#ifndef __BL_LOADER_MANIFEST_H_
#define __BL_LOADER_MANIFEST_H_

#include <baselib/loader/Platform.h>
#include <baselib/loader/Version.h>

#include <baselib/core/JsonUtils.h>
#include <baselib/core/PluginDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>
#include <baselib/core/FsUtils.h>

#include <unordered_set>

namespace bl
{
    namespace loader
    {
        /**
         * @brief class Manifest
         */

        template
        <
            typename E = void
        >
        class ManifestT : public om::Object
        {
            BL_DECLARE_OBJECT_IMPL_DEFAULT( ManifestT )

        private:

            static const std::string                                                g_fileExt;

            const om::serverid_t                                                    m_serverId;
            const std::size_t                                                       m_versionMajor;
            const std::size_t                                                       m_versionMinor;
            const std::size_t                                                       m_versionPatch;
            std::set< om::clsid_t >                                                 m_classIds;

            const om::clsid_t                                                       m_pluginClassId;
            std::string                                                             m_pluginName;
            std::string                                                             m_pluginDescription;
            const bool                                                              m_isClientPlugin;
            const bool                                                              m_isServerPlugin;

            om::ObjPtr< PlatformIdentity >                                          m_platform;
            const bl::uuid_t                                                        m_cppCompatibilityId;

        public:

            ManifestT( SAA_in ManifestT&& other )
                :
                m_serverId( other.m_serverId ),
                m_versionMajor( other.m_versionMajor ),
                m_versionMinor( other.m_versionMinor ),
                m_versionPatch( other.m_versionPatch ),
                m_classIds( std::move( other.m_classIds ) ),
                m_pluginClassId( other.m_pluginClassId ),
                m_pluginName( std::move( other.m_pluginName ) ),
                m_pluginDescription( std::move( other.m_pluginDescription ) ),
                m_isClientPlugin( other.m_isClientPlugin ),
                m_isServerPlugin( other.m_isServerPlugin ),
                m_platform( std::move( other.m_platform ) ),
                m_cppCompatibilityId( other.m_cppCompatibilityId )
            {
            }

            ManifestT& operator =( SAA_in ManifestT&& other )
            {
                m_serverId = other.m_serverId;
                m_versionMajor = other.m_versionMajor;
                m_versionMinor = other.m_versionMinor;
                m_versionPatch = other.m_versionPatch;
                m_classIds = std::move( other.m_classIds );
                m_pluginClassId = other.m_pluginClassId;
                m_pluginName = std::move( other.m_pluginName );
                m_pluginDescription = std::move( other.m_pluginDescription );
                m_isClientPlugin = other.m_isClientPlugin;
                m_isServerPlugin = other.m_isServerPlugin;
                m_platform = std::move( other.m_platform );
                m_cppCompatibilityId = other.m_cppCompatibilityId;

                return *this;
            }

            ManifestT(
                SAA_in      const om::serverid_t&                                   serverId,
                SAA_in      const std::size_t                                       versionMajor,
                SAA_in      const std::size_t                                       versionMinor,
                SAA_in      const std::size_t                                       versionPatch,
                SAA_in      std::set< om::clsid_t >&&                               classIds,
                SAA_in      const om::clsid_t&                                      pluginClassId,
                SAA_in      std::string&&                                           pluginName,
                SAA_in      std::string&&                                           pluginDescription,
                SAA_in      const bool                                              isClientPlugin,
                SAA_in      const bool                                              isServerPlugin,
                SAA_in      om::ObjPtr< PlatformIdentity >&&                        platform,
                SAA_in      const bl::uuid_t&                                       cppCompatibilityId
                )
                :
                m_serverId(  serverId ),
                m_versionMajor( versionMajor ),
                m_versionMinor( versionMinor ),
                m_versionPatch( versionPatch ),
                m_classIds( std::forward< std::set< om::clsid_t > >( classIds ) ),
                m_pluginClassId( pluginClassId ),
                m_pluginName( std::forward< std::string >( pluginName ) ),
                m_pluginDescription( std::forward< std::string >( pluginDescription ) ),
                m_isClientPlugin( isClientPlugin ),
                m_isServerPlugin( isServerPlugin ),
                m_platform( std::forward< om::ObjPtr< PlatformIdentity > >( platform ) ),
                m_cppCompatibilityId( cppCompatibilityId )
            {
            }

            const om::serverid_t& serverId() const NOEXCEPT
            {
                return m_serverId;
            }

            std::size_t versionMajor() const NOEXCEPT
            {
                return m_versionMajor;
            }

            std::size_t versionMinor() const NOEXCEPT
            {
                return m_versionMinor;
            }

            std::size_t versionPatch() const NOEXCEPT
            {
                return m_versionPatch;
            }

            std::string version() const
            {
                return Version::toString( m_versionMajor, m_versionMinor, m_versionPatch );
            }

            const std::set< om::clsid_t >& classIds() const NOEXCEPT
            {
                return m_classIds;
            }

            const om::clsid_t& pluginClassId() const NOEXCEPT
            {
                return m_pluginClassId;
            }

            const std::string& pluginName() const NOEXCEPT
            {
                return m_pluginName;
            }

            const std::string& pluginDescription() const NOEXCEPT
            {
                return m_pluginDescription;
            }

            bool isClientPlugin() const NOEXCEPT
            {
                return m_isClientPlugin;
            }

            bool isServerPlugin() const NOEXCEPT
            {
                return m_isServerPlugin;
            }

            om::ObjPtr< PlatformIdentity > platform() const NOEXCEPT
            {
                return om::copy( m_platform );
            }

            const bl::uuid_t& cppCompatibilityId() const NOEXCEPT
            {
                return m_cppCompatibilityId;
            }

            static fs::path getManifestPath( SAA_in const fs::path& binary )
            {
                return fs::path( binary.string() + g_fileExt );
            }

            static const std::string& fileExtension() NOEXCEPT
            {
                return g_fileExt;
            }
        };

        template
        <
            typename E
        >
        const std::string
        ManifestT< E >::g_fileExt = ".mf";

        typedef om::ObjectImpl< ManifestT<>, false, true, om::detail::LttLeaked > Manifest;

        /**
         * @brief class ManifestFactory
         */

        template
        <
            typename E = void
        >
        class ManifestFactoryT
        {
            BL_DECLARE_STATIC( ManifestFactoryT )

        private:

            static const std::size_t g_manifestVersionId;

            static const std::string g_manifestVersion;
            static const std::string g_serverId;
            static const std::string g_versionMajor;
            static const std::string g_versionMinor;
            static const std::string g_versionPatch;
            static const std::string g_classIds;
            static const std::string g_pluginClassId;
            static const std::string g_pluginName;
            static const std::string g_pluginDescription;
            static const std::string g_isClientPlugin;
            static const std::string g_isServerPlugin;
            static const std::string g_os;
            static const std::string g_architecture;
            static const std::string g_toolchain;
            static const std::string g_cppCompatibilityId;

        public:

            static om::ObjPtr< Manifest > create(
                SAA_in      fs::path&&                                              library,
                SAA_in      om::ObjPtr< PlatformIdentity >&&                        platform
                )
            {
                om::serverid_t serverId;
                std::size_t versionMajor;
                std::size_t versionMinor;
                std::size_t versionPatch;
                std::set< om::clsid_t > classIds;
                bl::uuid_t cppCompatId;
                om::clsid_t pluginClassId;
                std::string pluginName;
                std::string pluginDescription;
                bool isClientPlugin;
                bool isServerPlugin;

                registerPlugin(
                    BL_PARAM_FWD( library ),
                    serverId,
                    versionMajor,
                    versionMinor,
                    versionPatch,
                    classIds,
                    cppCompatId,
                    pluginClassId,
                    pluginName,
                    pluginDescription,
                    isClientPlugin,
                    isServerPlugin
                    );

                return Manifest::createInstance(
                    serverId,
                    versionMajor,
                    versionMinor,
                    versionPatch,
                    std::move( classIds ),
                    pluginClassId,
                    std::move( pluginName ),
                    std::move( pluginDescription ),
                    isClientPlugin,
                    isServerPlugin,
                    BL_PARAM_FWD( platform ),
                    cppCompatId
                    );
            }

            static om::ObjPtr< Manifest > readForBinary( SAA_in const fs::path& binary )
            {
                const auto manifest = Manifest::getManifestPath( binary );

                BL_CHK_T_USER_FRIENDLY(
                    false,
                    fs::is_regular_file( manifest ),
                    UnexpectedException(),
                    BL_MSG()
                        << "Manifest file does not exist for a binary executable file "
                        << binary
                    );

                return read( manifest );
            }

            static om::ObjPtr< Manifest > read( SAA_in const fs::path& filePath )
            {
                json::Value value;

                {
                    fs::SafeInputFileStreamWrapper file( filePath );
                    auto& is = file.stream();
                    value = json::readFromStream( is );
                }

                auto json = value.get_obj();

                const auto manifestVersion = getRequiredProperty( json, g_manifestVersion ).get_uint64();
                const auto serveridStr = getRequiredProperty( json, g_serverId ).get_str();
                const auto versionMajor = ( std::size_t ) getRequiredProperty( json, g_versionMajor ).get_uint64();
                const auto versionMinor = ( std::size_t ) getRequiredProperty( json, g_versionMinor ).get_uint64();
                const auto versionPatch = ( std::size_t ) getRequiredProperty( json, g_versionPatch ).get_uint64();
                const auto clsidsArr = getRequiredProperty( json, g_classIds ).get_array();

                const auto serverid = uuids::string2uuid( serveridStr );

                std::set< om::clsid_t > clsids;

                for( const auto& clsidStr : clsidsArr )
                {
                    clsids.insert( uuids::string2uuid( clsidStr.get_str() ) );
                }

                auto os = getRequiredProperty( json, g_os ).get_str();
                auto arch = getRequiredProperty( json, g_architecture ).get_str();
                auto toolchain = getRequiredProperty( json, g_toolchain ).get_str();

                auto platform = Platform::get(
                    std::move( os ),
                    std::move( arch ),
                    std::move( toolchain )
                    );

                const auto cppCompatIdStr = getRequiredProperty( json, g_cppCompatibilityId ).get_str();
                auto cppCompatId = uuids::string2uuid( cppCompatIdStr );

                BL_UNUSED( manifestVersion );

                om::ObjPtr< Manifest > manifest;

                const auto pluginClassIdVal = json[ g_pluginClassId ];
                const auto pluginClassId = uuids::string2uuid( pluginClassIdVal.get_str() );
                auto pluginName = getRequiredProperty( json, g_pluginName ).get_str();
                auto pluginDesc = getRequiredProperty( json, g_pluginDescription ).get_str();
                const auto isClient = getRequiredProperty( json, g_isClientPlugin ).get_bool();
                const auto isServer = getRequiredProperty( json, g_isServerPlugin ).get_bool();

                manifest = Manifest::createInstance(
                    serverid,
                    versionMajor,
                    versionMinor,
                    versionPatch,
                    std::move( clsids ),
                    pluginClassId,
                    std::move( pluginName ),
                    std::move( pluginDesc ),
                    isClient,
                    isServer,
                    std::move( platform ),
                    cppCompatId
                    );

                return manifest;
            }

            static void createAndWriteForLibrary(
                SAA_in      fs::path&&                                              library,
                SAA_in      om::ObjPtr< PlatformIdentity >&&                        platform
                )
            {
                auto output = Manifest::getManifestPath( library );
                auto manifest = create( std::move( library ), std::move( platform ) );

                write( std::move( manifest ), std::move( output ) );
            }

            static void writeForBinary(
                SAA_in      om::ObjPtr< Manifest >&&                                manifest,
                SAA_in      fs::path&&                                              binary
                )
            {
                auto output = Manifest::getManifestPath( binary );

                write( std::forward< om::ObjPtr< Manifest > >( manifest ), std::move( output ) );
            }

            static void write(
                SAA_in      om::ObjPtr< Manifest >&&                                manifest,
                SAA_in      fs::path&&                                              output
                )
            {
                json::Object json;

                json::Array clsids;

                for( const auto& clsid : manifest -> classIds() )
                {
                    clsids.push_back( uuids::uuid2string( clsid ) );
                }

                /*
                 * Must cast std::size_t to int for assignment to json value
                 */

                json[ g_manifestVersion ] = int( g_manifestVersionId );
                json[ g_serverId ] = uuids::uuid2string( manifest -> serverId() );
                json[ g_versionMajor ] = int( manifest -> versionMajor() );
                json[ g_versionMinor ] = int( manifest -> versionMinor() );
                json[ g_versionPatch ] = int( manifest -> versionPatch() );
                json[ g_classIds ] = clsids;
                json[ g_pluginClassId ] = uuids::uuid2string( manifest -> pluginClassId() );
                json[ g_pluginName ] = manifest -> pluginName();
                json[ g_pluginDescription ] = manifest -> pluginDescription();
                json[ g_isClientPlugin ] = manifest -> isClientPlugin();
                json[ g_isServerPlugin ] = manifest -> isServerPlugin();

                const auto& platform = manifest -> platform();

                json[ g_os ] = platform -> os();
                json[ g_architecture ] = platform -> architecture();
                json[ g_toolchain ] = platform -> toolchain();
                json[ g_cppCompatibilityId ] = uuids::uuid2string( manifest -> cppCompatibilityId() );

                {
                    fs::SafeOutputFileStreamWrapper file( output );
                    auto& os = file.stream();
                    json::saveToStream( json::Value( json ), os, json::OutputOptions::pretty_print );
                }
            }

        private:

            static const json::Value& getRequiredProperty(
                SAA_in      json::Object&               doc,
                SAA_in      const std::string&          property
                )
            {
                const auto& val = doc[ property ];

                BL_CHK_T_USER_FRIENDLY(
                    true,
                    val.is_null(),
                    UnexpectedException(),
                    BL_MSG()
                        << "Manifest does not contain required property '"
                        << property
                        << "'"
                    );

                return val;
            }

            static void registerPlugin(
                SAA_in      fs::path&&                                              library,
                SAA_out     om::serverid_t&                                         serverId,
                SAA_out     std::size_t&                                            versionMajor,
                SAA_out     std::size_t&                                            versionMinor,
                SAA_out     std::size_t&                                            versionPatch,
                SAA_out     std::set< om::clsid_t >&                                classIds,
                SAA_out     bl::uuid_t&                                             cppCompatId,
                SAA_out     om::clsid_t&                                            classId,
                SAA_out     std::string&                                            name,
                SAA_out     std::string&                                            description,
                SAA_out     bool&                                                   isClient,
                SAA_out     bool&                                                   isServer
                )
            {
                cpp::SafeUniquePtr< om::clsid_t[] > classIdsArrPtr;
                om::clsid_t* classIdsArr( classIdsArrPtr.get() );
                std::size_t classIdsArrSize = 0;
                const char* cname = nullptr;
                const char* cdesc = nullptr;

                const auto lib = os::loadLibrary( library.string() ).release();
                const auto fAddress = os::getProcAddress( lib, plugins::registerPluginProcName );
                const auto fPtr = cpp::union_cast< plugins::registerPluginProc >( fAddress );

                const auto rc = ( *fPtr )(
                    &serverId,
                    &versionMajor,
                    &versionMinor,
                    &versionPatch,
                    &cppCompatId,
                    &classIdsArr,
                    &classIdsArrSize,
                    &classId,
                    &cname,
                    &cdesc,
                    &isClient,
                    &isServer
                    );

                eh::EcUtils::checkErrorCode( rc );

                for( std::size_t i = 0; i < classIdsArrSize; ++i )
                {
                    classIds.insert( classIdsArr[ i ] );
                }

                name = std::string( cname );
                description = std::string( cdesc );
            }
        };

        template
        <
            typename E
        >
        const std::size_t
        ManifestFactoryT< E >::g_manifestVersionId = 1;

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_manifestVersion = "manifestVersion";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_serverId = "serverId";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_versionMajor = "versionMajor";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_versionMinor = "versionMinor";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_versionPatch = "versionPatch";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_classIds = "classIds";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_pluginClassId = "pluginClassId";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_pluginName = "pluginName";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_pluginDescription = "pluginDescription";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_isClientPlugin = "isClientPlugin";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_isServerPlugin = "isServerPlugin";
        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_os = "os";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_architecture = "architecture";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_toolchain = "toolchain";

        template
        <
            typename E
        >
        const std::string
        ManifestFactoryT< E >::g_cppCompatibilityId = "cppCompatibilityId";

        typedef ManifestFactoryT<> ManifestFactory;

    } // loader

} // bl

#endif /* __BL_LOADER_MANIFEST_H_ */
