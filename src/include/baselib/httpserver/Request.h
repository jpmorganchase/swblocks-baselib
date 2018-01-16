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

#ifndef __BL_HTTPSERVER_REQUEST_H_
#define __BL_HTTPSERVER_REQUEST_H_

#include <baselib/http/Globals.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace httpserver
    {
        /**
         * @brief class Request
         */

        template
        <
            typename E = void
        >
        class RequestT : public om::ObjectDefaultBase
        {
        public:

            typedef http::HeadersMap                            HeadersMap;

        private:

            const std::string                                   m_method;
            const std::string                                   m_uri;
            HeadersMap                                          m_headers;
            const std::string                                   m_body;

        protected:

            RequestT(
                SAA_in      std::string&&                       method,
                SAA_in      std::string&&                       uri,
                SAA_in_opt  HeadersMap&&                        headers = HeadersMap(),
                SAA_in_opt  std::string&&                       body = std::string()
                )
                :
                m_method( BL_PARAM_FWD( method ) ),
                m_uri( BL_PARAM_FWD( uri ) ),
                m_headers( BL_PARAM_FWD( headers ) ),
                m_body( BL_PARAM_FWD( body ) )
            {
            }

        public:

            auto method() const NOEXCEPT -> const std::string&
            {
                return m_method;
            }

            auto uri() const NOEXCEPT -> const std::string&
            {
                return m_uri;
            }

            auto headers() const NOEXCEPT -> const HeadersMap&
            {
                return m_headers;
            }

            auto body() const NOEXCEPT -> const std::string&
            {
                return m_body;
            }
        };

        typedef om::ObjectImpl< RequestT<> > Request;

    } // httpserver

} // bl

#endif /* __BL_HTTPSERVER_REQUEST_H_ */
