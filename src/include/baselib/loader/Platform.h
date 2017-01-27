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

#ifndef __BL_LOADER_PLATFORM_H_
#define __BL_LOADER_PLATFORM_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace loader
    {
        /**
         * @brief class PlatformIdentity
         */

        template
        <
            typename E = void
        >
        class PlatformIdentityT : public om::Object
        {
            BL_DECLARE_OBJECT_IMPL_DEFAULT( PlatformIdentityT )

        private:

            const std::string                                   m_os;
            const std::string                                   m_architecture;
            const std::string                                   m_toolchain;

            static std::string removeCompilerId( SAA_in const std::string& toolchain )
            {
                const auto parts = str::splitString( toolchain, "-" );

                BL_CHK(
                    false,
                    parts.size() == 2,
                    BL_MSG()
                        << "Toolchain string "
                        << str::quoteString( toolchain )
                        << " is not in expected format: <compiler id>-<build flavor>"
                    );

                return parts.back();
            }

        public:

            PlatformIdentityT( SAA_in PlatformIdentityT&& other )
                :
                m_os( std::move( other.m_os ) ),
                m_architecture( std::move( other.m_architecture ) ),
                m_toolchain( std::move( other.m_toolchain ) )
            {
            }

            PlatformIdentityT& operator= ( SAA_in PlatformIdentityT&& other )
            {
                m_os = std::move( other.m_os );
                m_architecture = std::move( other.m_architecture );
                m_toolchain = std::move( other.m_toolchain );

                return *this;
            }

            PlatformIdentityT(
                SAA_in      std::string&&                       os,
                SAA_in      std::string&&                       architecture,
                SAA_in      std::string&&                       toolchain
                )
                :
                m_os( BL_PARAM_FWD( os ) ),
                m_architecture( BL_PARAM_FWD( architecture ) ),
                m_toolchain( BL_PARAM_FWD( toolchain ) )
            {
            }

            const std::string& os() const NOEXCEPT
            {
                return m_os;
            }

            const std::string& architecture() const NOEXCEPT
            {
                return m_architecture;
            }

            const std::string& toolchain() const NOEXCEPT
            {
                return m_toolchain;
            }

            std::string name( SAA_in_opt const bool ignoreCompilerId = false ) const
            {
                cpp::SafeOutputStringStream name;

                name
                    << m_os
                    << "_"
                    << m_architecture
                    << "_"
                    << ( ignoreCompilerId ? removeCompilerId( m_toolchain ) : m_toolchain );

                return name.str();
            }

            bool matchesToolchain(
                SAA_in          const std::string&                  value,
                SAA_in_opt      const bool                          ignoreCompilerId = false
                )
            {
                if( ignoreCompilerId )
                {
                    return removeCompilerId( m_toolchain ) == removeCompilerId( value );
                }
                else
                {
                    return m_toolchain == value;
                }
            }
        };

        typedef om::ObjectImpl< PlatformIdentityT<>, false, true, om::detail::LttLeaked > PlatformIdentity;

        /**
         * @brief class Platform
         */

        template
        <
            typename E = void
        >
        class PlatformT
        {
            BL_DECLARE_STATIC( PlatformT )

        public:

            static om::ObjPtr< PlatformIdentity > get(
                SAA_in      std::string&&                       os,
                SAA_in      std::string&&                       architecture,
                SAA_in      std::string&&                       toolchain
                )
            {
                return PlatformIdentity::createInstance(
                    std::forward< std::string >( os ),
                    std::forward< std::string >( architecture ),
                    std::forward< std::string >( toolchain )
                    );
            }
        };

        typedef PlatformT<> Platform;

    } // loader

} // bl

#endif /* __BL_LOADER_PLATFORM_H_ */
