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

#ifndef __BL_PLUGINDEFS_H_
#define __BL_PLUGINDEFS_H_

#include <build/PluginBuildId.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

/**
 * @brief Macros to define plug-in
 */

namespace bl
{
    namespace plugins
    {
        /**
         * @brief Plug-in registration function prototype
         */

        namespace
        {
            const std::string   registerResolverProcName =  "registerResolver";
            const std::string   registerPluginProcName =    "registerPlugin";
        }

        extern "C"
        {
            typedef int ( *registerResolverProc ) (
                void*                   /* resolverPtr */,
                void**                  /* factoryPtr */
                );

            typedef int ( *registerPluginProc ) (
                om::serverid_t*         /* id */,
                std::size_t*            /* majorVersion */,
                std::size_t*            /* minorVersion */,
                std::size_t*            /* patchVersion */,
                uuid_t*                 /* cppCompatibilityId */,
                om::clsid_t**           /* clsids */,
                std::size_t*            /* clsidsSize */,
                om::clsid_t*            /* clsid */,
                const char**            /* name */,
                const char**            /* description */,
                bool*                   /* isClient */,
                bool*                   /* isServer */
                );
        }

        class PluginInfo
        {
            BL_NO_CREATE( PluginInfo )
            BL_NO_COPY_OR_MOVE( PluginInfo )

        public:

            virtual const bl::uuid_t& id() const = 0;
            virtual const std::size_t& majorVersion() const = 0;
            virtual const std::size_t& minorVersion() const = 0;
            virtual const std::size_t& patchVersion() const = 0;
            virtual const std::unordered_map< om::clsid_t, om::register_class_callback_t >* classes() const = 0;
            virtual const std::string& name() const = 0;
            virtual const std::string& description() const = 0;
            virtual const om::clsid_t& clsid() const = 0;
            virtual const bool& isClient() const = 0;
            virtual const bool& isServer() const = 0;
            virtual ~PluginInfo() NOEXCEPT {};
        };

        class PersonalityInfo
        {
            BL_NO_CREATE( PersonalityInfo )
            BL_NO_COPY_OR_MOVE( PersonalityInfo )

        public:

            virtual const std::string& name() const = 0;
            virtual const std::size_t& version() const = 0;
            virtual const std::vector< cpp::SafeUniquePtr< plugins::PluginInfo > >* plugins() const = 0;
            virtual ~PersonalityInfo() NOEXCEPT {};
        };

    } // plugins

} // bl

/**
 * Plug-in declaration macros
 *
 * @note Macro BL_PLUGINS_CLASS_IMPLEMENTATION must be defined in all compilation units which provide
 * actual implementations of plug-in classes (as opposed to just referencing them)
 */

#define BL_PLUGINS_DECLARE_PLUGIN_BEGIN( p_className, p_name, p_description, p_serverid, p_majorVersion, p_minorVersion, p_clsid, p_isClient, p_isServer ) \
    namespace bl \
    { \
        namespace plugins \
        { \
            template \
            < \
                typename E = void \
            > \
            class p_className ## T : public PluginInfo \
            { \
            private: \
                \
                const  bl::om::serverid_t       m_serverid; \
                \
                const  std::size_t              m_majorVersion; \
                const  std::size_t              m_minorVersion; \
                const  std::size_t              m_patchVersion; \
                \
                const  bool                     m_isClientPlugin; \
                const  bool                     m_isServerPlugin; \
                const  bl::om::clsid_t          m_pluginClassId; \
                \
            public: \
                \
                p_className ## T() \
                    : \
                    m_serverid( bl::uuids::string2uuid( p_serverid ) ), \
                    m_majorVersion( p_majorVersion ), \
                    m_minorVersion( p_minorVersion ), \
                    m_patchVersion( BL_PLUGINS_BUILD_ID ), \
                    m_isClientPlugin( p_isClient ), \
                    m_isServerPlugin( p_isServer ), \
                    m_pluginClassId( p_clsid ) \
                { \
                } \
                \
                virtual const bl::om::serverid_t& id() const NOEXCEPT OVERRIDE \
                { \
                    return m_serverid; \
                } \
                \
                virtual const std::size_t& majorVersion() const NOEXCEPT OVERRIDE \
                { \
                    return m_majorVersion; \
                } \
                \
                virtual const std::size_t& minorVersion() const NOEXCEPT OVERRIDE \
                { \
                    return m_minorVersion; \
                } \
                \
                virtual const std::size_t& patchVersion() const NOEXCEPT OVERRIDE \
                { \
                    return m_patchVersion; \
                } \
                \
                virtual const std::unordered_map< bl::om::clsid_t, bl::om::register_class_callback_t >* classes() const OVERRIDE \
                { \
                    static const std::unordered_map< bl::om::clsid_t, bl::om::register_class_callback_t >* classes( initClasses() ); \
                    return classes; \
                } \
                \
                virtual const std::string& name() const NOEXCEPT OVERRIDE \
                { \
                    static const std::string pluginName = p_name; \
                    return pluginName; \
                } \
                \
                virtual const std::string& description() const NOEXCEPT OVERRIDE \
                { \
                    static const std::string description = p_description; \
                    return description; \
                } \
                \
                virtual const bl::om::clsid_t& clsid() const NOEXCEPT OVERRIDE \
                { \
                    return m_pluginClassId; \
                } \
                \
                virtual const bool& isClient() const NOEXCEPT OVERRIDE \
                { \
                    return m_isClientPlugin; \
                } \
                \
                virtual const bool& isServer() const NOEXCEPT OVERRIDE \
                { \
                    return m_isServerPlugin; \
                } \
                \
            private: \
                \
                static std::unordered_map< bl::om::clsid_t, bl::om::register_class_callback_t >* initClasses() \
                { \
                    static std::unordered_map< bl::om::clsid_t, bl::om::register_class_callback_t > classes; \

#ifdef BL_PLUGINS_CLASS_IMPLEMENTATION

#define BL_PLUGINS_DECLARE_PLUGIN_CLASS( p_name, p_id ) \
                    { \
                        const auto cbCreate = [] \
                        ( \
                            SAA_in       const bl::om::iid_t&   iid, \
                            SAA_inout    bl::om::Object*        identity \
                        ) -> bl::om::objref_t \
                        { \
                            BL_UNUSED( identity ); \
                            return p_name::createInstance() -> queryInterface( iid ); \
                        }; \
                        \
                        classes[ p_id ] = cbCreate; \
                    } \

#else

#define BL_PLUGINS_DECLARE_PLUGIN_CLASS( p_name, p_id )     /* do not register the class */

#endif /* BL_PLUGINS_CLASS_IMPLEMENTATION */

#define BL_PLUGINS_DECLARE_PLUGIN_END( p_className ) \
                    \
                    return &classes; \
                } \
            }; \
            \
            typedef p_className ## T<> p_className; \
            \
        } \
    } \

/**
 * Personality declaration macros
 */

#define BL_DECLARE_PERSONALITY_START( p_className, p_name ) \
    namespace bl \
    { \
        namespace plugins \
        { \
            template \
            < \
                typename E = void \
            > \
            class p_className ## T : public PersonalityInfo \
            { \
            private: \
                \
                const std::string                                                       m_name;  \
                \
                const std::size_t                                                       m_version; \
                \
                const std::vector< bl::cpp::SafeUniquePtr< bl::plugins::PluginInfo > >* m_plugins; \
                \
            public: \
                \
                p_className ## T() \
                    : \
                    m_name( p_name ), \
                    m_version( BL_PLUGINS_BUILD_ID ), \
                    m_plugins( initPlugins() ) \
                { \
                }\
                \
                virtual const std::string& name() const NOEXCEPT OVERRIDE \
                { \
                    return m_name; \
                } \
                \
                virtual const std::size_t& version() const NOEXCEPT OVERRIDE \
                { \
                    return m_version; \
                } \
                \
                virtual const std::vector< bl::cpp::SafeUniquePtr< bl::plugins::PluginInfo > >* plugins() const OVERRIDE \
                { \
                    return m_plugins; \
                } \
                \
            private: \
                \
                static std::vector< bl::cpp::SafeUniquePtr< bl::plugins::PluginInfo > >* initPlugins() \
                { \
                    static std::vector< bl::cpp::SafeUniquePtr< bl::plugins::PluginInfo > > plugins; \

#define BL_DECLARE_PERSONALITY_PLUGIN( plugin ) \
                    plugins.push_back( bl::cpp::SafeUniquePtr< bl::plugins::PluginInfo >::attach( new plugin ) ); \

#define BL_DECLARE_PERSONALITY_END( p_className ) \
                    return &plugins; \
                } \
            }; \
            \
            typedef p_className ## T<> p_className; \
        } \
    } \

#define BL_REGISTER_PERSONALITIES_START() \
    namespace bl \
    { \
        namespace plugins \
        { \
            template \
            < \
                typename E = void \
            > \
            class PersonalitiesT \
            { \
                BL_DECLARE_STATIC( PersonalitiesT ) \
            \
            public: \
            \
                static const std::vector< bl::cpp::SafeUniquePtr< PersonalityInfo > >* get() \
                { \
                    static const std::vector< bl::cpp::SafeUniquePtr< PersonalityInfo > >* personalities( initPersonalities() ); \
                    return personalities; \
                } \
                \
            private: \
            \
                static std::vector< bl::cpp::SafeUniquePtr< PersonalityInfo > >* initPersonalities() \
                { \
                    static std::vector< bl::cpp::SafeUniquePtr< PersonalityInfo > > personalities; \

#define BL_REGISTER_PERSONALITY( personality ) \
                    personalities.push_back( bl::cpp::SafeUniquePtr< PersonalityInfo >::attach( new personality ) ); \

#define BL_REGISTER_PERSONALITIES_END() \
                    return &personalities; \
                } \
            }; \
            \
            typedef PersonalitiesT<> Personalities; \
        } \
    } \

/*
 * Plug-in registration macros
 */

#if defined( BL_PLUGINS_NO_EXPORT )
#define BL_PLUGINS_EXPORT
#else
#if defined( _WIN32 )
#define BL_PLUGINS_EXPORT __declspec( dllexport )
#else
#define BL_PLUGINS_EXPORT __attribute__ ( ( visibility ( "default" ) ) )
#endif // defined( _WIN32 )
#endif // defined( BL_PLUGINS_NO_EXPORT )

/**
 * UUID used to determine compatibility between agent and plug-ins in terms of
 * basic C++ interface, e.g. version of boost, etc.
 */

#define BL_PLUGINS_CPP_COMPATIBILITY_ID "bbdd10fe-4481-48dd-87f1-7d71c13364c8"

#define BL_PLUGINS_REGISTER_PLUGIN( plugin ) \
    extern "C" BL_PLUGINS_EXPORT int registerResolver( \
        SAA_in          void*                           resolverPtr, \
        SAA_out         void**                          factoryPtr \
        ) \
    { \
        return bl::eh::EcUtils::getErrorCode( \
            [ &resolverPtr, &factoryPtr ] \
            { \
                plugin p; \
                \
                for( const auto& cls : *p.classes() ) \
                { \
                    bl::om::clsid_t clsid = cls.first; \
                    bl::om::register_class_callback_t cbCreate = cls.second; \
                    \
                    bl::om::registerClass( std::move( clsid ), std::move( cbCreate ) ); \
                } \
                \
                *factoryPtr = bl::om::registerResolverImpl( resolverPtr ); \
            } \
            ); \
    } \
    \
    extern "C" BL_PLUGINS_EXPORT int registerPlugin( \
        SAA_out         bl::om::serverid_t*             id, \
        SAA_out         std::size_t*                    versionMajor, \
        SAA_out         std::size_t*                    versionMinor, \
        SAA_out         std::size_t*                    versionPatch, \
        SAA_out         bl::uuid_t*                     cppCompatibilityId, \
        SAA_out         bl::om::clsid_t**               clsids, \
        SAA_out         std::size_t*                    clsidsSize, \
        SAA_out         bl::om::clsid_t*                clsid, \
        SAA_out         const char**                    name, \
        SAA_out         const char**                    description, \
        SAA_out         bool*                           isClient, \
        SAA_out         bool*                           isServer \
        ) \
    { \
        return bl::eh::EcUtils::getErrorCode( \
            [ \
                &id, \
                &versionMajor, \
                &versionMinor, \
                &versionPatch, \
                &cppCompatibilityId, \
                &clsids, \
                &clsidsSize, \
                &clsid, \
                &name, \
                &description, \
                &isClient, \
                &isServer \
            ] \
            { \
                plugin p; \
                \
                *id = p.id(); \
                *versionMajor = p.majorVersion(); \
                *versionMinor = p.minorVersion(); \
                *versionPatch = p.patchVersion(); \
                *cppCompatibilityId = bl::uuids::string2uuid( BL_PLUGINS_CPP_COMPATIBILITY_ID ); \
                *clsidsSize = p.classes() -> size(); \
                *clsids = new bl::om::clsid_t[ *clsidsSize ]; \
                \
                int i = 0; \
                for( const auto& cls : *p.classes() ) \
                { \
                    ( *clsids )[ i++ ] = cls.first; \
                } \
                \
                *clsid = p.clsid(); \
                *name = p.name().c_str(); \
                *description = p.description().c_str(); \
                *isClient = p.isClient(); \
                *isServer = p.isServer(); \
            } \
            ); \
    } \

#endif /* __BL_PLUGINDEFS_H_ */
