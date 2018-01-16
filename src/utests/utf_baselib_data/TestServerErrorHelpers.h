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

#include <baselib/data/eh/ServerErrorHelpers.h>

#include <utests/baselib/UtfBaseLibCommon.h>
#include <utests/baselib/TestUtils.h>

UTF_AUTO_TEST_CASE( ErrorToJsonTests )
{
    using namespace bl;

#define UTEST_PROPERTY_REQUIRE_EQUAL( property, value ) \
        { \
            const auto errorProperty = eh::get_error_info< eh::property >( e ); \
            UTF_REQUIRE( errorProperty != nullptr ); \
            UTF_CHECK_EQUAL( *errorProperty, value ); \
        } \

    const eh::error_code errorCode(
        eh::errc::permission_denied,
        eh::generic_category()
        );

    const auto createServerErrorException = [ &errorCode ]()
        -> std::exception_ptr
    {
        return std::make_exception_ptr(
            eh::enable_current_exception( eh::enable_error_info( ServerErrorException() ) )
                << eh::throw_function( BOOST_CURRENT_FUNCTION )
                << eh::throw_file( __FILE__ )
                << eh::throw_line( __LINE__ )
                << eh::errinfo_errno                       ( 1001 )
                << eh::errinfo_file_name                   ( "file_name: ErrorToJsonTests" )
                << eh::errinfo_file_open_mode              ( "file_open_mode: ErrorToJsonTests" )
                << eh::errinfo_message                     ( "message: ErrorToJsonTests" )
                << eh::errinfo_time_thrown                 ( "2015-09-23T18:29:10.475824-04:00" )
                << eh::errinfo_function_name               ( "function_name: ErrorToJsonTests" )
                << eh::errinfo_system_code                 ( http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST )
                << eh::errinfo_category_name               ( errorCode.category().name() /* "generic" */ )
                << eh::errinfo_error_code                  ( errorCode )
                << eh::errinfo_error_code_message          ( errorCode.message() /* "Permission denied" */ )
                << eh::errinfo_is_expected                 ( true )
                << eh::errinfo_task_info                   ( "task_info: ErrorToJsonTests" )
                << eh::errinfo_host_name                   ( net::getShortHostName() )
                << eh::errinfo_service_name                ( "service_name: ErrorToJsonTests" )
                << eh::errinfo_endpoint_address            ( "endpoint_address: ErrorToJsonTests address" )
                << eh::errinfo_endpoint_port               ( 1002 )
                << eh::errinfo_http_url                    ( "www.mytest.com" )
                << eh::errinfo_http_redirect_url           ( "www.myredirectedtest.com" )
                << eh::errinfo_http_status_code            ( 1003 )
                << eh::errinfo_http_response_headers       ( "<header1><header2><header3>" )
                << eh::errinfo_http_request_details        ( "_http_request_details: ErrorToJsonTests" )
                << eh::errinfo_parser_file                 ( "Blablabla" )
                << eh::errinfo_parser_line                 ( 1004u )
                << eh::errinfo_parser_column               ( 1005u )
                << eh::errinfo_parser_reason               ( "syntax error" )
                << eh::errinfo_external_command_output     ( "external_command_output: ErrorToJsonTests" )
                << eh::errinfo_external_command_exit_code  ( 1006 )
                << eh::errinfo_string_value                ( "string_value: ErrorToJsonTests" )
                << eh::errinfo_is_user_friendly            ( false )
                << eh::errinfo_ssl_is_verify_failed        ( true )
                << eh::errinfo_ssl_is_verify_error         ( 1007 )
                << eh::errinfo_ssl_is_verify_error_message ( "ssl_is_verify_error_message: ErrorToJsonTests" )
                << eh::errinfo_ssl_is_verify_error_string  ( "ssl_is_verify_error_string: ErrorToJsonTests" )
                << eh::errinfo_ssl_is_verify_subject_name  ( "ssl_is_verify_subject_name: ErrorToJsonTests" )
            );
    };

    const auto validateException = [ &errorCode ]( SAA_in const BaseExceptionDefault& e )
    {
        UTF_REQUIRE_EQUAL( e.fullTypeName(),  "bl::ServerErrorException" );
        UTF_REQUIRE_EQUAL( e.what(),  "message: ErrorToJsonTests" );
        UTF_REQUIRE( ! e.details().empty() );

        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_errno                       , 1001 )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_file_name                   , "file_name: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_file_open_mode              , "file_open_mode: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_message                     , "message: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_time_thrown                 , "2015-09-23T18:29:10.475824-04:00" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_function_name               , "function_name: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_system_code                 , static_cast< int >( http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST ) )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_category_name               , "generic" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_error_code                  , errorCode )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_error_code_message          , "Permission denied" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_is_expected                 , true )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_task_info                   , "task_info: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_host_name                   , net::getShortHostName() )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_service_name                , "service_name: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_endpoint_address            , "endpoint_address: ErrorToJsonTests address" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_endpoint_port               , 1002 )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_http_url                    , "www.mytest.com" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_http_redirect_url           , "www.myredirectedtest.com" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_http_status_code            , 1003 )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_http_response_headers       , "<header1><header2><header3>" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_http_request_details        , "_http_request_details: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_parser_file                 , "Blablabla" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_parser_line                 , 1004u )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_parser_column               , 1005u )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_parser_reason               , "syntax error" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_external_command_output     , "external_command_output: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_external_command_exit_code  , 1006 )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_string_value                , "string_value: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_is_user_friendly            , false )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_ssl_is_verify_failed        , true )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_ssl_is_verify_error         , 1007 )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_ssl_is_verify_error_message , "ssl_is_verify_error_message: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_ssl_is_verify_error_string  , "ssl_is_verify_error_string: ErrorToJsonTests" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_ssl_is_verify_subject_name  , "ssl_is_verify_subject_name: ErrorToJsonTests" )
    };

    /*
     * Create and throw ServerErrorException test
     */

    try
    {
        const auto serverErrorExceptionPtr = createServerErrorException();

        const auto serverErrorJson = dm::ServerErrorHelpers::createServerErrorObject( serverErrorExceptionPtr );

        const auto exceptionPtr = dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson );

        std::rethrow_exception( exceptionPtr );
    }
    catch( BaseExceptionDefault& e )
    {
        validateException( e );
    }

    /*
     * Create and throw std::exception test
     */

    try
    {
        const auto stdExceptionPtr = std::make_exception_ptr( std::runtime_error( "std::exception: ErrorToJsonTests") );

        const auto serverErrorJson = dm::ServerErrorHelpers::createServerErrorObject( stdExceptionPtr );

        const auto exceptionPtr = dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson );

        std::rethrow_exception( exceptionPtr );
    }
    catch( std::exception& e )
    {
        UTF_REQUIRE_EQUAL( e.what(),  "std::exception: ErrorToJsonTests" );
    }

    /*
     * Create and throw SystemException test
     */

    try
    {
        const auto systemException = SystemException::create( errorCode, "It's a what() prefix" );
        systemException << eh::errinfo_message( "It's a message" );

        const auto stdExceptionPtr = std::make_exception_ptr( systemException );

        const auto serverErrorJson = dm::ServerErrorHelpers::createServerErrorObject( stdExceptionPtr );

        const auto exceptionPtr = dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson );

        std::rethrow_exception( exceptionPtr );
    }
    catch( SystemException& e )
    {
        UTF_REQUIRE_EQUAL( e.fullTypeName(), "bl::SystemException" );
        UTF_REQUIRE_EQUAL( e.what(), "It's a what() prefix: Permission denied" );

        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_message, "It's a message" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_category_name, "generic" )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_error_code, errorCode )
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_error_code_message, "Permission denied" )
    }

    /*
     * Throw an exception of an unknown type test
     */

    try
    {
        auto serverErrorJson = dm::ServerErrorJson::createInstance();

        serverErrorJson -> result( dm::ServerErrorResult::createInstance() );
        serverErrorJson -> result() -> exceptionProperties( dm::ExceptionProperties::createInstance() );
        serverErrorJson -> result() -> exceptionType( "Unknown-Exception-Type" );
        serverErrorJson -> result() -> exceptionProperties() -> message( "message: Unknown-Exception-Type" );

        const auto exceptionPtr = dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson );

        std::rethrow_exception( exceptionPtr );
    }
    catch( UnexpectedException& e )
    {
        UTF_REQUIRE_EQUAL( e.fullTypeName(), "bl::UnexpectedException" );
        UTF_CHECK_EQUAL( e.what(), "message: Unknown-Exception-Type" );
        UTEST_PROPERTY_REQUIRE_EQUAL( errinfo_message, "message: Unknown-Exception-Type" )
    }

    /*
     * Test that correct exceptions are thrown for invalid ServerErrorJson objects
     */

    {
        auto serverErrorJson = dm::ServerErrorJson::createInstance();

        UTF_REQUIRE_EXCEPTION(
            ( void )dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson ),
            ArgumentException,
            []( SAA_in const ArgumentException& e ) -> bool
            {
                return std::string( "ServerErrorJson: 'result' property is not set" ) == e.what();
            }
            );
    }

    {
        auto serverErrorJson = dm::ServerErrorJson::createInstance();
        serverErrorJson -> result( dm::ServerErrorResult::createInstance() );

        UTF_REQUIRE_EXCEPTION(
            ( void )dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson ),
            ArgumentException,
            []( SAA_in const ArgumentException& e ) -> bool
            {
                return std::string("ServerErrorResult: 'exceptionProperties' property is not set" ) == e.what();
            }
            );
    }

    {
        auto serverErrorJson = dm::ServerErrorJson::createInstance();
        serverErrorJson -> result( dm::ServerErrorResult::createInstance() );
        serverErrorJson -> result() -> exceptionType( "bl::ServerErrorException" );
        serverErrorJson -> result() -> exceptionProperties( dm::ExceptionProperties::createInstance() );
        serverErrorJson -> result() -> exceptionProperties() -> categoryName( "non-generic" );

        UTF_REQUIRE_EXCEPTION(
            ( void )dm::ServerErrorHelpers::createExceptionFromObject( serverErrorJson ),
            ArgumentException,
            []( SAA_in const ArgumentException& e ) -> bool
            {
                return std::string("Unknown error category: 'non-generic'" ) == e.what();
            }
            );
    }

#undef UTEST_PROPERTY_REQUIRE_EQUAL
}

UTF_AUTO_TEST_CASE( ServerErrorHelpersTests )
{
    using namespace bl;

    {
        /*
         * Let's test using UserMessageException
         */

        std::exception_ptr eptr;

        const std::string userMessage = "Test user exception";

        try
        {
            BL_THROW_USER( userMessage );
        }
        catch( std::exception& )
        {
            /*
             * Don't use std::make_exception_ptr() as it slices the exception object
             */

            eptr = std::current_exception();
        }

        UTF_REQUIRE( eptr );

        const auto serverError = dm::ServerErrorHelpers::createServerErrorObject( eptr );

        UTF_REQUIRE( serverError );
        UTF_REQUIRE( serverError -> result() );
        UTF_CHECK_EQUAL( serverError -> result() -> exceptionType(), "bl::UserMessageException" );
        UTF_CHECK_EQUAL( serverError -> result() -> exceptionMessage(), userMessage );
        UTF_CHECK( ! serverError -> result() -> exceptionFullDump().empty() );

        const auto& properties = serverError -> result() -> exceptionProperties();

        UTF_REQUIRE( properties );
        UTF_CHECK_EQUAL( properties -> isUserFriendly(), true );
        UTF_CHECK_EQUAL( properties -> message(), userMessage );

        auto reconstructedEptr = dm::ServerErrorHelpers::createExceptionFromObject( serverError );

        UTF_REQUIRE( reconstructedEptr );
        UTF_REQUIRE_THROW(
            cpp::safeRethrowException( reconstructedEptr ),
            UserMessageException
            );

        try
        {
            cpp::safeRethrowException( reconstructedEptr );
        }
        catch( UserMessageException& e )
        {
            UTF_CHECK_EQUAL( e.what(), userMessage );
        }
    }

    const auto errorCode = eh::errc::make_error_code( eh::errc::permission_denied );

    const auto exception = std::make_exception_ptr(
        eh::enable_current_exception( eh::enable_error_info( HttpServerException() ) )
            << eh::throw_function( BOOST_CURRENT_FUNCTION )
            << eh::throw_file( __FILE__ )
            << eh::throw_line( __LINE__ )
            << eh::errinfo_errno                       ( 1001 )
            << eh::errinfo_file_name                   ( "file_name: ServerErrorHelpersTests" )
            << eh::errinfo_file_open_mode              ( "file_open_mode: ServerErrorHelpersTests" )
            << eh::errinfo_message                     ( "message: Bad client request" )
            << eh::errinfo_time_thrown                 ( "2015-09-23T18:29:10.475824-04:00" )
            << eh::errinfo_function_name               ( "function_name: ServerErrorHelpersTests" )
            << eh::errinfo_system_code                 ( http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST )
            << eh::errinfo_category_name               ( errorCode.category().name() /* "generic" */ )
            << eh::errinfo_error_code                  ( errorCode )
            << eh::errinfo_error_code_message          ( errorCode.message() /* "Permission denied" */ )
            << eh::errinfo_is_expected                 ( true )
            << eh::errinfo_task_info                   ( "task_info: ServerErrorHelpersTests" )
            << eh::errinfo_host_name                   ( net::getShortHostName() )
            << eh::errinfo_service_name                ( "service_name: ServerErrorHelpersTests" )
            << eh::errinfo_endpoint_address            ( "endpoint_address: ServerErrorHelpersTests address" )
            << eh::errinfo_endpoint_port               ( 1002 )
            << eh::errinfo_http_url                    ( "www.mytest.com" )
            << eh::errinfo_http_redirect_url           ( "www.myredirectedtest.com" )
            << eh::errinfo_http_status_code            ( 1003 )
            << eh::errinfo_http_response_headers       ( "<header1><header2><header3>" )
            << eh::errinfo_http_request_details        ( "_http_request_details: ServerErrorHelpersTests" )
            << eh::errinfo_parser_file                 ( "Blablabla" )
            << eh::errinfo_parser_line                 ( 1004 )
            << eh::errinfo_parser_column               ( 1005 )
            << eh::errinfo_parser_reason               ( "syntax error" )
            << eh::errinfo_external_command_output     ( "external_command_output: ServerErrorHelpersTests" )
            << eh::errinfo_external_command_exit_code  ( 1006 )
            << eh::errinfo_string_value                ( "string_value: ServerErrorHelpersTests" )
            << eh::errinfo_is_user_friendly            ( false )
            << eh::errinfo_ssl_is_verify_failed        ( true )
            << eh::errinfo_ssl_is_verify_error         ( 1007 )
            << eh::errinfo_ssl_is_verify_error_message ( "ssl_is_verify_error_message: ServerErrorHelpersTests" )
            << eh::errinfo_ssl_is_verify_error_string  ( "ssl_is_verify_error_string: ServerErrorHelpersTests" )
            << eh::errinfo_ssl_is_verify_subject_name  ( "ssl_is_verify_subject_name: ServerErrorHelpersTests" )
        );

    const auto serverErrorAsJson = dm::ServerErrorHelpers::getServerErrorAsJson( exception );

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\nThe JSON text for server error message is: \n "
            << serverErrorAsJson
            << "\n"
        );

    const auto serverError = dm::ServerErrorHelpers::createServerErrorObject( exception );

    UTF_REQUIRE_EQUAL( serverError -> result() -> exceptionType(), "bl::HttpServerException" );
    UTF_REQUIRE_EQUAL( serverError -> result() -> exceptionMessage(), "message: Bad client request" );
    UTF_REQUIRE( ! serverError -> result() -> exceptionFullDump().empty() );

    const auto validateProperties = []( SAA_in const om::ObjPtr< dm::ExceptionProperties >& properties ) -> void
    {
        UTF_REQUIRE_EQUAL( properties -> errNo()                   , 1001 );
        UTF_REQUIRE_EQUAL( properties -> fileName()                , "file_name: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> fileOpenMode()            , "file_open_mode: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> message()                 , "message: Bad client request" );
        UTF_REQUIRE_EQUAL( properties -> timeThrown()              , "2015-09-23T18:29:10.475824-04:00" );
        UTF_REQUIRE_EQUAL( properties -> functionName()            , "function_name: ServerErrorHelpersTests" )
        UTF_REQUIRE_EQUAL( properties -> systemCode()              , static_cast< int >( http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST ) );
        UTF_REQUIRE_EQUAL( properties -> categoryName()            , "generic" );
        UTF_REQUIRE_EQUAL( properties -> errorCode()               , eh::errc::permission_denied );
        UTF_REQUIRE_EQUAL( properties -> errorCodeMessage()        , "Permission denied" );
        UTF_REQUIRE_EQUAL( properties -> isExpected()              , true );
        UTF_REQUIRE_EQUAL( properties -> taskInfo()                , "task_info: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> hostName()                , net::getShortHostName() );
        UTF_REQUIRE_EQUAL( properties -> serviceName()             , "service_name: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> endpointAddress()         , "endpoint_address: ServerErrorHelpersTests address" );
        UTF_REQUIRE_EQUAL( properties -> endpointPort()            , 1002 );
        UTF_REQUIRE_EQUAL( properties -> httpUrl()                 , "www.mytest.com" );
        UTF_REQUIRE_EQUAL( properties -> httpRedirectUrl()         , "www.myredirectedtest.com" );
        UTF_REQUIRE_EQUAL( properties -> httpStatusCode()          , 1003 );
        UTF_REQUIRE_EQUAL( properties -> httpResponseHeaders()     , "<header1><header2><header3>" );
        UTF_REQUIRE_EQUAL( properties -> httpRequestDetails()      , "_http_request_details: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> parserFile()              , "Blablabla" );
        UTF_REQUIRE_EQUAL( properties -> parserLine()              , 1004 );
        UTF_REQUIRE_EQUAL( properties -> parserColumn()            , 1005 );
        UTF_REQUIRE_EQUAL( properties -> parserReason()            , "syntax error" );
        UTF_REQUIRE_EQUAL( properties -> externalCommandOutput()   , "external_command_output: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> externalCommandExitCode() , 1006 );
        UTF_REQUIRE_EQUAL( properties -> stringValue()             , "string_value: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> isUserFriendly()          , false );
        UTF_REQUIRE_EQUAL( properties -> sslIsVerifyFailed()       , true );
        UTF_REQUIRE_EQUAL( properties -> sslIsVerifyError()        , 1007 );
        UTF_REQUIRE_EQUAL( properties -> sslIsVerifyErrorMessage() , "ssl_is_verify_error_message: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> sslIsVerifyErrorString () , "ssl_is_verify_error_string: ServerErrorHelpersTests" );
        UTF_REQUIRE_EQUAL( properties -> sslIsVerifySubjectName () , "ssl_is_verify_subject_name: ServerErrorHelpersTests" );
    };

    validateProperties( serverError -> result() -> exceptionProperties() );

    /*
     * Get the part related to the exception properties
     */

    auto pos = serverErrorAsJson.find( "exceptionProperties" );

    auto exceptionProperties = serverErrorAsJson.substr( pos );

    pos = exceptionProperties.find( "{" );
    const auto posEnd = exceptionProperties.find( "}" );

    UTF_REQUIRE( pos != std::string::npos );
    UTF_REQUIRE( posEnd != std::string::npos );

    exceptionProperties = exceptionProperties.substr( pos - 1U, posEnd - pos + 2U );

    const auto properties =
        dm::DataModelUtils::loadFromJsonText< dm::ExceptionProperties >( exceptionProperties );

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            <<"\nThe exception properties of a server error JSON are: "
            << dm::DataModelUtils::getDocAsPrettyJsonString( properties )
            << "\n"
        );

    validateProperties( properties );
}
