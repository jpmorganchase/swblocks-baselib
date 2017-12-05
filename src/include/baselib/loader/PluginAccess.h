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

#ifndef __BL_LOADER_PLUGINACCESS_H_
#define __BL_LOADER_PLUGINACCESS_H_

#include <baselib/loader/Manifest.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

#include <vector>
#include <unordered_map>

namespace bl
{
    namespace loader
    {
        /**
         * @brief class PluginAccess
         */

        template
        <
            typename E = void
        >
        class PluginAccessT : public om::Object
        {
            BL_DECLARE_OBJECT_IMPL_DEFAULT( PluginAccessT )

        private:

            std::unordered_map< om::clsid_t, om::serverid_t >               m_servers;
            std::unordered_map< om::serverid_t, std::string >               m_libraries;
            std::unordered_map< om::serverid_t, om::clsid_t >               m_plugins;

            bool registerPlugin(
                SAA_in      const fs::path&                         plugin,
                SAA_in      const om::ObjPtr< Manifest >&           manifest
                )
            {
                const auto& serverid = manifest -> serverId();

                if( ! m_plugins.emplace( serverid, manifest -> pluginClassId() ).second )
                {
                    BL_LOG(
                        Logging::warning(),
                        BL_MSG()
                            << "Ignoring plug-in '"
                            << manifest -> pluginName()
                            << "' having a duplicate server id: "
                            << uuids::uuid2string( serverid )
                        );

                    return false;
                }

                for( const auto& clsid : manifest -> classIds() )
                {
                    if( ! m_servers.emplace( clsid, serverid ).second )
                    {
                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "Found duplicate class id: "
                                << uuids::uuid2string( clsid )
                                << " in plug-in '"
                                << manifest -> pluginName()
                                << "'"
                            );
                    }
                }

                m_libraries.emplace( serverid, plugin.string() );

                return true;
            }

        public:

            PluginAccessT( SAA_in const std::vector< std::string >& plugins )
            {
                for( const auto& plugin : plugins )
                {
                    registerPlugin( plugin, ManifestFactory::readForBinary( plugin ) );
                }
            }

            PluginAccessT( SAA_in const std::unordered_map< fs::path, om::ObjPtr< Manifest > >& plugins )
            {
                for( const auto& entry : plugins )
                {
                    registerPlugin( entry.first, entry.second );
                }
            }

            void getPluginClassIds( SAA_out std::vector< om::clsid_t >& pluginClasses ) const
            {
                for( const auto& entry : m_plugins )
                {
                    pluginClasses.push_back( entry.second );
                }
            }

            om::serverid_t getServerId( SAA_in const om::clsid_t& clsid ) const
            {
                const auto pos = m_servers.find( clsid );

                if( pos != m_servers.end() )
                {
                    return pos -> second;
                }

                BL_THROW(
                    ClassNotFoundException(),
                    BL_MSG()
                        << "Could not find server id for class id: "
                        << uuids::uuid2string( clsid )
                    );
            }

            om::clsid_t getPluginClassId( SAA_in const om::serverid_t& serverid ) const
            {
                const auto pos = m_plugins.find( serverid );

                if( pos != m_plugins.end() )
                {
                    return pos -> second;
                }

                BL_THROW(
                    ClassNotFoundException(),
                    BL_MSG()
                        << "Could not find plug-in class id for server id: "
                        << uuids::uuid2string( serverid )
                    );
            }

            std::string getLibrary( SAA_in const om::serverid_t& serverid ) const
            {
                const auto pos = m_libraries.find( serverid );

                if( pos != m_libraries.end() )
                {
                    return pos -> second;
                }

                BL_THROW(
                    ClassNotFoundException(),
                    BL_MSG()
                        << "Could not find library for server id: "
                        << uuids::uuid2string( serverid )
                    );
            }
        };

        typedef om::ObjectImpl< PluginAccessT<>, false, true, om::detail::LttLeaked > PluginAccess;

    } // loader

} // bl

#endif /* __BL_LOADER_PLUGINACCESS_H_ */
