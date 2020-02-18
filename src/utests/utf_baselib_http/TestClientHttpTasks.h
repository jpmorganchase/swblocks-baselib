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

UTF_AUTO_TEST_CASE( Client_SimpleHttpTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    utest::http::HttpServerHelpers::startHttpServerAndExecuteCallback(
        []() -> void
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n******************************** Starting test: Client_SimpleHttpTests ********************************\n"
                );

            scheduleAndExecuteInParallel(
                []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( ExecutionQueue::OptionKeepAll );

                    /*
                     * Success test case
                     */

                    {
                        http::HeadersMap headers;

                        headers[ "MyHeader" ] = "MyValue";

                        const auto stask = SimpleHttpGetTaskImpl::createInstance(
                            cpp::copy( test::UtfArgsParser::host() ),
                            cpp::copy( test::UtfArgsParser::port() ),
                            utest::http::g_requestUri,
                            std::move( headers )
                            );

                        const auto task = om::qi< Task >( stask );
                        UTF_REQUIRE_EQUAL( Task::Created, task -> getState() );

                        eq -> push_back( task );
                        const auto executedTask = eq -> pop( true );

                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* HTTP task executed ******* \n" );

                        UTF_REQUIRE( executedTask );
                        UTF_REQUIRE( om::areEqual( task, executedTask ) );
                        UTF_REQUIRE_EQUAL( Task::Completed, task -> getState() );
                        UTF_REQUIRE( eq -> isEmpty() );

                        if( stask -> isFailed() )
                        {
                            UTF_REQUIRE( nullptr != stask -> exception() );
                            cpp::safeRethrowException( stask -> exception() );
                        }

                        // Check the response code is 200, and content was received
                        UTF_REQUIRE( nullptr == stask -> exception() );
                        UTF_REQUIRE_EQUAL( 200U, stask -> getHttpStatus() );

                        const auto contentType = stask -> tryGetResponseHeader( http::Parameters::HttpHeader::g_contentType );

                        UTF_REQUIRE( contentType );
                        UTF_REQUIRE( str::istarts_with( *contentType, "application/json;" ) );

                        const auto userAgentValue = stask -> tryGetResponseHeader( "request-user-agent-id" );
                        UTF_REQUIRE( userAgentValue == nullptr );

                        const auto& response = stask -> getResponse();

                        UTF_REQUIRE( response.size() );
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* begin HTTP response ******* \n" );
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << response );
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* end HTTP response ******* \n" );
                    }

                    /*
                     * Success test case with user agent string provided
                     */

                    {
                        UTF_REQUIRE( str::icontains( http::HttpHeader::g_userAgentBotDefault, "bot" ) );

                        UTF_REQUIRE_EQUAL( http::Parameters::userAgentDefault(), str::empty() );
                        http::Parameters::userAgentDefault( cpp::copy( http::HttpHeader::g_userAgentBotDefault ) );

                        {
                            BL_SCOPE_EXIT(
                                {
                                    http::Parameters::userAgentDefault( std::string() );
                                }
                                );

                            http::HeadersMap headers;

                            headers[ "MyHeader" ] = "MyValue";

                            const auto taskImpl = SimpleHttpGetTaskImpl::createInstance(
                                cpp::copy( test::UtfArgsParser::host() ),
                                cpp::copy( test::UtfArgsParser::port() ),
                                utest::http::g_requestUri,
                                std::move( headers )
                                );

                            const auto task = om::qi< Task >( taskImpl );
                            UTF_REQUIRE_EQUAL( Task::Created, task -> getState() );

                            eq -> push_back( task );
                            const auto executedTask = eq -> pop( true );

                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "\n******* HTTP task with user agent value executed ******* \n"
                                    );

                            UTF_REQUIRE( executedTask );
                            UTF_REQUIRE( om::areEqual( task, executedTask ) );
                            UTF_REQUIRE_EQUAL( Task::Completed, task -> getState() );
                            UTF_REQUIRE( eq -> isEmpty() );

                            if( taskImpl -> isFailed() )
                            {
                                UTF_REQUIRE( nullptr != taskImpl -> exception() );
                                cpp::safeRethrowException( taskImpl -> exception() );
                            }

                            UTF_REQUIRE( nullptr == taskImpl -> exception() );
                            UTF_REQUIRE_EQUAL( http::Parameters::HTTP_SUCCESS_OK, taskImpl -> getHttpStatus() );

                            const auto userAgentValue = taskImpl -> tryGetResponseHeader( "request-user-agent-id" );

                            UTF_REQUIRE( userAgentValue );
                            UTF_REQUIRE_EQUAL( http::HttpHeader::g_userAgentBotDefault, *userAgentValue );

                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "\n******* user agent value: "
                                    << *userAgentValue
                                    );
                        }
                    }

                    /*
                     * Failure test case
                     */

                    {
                        const auto stask = SimpleHttpGetTaskImpl::createInstance(
                            cpp::copy( test::UtfArgsParser::host() ),
                            cpp::copy( test::UtfArgsParser::port() ),
                            "/invalid_page"
                            );

                        const auto task = om::qi< Task >( stask );
                        UTF_REQUIRE_EQUAL( Task::Created, task -> getState() );

                        eq -> push_back( task );
                        const auto executedTask = eq -> pop( true );

                        UTF_REQUIRE( executedTask );
                        UTF_REQUIRE( om::areEqual( task, executedTask ) );
                        UTF_REQUIRE_EQUAL( Task::Completed, task -> getState() );
                        UTF_REQUIRE( eq -> isEmpty() );

                        // Check the response code is not 200 the request has failed
                        UTF_REQUIRE( stask -> isFailed() );
                        UTF_REQUIRE( ! stask -> isTimedOut() );
                        UTF_REQUIRE( nullptr != stask -> exception() );
                        UTF_REQUIRE( 200 != stask -> getHttpStatus() );

                        const auto contentType = stask -> tryGetResponseHeader( http::Parameters::HttpHeader::g_contentType );

                        UTF_REQUIRE( contentType );
                        UTF_REQUIRE( str::istarts_with( *contentType, bl::http::HttpHeader::g_contentTypeDefault ) );

                        UTF_REQUIRE( ! stask -> getRemoteEndpointId().empty() );

                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* begin HTTP task exception ******* \n" );
                        try
                        {
                            cpp::safeRethrowException( stask -> exception() );
                        }
                        catch( std::exception& e )
                        {
                            BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << eh::diagnostic_information( e ) );
                        }
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* end HTTP task exception ******* \n" );
                    }

                    /*
                     * Redirect test case
                     */

                    {
                        const auto stask = SimpleHttpGetTaskImpl::createInstance(
                            cpp::copy( test::UtfArgsParser::host() ),
                            cpp::copy( test::UtfArgsParser::port() ),
                            utest::http::g_redirectedRequestUri
                            );

                        const auto task = om::qi< Task >( stask );

                        eq -> push_back( task );

                        const std::string* redirectUrl = nullptr;

                        try
                        {
                            eq -> waitForSuccess( task );
                        }
                        catch ( HttpException& e )
                        {
                            redirectUrl = e.httpRedirectUrl();
                        }

                        if( test::UtfArgsParser::host() != "localhost" )
                        {
                            UTF_REQUIRE(
                                stask -> getHttpStatus() >= http::Parameters::HTTP_REDIRECT_START_RANGE &&
                                stask -> getHttpStatus() <= http::Parameters::HTTP_REDIRECT_END_RANGE
                                );

                            UTF_REQUIRE( nullptr != redirectUrl );
                        }
                    }
                });
        }
        );
}

UTF_AUTO_TEST_CASE( Client_SimpleHttpTimeoutTests )
{
    /*
     * Manual HTTP timeout test case. Works on Linux only, unless you use a
     * netcat or similar port for Windows. Follow these steps in order to run it:
     * - Start local netcat instance and pass to it few headers required to be dumped as part of the exception:
     *     echo -e "HTTP/1.0 500 ERROR\r\nrequestId: 123\r\nVersion: 1.0\r\nline1\r\n\line2\r\n\r\n" | nc -l 4545 -q 10
     * - Pass --is-client argument to the utest binary:
     *     ./utf-baselib-http --log_level=message --run_test=Client_SimpleHttpTimeoutTests --is-client
     * - After the test finished netcat instance will exit. Run another one to repeat the test
     */

    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    utest::http::HttpServerHelpers::startHttpServerAndExecuteCallback(
        []() -> void
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n******************************** Starting test: Client_SimpleHttpTimeoutTests ********************************\n"
                );

            scheduleAndExecuteInParallel(
                []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( ExecutionQueue::OptionKeepAll );

                    const auto stask = SimpleHttpGetTaskImpl::createInstance(
                        "localhost",
                        4545,
                        "/invalid_page"
                        );

                    stask -> setTimeout( time::seconds( 5 ) );

                    const auto task = om::qi< Task >( stask );
                    UTF_REQUIRE_EQUAL( Task::Created, task -> getState() );

                    eq -> push_back( task );
                    const auto executedTask = eq -> pop( true );

                    UTF_REQUIRE( executedTask );
                    UTF_REQUIRE( om::areEqual( task, executedTask ) );
                    UTF_REQUIRE_EQUAL( Task::Completed, task -> getState() );
                    UTF_REQUIRE( eq -> isEmpty() );

                    // Check the response code is not 200 the request has failed
                    UTF_REQUIRE( stask -> isFailed() );
                    UTF_REQUIRE( stask -> isTimedOut() );
                    UTF_REQUIRE( nullptr != stask -> exception() );
                    UTF_REQUIRE( 200 != stask -> getHttpStatus() );

                    UTF_REQUIRE( ! stask -> getRemoteEndpointId().empty() );

                    BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* begin HTTP task timeout exception ******* \n" );
                    try
                    {
                        cpp::safeRethrowException( stask -> exception() );
                    }
                    catch( std::exception& e )
                    {
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << eh::diagnostic_information( e ) );

                        const auto* headers = bl::eh::get_error_info< bl::eh::errinfo_http_response_headers >( e );
                        UTF_REQUIRE( headers );
                        UTF_REQUIRE( ! headers -> empty() );
                    }
                    BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* end HTTP task timeout exception ******* \n" );
                });
        }
        );
}

UTF_AUTO_TEST_CASE( Client_SimpleHttpPerfTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    utest::http::HttpServerHelpers::startHttpServerAndExecuteCallback(
        []() -> void
        {
            const std::size_t count = 50;

            const auto t1 = bl::time::microsec_clock::universal_time();

            {
                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "\n******************************** Starting test: Client_SimpleHttpPerfTests ********************************\n"
                    );

                scheduleAndExecuteInParallel(
                    [ count ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                    {
                        eq -> setOptions( ExecutionQueue::OptionKeepFailed );

                        /*
                         * Simple perf test
                         */
                        for( std::size_t i = 0; i < count; ++i )
                        {
                            const auto stask = SimpleHttpGetTaskImpl::createInstance(
                                cpp::copy( test::UtfArgsParser::host() ),
                                cpp::copy( test::UtfArgsParser::port() ),
                                utest::http::g_requestPerfUri
                                );

                            eq -> push_back( om::qi< Task >( stask ) );
                        }

                        executeQueueAndCancelOnFailure( eq );
                    });
            }

            const auto duration = bl::time::microsec_clock::universal_time() - t1;

            const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Executing "
                    << count
                    << " HTTP requests took "
                    << durationInSeconds
                    << " seconds"
                );
        }
        );
}

UTF_AUTO_TEST_CASE( Client_SimpleSecureHttpSslGetTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    utest::http::HttpServerHelpers::startHttpServerAndExecuteCallback< bl::httpserver::HttpSslServer >(
        []() -> void
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n******************************** Starting test: Client_SimpleSecureHttpSslGetTests ********************************\n"
                );

            scheduleAndExecuteInParallel(
                []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( ExecutionQueue::OptionKeepAll );

                    {
                        http::HeadersMap headers;

                        headers[ "MyHeader" ] = "MyValue";

                        bl::str::SecureStringWrapper content( "Hidden content" );

                        const auto stask = SimpleSecureHttpSslGetTaskImpl::createInstance(
                            cpp::copy( test::UtfArgsParser::host() ),
                            cpp::copy( test::UtfArgsParser::port() ),
                            utest::http::g_requestUri,
                            content,
                            std::move( headers )
                            );

                        UTF_REQUIRE_EQUAL( stask -> isSecureMode(), true );

                        const auto task = om::qi< Task >( stask );
                        UTF_REQUIRE_EQUAL( Task::Created, task -> getState() );

                        eq -> push_back( task );
                        const auto executedTask = eq -> pop( true );

                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* HTTP task executed ******* \n" );

                        UTF_REQUIRE( executedTask );
                        UTF_REQUIRE( om::areEqual( task, executedTask ) );
                        UTF_REQUIRE_EQUAL( Task::Completed, task -> getState() );
                        UTF_REQUIRE( eq -> isEmpty() );

                        if( stask -> isFailed() )
                        {
                            UTF_REQUIRE( nullptr != stask -> exception() );
                            cpp::safeRethrowException( stask -> exception() );
                        }

                        // Check the response code is 200, and content was received
                        UTF_REQUIRE( nullptr == stask -> exception() );
                        UTF_REQUIRE_EQUAL( 200U, stask -> getHttpStatus() );

                        const auto contentType = stask -> tryGetResponseHeader( http::Parameters::HttpHeader::g_contentType );

                        UTF_REQUIRE( contentType );
                        UTF_REQUIRE( str::istarts_with( *contentType, "application/json;" ) );

                        const auto& response = stask -> getResponse();

                        UTF_REQUIRE( response.size() );
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* begin HTTP response ******* \n" );
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << response );
                        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******* end HTTP response ******* \n" );
                    }

                });
        }
        );
}

