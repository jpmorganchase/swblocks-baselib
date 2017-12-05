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

#ifndef __BL_LOADER_PERSONALITY_H_
#define __BL_LOADER_PERSONALITY_H_

#include <baselib/loader/Manifest.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <vector>
#include <unordered_map>

BL_IID_DECLARE( Personality, "946cd274-ed13-4cd0-ba3d-f5b51b23d72a" )

namespace bl
{
    namespace loader
    {
        /*
         * @brief interface Personality
         */

        class Personality : public om::Object
        {
            BL_DECLARE_INTERFACE( Personality )

        public:

            virtual const std::unordered_map< fs::path, om::ObjPtr< Manifest > >& getClientPlugins() const = 0;

            virtual const std::unordered_map< fs::path, om::ObjPtr< Manifest > >& getServerPlugins() const = 0;

            virtual bool checkForUpdate() = 0;
        };

        /**
         * @brief class PersonalityBase
         */

        class PersonalityBase : public Personality
        {
            BL_DECLARE_ABSTRACT( PersonalityBase )

        protected:

            std::unordered_map< fs::path, om::ObjPtr< Manifest > >              m_clientPlugins;
            std::unordered_map< fs::path, om::ObjPtr< Manifest > >              m_serverPlugins;

        public:

            virtual const std::unordered_map< fs::path, om::ObjPtr< Manifest > >& getClientPlugins() const OVERRIDE
            {
                return m_clientPlugins;
            }

            virtual const std::unordered_map< fs::path, om::ObjPtr< Manifest > >& getServerPlugins() const OVERRIDE
            {
                return m_serverPlugins;
            }

        protected:

            void initPlugins( SAA_in const std::vector< fs::path >& plugins )
            {
                for( const auto& plugin : plugins )
                {
                    const auto manifest = ManifestFactory::readForBinary( plugin );

                    logPluginProperties( plugin, manifest );

                    if( manifest -> isClientPlugin() )
                    {
                        m_clientPlugins.emplace( plugin, om::copy( manifest ) );
                    }

                    if( manifest -> isServerPlugin() )
                    {
                        m_serverPlugins.emplace( plugin, om::copy( manifest ) );
                    }
                }
            }

            static void findPlugins(
                SAA_in      const fs::path&                     dir,
                SAA_out     std::vector< fs::path >&            plugins
                )
            {
                BL_CHK_T_USER_FRIENDLY(
                    false,
                    fs::exists( dir ) && fs::is_directory( dir ),
                    UnexpectedException(),
                    BL_MSG()
                        << "Directory "
                        << dir
                        << " does not exist"
                    );

                fs::directory_iterator end;

                for( fs::directory_iterator itr( dir ); itr != end; ++itr )
                {
                    const auto path = itr -> path();

                    if( isPlugin( path ) )
                    {
                        plugins.push_back( path );
                    }
                }
            }

            static bool isPlugin( SAA_in const fs::path& path )
            {
                return fs::is_regular_file( Manifest::getManifestPath( path ) );
            }

            static void logPluginProperties(
                SAA_in      const fs::path&                             plugin,
                SAA_in      const om::ObjPtr< Manifest >&               manifest
                )
            {
                cpp::SafeOutputStringStream clsids;

                clsids << "[";

                bool isFirst = true;

                for( const auto& clsid : manifest -> classIds() )
                {
                    if( isFirst )
                    {
                        isFirst = false;
                    }
                    else
                    {
                        clsids << ",";
                    }

                    clsids << "\n    " << uuids::uuid2string( clsid );
                }

                clsids << "\n  ]";

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "Properties for plug-in '"
                        << plugin.filename().string()
                        << "':\n  serverId: "
                        << uuids::uuid2string( manifest -> serverId() )
                        << "\n  version: "
                        << manifest -> version()
                        << "\n  platform: "
                        << manifest -> platform() -> name()
                        << "\n  cppCompatibilityId: "
                        << uuids::uuid2string( manifest -> cppCompatibilityId() )
                        << "\n  pluginName: "
                        << manifest -> pluginName()
                        << "\n  pluginClassId: "
                        << uuids::uuid2string( manifest -> pluginClassId() )
                        << "\n  pluginDescription: "
                        << manifest -> pluginDescription()
                        << "\n  isClientPlugin: "
                        << ( manifest -> isClientPlugin() ? "true" : "false" )
                        << "\n  isServerPlugin: "
                        << ( manifest -> isServerPlugin() ? "true" : "false" )
                        << "\n  classIds: "
                        << clsids.str()
                    );
            }
        };

        /**
         * @brief class PersonalityFileSystem
         */

        template
        <
            typename E = void
        >
        class PersonalityFileSystemT : public PersonalityBase
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( PersonalityFileSystemT, Personality )

        public:

            PersonalityFileSystemT( SAA_in const fs::path& dir )
            {
                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Reading personality from directory '"
                        << dir.string()
                        << "'"
                    );

                std::vector< fs::path > plugins;

                findPlugins( dir, plugins );

                if( plugins.empty() )
                {
                    BL_LOG( Logging::warning(), "No plug-ins found" );
                }
                else
                {
                    initPlugins( plugins );
                }
            }

            virtual bool checkForUpdate() OVERRIDE
            {
                /*
                 * Not applicable
                 */

                return false;
            }
        };

        typedef om::ObjectImpl< PersonalityFileSystemT<>, false, true, om::detail::LttLeaked > PersonalityFileSystem;

    } // loader

} // bl

#endif /* __BL_LOADER_PERSONALITY_H_ */
