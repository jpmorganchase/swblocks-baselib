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

#ifndef __BL_LOADER_RESOLVER_H_
#define __BL_LOADER_RESOLVER_H_

#include <baselib/loader/PluginAccess.h>

#include <baselib/core/EcUtils.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <unordered_map>
#include <set>

namespace bl
{
    namespace loader
    {
        /**
         * @brief class ResolverImpl
         */

        template
        <
            typename E = void
        >
        class ResolverImplT : public om::Resolver
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( ResolverImplT, om::Resolver )
            BL_CTR_DEFAULT( ResolverImplT, protected )

        private:

            om::ObjPtr< PluginAccess >                                                      m_serverAccess;
            om::ObjPtr< om::Object >                                                        m_coreServices;

            std::unordered_map< om::clsid_t, om::serverid_t >                               m_servers;
            std::unordered_map< om::serverid_t, om::clsid_t >                               m_plugins;
            std::unordered_map< om::serverid_t, om::ObjPtr< om::Factory > >                 m_factories;

        public:

            ResolverImplT( SAA_in om::ObjPtr< PluginAccess >&& serverAccess )
                :
                m_serverAccess( BL_PARAM_FWD( serverAccess ) )
            {
            }

            virtual int resolveServer(
                SAA_in          const om::clsid_t&                  clsid,
                SAA_out         om::serverid_t&                     serverid
                ) NOEXCEPT OVERRIDE
            {
                return eh::EcUtils::getErrorCode(
                    [ this, &clsid, &serverid ]
                    {
                        const auto pos = m_servers.find( clsid );

                        if( pos != m_servers.end() )
                        {
                            serverid = pos -> second;
                        }
                        else
                        {
                            serverid = m_serverAccess -> getServerId( clsid );

                            m_servers.emplace( clsid, serverid );
                        }
                    }
                    );
            }

            virtual int getFactory(
                SAA_in      const om::serverid_t&                   serverid,
                SAA_out     om::objref_t&                           factoryRef,
                SAA_in      const bool                              onlyIfLoaded = false
                ) NOEXCEPT OVERRIDE
            {
                return eh::EcUtils::getErrorCode(
                    [ this, &serverid, &factoryRef, &onlyIfLoaded ]
                    {
                        om::ObjPtr< om::Factory > factory;

                        const auto pos = m_factories.find( serverid );

                        if( pos != m_factories.end() )
                        {
                            factory = om::copy( pos -> second );
                        }
                        else
                        {
                            BL_CHK_T(
                                true,
                                onlyIfLoaded,
                                ClassNotFoundException(),
                                BL_MSG()
                                    << "Requested server has not been loaded: "
                                    << uuids::uuid2string( serverid )
                                );

                            factory = loadFactory( serverid );

                            m_factories.emplace( serverid, om::copy( factory ) );
                        }

                        factoryRef = reinterpret_cast< om::objref_t >( factory.release() );
                    }
                );
            }

            virtual int registerHost() NOEXCEPT OVERRIDE
            {
                return eh::EcUtils::getErrorCode(
                    [ this ]
                    {
                        const auto manifest = ManifestFactory::readForBinary(
                            os::getCurrentExecutablePath()
                            );

                        const auto serverid = manifest -> serverId();

                        for( const auto& clsid : manifest -> classIds() )
                        {
                            m_servers.emplace( clsid, serverid );
                        }

                        const auto ldr = om::detail::GlobalInit::getLoader();
                        const auto factory = ldr -> getDefaultFactory();

                         m_factories.emplace( serverid, om::copy( factory ) );
                    }
                    );
            }

            virtual int getCoreServices( SAA_out_opt om::objref_t& coreServices ) NOEXCEPT OVERRIDE
            {
                if( m_coreServices )
                {
                    m_coreServices -> addRef();
                }

                coreServices = reinterpret_cast< om::objref_t >( m_coreServices.get() );

                return eh::errc::success;
            }

            virtual int setCoreServices( SAA_in_opt om::objref_t coreServices ) NOEXCEPT OVERRIDE
            {
                m_coreServices = om::copy( reinterpret_cast< Object* >( coreServices ) );

                return eh::errc::success;
            }

        protected:

            om::ObjPtr< om::Factory > loadFactory( SAA_in const om::serverid_t& serverid )
            {
                /*
                 * Load library and get pointer to registerResolver function
                 */

                const auto library = m_serverAccess -> getLibrary( serverid );
                const auto libraryRef = os::loadLibrary( library ).release();

                const auto fAddress = os::getProcAddress(
                    libraryRef,
                    plugins::registerResolverProcName
                    );

                /*
                 * Call registerResolver function and return factory
                 */

                const auto fnRegisterResolver = cpp::union_cast< plugins::registerResolverProc >( fAddress );
                const auto resolverPtr = static_cast< void* >( this );
                void* factoryPtr = nullptr;

                eh::EcUtils::checkErrorCode(
                    fnRegisterResolver( resolverPtr, &factoryPtr )
                    );

                auto factory = om::wrap< om::Factory >(
                    reinterpret_cast< om::objref_t >( factoryPtr )
                    );

                initPlugin( serverid, factory );

                return factory;
            }

            void initPlugin(
                SAA_in      const om::serverid_t&                       serverid,
                SAA_in      const om::ObjPtr< om::Factory >&            factory
                )
            {
                om::clsid_t pluginClassId = getPluginClassId( serverid );

                if( pluginClassId != uuids::nil() )
                {
                    /*
                     * The class is not in the host (agent), it is in a plug-in
                     */

                    auto plugin = om::wrap< om::Plugin >(
                        factory -> createInstance( pluginClassId, iids::Plugin() )
                        );

                    eh::EcUtils::checkErrorCode(
                        plugin -> init()
                        );
                }
            }

            om::clsid_t getPluginClassId( SAA_in const om::serverid_t& serverid )
            {
                const auto pos = m_plugins.find( serverid );

                if( pos != m_plugins.end() )
                {
                    return pos -> second;
                }

                const auto clsid = m_serverAccess -> getPluginClassId( serverid );

                m_plugins.emplace( serverid, clsid );

                return clsid;
            }
        };

        typedef om::ObjectImpl< ResolverImplT<>, false /* enableSharedPtr */, true /* isMultiThreaded */, om::detail::LttLeaked > ResolverImplDefault;

    } // loader

} // bl

#endif /* __BL_LOADER_RESOLVER_H_ */

