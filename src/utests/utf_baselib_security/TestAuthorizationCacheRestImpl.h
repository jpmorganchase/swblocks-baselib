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

#include <baselib/security/AuthorizationServiceRest.h>

#include <utests/baselib/TestAuthorizationCacheImplUtils.h>

#include <baselib/crypto/HashCalculator.h>

#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/HttpServerHelpers.h>
#include <utests/baselib/UtfArgsParser.h>
#include <utests/baselib/Utf.h>

namespace utest
{
    namespace security
    {
        template
        <
            typename E = void
        >
        class LocalBackendStateT : public bl::om::ObjectDefaultBase
        {
            BL_CTR_DEFAULT( LocalBackendStateT, protected )

        public:

            typedef std::tuple
            <
                std::string                     /* httpAction */,
                std::string                     /* urlPath */,
                std::string                     /* response */,
                std::string                     /* responseContentType */
            >
            content_entry_t;

            typedef std::unordered_map< std::string, content_entry_t >          content_data_map_t;

        protected:

            const content_data_map_t                                            m_contentData;
            const bl::om::ObjPtr< bl::str::StringTemplateResolver >             m_responseTemplate;

            LocalBackendStateT( SAA_in content_data_map_t&& contentData ) NOEXCEPT
                :
                m_contentData( BL_PARAM_FWD( contentData ) ),
                m_responseTemplate(
                    bl::str::StringTemplateResolver::createInstance(
                        TestUtils::loadDataFile( "content_template.txt", true /* normalizeLineBreaks */ ),
                        false /* skipUndefined */
                        )
                    )
            {
            }

        public:

            static auto getKey(
                SAA_in              const std::string&                          httpAction,
                SAA_in              const std::string&                          urlPath
                )
                -> std::string
            {
                bl::hash::HashCalculatorDefault hash;

                hash.update( httpAction.c_str(), httpAction.size() );
                hash.update( urlPath.c_str(), urlPath.size() );
                hash.finalize();

                return hash.digestStr();
            }

            static auto getKey( SAA_in const content_entry_t& entry ) -> std::string
            {
                return getKey(
                    std::get< 0 /* httpAction */ >( entry ),
                    std::get< 1 /* urlPath */ >( entry )
                    );
            }

            auto contentData() const NOEXCEPT -> const content_data_map_t&
            {
                return m_contentData;
            }

            auto responseTemplate() const NOEXCEPT -> const bl::om::ObjPtr< bl::str::StringTemplateResolver >&
            {
                return m_responseTemplate;
            }
        };

        typedef bl::om::ObjectImpl< LocalBackendStateT<> > LocalBackendState;

        /**
         * @brief class HttpBackendTask
         */

        template
        <
            typename BACKENDSTATE
        >
        class LocalHttpBackendTask :
            public bl::httpserver::HttpServerProcessingTaskDefault< BACKENDSTATE >
        {
            BL_DECLARE_OBJECT_IMPL( LocalHttpBackendTask )

        protected:

            typedef bl::httpserver::HttpServerProcessingTaskDefault< BACKENDSTATE >     base_type;
            typedef bl::http::Parameters::HttpStatusCode                                HttpStatusCode;
            typedef bl::httpserver::Request                                             Request;

            using base_type::m_backendState;
            using base_type::m_statusCode;
            using base_type::m_request;
            using base_type::m_responseType;
            using base_type::m_response;

            LocalHttpBackendTask(
                SAA_in              bl::om::ObjPtr< Request >&&                         request,
                SAA_in              bl::om::ObjPtr< BACKENDSTATE >&&                    backendState
                )
                :
                base_type( BL_PARAM_FWD( request ), BL_PARAM_FWD( backendState ) )
            {
            }

            virtual void requestProcessing() OVERRIDE
            {
                const auto key = m_backendState -> getKey(
                    m_request -> method()       /* httpAction */,
                    m_request -> uri()          /* urlPath */
                    );

                const auto& dataMap = m_backendState -> contentData();

                const auto pos = dataMap.find( key );

                if( pos == dataMap.end() )
                {
                    BL_THROW_EC(
                        bl::eh::errc::make_error_code( bl::eh::errc::permission_denied ),
                        BL_MSG()
                            << "Content not found!"
                        );
                }

                const auto& entry = pos -> second;

                bl::cpp::SafeOutputStringStream oss;

                oss << std::get< 2 /* response */ >( entry );

                std::unordered_map< std::string, std::string > variables;

                variables.emplace( "tokenProperty1", bl::uuids::uuid2string( bl::uuids::create() ) );
                variables.emplace( "tokenProperty2", bl::uuids::uuid2string( bl::uuids::create() ) );
                variables.emplace( "tokenProperty3", bl::uuids::uuid2string( bl::uuids::create() ) );

                oss << m_backendState -> responseTemplate() -> resolve( variables );

                m_response = oss.str();
                m_responseType = std::get< 3 /* responseContentType */ >( entry );
                m_statusCode = HttpStatusCode::HTTP_SUCCESS_OK;
            };
        };

    } // security

} // utest

UTF_AUTO_TEST_CASE( AuthorizationCacheRestImplBasicTests )
{
    using namespace bl;
    using namespace bl::security;
    using namespace utest;
    using namespace utest::http;
    using namespace utest::security;

    const auto testAuthorizationServiceForResponse =
        [](
            SAA_in          const om::ObjPtr< AuthorizationServiceRest >&           authorizationService,
            SAA_in          const fs::path&                                         fileName
            )
    {
        typedef om::ObjectImpl
        <
            httpserver::ServerBackendProcessingImplDefault
            <
                LocalBackendState,
                LocalHttpBackendTask
            >
        >
        backend_impl_t;

        LocalBackendState::content_data_map_t contentData;

        {
            auto response = TestUtils::loadDataFile( fileName, true /* normalizeLineBreaks */ );

            auto entry = std::make_tuple(
                std::string( "GET"  )                                               /* httpAction */,
                std::string( "/test/authorize?tokenId=tokenId1" )                   /* urlPath */,
                std::move( response ),
                cpp::copy( bl::http::HttpHeader::g_contentTypePlainTextUtf8 )       /* responseContentType */
                );

            auto key = LocalBackendState::getKey( entry );

            contentData.emplace( std::move( key ), std::move( entry ) );
        }

        const auto backendState = LocalBackendState::createInstance( std::move( contentData ) );

        const auto authenticationTokenData =
            "tokenId=tokenId1;"
            "tokenProperty1=tokenPropertyValue1;"
            "tokenProperty2=tokenPropertyValue2;"
            "tokenProperty3=tokenPropertyValue3;";

        TestAuthorizationCacheImplUtils::basicTests< AuthorizationServiceRest /* SERVICE */ >(
            authenticationTokenData                 /* validAuthenticationTokenData1 */,
            "ID_1234"                               /* expectedSecureIdentity1 */,
            backend_impl_t::createInstance< ServerBackendProcessing >(
                om::copy( backendState )
                )                                   /* backend */,
            om::copy( authorizationService )        /* authorizationService */
            );
    };

    const auto testAuthorizationService =
        [ & ]( SAA_in const om::ObjPtr< AuthorizationServiceRest >& authorizationService )
    {
        testAuthorizationServiceForResponse( authorizationService, "response_success.txt" );
    };

    const auto configPath =
        TestUtils::resolveDataFilePath( "authorization_service_rest_config.json" );

    testAuthorizationService( AuthorizationServiceRest::create( cpp::copy( configPath ) ) );

    /*
     * Now let's test the cases where 1) we have both contentTemplateFilePath
     * and contentTemplateBase64 or 2) only contentTemplateBase64
     */

   typedef dm::config::AuthorizationServiceRestConfig rest_config_t;

   const auto templateText =
        encoding::readTextFile( TestUtils::resolveDataFilePath( "content_template.txt" ) );

   {
        auto config = dm::DataModelUtils::loadFromFile< rest_config_t >( configPath );

        {
            config -> contentTemplateBase64( SerializationUtils::base64EncodeString( templateText ) );

            fs::TmpDir tempPath;

            auto newConfigPath = tempPath.path() / "config.json";

            encoding::writeTextFile(
                newConfigPath                                                       /* filePath */,
                dm::DataModelUtils::getJsonString( config, true /* prettyPrint */ ) /* content */,
                encoding::TextFileEncoding::Utf8_NoPreamble
                );

            encoding::writeTextFile(
                tempPath.path() / "content_template.txt"                            /* filePath */,
                templateText                                                        /* content */,
                encoding::TextFileEncoding::Utf8_NoPreamble
                );

            testAuthorizationService( AuthorizationServiceRest::create( cpp::copy( newConfigPath ) ) );

            config -> contentTemplateBase64( SerializationUtils::base64EncodeString( "invalid" ) );

            encoding::writeTextFile(
                newConfigPath                                                       /* filePath */,
                dm::DataModelUtils::getJsonString( config, true /* prettyPrint */ ) /* content */,
                encoding::TextFileEncoding::Utf8_NoPreamble
                );

            UTF_REQUIRE_THROW_MESSAGE(
                AuthorizationServiceRest::create( cpp::copy( newConfigPath ) ),
                InvalidDataFormatException,
                resolveMessage(
                    BL_MSG()
                        << "The content referenced by contentTemplateFilePath doesn't match the "
                        << "embedded content in contentTemplateBase64 property"
                    )
                );

            config -> contentTemplateBase64( SerializationUtils::base64EncodeString( templateText ) );

            /*
             * Cast to dm::Payload, so when the object is serialized all unknown properties are preserved
             */

            const auto payload = dm::DataModelUtils::castTo< dm::Payload >( config );

            payload -> unmappedLvalue().emplace( "unknownProperty", "unknownPropertyValue" );

            encoding::writeTextFile(
                newConfigPath                                                           /* filePath */,
                dm::DataModelUtils::getJsonString( payload, true /* prettyPrint */ )    /* content */,
                encoding::TextFileEncoding::Utf8_NoPreamble
                );

            UTF_REQUIRE_THROW_MESSAGE(
                AuthorizationServiceRest::create( cpp::copy( newConfigPath ) ),
                InvalidDataFormatException,
                resolveMessage(
                    BL_MSG()
                        << "Unexpected properties encountered in authorization service config file: "
                        << "'unknownProperty'"
                    )
                );
        }

        {
            config -> contentTemplateBase64( SerializationUtils::base64EncodeString( templateText ) );
            config -> contentTemplateFilePathLvalue().clear();

            fs::TmpDir tempPath;

            auto newConfigPath = tempPath.path() / "config.json";

            encoding::writeTextFile(
                newConfigPath                                                       /* filePath */,
                dm::DataModelUtils::getJsonString( config, true /* prettyPrint */ ) /* content */,
                encoding::TextFileEncoding::Utf8_NoPreamble
                );

            testAuthorizationService( AuthorizationServiceRest::create( std::move( newConfigPath ) ) );
        }

        testAuthorizationService( AuthorizationServiceRest::create( om::copy( config ) ) );

        testAuthorizationServiceForResponse(
            AuthorizationServiceRest::create( om::copy( config ) ),
            "response_success_other_status.txt"
            );

        /*
         * Test other config validation cases - e.g. invalid HTTP action
         */

        config -> readOnly( false );
        config -> httpAction( "invalid" );

        UTF_REQUIRE_THROW_MESSAGE(
            AuthorizationServiceRest::create( om::copy( config ) ),
            InvalidDataFormatException,
            resolveMessage(
                BL_MSG()
                    << "The 'httpAction' property value '"
                    << config -> httpAction()
                    << "' is invalid; it can only be one of the following: {'GET', 'PUT', 'POST'}"
                )
            );

        config -> readOnly( false );
        config -> httpAction( "GET" );
        config -> isTokenBinary( true );
        config -> isTokenMultiProperties( true );

        UTF_REQUIRE_THROW_MESSAGE(
            AuthorizationServiceRest::create( om::copy( config ) ),
            InvalidDataFormatException,
            "The config properties 'isTokenBinary' and 'isTokenMultiProperties' cannot be both 'true'"
            );

        /*
         * Now let's test some expected authorization failures -
         * they should throw SecurityException
         */

        config -> readOnly( false );
        config -> isTokenBinary( false );

        UTF_REQUIRE_THROW_MESSAGE(
            testAuthorizationServiceForResponse(
                AuthorizationServiceRest::create( om::copy( config ) ),
                "response_failed_no_status.txt"
                ),
            SecurityException,
            resolveMessage(
                BL_MSG()
                    << "A required property with name 'Status' "
                    << "could not be parsed from an authorization response"
                )
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testAuthorizationServiceForResponse(
                AuthorizationServiceRest::create( om::copy( config ) ),
                "response_failed_no_sid.txt"
                ),
            SecurityException,
            resolveMessage(
                BL_MSG()
                    << "A required property with name 'Sid' "
                    << "could not be parsed from an authorization response"
                )
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testAuthorizationServiceForResponse(
                AuthorizationServiceRest::create( om::copy( config ) ),
                "response_failed_normal.txt"
                ),
            SecurityException,
            resolveMessage(
                BL_MSG()
                    << "Authorization service failed with the following error details: "
                    << "[status='-3'; statusCategory='-6'; message='Token cannot be validated']"
                )
            );
   }
}

UTF_AUTO_TEST_CASE( AuthorizationCacheRestImplFullTests )
{
    using namespace bl::security;
    using namespace utest;
    using namespace utest::http;
    using namespace utest::security;

    typedef bl::om::ObjectImpl
    <
        bl::httpserver::ServerBackendProcessingImplDefault
        <
            LocalBackendState,
            LocalHttpBackendTask
        >
    >
    backend_impl_t;

    LocalBackendState::content_data_map_t contentData;

    {
        auto entry = std::make_tuple(
            std::string( "GET"  )                                                               /* httpAction */,
            std::string( "/test/authorize?tokenId=tokenId1" )                                   /* urlPath */,
            TestUtils::loadDataFile( "response_success.txt", true /* normalizeLineBreaks */ )   /* response */,
            bl::cpp::copy( http::HttpHeader::g_contentTypePlainTextUtf8 )                       /* responseContentType */
            );

        auto key = LocalBackendState::getKey( entry );

        contentData.emplace( std::move( key ), std::move( entry ) );
    }

    {
        auto entry = std::make_tuple(
            std::string( "GET"  )                                                               /* httpAction */,
            std::string( "/test/authorize?tokenId=tokenId2" )                                   /* urlPath */,
            TestUtils::loadDataFile( "response_success2.txt", true /* normalizeLineBreaks */ )  /* response */,
            bl::cpp::copy( http::HttpHeader::g_contentTypePlainTextUtf8 )                       /* responseContentType */
            );

        auto key = LocalBackendState::getKey( entry );

        contentData.emplace( std::move( key ), std::move( entry ) );
    }

    const auto backendState = LocalBackendState::createInstance( std::move( contentData ) );

    const auto authenticationTokenData1 =
        "tokenId=tokenId1;"
        "tokenProperty1=tokenPropertyValue1;"
        "tokenProperty2=tokenPropertyValue2;"
        "tokenProperty3=tokenPropertyValue3;";

    const auto authenticationTokenData2 =
        "tokenId=tokenId2;"
        "tokenProperty1=tokenPropertyValue4;"
        "tokenProperty2=tokenPropertyValue5;"
        "tokenProperty3=tokenPropertyValue6;";

    const auto invalidAuthenticationTokenData =
        "tokenId=invalidTokenId;"
        "tokenProperty1=invalidTokenPropertyValue;"
        "tokenProperty2=invalidTokenPropertyValue;"
        "tokenProperty3=invalidTokenPropertyValue;";

    TestAuthorizationCacheImplUtils::fullTests< AuthorizationServiceRest /* SERVICE */ >(
        authenticationTokenData1                /* validAuthenticationTokenData1 */,
        authenticationTokenData2                /* validAuthenticationTokenData2 */,
        invalidAuthenticationTokenData          /* invalidAuthenticationTokenData */,
        "ID_1234"                               /* expectedSecureIdentity1 */,
        "ID_5678"                               /* expectedSecureIdentity2 */,
        backend_impl_t::createInstance< ServerBackendProcessing >(
            bl::om::copy( backendState )
            )                                   /* backend */,
        AuthorizationServiceRest::create(
            TestUtils::resolveDataFilePath( "authorization_service_rest_config.json" )
            )                                   /* authorizationService */
        );
}

UTF_AUTO_TEST_CASE( AuthorizationCacheRestImplManualInvoke )
{
    using namespace utest;
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::security;

    /*
     * This is a manual invoke test against some real service
     */

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    if( test::UtfArgsParser::path().empty() ||  test::UtfArgsParser::password().empty() )
    {
        UTF_FAIL( "The --path and --password are required parameters for this test" );
    }

    typedef om::ObjectImpl< AuthorizationCacheImpl< AuthorizationServiceRest > > cache_t;

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\n***** Starting test: AuthorizationCacheRestImplManualInvoke *****\n"
            );

    const auto cache = cache_t::template createInstance< AuthorizationCache >(
        AuthorizationServiceRest::create( test::UtfArgsParser::path() )
        );

    const auto authenticationToken =
        AuthorizationCache::createAuthenticationToken( test::UtfArgsParser::password() );

    const auto principal = cache -> update( authenticationToken );
    UTF_REQUIRE( principal );

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\nSID: "
            << principal -> secureIdentity()
            << "\nFirst: "
            << principal -> givenName()
            << "\nLast: "
            << principal -> familyName()
            << "\nEmail: "
            << principal -> email()
            << "\nTypeId: "
            << principal -> typeId()
        );

    const auto& updatedAuthenticationToken = principal -> authenticationToken();

    const std::string tokenTextData(
        updatedAuthenticationToken -> begin(),
        updatedAuthenticationToken -> end()
        );

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\n\nUpdated token: "
            << tokenTextData
        );
}

