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

#ifndef __BL_HTTPSERVER_DETAIL_PARSERHELPERS_H_
#define __BL_HTTPSERVER_DETAIL_PARSERHELPERS_H_

#include <baselib/http/Globals.h>

#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace httpserver
    {
        namespace detail
        {
            enum HttpParserResult : std::size_t
            {
                PARSING_ERROR,
                PARSED,
                MORE_DATA_REQUIRED
            };

            typedef std::pair< HttpParserResult, std::exception_ptr >               ServerResult;

            /**
             * @brief class Context
             */

            template
            <
                typename E = void
            >
            class ContextT
            {
            public:

                std::string                                                         m_method;
                std::string                                                         m_uri;
                http::HeadersMap                                                    m_headers;
                std::string                                                         m_body;

                std::string                                                         m_buffer;

                bool                                                                m_headersParsed;
                bool                                                                m_parsed;

                std::size_t                                                         m_bodyBeginPos;
                std::size_t                                                         m_expectedBodyLength;
                std::size_t                                                         m_maxRequestLength;

                ContextT()
                    :
                    m_headersParsed( false ),
                    m_parsed( false ),
                    m_bodyBeginPos( 0U ),
                    m_expectedBodyLength( 0U ),
                    m_maxRequestLength( 0U )
                {
                }
            };

            typedef ContextT<> Context;

            /**
             * @brief class ParserHelpers
             */

            template
            <
                typename E = void
            >
            class ParserHelpersT
            {
                BL_DECLARE_STATIC( ParserHelpersT )

            public:

                typedef http::Parameters::HttpHeader                                HttpHeader;

                static auto serverError( SAA_in const MessageBuffer& message ) -> ServerResult
                {
                    return std::make_pair(
                        HttpParserResult::PARSING_ERROR,
                        std::make_exception_ptr(
                            BL_EXCEPTION(
                                HttpServerException(),
                                message.text()
                                ))
                        );
                }

                static auto serverResult( SAA_in const HttpParserResult resultCode ) -> ServerResult
                {
                    if( resultCode == HttpParserResult::PARSING_ERROR )
                    {
                        return serverError(
                            BL_MSG()
                                << "Unknown server error"
                            );
                    }

                    return std::make_pair( resultCode, nullptr );
                }

                static bool isChar( SAA_in const int ch ) NOEXCEPT
                {
                    return ch >= 0 && ch <= 127;
                }

                static bool isCtl( SAA_in const int ch ) NOEXCEPT
                {
                    return ( ch >= 0 && ch <= 31 ) || ( ch == 127 );
                }

                static bool isTSpecial( SAA_in const int ch ) NOEXCEPT
                {
                    switch( ch )
                    {
                        case '(':
                        case ')':
                        case '<':
                        case '>':
                        case '@':
                        case ',':
                        case ';':
                        case ':':
                        case '\\':
                        case '"':
                        case '/':
                        case '[':
                        case ']':
                        case '?':
                        case '=':
                        case '{':
                        case '}':
                        case ' ':
                        case '\t':
                            return true;

                        default:
                            return false;
                    }
                }

                static bool isValidName( SAA_in const std::string& name ) NOEXCEPT
                {
                    if( name.empty() )
                    {
                        return false;
                    }

                    for( const auto ch : name )
                    {
                        if( ! isChar( ch ) || isCtl( ch ) || isTSpecial( ch ) )
                        {
                            return false;
                        }
                    }

                    return true;
                }

                static auto parseMethodURIVersion(
                    SAA_in      const std::string&                                  input,
                    SAA_inout   Context&                                            context
                    )
                    -> ServerResult
                {
                    std::vector< std::string > elements;

                    str::split( elements, input, str::is_equal_to( HttpHeader::g_space ) );

                    if(
                        elements.size() != 3U   ||
                        elements[ 0 ].empty()   ||
                        elements[ 1 ].empty()   ||
                        elements[ 2 ].empty()
                        )
                    {
                        return serverError(
                            BL_MSG()
                                << "Invalid start line: '"
                                << input
                                << "'"
                                );
                    }

                    if( ! isValidName( elements[ 0 ] ) )
                    {
                        return serverError(
                            BL_MSG()
                                << "Invalid characters in the method name: '"
                                << elements[ 0 ]
                                << "'"
                            );
                    }

                    context.m_method = std::move( elements[ 0 ] );

                    context.m_uri = std::move( elements[ 1 ] );

                    if( elements[ 2 ] != HttpHeader::g_httpDefaultVersion )
                    {
                        return serverError(
                            BL_MSG()
                                << "Invalid protocol version: '"
                                << elements[ 2 ]
                                << "'"
                            );
                    }

                    return serverResult( HttpParserResult::PARSED );
                }

                static auto parseHeader(
                    SAA_in      const::std::string&                                 input,
                    SAA_inout   Context&                                            context
                    )
                    -> ServerResult
                {
                    const auto pos = input.find( HttpHeader::g_nameSeparator );

                    if( pos == std::string::npos || ( pos + 1 ) >= input.length() )
                    {
                        return serverError(
                            BL_MSG()
                                << "Invalid header: '"
                                << input
                                << "'"
                            );
                    }

                    auto name = input.substr( 0, pos );

                    str::trim( name );

                    if( name.empty() )
                    {
                        return serverError(
                            BL_MSG()
                                << "Empty header name"
                            );
                    }

                    if( ! isValidName( name ) )
                    {
                        return serverError(
                            BL_MSG()
                                << "Invalid characters in the header name: '"
                                << std::move( name )
                                << "'"
                            );
                    }

                    auto value = input.substr( pos + 1 );

                    str::trim_left( value );

                    if( value.empty() )
                    {
                        return serverError(
                            BL_MSG()
                                << "Header with no value: '"
                                << input
                                << "'"
                            );
                    }

                    if( ! context.m_headers.emplace( name, std::move( value ) ).second )
                    {
                        return serverError(
                            BL_MSG()
                                << "Duplicate header name: '"
                                << name
                                << "'"
                            );
                    }

                    return serverResult( HttpParserResult::PARSED );
                }
            };

            typedef ParserHelpersT<> ParserHelpers;

        } // detail

    } // httpserver

} // bl

#endif /*__BL_HTTPSERVER_DETAIL_PARSERHELPERS_H_ */
