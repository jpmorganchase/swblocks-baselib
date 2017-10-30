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

#include <utests/baselib/TestRestUtils.h>

UTF_AUTO_TEST_CASE( RestServiceSslBackendTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto callbackTests = [ & ]() -> void
    {
        std::unordered_set< std::string > tokenCookieNames;
        tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

        utest::TestRestUtils::httpRestWithMessagingBackendTests(
            cpp::void_callback_t()                                          /* callback */,
            false                                                           /* waitOnServer */,
            false                                                           /* isQuietMode */,
            1U                                                              /* requestsCount */,
            uuids::create()                                                 /* gatewayPeerId */,
            uuids::create()                                                 /* serverPeerId */,
            om::copy( controlToken )                                        /* controlToken */,
            test::UtfArgsParser::host()                                     /* brokerHostName */,
            test::UtfArgsParser::port()                                     /* brokerInboundPort */,
            test::UtfArgsParser::connections()                              /* noOfConnections */,
            cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
            std::move( tokenCookieNames )                                   /* tokenCookieNames */,
            cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
            );
    };

    utest::TestRestUtils::startBrokerAndRunTests( callbackTests, controlToken );
}

UTF_AUTO_TEST_CASE( RestServiceSslBackendAssortedTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto gatewayPeerId = uuids::create();
    const auto serverPeerId = uuids::create();

    const auto callbackTests = [ & ]() -> void
    {
        std::unordered_set< std::string > tokenCookieNames;
        tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

        const auto callback = [ & ]() -> void
        {
            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                    const os::port_t httpPort =
                        utest::TestRestUtils::getHttpPort( test::UtfArgsParser::port() /* brokerInboundPort */ );

                    const auto payload = dm::DataModelUtils::loadFromFile< Payload >(
                        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                        );

                    auto payloadDataString = dm::DataModelUtils::getDocAsPackedJsonString( payload );

                    /*
                     * Simple JSON echo request response
                     */

                    {
                        auto taskImpl = utest::TestRestUtils::executeHttpRequest(
                            eq,
                            httpPort,
                            false                                       /* allowFailure */,
                            http::HttpHeader::g_contentTypeJsonUtf8     /* contentType */,
                            cpp::copy( payloadDataString )              /* content */
                            );

                        const auto& response = taskImpl -> getResponse();

                        UTF_REQUIRE( ! response.empty() );
                        UTF_REQUIRE_EQUAL( payloadDataString, response );
                    }

                    /*
                     * Get the request metadata and verify it is what is expected
                     */

                    {
                        auto taskImpl = utest::TestRestUtils::executeHttpRequest(
                            eq,
                            httpPort,
                            false                                       /* allowFailure */,
                            str::empty()                                /* contentType */,
                            std::string()                               /* content */,
                            "/requestMetadata"                          /* urlPath */
                            );

                        const auto& response = taskImpl -> getResponse();

                        UTF_REQUIRE( ! response.empty() );

                        const auto brokerProtocol =
                            dm::DataModelUtils::loadFromJsonText< BrokerProtocol >( response );

                        UTF_REQUIRE( brokerProtocol );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\n**********************************************\n"
                                << "\nBroker protocol message (request metadata):\n\n"
                                << dm::DataModelUtils::getDocAsPrettyJsonString( brokerProtocol )
                                << "\n\n"
                            );

                        /*
                         * Verify that the broker protocol message object has the expected content
                         */

                        UTF_REQUIRE( ! brokerProtocol -> conversationId().empty() );
                        ( void ) uuids::string2uuid( brokerProtocol -> conversationId() );

                        UTF_REQUIRE( ! brokerProtocol -> messageId().empty() );
                        ( void ) uuids::string2uuid( brokerProtocol -> messageId() );

                        UTF_REQUIRE_EQUAL( brokerProtocol -> messageType(), std::string( "AsyncRpcDispatch" ) );

                        {
                            UTF_REQUIRE( ! brokerProtocol -> sourcePeerId().empty() );
                            const auto sourcePeerId = uuids::string2uuid( brokerProtocol -> sourcePeerId() );
                            UTF_REQUIRE_EQUAL( sourcePeerId, gatewayPeerId );

                            UTF_REQUIRE( ! brokerProtocol -> targetPeerId().empty() );
                            const auto targetPeerId = uuids::string2uuid( brokerProtocol -> targetPeerId() );
                            UTF_REQUIRE_EQUAL( targetPeerId, serverPeerId );
                        }

                        {
                            const auto& passThroughUserData = brokerProtocol -> passThroughUserData();
                            UTF_REQUIRE( passThroughUserData );

                            const auto payload =
                                dm::DataModelUtils::castTo< dm::http::HttpRequestMetadataPayload >(
                                    passThroughUserData
                                    );

                            const auto& requestMetadata = payload -> httpRequestMetadata();
                            UTF_REQUIRE( requestMetadata );

                            const auto& cookies = utest::DummyAuthorizationCache::dummyTokenData();

                            UTF_REQUIRE_EQUAL( requestMetadata -> method(), "GET" );
                            UTF_REQUIRE_EQUAL( requestMetadata -> urlPath(), "/requestMetadata" );

                            UTF_REQUIRE( ! requestMetadata -> headers().at( "Host" ).empty() );
                            UTF_REQUIRE_EQUAL( requestMetadata -> headers().at( "Accept" ), "*/*" );
                            UTF_REQUIRE_EQUAL( requestMetadata -> headers().at( "Connection" ), "close" );
                            UTF_REQUIRE_EQUAL( requestMetadata -> headers().at( "Cookie" ), cookies );
                        }

                        {
                            const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();
                            UTF_REQUIRE( principalIdentityInfo );

                            const auto& securityPrincipal = principalIdentityInfo -> securityPrincipal();
                            UTF_REQUIRE( securityPrincipal );
                            UTF_REQUIRE( nullptr == principalIdentityInfo -> authenticationToken() );


                            const auto& sid = utest::DummyAuthorizationCache::dummySid();

                            UTF_REQUIRE_EQUAL( securityPrincipal -> email(), "john.smith@host.com" );
                            UTF_REQUIRE_EQUAL( securityPrincipal -> familyName(), "Smith" );
                            UTF_REQUIRE_EQUAL( securityPrincipal -> givenName(), "John" );
                            UTF_REQUIRE_EQUAL( securityPrincipal -> sid(), sid );
                        }
                    }

                    /*
                     * Get the response metadata and verify it is what is expected
                     */

                    const auto testResponseMetadata = [ & ](
                        SAA_in          const std::string&                  urlPath,
                        SAA_in          const std::string&                  expectedCookie
                        )
                        -> void
                    {
                        auto taskImpl = utest::TestRestUtils::executeHttpRequest(
                            eq,
                            httpPort,
                            false                                       /* allowFailure */,
                            str::empty()                                /* contentType */,
                            std::string()                               /* content */,
                            cpp::copy( urlPath )                        /* urlPath */
                            );

                        const auto& response = taskImpl -> getResponse();

                        UTF_REQUIRE( ! response.empty() );

                        const auto brokerProtocol =
                            dm::DataModelUtils::loadFromJsonText< BrokerProtocol >( response );

                        UTF_REQUIRE( brokerProtocol );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\n**********************************************\n"
                                << "\nBroker protocol message (response metadata):\n\n"
                                << dm::DataModelUtils::getDocAsPrettyJsonString( brokerProtocol )
                                << "\n\n"
                            );

                        /*
                         * Verify that the broker protocol message object has the expected content
                         */

                        UTF_REQUIRE( ! brokerProtocol -> conversationId().empty() );
                        ( void ) uuids::string2uuid( brokerProtocol -> conversationId() );

                        UTF_REQUIRE( ! brokerProtocol -> messageId().empty() );
                        ( void ) uuids::string2uuid( brokerProtocol -> messageId() );

                        UTF_REQUIRE_EQUAL( brokerProtocol -> messageType(), std::string( "AsyncRpcDispatch" ) );

                        UTF_REQUIRE( brokerProtocol -> sourcePeerId().empty() );
                        UTF_REQUIRE( brokerProtocol -> targetPeerId().empty() );

                        {
                            const auto& passThroughUserData = brokerProtocol -> passThroughUserData();
                            UTF_REQUIRE( passThroughUserData );

                            const auto payload =
                                dm::DataModelUtils::castTo< dm::http::HttpResponseMetadataPayload >(
                                    passThroughUserData
                                    );

                            const auto& responseMetadata = payload -> httpResponseMetadata();
                            UTF_REQUIRE( responseMetadata );

                            UTF_REQUIRE_EQUAL(
                                static_cast< http::Parameters::HttpStatusCode >(
                                    responseMetadata -> httpStatusCode()
                                    ),
                                http::Parameters::HTTP_SUCCESS_OK
                                );

                            UTF_REQUIRE_EQUAL(
                                responseMetadata -> contentType(),
                                http::HttpHeader::g_contentTypeJsonUtf8
                                );

                            UTF_REQUIRE_EQUAL(
                                responseMetadata -> headers().at( "Set-Cookie" ),
                                expectedCookie
                                );
                        }

                        {
                            const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();
                            UTF_REQUIRE( principalIdentityInfo );

                            const auto& authenticationToken = principalIdentityInfo -> authenticationToken();
                            UTF_REQUIRE( authenticationToken );
                            UTF_REQUIRE( nullptr == principalIdentityInfo -> securityPrincipal() );

                            UTF_REQUIRE_EQUAL(
                                authenticationToken -> type(),
                                utest::DummyAuthorizationCache::dummyTokenType()
                                );

                            UTF_REQUIRE_EQUAL(
                                authenticationToken -> data(),
                                utest::DummyAuthorizationCache::dummyTokenData()
                                );
                        }
                    };

                    testResponseMetadata(
                        "/responseMetadata"                             /* urlPath */,
                        "responseCookieName=responseCookieValue;"       /* expectedCookie */
                        );

                    testResponseMetadata(
                        "/cookie/foo/bar/baz"                           /* urlPath */,
                        "responseCookieName=/cookie/foo/bar/baz;"       /* expectedCookie */
                        );

                    /*
                     * Get HTTP status error response and verify it is what is expected
                     */

                    const auto testErrorScenario = [ & ](
                        SAA_in          const std::string&                  urlPath,
                        SAA_in          const unsigned int                  expectedHttpStatus
                        )
                        -> void
                    {
                        http::StatusesList expectedHttpStatuses;
                        expectedHttpStatuses.insert( expectedHttpStatus );

                        auto taskImpl = utest::TestRestUtils::executeHttpRequest(
                            eq,
                            httpPort,
                            true                                        /* allowFailure */,
                            str::empty()                                /* contentType */,
                            std::string()                               /* content */,
                            cpp::copy( urlPath )                        /* urlPath */,
                            "GET"                                       /* action */,
                            std::string()                               /* tokenData */,
                            expectedHttpStatuses                        /* expectedHttpStatuses */
                            );

                        UTF_REQUIRE( taskImpl -> isFailed() );
                        UTF_REQUIRE( taskImpl -> exception() );

                        UTF_REQUIRE_EQUAL( taskImpl -> getHttpStatus(), expectedHttpStatus );

                        const auto& response = taskImpl -> getResponse();

                        UTF_REQUIRE( ! response.empty() );

                        const auto errorJson =
                            dm::DataModelUtils::loadFromJsonText< dm::ServerErrorJson >( response );

                        UTF_REQUIRE( errorJson );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\n**********************************************\n"
                                << "\nError as JSON response:\n\n"
                                << dm::DataModelUtils::getDocAsPrettyJsonString( errorJson )
                                << "\n\n"
                            );

                        UTF_REQUIRE( errorJson -> result() );
                        UTF_REQUIRE( ! errorJson -> result() -> exceptionFullDump().empty() );

                        const auto ecExpected = eh::errc::make_error_code( eh::errc::bad_file_descriptor );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> exceptionMessage(),
                            std::string( "System error has occurred: " ) + ecExpected.message()
                            );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> exceptionType(),
                            std::string( "bl::SystemException" )
                            );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> message(),
                            std::string( "An unexpected error has occurred" )
                            );

                        UTF_REQUIRE( errorJson -> result() -> exceptionProperties() );

                        const auto& exceptionProperties = errorJson -> result() -> exceptionProperties();

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> categoryName(),
                            std::string( ecExpected.category().name() )
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errNo(),
                            ecExpected.value()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errorCode(),
                            ecExpected.value()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errorCodeMessage(),
                            ecExpected.message()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> message(),
                            std::string( "System error has occurred" )
                            );
                    };

                    {
                        testErrorScenario(
                            "/error/HTTP_CLIENT_ERROR_UNAUTHORIZED"                         /* urlPath */,
                            http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED                /* expectedHttpStatus */
                            );

                        testErrorScenario(
                            "/error/HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE"                  /* urlPath */,
                            http::Parameters::HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE         /* expectedHttpStatus */
                            );

                        testErrorScenario(
                            "/error/HTTP_SERVER_ERROR_INTERNAL"                             /* urlPath */,
                            http::Parameters::HTTP_SERVER_ERROR_INTERNAL                    /* expectedHttpStatus */
                            );

                        testErrorScenario(
                            "/error/HTTP_CLIENT_ERROR_NOT_FOUND"                            /* urlPath */,
                            http::Parameters::HTTP_CLIENT_ERROR_NOT_FOUND                   /* expectedHttpStatus */
                            );

                        testErrorScenario(
                            "/error/HTTP_SERVER_ERROR_NOT_IMPLEMENTED"                      /* urlPath */,
                            http::Parameters::HTTP_SERVER_ERROR_NOT_IMPLEMENTED             /* expectedHttpStatus */
                            );

                        testErrorScenario(
                            "/error/HTTP_CLIENT_ERROR_FORBIDDEN"                            /* urlPath */,
                            http::Parameters::HTTP_CLIENT_ERROR_FORBIDDEN                   /* expectedHttpStatus */
                            );
                    }

                    /*
                     * Test the case where the broker authorization fails
                     */

                    {
                        http::StatusesList expectedHttpStatuses;
                        expectedHttpStatuses.insert( http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED );

                        std::string tokenData = utest::DummyAuthorizationCache::dummyTokenDataUnauthorized();

                        auto taskImpl = utest::TestRestUtils::executeHttpRequest(
                            eq,
                            httpPort,
                            true                                        /* allowFailure */,
                            str::empty()                                /* contentType */,
                            std::string()                               /* content */,
                            "/foo/bar"                                  /* urlPath */,
                            "GET"                                       /* action */,
                            std::move( tokenData )                      /* tokenData */,
                            expectedHttpStatuses                        /* expectedHttpStatuses */
                            );

                        UTF_REQUIRE( taskImpl -> isFailed() );
                        UTF_REQUIRE( taskImpl -> exception() );

                        UTF_REQUIRE_EQUAL(
                            taskImpl -> getHttpStatus(),
                            http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED
                            );

                        const auto& response = taskImpl -> getResponse();

                        UTF_REQUIRE( ! response.empty() );

                        const auto errorJson =
                            dm::DataModelUtils::loadFromJsonText< dm::ServerErrorJson >( response );

                        UTF_REQUIRE( errorJson );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\n**********************************************\n"
                                << "\nError as JSON response:\n\n"
                                << dm::DataModelUtils::getDocAsPrettyJsonString( errorJson )
                                << "\n\n"
                            );

                        UTF_REQUIRE( errorJson -> result() );
                        UTF_REQUIRE( ! errorJson -> result() -> exceptionFullDump().empty() );

                        const auto ecExpected = eh::errc::make_error_code( eh::errc::permission_denied );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> exceptionMessage(),
                            std::string( "Server error has occurred: " ) + ecExpected.message()
                            );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> exceptionType(),
                            std::string( "bl::ServerErrorException" )
                            );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> message(),
                            std::string( "An unexpected error has occurred" )
                            );

                        UTF_REQUIRE( errorJson -> result() -> exceptionProperties() );

                        const auto& exceptionProperties = errorJson -> result() -> exceptionProperties();

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errNo(),
                            ecExpected.value()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errorCode(),
                            ecExpected.value()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> message(),
                            std::string( "Server error has occurred: " ) + ecExpected.message()
                            );
                    }

                    /*
                     * Test the case where the gateway authorization fails because
                     * cookie name is incorrect and authentication token can't be created
                     */

                    {
                        http::StatusesList expectedHttpStatuses;
                        expectedHttpStatuses.insert( http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED );

                        std::string tokenData( "invalidCookieName=invalidCookieNameValue;" );

                        auto taskImpl = utest::TestRestUtils::executeHttpRequest(
                            eq,
                            httpPort,
                            true                                        /* allowFailure */,
                            str::empty()                                /* contentType */,
                            std::string()                               /* content */,
                            "/foo/bar"                                  /* urlPath */,
                            "GET"                                       /* action */,
                            std::move( tokenData )                      /* tokenData */,
                            expectedHttpStatuses                        /* expectedHttpStatuses */
                            );

                        UTF_REQUIRE( taskImpl -> isFailed() );
                        UTF_REQUIRE( taskImpl -> exception() );

                        UTF_REQUIRE_EQUAL(
                            taskImpl -> getHttpStatus(),
                            http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED
                            );

                        const auto& response = taskImpl -> getResponse();

                        UTF_REQUIRE( ! response.empty() );

                        const auto errorJson =
                            dm::DataModelUtils::loadFromJsonText< dm::ServerErrorJson >( response );

                        UTF_REQUIRE( errorJson );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\n**********************************************\n"
                                << "\nError as JSON response:\n\n"
                                << dm::DataModelUtils::getDocAsPrettyJsonString( errorJson )
                                << "\n\n"
                            );

                        UTF_REQUIRE( errorJson -> result() );
                        UTF_REQUIRE( ! errorJson -> result() -> exceptionFullDump().empty() );

                        const auto ecExpected = eh::errc::make_error_code( eh::errc::permission_denied );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> exceptionMessage(),
                            std::string( "Authentication information is required in the HTTP request: " )
                                + ecExpected.message()
                            );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> exceptionType(),
                            std::string( "bl::SystemException" )
                            );

                        UTF_REQUIRE_EQUAL(
                            errorJson -> result() -> message(),
                            std::string( "Authentication information is required in the HTTP request: " )
                                + ecExpected.message()
                            );

                        UTF_REQUIRE( errorJson -> result() -> exceptionProperties() );

                        const auto& exceptionProperties = errorJson -> result() -> exceptionProperties();

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> categoryName(),
                            std::string( ecExpected.category().name() )
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errNo(),
                            ecExpected.value()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errorCode(),
                            ecExpected.value()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> errorCodeMessage(),
                            ecExpected.message()
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> isUserFriendly(),
                            true
                            );

                        UTF_REQUIRE_EQUAL(
                            exceptionProperties -> message(),
                            std::string( "Authentication information is required in the HTTP request" )
                            );
                    }
                }
                );
        };

        utest::TestRestUtils::httpRestWithMessagingBackendTests(
            callback                                                        /* callback */,
            false                                                           /* waitOnServer */,
            false                                                           /* isQuietMode */,
            1U                                                              /* requestsCount */,
            gatewayPeerId                                                   /* gatewayPeerId */,
            serverPeerId                                                    /* serverPeerId */,
            om::copy( controlToken )                                        /* controlToken */,
            test::UtfArgsParser::host()                                     /* brokerHostName */,
            test::UtfArgsParser::port()                                     /* brokerInboundPort */,
            test::UtfArgsParser::connections()                              /* noOfConnections */,
            cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
            std::move( tokenCookieNames )                                   /* tokenCookieNames */,
            cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
            );
    };

    utest::TestRestUtils::startBrokerAndRunTests( callbackTests, controlToken );
}


UTF_AUTO_TEST_CASE( RestServiceSslBackendPerfTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto callbackTests = [ & ]() -> void
    {
        std::unordered_set< std::string > tokenCookieNames;
        tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

        utest::TestRestUtils::httpRestWithMessagingBackendTests(
            cpp::void_callback_t()                                          /* callback */,
            false                                                           /* waitOnServer */,
            true                                                            /* isQuietMode */,
            50U                                                             /* requestsCount */,
            uuids::create()                                                 /* gatewayPeerId */,
            uuids::create()                                                 /* serverPeerId */,
            om::copy( controlToken )                                        /* controlToken */,
            test::UtfArgsParser::host()                                     /* brokerHostName */,
            test::UtfArgsParser::port()                                     /* brokerInboundPort */,
            test::UtfArgsParser::connections()                              /* noOfConnections */,
            cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
            std::move( tokenCookieNames )                                   /* tokenCookieNames */,
            cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
            );
    };

    utest::TestRestUtils::startBrokerAndRunTests( callbackTests, controlToken );
}

UTF_AUTO_TEST_CASE( RestServiceSslHttpGatewayOnlyTests )
{
    using namespace bl;
    using namespace bl::tasks;

    if( ! test::UtfArgsParser::isServer() )
    {
        /*
         * This is a manual run test
         */

        return;
    }

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    std::unordered_set< std::string > tokenCookieNames;
    tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

    utest::TestRestUtils::httpRestWithMessagingBackendTests(
        cpp::void_callback_t()                                          /* callback */,
        true                                                            /* waitOnServer */,
        true                                                            /* isQuietMode */,
        0U                                                              /* requestsCount */,
        uuids::create()                                                 /* gatewayPeerId */,
        uuids::create()                                                 /* serverPeerId */,
        om::copy( controlToken )                                        /* controlToken */,
        test::UtfArgsParser::host()                                     /* brokerHostName */,
        test::UtfArgsParser::port()                                     /* brokerInboundPort */,
        test::UtfArgsParser::connections()                              /* noOfConnections */,
        cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
        std::move( tokenCookieNames )                                   /* tokenCookieNames */,
        cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
        );
}

UTF_AUTO_TEST_CASE( RestServiceSslBackendHttpOnlyTests )
{
    if( ! test::UtfArgsParser::isClient() )
    {
        /*
         * This is a manual run test
         */

        return;
    }

    if( test::UtfArgsParser::tokenData().empty() )
    {
        UTF_FAIL( "--token-data is a required parameter for this test" );

        return;
    }

    utest::TestRestUtils::httpRunSimpleRequest(
        test::UtfArgsParser::port()                 /* httpPort */,
        test::UtfArgsParser::tokenData()            /* tokenData */,
        test::UtfArgsParser::connections()          /* requestsCount */,
        true                                        /* isQuietMode */
        );

}
