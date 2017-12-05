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

#ifndef __BL_HTTPSERVER_RESPONSE_H_
#define __BL_HTTPSERVER_RESPONSE_H_

#include <baselib/http/Globals.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace httpserver
    {
        /**
         * @brief class HttpResponse
         */

        template
        <
            typename E = void
        >
        class ResponseT : public om::ObjectDefaultBase
        {
        public:

            typedef http::Parameters::HttpHeader                                HttpHeader;
            typedef http::HeadersMap                                            HeadersMap;
            typedef http::Parameters::HttpStatusCode                            HttpStatusCode;

        private:

            const HttpStatusCode                                                m_status;

            const std::string                                                   m_content;

            HeadersMap                                                          m_headers;

            std::string                                                         m_serialized;

            static auto getRequiredHeaders(
                SAA_in      const std::string&                                  content,
                SAA_in      std::string&&                                       contentType
                )
                -> HeadersMap
            {
                HeadersMap headers;

                headers.emplace(
                    HttpHeader::g_contentType,
                    BL_PARAM_FWD( contentType )
                    );

                headers.emplace(
                    HttpHeader::g_contentLength,
                    utils::lexical_cast< std::string >( content.size() )
                    );

                /*
                 * Currently the HTTP server implementation does not support HTTP 1.1
                 * persistent connections (Keep-Alive) and pipelining, so we request
                 * that the connection is closed
                 */

                headers.emplace(
                    HttpHeader::g_connection,
                    cpp::copy( HttpHeader::g_close )
                    );

                return headers;
            }

            static std::string getStockResponse( HttpStatusCode status )
            {
                MessageBuffer buffer;

                buffer
                    << "{"
                    << "\"statusCode\" : "
                    << status
                    << "}"
                    ;

                return resolveMessage( buffer );
            }

        protected:

            ResponseT( SAA_in const HttpStatusCode status )
                :
                m_status( status ),
                m_content( getStockResponse( status ) ),
                m_headers( getRequiredHeaders( m_content, cpp::copy( HttpHeader::g_contentTypeDefault ) ) )
            {
                m_serialized = buildResponse();
            }

            ResponseT(
                SAA_in          const HttpStatusCode                            status,
                SAA_in          std::string&&                                   content,
                SAA_in_opt      std::string&&                                   contentType = std::string(),
                SAA_in_opt      HeadersMap&&                                    customHeaders = HeadersMap()
                )
                :
                m_status( status ),
                m_content( BL_PARAM_FWD( content ) ),
                m_headers(
                    getRequiredHeaders( m_content,
                    contentType.empty() ?
                        cpp::copy( HttpHeader::g_contentTypeDefault ) :
                        BL_PARAM_FWD( contentType ) )
                    )
            {
                for( auto& header : customHeaders )
                {
                    const auto pair = m_headers.emplace( std::move( header ) );

                    BL_CHK(
                        false,
                        pair.second,
                        BL_MSG()
                            << "Custom header matching an automatically generated header: '"
                            << pair.first -> first /* header name */
                            << HttpHeader::g_nameSeparator
                            << HttpHeader::g_space
                            << pair.first -> second /* header value */
                            << "'"
                        );
                }

                m_serialized = buildResponse();
            }

            auto buildResponse() const -> std::string
            {
                cpp::SafeOutputStringStream oss;

                /*
                 * Get the string for the status code (the string ends with \r\n)
                 */

                oss << http::StatusStrings::get( m_status );

                for( const auto& header : m_headers )
                {
                    oss
                        << header.first
                        << HttpHeader::g_nameSeparator
                        << HttpHeader::g_space
                        << header.second
                        << HttpHeader::g_crlf;
                }

                /*
                 * Add the empty line separating the headers from the content
                 */

                oss << HttpHeader::g_crlf;

                if( ! m_content.empty() )
                {
                    oss << m_content;
                }

                return oss.str();
            }

        public:

            auto content() const NOEXCEPT -> const std::string&
            {
                return m_content;
            }

            auto status() const NOEXCEPT -> HttpStatusCode
            {
                return m_status;
            }

            auto headers() const NOEXCEPT -> const HeadersMap&
            {
                return m_headers;
            }

            auto getSerialized() const NOEXCEPT -> const std::string&
            {
                return m_serialized;
            }
        };

        typedef om::ObjectImpl< ResponseT<> > Response;

    } // httpserver

} // bl

#endif /* __BL_HTTPSERVER_RESPONSE_H_ */
