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

#ifndef __BL_HTTPSERVER_PARSER_H_
#define __BL_HTTPSERVER_PARSER_H_

#include <baselib/httpserver/detail/ParserHelpers.h>

#include <baselib/httpserver/Request.h>

#include <baselib/http/Globals.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/Utils.h>

namespace bl
{
    namespace httpserver
    {
        /**
         * @brief class Parser
         */

        template
        <
            typename E = void
        >
        class ParserT : public om::ObjectDefaultBase
        {
            BL_NO_COPY_OR_MOVE( ParserT )
            BL_CTR_DEFAULT( ParserT, protected )

        public:

            typedef http::Parameters::HttpHeader                HttpHeader;
            typedef httpserver::Request                         Request;
            typedef httpserver::detail::Context                 Context;
            typedef httpserver::detail::ParserHelpers           ParserHelpers;
            typedef httpserver::detail::HttpParserResult        HttpParserResult;
            typedef httpserver::detail::ServerResult            ServerResult;

        protected:

            Context                                             m_context;

        public:

            static const std::size_t                            g_maxHeadersSize;
            static const std::size_t                            g_maxContentSize;

            auto buildRequest() -> om::ObjPtr< Request >
            {
                BL_CHK(
                    false,
                    m_context.m_parsed,
                    BL_MSG()
                        << "The HTTP parser is expecting more data"
                    );

                auto request = Request::createInstance(
                    std::move( m_context.m_method ),
                    std::move( m_context.m_uri ),
                    std::move( m_context.m_headers ),
                    std::move( m_context.m_body )
                    );

                return request;
            }

            void reset()
            {
                m_context = Context();
            }

            auto parse(
                SAA_in  const char*                             begin,
                SAA_in  const char*                             end
              )
              -> ServerResult
            {
                if( m_context.m_parsed )
                {
                    return ParserHelpers::serverError(
                        BL_MSG()
                            << "Unexpected input data: the HTTP request has been parsed already"
                        );
                }

                BL_CHK(
                    false,
                    end > begin,
                    BL_MSG()
                        << "Unexpected input data: the HTTP parser called with an invalid data buffer"
                    );

                const auto bufferLength = m_context.m_buffer.size() + ( end - begin );

                if( bufferLength > ( g_maxContentSize + g_maxHeadersSize ) )
                {
                    return ParserHelpers::serverError(
                        BL_MSG()
                            << "HTTP "
                            << ( ! m_context.m_headersParsed ? "request" : "content" )
                            <<  " size too large"
                        );
                }

                m_context.m_buffer.append( begin, end );

                if( ! m_context.m_headersParsed )
                {
                    /*
                     * We need to parse the headers
                     */

                    const auto posSentinel = m_context.m_buffer.find( HttpHeader::g_sentinel );

                    const auto currentHeadersSize =
                        posSentinel == std::string::npos ? bufferLength : posSentinel;

                    if( currentHeadersSize > g_maxHeadersSize )
                    {
                        return ParserHelpers::serverError(
                            BL_MSG()
                                << "HTTP header size too large"
                            );
                    }

                    if( posSentinel == std::string::npos )
                    {
                        if( bufferLength > 0 && ! ParserHelpers::isValidName( std::string{ m_context.m_buffer[ 0 ] } ) )
                        {
                            return ParserHelpers::serverError(
                                BL_MSG()
                                    << "Non HTTP request"
                                );
                        }

                        return ParserHelpers::serverResult( HttpParserResult::MORE_DATA_REQUIRED );
                    }

                    const auto headers = str::splitString(
                        m_context.m_buffer,
                        HttpHeader::g_crlf,
                        0U,
                        posSentinel
                        );

                    if( headers.empty() )
                    {
                        return ParserHelpers::serverError(
                            BL_MSG()
                                << "Missing required HTTP headers"
                            );
                    }

                    {
                        std::size_t index = 0U;

                        for( const auto& header : headers )
                        {
                            const auto result = index++ ?
                                ParserHelpers::parseHeader( header, m_context )
                                :
                                ParserHelpers::parseMethodURIVersion( header, m_context );

                            if( result.first != HttpParserResult::PARSED )
                            {
                                return result;
                            }
                        }
                    }

                    {
                        const auto pos = std::find_if(
                            m_context.m_headers.begin(),
                            m_context.m_headers.end(),
                            []( SAA_in const std::pair<std::string, std::string>& pair ) -> bool
                            {
                                return bl::str::iequals( pair.first, HttpHeader::g_contentLength );
                            }
                            );

                        if( pos == m_context.m_headers.end() )
                        {
                            m_context.m_expectedBodyLength = 0U;
                        }
                        else
                        {
                            try
                            {
                                m_context.m_expectedBodyLength = utils::lexical_cast< std::size_t >( pos -> second );
                            }
                            catch( utils::bad_lexical_cast& )
                            {
                                return ParserHelpers::serverError(
                                    BL_MSG()
                                        << "Invalid Content-Length value: "
                                        << pos -> second
                                    );
                            }
                        }
                    }

                    m_context.m_headersParsed = true;

                    /*
                     * Jump after the sentinel
                     */

                    m_context.m_bodyBeginPos = posSentinel + HttpHeader::g_sentinel.length();
                    m_context.m_maxRequestLength = m_context.m_bodyBeginPos + m_context.m_expectedBodyLength;
                }

                if( m_context.m_expectedBodyLength > g_maxContentSize )
                {
                    return ParserHelpers::serverError(
                        BL_MSG()
                            << "HTTP content size too large"
                        );
                }

                if( bufferLength == m_context.m_maxRequestLength )
                {
                    if( m_context.m_expectedBodyLength > 0U )
                    {
                        m_context.m_body = m_context.m_buffer.substr(
                            m_context.m_bodyBeginPos,
                            m_context.m_expectedBodyLength
                            );
                    }

                    m_context.m_parsed = true;

                    return ParserHelpers::serverResult( HttpParserResult::PARSED );
                }

                if( bufferLength < m_context.m_maxRequestLength )
                {
                    return ParserHelpers::serverResult( HttpParserResult::MORE_DATA_REQUIRED );
                }

                return ParserHelpers::serverError(
                    BL_MSG()
                        << "The HTTP body size is larger than the expected size provided in the Content-Length header: "
                        << m_context.m_expectedBodyLength
                    );
            }
        };

        typedef om::ObjectImpl< ParserT<> > Parser;

        BL_DEFINE_STATIC_MEMBER( ParserT, const std::size_t, g_maxHeadersSize ) = 1U << 16;
        BL_DEFINE_STATIC_MEMBER( ParserT, const std::size_t, g_maxContentSize ) = 1U << 20;

    } // httpserver

} // bl

#endif /* __BL_HTTPSERVER_PARSER_H_ */
