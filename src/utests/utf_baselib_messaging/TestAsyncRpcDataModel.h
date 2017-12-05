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

#include <utests/baselib/TestUtils.h>

#include <baselib/messaging/MessagingCommonTypes.h>

#include <baselib/core/FileEncoding.h>
#include <baselib/core/BaseIncludes.h>

UTF_AUTO_TEST_CASE( DataModelTests )
{
    using namespace bl;
    using namespace messaging;

    const auto asyncRpcRequestPayload = dm::DataModelUtils::loadFromFile< AsyncRpcPayload >(
        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
        );

    const auto asyncRpcResponsePayload = dm::DataModelUtils::loadFromFile< AsyncRpcPayload >(
        utest::TestUtils::resolveDataFilePath( "async_rpc_response.json" )
        );

    const auto asyncRpcResponseWithExceptionPayload = dm::DataModelUtils::loadFromFile< AsyncRpcPayload >(
        utest::TestUtils::resolveDataFilePath( "async_rpc_response_with_exception.json" )
        );

    UTF_REQUIRE( asyncRpcRequestPayload );
    UTF_REQUIRE( asyncRpcResponsePayload );
    UTF_REQUIRE( asyncRpcResponseWithExceptionPayload );

    {
        const auto& asyncRpcRequest = asyncRpcRequestPayload -> asyncRpcRequest();
        UTF_REQUIRE( asyncRpcRequest );

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Request payload JSON:\n"
                << dm::DataModelUtils::getDocAsPrettyJsonString( asyncRpcRequestPayload )
            );
    }

    {
        const auto& asyncRpcResponse = asyncRpcResponsePayload -> asyncRpcResponse();
        UTF_REQUIRE( asyncRpcResponse );

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Response payload JSON:\n"
                << dm::DataModelUtils::getDocAsPrettyJsonString( asyncRpcResponsePayload )
            );
    }

    {
        const auto& asyncRpcResponse = asyncRpcResponseWithExceptionPayload -> asyncRpcResponse();
        UTF_REQUIRE( asyncRpcResponse );

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Response with exception payload JSON:\n"
                << dm::DataModelUtils::getDocAsPrettyJsonString( asyncRpcResponseWithExceptionPayload )
            );

        const auto& serverError = asyncRpcResponse -> serverErrorJson();
        UTF_REQUIRE( serverError );

        const auto& result = serverError -> result();
        UTF_REQUIRE( result );

        UTF_REQUIRE_EQUAL( result -> message(), "User friendly message" );
        UTF_REQUIRE_EQUAL( result -> exceptionType(), "bl::ServerErrorException" );
        UTF_REQUIRE_EQUAL( result -> exceptionMessage(), "message: ServerErrorException" );
        UTF_REQUIRE_EQUAL( result -> exceptionFullDump(), "fullDump: ServerErrorException" );

        const auto& properties = result -> exceptionProperties();
        UTF_REQUIRE( properties );

        UTF_REQUIRE_EQUAL( properties -> message(), "message: ServerErrorException" );
        UTF_REQUIRE_EQUAL( properties -> isExpected(), true );
        UTF_REQUIRE_EQUAL( properties -> categoryName(), "generic" );
    }
}

UTF_AUTO_TEST_CASE( MessagingHelpersDataIntegrityTest )
{
    using namespace bl;
    using namespace bl::messaging;

    const auto logFailureInfo = []() -> void
    {
        BL_LOG_MULTILINE(
            Logging::notify(),
            BL_MSG()
                << "Hard-coded hash is different than the calculated one, please read further...\n"
                << "This is to test against modifications to messaging data model definitions.\n"
                << "Since those objects could hold sensitive information there is a need to\n"
                << "block printing of such data (see operator<< definitions in MessagingUtils.h).\n"
                << "To protect against future changes which could exhibit such information we\n"
                << "are going to calculate a hash of an empty object's JSON string and use it\n"
                << "as a mean of comparison. If this test fails it means that object definitions\n"
                << "has changed and the changes should be review with regard to printing them,\n"
                << "then the new hash has to be calculated and the test updated.\n"
                << "If you believe no new sensitive information fields were added then\n"
                << " copy the 'rhs value' from below into the hard-coded test value."
            );
    };

    auto principalIdentityInfo = PrincipalIdentityInfo::createInstance();
    principalIdentityInfo -> authenticationToken( AuthenticationToken::createInstance() );
    principalIdentityInfo -> securityPrincipal( SecurityPrincipal::createInstance() );

    const auto brokerProtocol = BrokerProtocol::createInstance();
    brokerProtocol -> principalIdentityInfo( std::move( principalIdentityInfo ) );

    const auto brokerProtocolHash =
        "62f2ac308b76cf247c9a818d616b9373af3f0ac489e3904c7f20e052121fc32f"
        "1643cb9429c56553f7667524c0dfc88537df491cf6dbe67f1eeb6d8850d01170";

    if( brokerProtocolHash != dm::DataModelUtils::getObjectHashCanonical( brokerProtocol ) )
    {
        logFailureInfo();
    }

    UTF_CHECK_EQUAL( brokerProtocolHash, dm::DataModelUtils::getObjectHashCanonical( brokerProtocol ) );

    auto request = AsyncRpcRequest::createInstance();

    auto response = AsyncRpcResponse::createInstance();

    const auto payload = AsyncRpcPayload::createInstance();
    payload -> asyncRpcRequest( std::move( request ) );
    payload -> asyncRpcResponse( std::move( response ) );

    const auto payloadHash =
        "60423dac57b80158cb6ec370937f72a577a2db59c328b670452cc700e3e4ca11"
        "dba89a853512174f1d0441a004e16a68193e24b379f093a891e8931fe21fcd56";

    if( payloadHash != dm::DataModelUtils::getObjectHashCanonical( payload ) )
    {
        logFailureInfo();
    }

    UTF_CHECK_EQUAL( payloadHash, dm::DataModelUtils::getObjectHashCanonical( payload ) );
}

