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

#ifndef __BL_UUID_H_
#define __BL_UUID_H_

#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/detail/UuidImpl.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/TlsState.h>

#include <sstream>
#include <iostream>

#define BL_UUID_DECLARE_FULL( ns, name, value ) \
    namespace ns \
    { \
        namespace detail \
        { \
            template \
            < \
                typename E = void \
            > \
            class uuiddef_ ## name ## _t \
            { \
            public: \
            \
                static const bl::uuid_t g_uuid; \
            }; \
            \
            \
            template \
            < \
                typename E \
            > \
            const bl::uuid_t \
            uuiddef_ ## name ## _t< E >::g_uuid = bl::uuids::string2uuid( value ); \
            \
            typedef uuiddef_ ## name ## _t<> uuiddef_ ## name; \
        } \
        \
        inline const bl::uuid_t& name() \
        { \
            return detail::uuiddef_ ## name::g_uuid; \
        } \
    } \

#define BL_UUID_DECLARE( name, value ) \
    BL_UUID_DECLARE_FULL( uuiddefs, name, value ) \

namespace bl
{
    namespace detail
    {
        /**
         * @brief class Uuid
         */
        template
        <
            typename E = void
        >
        class UuidT
        {
        public:

            static std::string uuid2string( SAA_in const uuid_t& uuid )
            {
                cpp::SafeOutputStringStream os;
                os << uuid;
                return os.str();
            }

            static uuid_t string2uuid( SAA_in const std::string& value )
            {
                cpp::SafeInputStringStream is( value );

                /*
                 * Keeping std::ios::failbit in the exceptions mask for now
                 * Need to implement bl::cpp::getline to handle std::ios::failbit for input streams
                 */

                is.exceptions( std::ios::failbit );
                uuid_t uuid;
                is >> uuid;
                return uuid;
            }

            static bool isUuid( SAA_in const std::string& value )
            {
                return str::regex_match( value, g_uuidRegex );
            }

            static bool containsUuid( SAA_in const std::string& value )
            {
                return str::regex_search( value, g_uuidRegex );
            }

            static const uuid_t& nil()
            {
                return g_nil;
            }

            static std::string generateUuidDef(
                SAA_in    const uuid_t&            uuid,
                SAA_in    const std::string&       name = "MyUuid"
                )
            {
                std::string value;

                value += "BL_UUID_DECLARE( ";
                value += name;
                value += ", \"";
                value += uuid2string( uuid );
                value += "\" )";

                return value;
            }

        private:

            static uuid_t               g_nil;
            static str::regex           g_uuidRegex;
        };

        BL_DEFINE_STATIC_MEMBER( UuidT, uuid_t, g_nil ) = uuids::nil_uuid();

        BL_DEFINE_STATIC_MEMBER( UuidT, str::regex, g_uuidRegex ) =
            str::regex( "[a-f\\d]{8}-([a-f\\d]{4}-){3}[a-f\\d]{12}", str::regex::icase );

        typedef UuidT<> Uuid;

    } // detail

    /*
     * The uuid public interface
     */

    namespace uuids
    {
        inline uuid_t create()
        {
            return tlsData().random.uuidGen()();
        }

        inline std::string uuid2string( SAA_in const uuid_t& uuid )
        {
            return detail::Uuid::uuid2string( uuid );
        }

        inline uuid_t string2uuid( SAA_in const std::string& value )
        {
            return detail::Uuid::string2uuid( value );
        }

        inline bool isUuid( SAA_in const std::string& value )
        {
            return detail::Uuid::isUuid( value );
        }

        inline bool containsUuid( SAA_in const std::string& value )
        {
            return detail::Uuid::containsUuid( value );
        }

        inline const uuid_t& nil()
        {
            return detail::Uuid::nil();
        }

        inline std::string generateUuidDef(
            SAA_in    const uuid_t&            uuid,
            SAA_in    const std::string&       name = "MyUuid"
            )
        {
            return detail::Uuid::generateUuidDef( uuid, name );
        }

    } // uuids

} // bl

#endif /* __BL_UUID_H_ */

