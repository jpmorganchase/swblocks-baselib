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

#ifndef __BL_HTTP_GLOBALS_H_
#define __BL_HTTP_GLOBALS_H_

#include <baselib/core/BaseIncludes.h>

#include <unordered_map>
#include <set>

namespace bl
{
    namespace http
    {
        typedef std::unordered_map< std::string, std::string >  HeadersMap;
        typedef std::set< unsigned int >                        StatusesList;

        /**
         * @brief class HttpHeader - a static class to hold the HTTP header parameters
         */

        template
        <
            typename E = void
        >
        class HttpHeaderT
        {
            BL_DECLARE_STATIC( HttpHeaderT )

        public:

            static const std::string                            g_acceptCharset;
            static const std::string                            g_iso8859_1;
            static const std::string                            g_latin1;
            static const std::string                            g_windows1252;
            static const std::string                            g_utf8;

            static const std::string                            g_contentType;
            static const std::string                            g_contentLength;
            static const std::string                            g_contentTypeJson;
            static const std::string                            g_contentTypeJsonIso8859_1;
            static const std::string                            g_contentTypeJsonUtf8;
            static const std::string                            g_contentTypeDefault;
            static const std::string                            g_contentTypeXml;
            static const std::string                            g_contentTypePlainText;
            static const std::string                            g_contentTypePlainTextUtf8;

            static const std::string                            g_authorization;
            static const std::string                            g_cookie;
            static const std::string                            g_setCookie;
            static const std::string                            g_userAgent;

            /**
             * Default values for the user agent header (see Parameters::userAgentDefault below)
             *
             * Important Note: many HTTP servers and sites require user agent header to be provided
             * otherwise they will block and terminate the connection
             *
             * Also the established convention for private HTTP clients is to use "bot" in the name
             * of their user agent string, so they identify correctly as a bot (see here:
             * https://en.wikipedia.org/wiki/User_agent#User_agent_identification), so if you want
             * to make sure your private HTTP client is not blocked then make sure it has "bot" in
             * the value of the user agent header
             */

            static const std::string                            g_userAgentDefault;
            static const std::string                            g_userAgentBotDefault;

            static const std::string                            g_connection;
            static const std::string                            g_close;

            static const char                                   g_nameSeparator;
            static const char                                   g_cookieSeparator;
            static const char                                   g_space;

            static const std::string                            g_crlf;
            static const std::string                            g_sentinel;
            static const std::string                            g_nameValueSeparator;

            static const std::string                            g_httpVersion1_0;
            static const std::string                            g_httpVersion1_1;
        };

        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_acceptCharset )               = "Accept-Charset";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_iso8859_1 )                   = "ISO-8859-1";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_latin1 )                      = "LATIN-1";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_windows1252 )                 = "WINDOWS-1252";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_utf8 )                        = "UTF-8";

        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentType )                 = "Content-Type";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentLength )               = "Content-Length";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypeJson )             = "application/json";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypeJsonIso8859_1 )    = "application/json; charset=ISO-8859-1";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypeJsonUtf8 )         = "application/json; charset=UTF-8";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypeDefault )          = "application/json; charset=UTF-8";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypeXml )              = "text/xml";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypePlainText )        = "text/plain";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_contentTypePlainTextUtf8 )    = "text/plain; charset=UTF-8";


        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_authorization )               = "Authorization";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_cookie )                      = "Cookie";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_setCookie )                   = "Set-Cookie";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_userAgent )                   = "User-Agent";

        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_userAgentDefault )            = "swblocks-baselib-client/1.0";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_userAgentBotDefault )         = "swblocks-baselib-client-bot/1.0";

        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_connection )                  = "Connection";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_close )                       = "close";

        BL_DEFINE_STATIC_MEMBER( HttpHeaderT, const char, g_nameSeparator )         = ':';
        BL_DEFINE_STATIC_MEMBER( HttpHeaderT, const char, g_cookieSeparator )       = ';';
        BL_DEFINE_STATIC_MEMBER( HttpHeaderT, const char, g_space )                 = ' ';

        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_crlf )                        = "\r\n";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_sentinel )                    = "\r\n\r\n";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_nameValueSeparator )          = "=";

        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_httpVersion1_0 )              = "HTTP/1.0";
        BL_DEFINE_STATIC_CONST_STRING( HttpHeaderT, g_httpVersion1_1 )              = "HTTP/1.1";

        typedef HttpHeaderT<> HttpHeader;

        /**
         * @brief class Parameters - a static class to hold the HTTP task parameters
         */

        template
        <
            typename E = void
        >
        class ParametersT
        {
            BL_NO_CREATE( ParametersT )

        public:

            typedef http::HttpHeader                            HttpHeader;

            enum HttpStatusCode : unsigned int
            {
                HTTP_STATUS_UNDEFINED                   = 0U,
                HTTP_SUCCESS_OK                         = 200U,
                HTTP_SUCCESS_CREATED                    = 201U,
                HTTP_SUCCESS_ACCEPTED                   = 202U,
                HTTP_SUCCESS_NO_CONTENT                 = 204U,
                HTTP_REDIRECT_START_RANGE               = 300U,
                HTTP_REDIRECT_MULTIPLE_CHOICES          = 300U,
                HTTP_REDIRECT_PERMANENTLY               = 301U,
                HTTP_REDIRECT_TEMPORARILY               = 302U,
                HTTP_REDIRECT_NOT_MODIFIED              = 304U,
                HTTP_REDIRECT_END_RANGE                 = 399U,
                HTTP_CLIENT_ERROR_BAD_REQUEST           = 400U,
                HTTP_CLIENT_ERROR_UNAUTHORIZED          = 401U,
                HTTP_CLIENT_ERROR_FORBIDDEN             = 403U,
                HTTP_CLIENT_ERROR_NOT_FOUND             = 404U,
                HTTP_CLIENT_ERROR_CONFLICT              = 409U,
                HTTP_SERVER_ERROR_INTERNAL              = 500U,
                HTTP_SERVER_ERROR_NOT_IMPLEMENTED       = 501U,
                HTTP_SERVER_ERROR_BAD_GATEWAY           = 502U,
                HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE   = 503U,
                HTTP_SERVER_ERROR_GATEWAY_TIMEOUT       = 504U,
            };

            enum : long
            {
                TIMEOUT_IN_SECONDS_GET_DEFAULT = 30L * 60L,
                TIMEOUT_IN_SECONDS_OTHER_DEFAULT = 30L * 60L,
            };

            static long timeoutInSecondsGet() NOEXCEPT
            {
                return g_timeoutInSecondsGet;
            }

            static void timeoutInSecondsGet( SAA_in const long timeoutInSecondsGet ) NOEXCEPT
            {
                g_timeoutInSecondsGet = timeoutInSecondsGet;
            }

            static long timeoutInSecondsOther() NOEXCEPT
            {
                return g_timeoutInSecondsOther;
            }

            static void timeoutInSecondsOther( SAA_in const long timeoutInSecondsOther ) NOEXCEPT
            {
                g_timeoutInSecondsOther = timeoutInSecondsOther;
            }

            static auto errorResponseHeaderNamesLvalue() NOEXCEPT -> std::vector< std::string >&
            {
                return g_errorResponseHeaderNames;
            }

            static auto userAgentDefault() NOEXCEPT -> const std::string&
            {
                return g_userAgentDefault;
            }

            static void userAgentDefault( SAA_in std::string&& userAgentDefault ) NOEXCEPT
            {
                g_userAgentDefault = BL_PARAM_FWD( userAgentDefault );
            }

            /*************************************************************************************
             * HTTP status sets
             */

            static const StatusesList& emptyStatuses() NOEXCEPT
            {
                return g_emptyStatuses;
            }

            static const StatusesList& conflictStatuses() NOEXCEPT
            {
                return g_conflictStatuses;
            }

            static const StatusesList& unauthorizedStatuses() NOEXCEPT
            {
                return g_unauthorizedStatuses;
            }

            static const StatusesList& redirectStatuses() NOEXCEPT
            {
                return g_redirectStatuses;
            }

            static const StatusesList& securityDefaultStatuses() NOEXCEPT
            {
                return g_securityDefaultStatuses;
            }

            static const StatusesList& badRequestStatuses() NOEXCEPT
            {
                return g_badRequestStatuses;
            }

        private:

            static StatusesList initStatusesList( SAA_in const unsigned int httpStatus )
            {
                StatusesList result;

                result.insert( httpStatus );

                return result;
            }

            static StatusesList initStatusesList(
                SAA_in              const unsigned int              httpStatus1,
                SAA_in              const unsigned int              httpStatus2
                )
            {
                StatusesList result;

                result.insert( httpStatus1 );
                result.insert( httpStatus2 );

                return result;
            }

            static long                                             g_timeoutInSecondsGet;
            static long                                             g_timeoutInSecondsOther;
            static std::vector< std::string >                       g_errorResponseHeaderNames;
            static std::string                                      g_userAgentDefault;

            static const StatusesList                               g_emptyStatuses;
            static const StatusesList                               g_conflictStatuses;
            static const StatusesList                               g_unauthorizedStatuses;
            static const StatusesList                               g_redirectStatuses;
            static const StatusesList                               g_securityDefaultStatuses;
            static const StatusesList                               g_badRequestStatuses;
        };

        BL_DEFINE_STATIC_MEMBER( ParametersT, long, g_timeoutInSecondsGet )                     = ParametersT< TCLASS >::TIMEOUT_IN_SECONDS_GET_DEFAULT;
        BL_DEFINE_STATIC_MEMBER( ParametersT, long, g_timeoutInSecondsOther )                   = ParametersT< TCLASS >::TIMEOUT_IN_SECONDS_OTHER_DEFAULT;
        BL_DEFINE_STATIC_MEMBER( ParametersT, std::vector< std::string >, g_errorResponseHeaderNames );

        /*
         * By default the user agent string is empty, which will tell the HTTP client code to not provide it
         */

        BL_DEFINE_STATIC_STRING( ParametersT, g_userAgentDefault ) = "";

        BL_DEFINE_STATIC_MEMBER( ParametersT, const StatusesList, g_emptyStatuses );

        BL_DEFINE_STATIC_MEMBER( ParametersT, const StatusesList, g_conflictStatuses ) =
            ParametersT< TCLASS >::initStatusesList( ParametersT< TCLASS >::HTTP_CLIENT_ERROR_CONFLICT );

        BL_DEFINE_STATIC_MEMBER( ParametersT, const StatusesList, g_unauthorizedStatuses ) =
            ParametersT< TCLASS >::initStatusesList( ParametersT< TCLASS >::HTTP_CLIENT_ERROR_UNAUTHORIZED );

        BL_DEFINE_STATIC_MEMBER( ParametersT, const StatusesList, g_redirectStatuses ) =
            ParametersT< TCLASS >::initStatusesList( ParametersT< TCLASS >::HTTP_REDIRECT_TEMPORARILY );

        BL_DEFINE_STATIC_MEMBER( ParametersT, const StatusesList, g_securityDefaultStatuses ) =
            ParametersT< TCLASS >::initStatusesList(
                ParametersT< TCLASS >::HTTP_CLIENT_ERROR_UNAUTHORIZED,
                ParametersT< TCLASS >::HTTP_REDIRECT_TEMPORARILY
                );

        BL_DEFINE_STATIC_MEMBER( ParametersT, const StatusesList, g_badRequestStatuses ) =
            ParametersT< TCLASS >::initStatusesList( ParametersT< TCLASS >::HTTP_CLIENT_ERROR_BAD_REQUEST );

        typedef ParametersT<> Parameters;

        /**
         * @brief class StatusStrings
         */

        template
        <
            typename E = void
        >
        class StatusStringsT
        {
            BL_DECLARE_STATIC( StatusStringsT )

        private:

            static const std::string                                                g_undefined;
            static const std::string                                                g_ok;
            static const std::string                                                g_created;
            static const std::string                                                g_accepted;
            static const std::string                                                g_noContent;
            static const std::string                                                g_multipleChoices;
            static const std::string                                                g_movedPermanently;
            static const std::string                                                g_movedTemporarily;
            static const std::string                                                g_notModified;
            static const std::string                                                g_endRange;
            static const std::string                                                g_badRequest;
            static const std::string                                                g_unauthorized;
            static const std::string                                                g_forbidden;
            static const std::string                                                g_notFound;
            static const std::string                                                g_conflict;
            static const std::string                                                g_internalError;
            static const std::string                                                g_notImplemented;
            static const std::string                                                g_badGateway;
            static const std::string                                                g_serviceUnavailable;

        public:

            static const std::string& get( SAA_in const Parameters::HttpStatusCode status )
            {
                switch( status )
                {
                    case Parameters::HTTP_STATUS_UNDEFINED:
                        return g_undefined;

                    case Parameters::HTTP_SUCCESS_OK:
                        return g_ok;

                    case Parameters::HTTP_SUCCESS_CREATED:
                        return g_created;

                    case Parameters::HTTP_SUCCESS_ACCEPTED:
                        return g_accepted;

                    case Parameters::HTTP_SUCCESS_NO_CONTENT:
                        return g_noContent;

                    case Parameters::HTTP_REDIRECT_MULTIPLE_CHOICES:
                        return g_multipleChoices;

                    case Parameters::HTTP_REDIRECT_PERMANENTLY:
                        return g_movedPermanently;

                    case Parameters::HTTP_REDIRECT_TEMPORARILY:
                        return g_movedTemporarily;

                    case Parameters::HTTP_REDIRECT_NOT_MODIFIED:
                        return g_notModified;

                    case Parameters::HTTP_REDIRECT_END_RANGE:
                        return g_endRange;

                    case Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST:
                        return g_badRequest;

                    case Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED:
                        return g_unauthorized;

                    case Parameters::HTTP_CLIENT_ERROR_FORBIDDEN:
                        return g_forbidden;

                    case Parameters::HTTP_CLIENT_ERROR_NOT_FOUND:
                        return g_notFound;

                    case Parameters::HTTP_CLIENT_ERROR_CONFLICT:
                        return g_conflict;

                    case Parameters::HTTP_SERVER_ERROR_INTERNAL:
                        return g_internalError;

                    case Parameters::HTTP_SERVER_ERROR_NOT_IMPLEMENTED:
                        return g_notImplemented;

                    case Parameters::HTTP_SERVER_ERROR_BAD_GATEWAY:
                        return g_badGateway;

                    case Parameters::HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE:
                        return g_serviceUnavailable;

                    default:
                        return g_internalError;
                }
            }
        };

        typedef StatusStringsT<> StatusStrings;

        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_undefined )                = "HTTP/1.0 0 Status Undefined\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_ok )                       = "HTTP/1.0 200 OK\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_created )                  = "HTTP/1.0 201 Created\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_accepted )                 = "HTTP/1.0 202 Accepted\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_noContent )                = "HTTP/1.0 204 No Content\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_multipleChoices )          = "HTTP/1.0 300 Multiple Choices\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_movedPermanently )         = "HTTP/1.0 301 Moved Permanently\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_movedTemporarily )         = "HTTP/1.0 302 Moved Temporarily\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_notModified )              = "HTTP/1.0 304 Not Modified\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_endRange )                 = "HTTP/1.0 399 End Range\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_badRequest )               = "HTTP/1.0 400 Bad Request\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_unauthorized )             = "HTTP/1.0 401 Unauthorized\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_forbidden )                = "HTTP/1.0 403 Forbidden\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_notFound )                 = "HTTP/1.0 404 Not Found\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_conflict )                 = "HTTP/1.0 409 Conflict\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_internalError )            = "HTTP/1.0 500 Internal Server Error\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_notImplemented )           = "HTTP/1.0 501 Not Implemented\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_badGateway )               = "HTTP/1.0 502 Bad Gateway\r\n";
        BL_DEFINE_STATIC_CONST_STRING( StatusStringsT, g_serviceUnavailable )       = "HTTP/1.0 503 Service Unavailable\r\n";

    } //http

} // bl

#endif /* __BL_HTTP_GLOBALS_H_ */
