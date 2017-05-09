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

#include <utests/baselib/HttpServerHelpers.h>

UTF_AUTO_TEST_CASE( BaseLib_StatusStringsTest )
{
    using namespace bl;

    typedef http::Parameters::HttpStatusCode        StatusCode;
    typedef http::StatusStrings                     StatusStrings;

    const auto ok = "HTTP/1.0 200 OK\r\n";
    const auto created = "HTTP/1.0 201 Created\r\n";
    const auto accepted = "HTTP/1.0 202 Accepted\r\n";
    const auto noContent = "HTTP/1.0 204 No Content\r\n";
    const auto multipleChoices = "HTTP/1.0 300 Multiple Choices\r\n";
    const auto movedPermanently = "HTTP/1.0 301 Moved Permanently\r\n";
    const auto movedTemporarily = "HTTP/1.0 302 Moved Temporarily\r\n";
    const auto notModified = "HTTP/1.0 304 Not Modified\r\n";
    const auto badRequest = "HTTP/1.0 400 Bad Request\r\n";
    const auto unauthorized = "HTTP/1.0 401 Unauthorized\r\n";
    const auto forbidden = "HTTP/1.0 403 Forbidden\r\n";
    const auto notFound = "HTTP/1.0 404 Not Found\r\n";
    const auto internalError = "HTTP/1.0 500 Internal Server Error\r\n";
    const auto notImplemented = "HTTP/1.0 501 Not Implemented\r\n";
    const auto badGateway = "HTTP/1.0 502 Bad Gateway\r\n";
    const auto serviceUnavailable = "HTTP/1.0 503 Service Unavailable\r\n";

    std::vector< std::pair< std::string, StatusCode > > stringToCode;

    stringToCode.emplace_back( ok, StatusCode::HTTP_SUCCESS_OK );
    stringToCode.emplace_back( created, StatusCode::HTTP_SUCCESS_CREATED );
    stringToCode.emplace_back( accepted, StatusCode::HTTP_SUCCESS_ACCEPTED );
    stringToCode.emplace_back( noContent, StatusCode::HTTP_SUCCESS_NO_CONTENT );
    stringToCode.emplace_back( multipleChoices, StatusCode::HTTP_REDIRECT_MULTIPLE_CHOICES );
    stringToCode.emplace_back( movedPermanently, StatusCode::HTTP_REDIRECT_PERMANENTLY );
    stringToCode.emplace_back( movedTemporarily, StatusCode::HTTP_REDIRECT_TEMPORARILY );
    stringToCode.emplace_back( notModified, StatusCode::HTTP_REDIRECT_NOT_MODIFIED );
    stringToCode.emplace_back( badRequest, StatusCode::HTTP_CLIENT_ERROR_BAD_REQUEST );
    stringToCode.emplace_back( unauthorized, StatusCode::HTTP_CLIENT_ERROR_UNAUTHORIZED );
    stringToCode.emplace_back( forbidden, StatusCode::HTTP_CLIENT_ERROR_FORBIDDEN );
    stringToCode.emplace_back( notFound, StatusCode::HTTP_CLIENT_ERROR_NOT_FOUND );
    stringToCode.emplace_back( internalError, StatusCode::HTTP_SERVER_ERROR_INTERNAL );
    stringToCode.emplace_back( notImplemented, StatusCode::HTTP_SERVER_ERROR_NOT_IMPLEMENTED );
    stringToCode.emplace_back( badGateway, StatusCode::HTTP_SERVER_ERROR_BAD_GATEWAY );
    stringToCode.emplace_back( serviceUnavailable, StatusCode::HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE );

    for( const auto& pair : stringToCode )
    {
        UTF_REQUIRE_EQUAL( StatusStrings::get( pair.second ), pair.first );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_ResponseTest )
{
    using namespace bl;

    typedef http::Parameters::HttpStatusCode        StatusCode;
    typedef httpserver::Response                    Response;
    typedef http::Parameters::HttpHeader            HttpHeader;

    {
        const auto response = Response::createInstance( StatusCode::HTTP_SUCCESS_OK);

        const auto& headers = response -> headers();

        for( const auto& header : headers )
        {
            UTF_REQUIRE_EQUAL(
                header.first == HttpHeader::g_contentType || header.first == HttpHeader::g_contentLength,
                true
                );
        }

        UTF_REQUIRE( response -> getSerialized().size() );
    }

    {
        http::HeadersMap headersIn;

        headersIn.emplace( "Custom-header1", "value1" );
        headersIn.emplace( "Custom-header2", "value2" );

        const std::string content = "abcdefgh";

        const auto response = Response::createInstance(
            StatusCode::HTTP_SUCCESS_CREATED,
            bl::cpp::copy( content ),
            std::string(),
            std::move( headersIn )
            );

        const auto& headers = response -> headers();

        UTF_REQUIRE_EQUAL( headers.size(), 4U );

        bool invalidHeader = false;

        for( const auto& header : headers )
        {
            if( header.first == "Custom-header1" )
            {
                UTF_REQUIRE_EQUAL( header.second, "value1" );
            }
            else if( header.first == "Custom-header2" )
            {
                UTF_REQUIRE_EQUAL( header.second, "value2" );
            }
            else if( header.first == HttpHeader::g_contentType )
            {
                UTF_REQUIRE_EQUAL( header.second, HttpHeader::g_contentTypeDefault );
            }
            else if( header.first == HttpHeader::g_contentLength )
            {
                UTF_REQUIRE_EQUAL( header.second, bl::utils::lexical_cast< std::string >( content.size() ) );
            }
            else
            {
                invalidHeader = true;

                break;
            }
        }

        UTF_REQUIRE_EQUAL( invalidHeader, false );

        const auto responseString = response -> getSerialized();

        UTF_REQUIRE( responseString.find( content ) != std::string::npos );

        const auto value1Pos = responseString.find( "value1" );
        const auto value2Pos = responseString.find( "value2" );
        const auto contentPos = responseString.find( content );
        const auto responseLength = responseString.length();

        UTF_REQUIRE( value1Pos != std::string::npos && value2Pos != std::string::npos && contentPos != std::string::npos );
        UTF_REQUIRE( contentPos >= 4U && contentPos > std::max( value1Pos , value2Pos ) );

        UTF_REQUIRE( ( value1Pos + std::strlen( "value1" ) + 2U ) <= responseLength );
        UTF_REQUIRE( ( value2Pos + std::strlen( "value2" ) + 2U ) <= responseLength );

        UTF_REQUIRE_EQUAL( responseString.substr( value1Pos + std::strlen( "value1" ), 2U ), "\r\n" );
        UTF_REQUIRE_EQUAL( responseString.substr( value2Pos + std::strlen( "value2" ), 2U ), "\r\n" );
        UTF_REQUIRE_EQUAL( responseString.substr( contentPos - 4U, 4U ), "\r\n\r\n" );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_RequestTest )
{
    using namespace bl;

    typedef http::Parameters::HttpHeader            HttpHeader;

    const std::string method = "POST";

    const std::string uri = utest::http::g_requestUri;

    const std::string protocol = HttpHeader::g_httpDefaultVersion;

    const std::string header1 = "Host";
    const std::string value1 = "";

    const std::string header2 = "Authorization";
    const std::string value2 = "JANUS token=\"GD32CMCL25aZ-v____8B\"";

    const std::string header3 = "Accept";
    const std::string value3 = HttpHeader::g_contentTypeDefault;

    const std::string header4 = "Connection";
    const std::string value4 = "keep-alive";

    http::HeadersMap headers;

    headers.emplace( bl::cpp::copy( header1 ), bl::cpp::copy( value1 ) );
    headers.emplace( bl::cpp::copy( header2 ), bl::cpp::copy( value2 ) );
    headers.emplace( bl::cpp::copy( header3 ), bl::cpp::copy( value3 ) );
    headers.emplace( bl::cpp::copy( header4 ), bl::cpp::copy( value4 ) );

    const std::string body = "abcdefghi kl noq124";

    const auto r = httpserver::Request::createInstance(
        bl::cpp::copy( method ),
        bl::cpp::copy( uri ),
        std::move( headers ),
        bl::cpp::copy( body )
        );

    UTF_REQUIRE_EQUAL( r -> method(), method );
    UTF_REQUIRE_EQUAL( r -> uri(), uri );
    UTF_REQUIRE_EQUAL( r -> body(), body );
    UTF_REQUIRE_EQUAL( r -> headers().size(), 4U );

    for( const auto& header : r -> headers() )
    {
        UTF_REQUIRE(
            header.first == header1 ||
            header.first == header2 ||
            header.first == header3 ||
            header.first == header4
            );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_ParserHelpersTestMethodURIProtocol )
{
    using namespace bl;

    typedef http::Parameters::HttpHeader                HttpHeader;

    typedef httpserver::detail::ParserHelpers           ParserHelpers;
    typedef httpserver::detail::HttpParserResult        HttpParserResult;

    const std::string method = "GET";

    const std::string uri = utest::http::g_requestUri;

    {
        /*
         * Test the case when all elements of the first line are present in the buffer
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << method
            << HttpHeader::g_space
            << uri
            << HttpHeader::g_space
            << HttpHeader::g_httpDefaultVersion;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        UTF_REQUIRE_EQUAL( request, context.m_buffer );

        const auto result = ParserHelpers::parseMethodURIVersion( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result.second == nullptr );
        UTF_REQUIRE_EQUAL( context.m_method, method );
        UTF_REQUIRE_EQUAL( context.m_uri, uri );
    }

    {
        /*
         * Test the case when the data is invalid
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << method
            << HttpHeader::g_space
            << uri
            << HttpHeader::g_space
            << "HTTP/a.1";

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseMethodURIVersion( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_ParserHelpersParseHeader )
{
    using namespace bl;

    typedef http::Parameters::HttpHeader                HttpHeader;

    typedef httpserver::detail::ParserHelpers           ParserHelpers;
    typedef httpserver::detail::HttpParserResult        HttpParserResult;

    const std::string headerName    = "Authorization";
    const std::string value         = "JANUS token=\"GD32CMCL25aZ-v____8B\"";

    {
        /*
         * Test with a valid header (a space after the name-value separator)
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << headerName
            << HttpHeader::g_nameSeparator
            << HttpHeader::g_space
            << value;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result.second == nullptr );
        UTF_REQUIRE_EQUAL( context.m_headers.size(), 1U );
        UTF_REQUIRE_EQUAL( context.m_headers.find( headerName ) -> second, value );
    }

    {
        /*
         * Test with a valid header (no space after the name-value separator)
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << headerName
            << HttpHeader::g_nameSeparator
            << value;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result.second == nullptr );
        UTF_REQUIRE_EQUAL( context.m_headers.size(), 1U );
        UTF_REQUIRE_EQUAL( context.m_headers.find( headerName ) -> second, value );
    }

    {
        /*
         * Test duplicated header names
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << headerName
            << HttpHeader::g_nameSeparator
            << HttpHeader::g_space
            << value;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_headers[ headerName ] = value;

        UTF_REQUIRE_EQUAL( context.m_headers.size(), 1U );
        UTF_REQUIRE_EQUAL( context.m_headers.find( headerName ) -> second, value );

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test invalid header (no name value separator)
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << headerName
            << HttpHeader::g_space
            << value;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test invalid header (spaces instead of a value)
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << headerName
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_space;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test with an invalid header (no header name)
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << HttpHeader::g_nameSeparator
            << value;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test with an invalid header (spaces instead of a header name)
         */

        bl::cpp::SafeOutputStringStream oss;

        oss
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_space
            << HttpHeader::g_nameSeparator
            << value;

        const auto request = oss.str();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        httpserver::detail::Context context;

        context.m_buffer.append( begin, end );

        const auto result = ParserHelpers::parseHeader( context.m_buffer, context );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_ParserTest )
{
    using namespace bl;

    typedef httpserver::Parser                          Parser;

    typedef httpserver::detail::HttpParserResult        HttpParserResult;

    {
        /*
         * Test the case when all elements of the first line are present in the buffer
         */

        const std::string firstLine = "PUT sign-request HTTP/1.0\r\n";

        const auto buffer = firstLine.c_str();

        const char* begin = buffer;
        const char* end = buffer + firstLine.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::MORE_DATA_REQUIRED );
        UTF_REQUIRE( result.second == nullptr );
    }

    {
        /*
         * Test the case when not all elements of the first line are present in the buffer
         */

        const std::string firstLine = "PUT sign-request HTTP/1.";

        const auto buffer = firstLine.c_str();

        const char* begin = buffer;
        const char* end = buffer + firstLine.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::MORE_DATA_REQUIRED );
        UTF_REQUIRE( result.second == nullptr );

        const std::string firstLineCompliment = "1\r\n";

        const char* beginCompliment = firstLineCompliment.c_str();
        const char* endCompliment = beginCompliment + firstLineCompliment.length();

        const auto nextResult = parser -> parse( beginCompliment, endCompliment );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::MORE_DATA_REQUIRED );
        UTF_REQUIRE( result.second == nullptr );
    }

    {
        /*
         * Test the case when CRLFs are the only elements to parse
         */

        const std::string firstLine = "\r\n\r\n";

        const auto buffer = firstLine.c_str();

        const char* begin = buffer;
        const char* end = buffer + firstLine.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test the case when there is one valid header but no content
         */

        const std::string request =
            "PUT sign-request HTTP/1.0\r\n"
            "Heade1: value1\r\n\r\n";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result.second == nullptr );
        UTF_REQUIRE( parser -> buildRequest() != nullptr );
    }

    {
        /*
         * Test the case when the header comes in parts
         */

        const std::string request =
            "PUT sign-request HTTP/1.0\r\n"
            "Heade1: value1\r";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::MORE_DATA_REQUIRED );
        UTF_REQUIRE( result.second == nullptr );

        UTF_REQUIRE_THROW( parser -> buildRequest(), bl::UnexpectedException );

        const std::string request2 = "\n\r\n";

        const auto buffer2 = request2.c_str();

        const char* begin2 = buffer2;
        const char* end2 = buffer2 + request2.length();

        const auto result2 = parser -> parse( begin2, end2 );

        UTF_REQUIRE_EQUAL( result2.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result2.second == nullptr );
    }

    {
        /*
         * Test the case when the first header is present and there is no content
         */

        const std::string request =
            "PUT sign-request HTTP/1.0\r\n\r\n";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result.second == nullptr );
    }

    {
        /*
         * Test the case when the content related headers are provided and define content with length == 0
         */

        const std::string request =
            "PUT sign-request HTTP/1.0\r\n"
            "Content-Type: text/xml\r\n"
            "Content-Length: 0\r\n\r\n";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSED );
        UTF_REQUIRE( result.second == nullptr );

        UTF_REQUIRE_EQUAL(
            ( parser -> parse( begin, end ) ).first,
            HttpParserResult::PARSING_ERROR );
    }

    {
        /*
         * Test the case when the content related headers are provided but Content-Length is invalid
         */

        const std::string request =
            "PUT sign-request HTTP/1.0\r\n"
            "Content-Type: text/xml\r\n"
            "Content-Length: 10d\r\n\r\n";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test the case when only one of the the content related headers is provided
         */

        const std::string request =
            "PUT sign-request HTTP/1.0\r\n"
            "Content-Length: text/xml\r\n\r\n";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        const auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test the case when the parser is called with too large headers
         */

        const std::string request = "PUT sign-request HTTP/1.0\r\n";

        auto bytesCount = request.length();

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + bytesCount;

        const auto parser = Parser::createInstance();

        auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::MORE_DATA_REQUIRED );

        for( std::size_t i = 0U; result.first != HttpParserResult::PARSING_ERROR; ++i )
        {
            if( bytesCount > httpserver::Parser::g_maxHeadersSize )
            {
                break;
            }

            bl::cpp::SafeOutputStringStream oss;

            oss
                << "Header_"
                << i
                << ": "
                << i
                << "\r\n";

            const auto header = oss.str();

            const auto length = header.length();

            const auto headerBuffer = header.c_str();

            const char* beginHeader = headerBuffer;
            const char* endHeader = headerBuffer + length;

            bytesCount += length;

            result = parser -> parse( beginHeader, endHeader );
        }

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test the case when the content is larger than the supported Content size
         */

        const std::string request =
            std::string( "PUT sign-request HTTP/1.0\r\n" ) +
            std::string( "Content-Length: " ) +
            std::to_string( Parser::g_maxContentSize + 1 ) +
            std::string( "\r\n\r\n" );

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer + request.length();

        const auto parser = Parser::createInstance();

        auto result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );

        result = parser -> parse( begin, end );

        UTF_REQUIRE_EQUAL( result.first, HttpParserResult::PARSING_ERROR );
        UTF_REQUIRE( result.second != nullptr );
    }

    {
        /*
         * Test the case when the parser is called with an invalid buffer
         */

        const std::string request = "PUT sign-request HTTP/1.0\r\n\r\n";

        const auto buffer = request.c_str();

        const char* begin = buffer;
        const char* end = buffer;

        const auto parser = Parser::createInstance();

        UTF_REQUIRE_THROW( parser -> parse( begin, end ), bl::UnexpectedException );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_HttpServerImplTest )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace utest::http;

    HttpServerHelpers::startHttpServerAndExecuteCallback(
        []() -> void
        {
            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    /*
                     * Run an assorted set of HTTP requests
                     */

                    HttpServerHelpers::sendAndVerifyAssortedHttpRequests( eq );
                });
        }
        );
}

UTF_AUTO_TEST_CASE( BaseLib_HttpServerPerfTest )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace utest::http;

    HttpServerHelpers::startHttpServerAndExecuteCallback(
        []() -> void
        {
            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    typedef SimpleHttpPutTaskImpl task_impl_t;

                    {
                        /*
                         * Run an assorted set of HTTP requests (multiple sequential)
                         */

                        Logging::LevelPusher level( Logging::LL_INFO, true /* global */ );

                        const std::size_t count = 10;

                        utils::ExecutionTimer timer(
                            resolveMessage(
                                BL_MSG()
                                    << "Execute "
                                    << count
                                    << " tasks sequentially"
                                )
                            );

                        for( std::size_t i = 0; i < count; ++i )
                        {
                            HttpServerHelpers::sendAndVerifyAssortedHttpRequests< task_impl_t >( eq );
                        }
                    }

                    HttpServerHelpers::completion_results_map_t results;

                    {
                        /*
                         * Run an assorted set of HTTP requests (multiple parallel)
                         */

                        Logging::LevelPusher level( Logging::LL_INFO, true /* global */ );

                        eq -> setOptions( ExecutionQueue::OptionKeepNone );

                        const std::size_t count = 10;

                        utils::ExecutionTimer timer(
                            resolveMessage(
                                BL_MSG()
                                    << "Execute "
                                    << count
                                    << " tasks in parallel"
                                )
                            );

                        for( std::size_t i = 0; i < count; ++i )
                        {
                            HttpServerHelpers::sendAndVerifyAssortedHttpRequests< task_impl_t >( eq, &results );
                        }

                        eq -> flush(
                            false   /* discardPending */,
                            true    /* nothrowIfFailed */,
                            false   /* discardReady */,
                            false   /* cancelExecuting */
                            );
                    }

                    for( const auto& pair : results )
                    {
                        const auto taskImpl = om::qi< task_impl_t >( pair.first );
                        const auto& result = pair.second;

                        const auto status = taskImpl -> getHttpStatus();
                        const auto& response = taskImpl -> getResponse();

                        const bool& exceptionExpected = std::get< 0 >( result );
                        const bl::http::Parameters::HttpStatusCode& statusCodeExpected = std::get< 1 >( result );
                        const std::string& contentExpected = std::get< 2 >( result );

                        if( taskImpl -> isFailed() != exceptionExpected )
                        {
                            if( exceptionExpected )
                            {
                                UTF_FAIL( "Task has succeeded even though exception was expected" );
                            }
                            else
                            {
                                UTF_REQUIRE( taskImpl -> isFailed() );
                                UTF_REQUIRE( taskImpl -> exception() );

                                cpp::safeRethrowException( taskImpl -> exception() );
                            }
                        }

                        if( statusCodeExpected != status )
                        {
                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "HTTP status code "
                                    << status
                                    << " is different than the expected HTTP status code "
                                    << statusCodeExpected
                                );

                            UTF_FAIL( "Invariant broken - see message above" );
                        }

                        if( ! contentExpected.empty() )
                        {
                            UTF_REQUIRE( response.find( contentExpected ) != std::string::npos );
                        }
                    }
                });
        }
        );
}

