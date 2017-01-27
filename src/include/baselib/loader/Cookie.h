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

#ifndef __BL_LOADER_COOKIE_H_
#define __BL_LOADER_COOKIE_H_

#include <baselib/core/Uuid.h>
#include <baselib/core/FsUtils.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>

namespace bl
{
    namespace loader
    {
        /**
         * @brief class Cookie
         */

        template
        <
            typename E = void
        >
        class CookieT : public om::Object
        {
            BL_DECLARE_OBJECT_IMPL_DEFAULT( CookieT )

        private:

            const bl::uuid_t            m_id;
            const std::string           m_idStr;

        public:

            CookieT( SAA_in const bl::uuid_t& id )
                :
                m_id( id ),
                m_idStr( uuids::uuid2string( id ) )
            {
            }

            const uuid_t& getId() const NOEXCEPT
            {
                return m_id;
            }

            const std::string& getIdStr() const NOEXCEPT
            {
                return m_idStr;
            }
        };

        typedef om::ObjectImpl< CookieT<>, false, true, om::detail::LttLeaked > Cookie;

        /**
         * @brief class CookieFactory
         */

        template
        <
            typename E = void
        >
        class CookieFactoryT
        {
        public:

            static om::ObjPtr< Cookie > getCookie(
                SAA_in      const fs::path&                 filePath,
                SAA_in      const bool                      createIfNecessary
                )
            {
                if( ! fs::exists( filePath ) )
                {
                    BL_CHK_USER_FRIENDLY(
                        createIfNecessary,
                        false,
                        BL_MSG()
                            << "Cookie file does not exist at path "
                            << filePath
                        );

                    fs::safeMkdirs( filePath.parent_path() );

                    /*
                     * Write new id to file
                     */

                    fs::SafeOutputFileStreamWrapper file( filePath );
                    auto& os = file.stream();

                    os << uuids::uuid2string( uuids::create() ) << std::endl;
                }

                BL_CHK_USER_FRIENDLY(
                    fs::is_regular_file( filePath ),
                    false,
                    BL_MSG()
                        << "Cannot read cookie file "
                        << filePath
                        << "; path exists but is not a regular file"
                    );

                fs::SafeInputFileStreamWrapper file( filePath );
                auto& is = file.stream();

                /*
                 * Read cookie from file
                 */

                std::string line;
                std::getline( is, line );

                bl::uuid_t cookie = uuids::string2uuid( line );

                return Cookie::createInstance( cookie );
            }
        };

        typedef CookieFactoryT<> CookieFactory;

    } // loader

} // bl

#endif /* __BL_LOADER_COOKIE_H_ */
