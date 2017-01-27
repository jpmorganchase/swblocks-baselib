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

#ifndef __BL_SECURITY_AUTHORIZATIONSERVICEREST_H_
#define __BL_SECURITY_AUTHORIZATIONSERVICEREST_H_

#include <baselib/http/SimpleHttpTask.h>
#include <baselib/http/SimpleHttpSslTask.h>
#include <baselib/http/Globals.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/Algorithms.h>

#include <baselib/security/SecurityInterfaces.h>

#include <baselib/data/DataBlock.h>

#include <baselib/data/models/ServicesConfig.h>

#include <baselib/core/SerializationUtils.h>
#include <baselib/core/StringTemplateResolver.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/OS.h>
#include <baselib/core/EnumUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace security
    {
        /**
         * @brief class AuthorizationServiceRest - a REST API based config driven authorization
         * service implementation - see the expected interface in the description of
         * AuthorizationCacheImpl
         *
         * class AUTHORIZATION_SERVICE
         * {
         * protected:
         *
         *     typedef AUTHORIZATION_TASK_IMPL task_impl_t;
         *
         *     static auto getTokenType() NOEXCEPT -> const std::string&;
         *
         *     [static] auto createAuthorizationTask( SAA_in const om::ObjPtr< data::DataBlock >& authenticationToken )
         *         -> om::ObjPtr< tasks::Task >;
         *
         *     [static] auto extractSecurityPrincipal( SAA_in const om::ObjPtr< tasks::Task >& executedAuthorizationTask )
         *         -> om::ObjPtr< SecurityPrincipal >;
         * };
         */

        template
        <
            typename E = void
        >
        class AuthorizationServiceRestT;

        typedef om::ObjectImpl< AuthorizationServiceRestT<> > AuthorizationServiceRest;

        template
        <
            typename E
        >
        class AuthorizationServiceRestT : public om::ObjectDefaultBase
        {
        public:

            typedef tasks::SimpleHttpSslTaskImpl                                    task_impl_t;

        protected:

            BL_DEFINE_ENUM_WITH_STRING_CONVERSIONS( PropertyId, "PropertyId",
                ( Sid )
                ( GivenName )
                ( FamilyName )
                ( Email )
                ( TypeId )
                ( UpdatedTokenProperty )
                ( UpdatedTextToken )
                ( UpdatedBinaryToken )
                ( Status )
                ( StatusCategory )
                ( StatusMessage )
                )

            typedef std::tuple
            <
                str::regex                  /* regex */,
                bool                        /* isRequired */,
                bool                        /* isFound */,
                std::string                 /* value */
            >
            property_info_t;

            typedef typename PropertyId::Enum                                       property_enum_t;

            typedef std::unordered_map
            <
                property_enum_t,
                property_info_t,
                cpp::EnumHasher
            >
            properties_map_t;

            typedef dm::config::AuthorizationServiceRestConfig                      rest_config_t;

            const om::ObjPtr< str::StringTemplateResolver >                         m_requestTemplate;
            const om::ObjPtr< str::StringTemplateResolver >                         m_urlPathTemplate;
            const om::ObjPtr< rest_config_t >                                       m_config;
            const fs::path                                                          m_configPath;
            const properties_map_t                                                  m_uniqueProperties;
            const str::regex                                                        m_regexUpdatedTokenProperty;

            AuthorizationServiceRestT(
                SAA_in          om::ObjPtr< rest_config_t >&&                       config,
                SAA_in_opt      std::string&&                                       contentTemplate,
                SAA_in_opt      fs::path&&                                          configPath
                )
                :
                m_requestTemplate(
                    str::StringTemplateResolver::createInstance(
                        BL_PARAM_FWD( contentTemplate ),
                        true /* skipUndefined */
                        )
                    ),
                m_urlPathTemplate(
                    str::StringTemplateResolver::createInstance(
                        cpp::copy( config -> urlPathTemplate() ),
                        true /* skipUndefined */
                        )
                    ),
                m_config( BL_PARAM_FWD( config ) ),
                m_configPath( BL_PARAM_FWD( configPath ) ),
                m_uniqueProperties( getUniqueProperties( m_config ) ),
                m_regexUpdatedTokenProperty( m_config -> regexUpdatedTokenProperty() )
            {
                BL_CHK_T(
                    false,
                    m_config -> httpAction() == "GET" ||
                    m_config -> httpAction() == "PUT" ||
                    m_config -> httpAction() == "POST",
                    InvalidDataFormatException()
                        << eh::errinfo_is_user_friendly( true ),
                    BL_MSG()
                        << "The 'httpAction' property value '"
                        << m_config -> httpAction()
                        << "' is invalid; it can only be one of the following: {'GET', 'PUT', 'POST'}"
                    );

                BL_CHK_T(
                    true,
                    m_config -> isTokenBinary() && m_config -> isTokenMultiProperties(),
                    InvalidDataFormatException()
                        << eh::errinfo_is_user_friendly( true ),
                    BL_MSG()
                        << "The config properties 'isTokenBinary' and 'isTokenMultiProperties' cannot be both 'true'"
                    );
            }

            static auto getUniqueProperties( SAA_in const om::ObjPtr< rest_config_t >& config )
                -> properties_map_t
            {
                properties_map_t properties;

                const auto createEntry = [ & ](
                    SAA_in              const property_enum_t   propertyId,
                    SAA_in              const std::string&      regexPattern,
                    SAA_in              const bool              isRequired
                    )
                    -> void
                {
                    properties.emplace(
                        propertyId,
                        std::make_tuple(
                            str::regex( regexPattern )              /* regex */,
                            isRequired,
                            false                                   /* isFound */,
                            std::string()                           /* value */
                            )
                        );
                };

                const auto createDefaultEntry = [ & ](
                    SAA_in              const property_enum_t   propertyId,
                    SAA_in              const std::string&      regexPattern
                    )
                    -> void
                {
                    createEntry( propertyId, regexPattern, false /* isRequired */ );
                };

                createEntry( PropertyId::Sid, config -> regexSid(), true /* isRequired */ );
                createDefaultEntry( PropertyId::GivenName, config -> regexGivenName() );
                createDefaultEntry( PropertyId::FamilyName, config -> regexFamilyName() );
                createDefaultEntry( PropertyId::Email, config -> regexEmail() );
                createDefaultEntry( PropertyId::TypeId, config -> regexTypeId() );

                createDefaultEntry( PropertyId::UpdatedTextToken, config -> regexUpdatedTextToken() );
                createDefaultEntry( PropertyId::UpdatedBinaryToken, config -> regexUpdatedBinaryToken() );

                createEntry( PropertyId::Status, config -> regexStatus(), true /* isRequired */ );
                createDefaultEntry( PropertyId::StatusCategory, config -> regexStatusCategory() );
                createDefaultEntry( PropertyId::StatusMessage, config -> regexStatusMessage() );

                return properties;
            }

            static void chkProperty(
                SAA_in          const bool                                          condition,
                SAA_in          property_enum_t                                     propertyId,
                SAA_in_opt      const bool                                          isRequired = false
                )
            {
                BL_CHK_T(
                    false,
                    condition,
                    SecurityException()
                        << eh::errinfo_is_user_friendly( true ),
                    BL_MSG()
                        << "A "
                        << ( isRequired ? "required " : "" )
                        << "property with name '"
                        << PropertyId::toString( propertyId )
                        << "' could not be parsed from an authorization response"
                    );
            }

        public:

            static auto create( SAA_in om::ObjPtr< rest_config_t >&& config )
                -> om::ObjPtr< AuthorizationServiceRest >
            {
                config -> readOnly( true );

                std::string contentTemplate;

                if( ! config -> contentTemplateBase64().empty() )
                {
                    contentTemplate = SerializationUtils::base64DecodeString(
                        config -> contentTemplateBase64()
                        );
                }

                return AuthorizationServiceRest::createInstance(
                    BL_PARAM_FWD( config ),
                    std::move( contentTemplate ),
                    fs::path() /* configPath */
                    );
            }

            static auto create( SAA_in fs::path&& configPath ) -> om::ObjPtr< AuthorizationServiceRest >
            {
                auto config = dm::DataModelUtils::loadFromFile< rest_config_t >( configPath );

                BL_CHK_T(
                    false,
                    0U == config -> unmapped().size(),
                    InvalidDataFormatException()
                        << eh::errinfo_is_user_friendly( true ),
                    BL_MSG()
                        << "Unexpected properties encountered in authorization service config file: "
                        << str::joinQuoteFormattedKeys( config -> unmapped() )
                    );

                config -> readOnly( true );

                std::string contentTemplate;

                const fs::path filePath = config -> contentTemplateFilePath();

                if( ! filePath.empty() )
                {
                    const auto& templatePath =
                        filePath.is_absolute() ? filePath : fs::path( configPath.parent_path() / filePath );

                    contentTemplate = encoding::readTextFile( templatePath );
                }

                if( ! config -> contentTemplateBase64().empty() )
                {
                    const auto contentTemplateDecoded = SerializationUtils::base64DecodeString(
                        config -> contentTemplateBase64()
                        );

                    BL_CHK_T(
                        true,
                        ! contentTemplate.empty() && contentTemplate != contentTemplateDecoded,
                        InvalidDataFormatException()
                            << eh::errinfo_is_user_friendly( true ),
                        BL_MSG()
                            << "The content referenced by contentTemplateFilePath doesn't match the "
                            << "embedded content in contentTemplateBase64 property"
                        );
                }

                return AuthorizationServiceRest::createInstance(
                    std::move( config ),
                    std::move( contentTemplate ),
                    BL_PARAM_FWD( configPath )
                    );
            }

            auto getTokenType() const NOEXCEPT -> const std::string&
            {
                return m_config -> tokenType();
            }

            auto createAuthorizationTask( SAA_in const om::ObjPtr< data::DataBlock >& authenticationToken ) const
                -> om::ObjPtr< tasks::Task >
            {
                std::unordered_map< std::string, std::string > variables;

                if( m_config -> isTokenBinary() )
                {
                    variables.emplace(
                        "$binaryToken",
                        SerializationUtils::base64Encode(
                            authenticationToken -> pv(),
                            authenticationToken -> size()
                            )
                        );
                }
                else
                {
                    std::string textToken( authenticationToken -> begin(), authenticationToken -> end() );

                    if( m_config -> isTokenMultiProperties() )
                    {
                        variables = str::parsePropertiesList( textToken );
                    }

                    variables.emplace( "$textToken", std::move( textToken ) );
                }

                typename task_impl_t::HeadersMap headers;

                headers[ task_impl_t::HttpHeader::g_contentType ] = m_config -> contentType();

                auto taskImpl = task_impl_t::createInstance(
                    cpp::copy( m_config -> host() ),
                    cpp::copy( m_config -> port() ),
                    m_urlPathTemplate -> resolve( variables )       /* urlPath */,
                    m_config -> httpAction(),
                    m_requestTemplate -> resolve( variables )       /* content */,
                    std::move( headers )
                    );

                /*
                 * Ensure that sensitive secure information is redacted in exceptions
                 */

                taskImpl -> isSecureMode( true );

                return om::moveAs< tasks::Task >( taskImpl );
            }

            auto extractSecurityPrincipal( SAA_in const om::ObjPtr< tasks::Task >& executedAuthorizationTask ) const
                -> om::ObjPtr< SecurityPrincipal >
            {
                const auto taskImpl = om::qi< task_impl_t >( executedAuthorizationTask );

                auto content = taskImpl -> getResponse();

                /*
                 * We have to remove the '\r' from the buffer in order to make sure loadFromString will
                 * parse successfully Janus response files which were written by older versions of our
                 * code. In the older versions the stream was opened in text mode so the '\r's were
                 * removed automatically
                 */

                content.erase( std::remove( content.begin(), content.end(), '\r' ), content.end() );

                auto uniqueProperties = m_uniqueProperties;
                std::unordered_map< std::string, std::string > tokenProperties;

                std::string line;
                cpp::SafeInputStringStream is( content );

                while( is.good() )
                {
                    std::getline( is, line );
                    str::trim( line );

                    if( line.empty() )
                    {
                        continue;
                    }

                    for( auto& pair : uniqueProperties )
                    {
                        auto& propertyData = pair.second;
                        auto& isFound = std::get< 2 >( propertyData );

                        if( isFound )
                        {
                            continue;
                        }

                        const auto& regex = std::get< 0 >( propertyData );

                        str::smatch results;
                        if( str::regex_match( line, results, regex ) )
                        {
                            BL_CHK_T(
                                false,
                                2U == results.size(),
                                SecurityException()
                                    << eh::errinfo_is_user_friendly( true ),
                                BL_MSG()
                                    << "Regular expression pattern in the authorization config "
                                    << "is expected to have one group only"
                                );

                            auto& value = std::get< 3 >( propertyData );

                            value = results[ 1 ];
                            isFound = true;

                            /*
                             * We are done matching this line
                             */

                            line.clear();
                            break;
                        }
                    }

                    if( line.empty() )
                    {
                        continue;
                    }

                    if( ! m_config -> isTokenBinary() && m_config -> isTokenMultiProperties() )
                    {
                        /*
                         * This line was not matched by some of the unique properties
                         *
                         * Let's try to match the updatedTokenProperties
                         */

                        str::smatch results;

                        if( str::regex_match( line, results, m_regexUpdatedTokenProperty ) )
                        {
                            BL_CHK_T(
                                false,
                                3U == results.size(),
                                SecurityException()
                                    << eh::errinfo_is_user_friendly( true ),
                                BL_MSG()
                                    << "Regular expression pattern for token property in authorization "
                                    << "config is expected to have exactly two groups"
                                );

                            std::string propertyName = results[ 1 ];
                            std::string propertyValue = results[ 2 ];

                            BL_CHK_T(
                                false,
                                0U != propertyName.size(),
                                SecurityException()
                                    << eh::errinfo_is_user_friendly( true ),
                                BL_MSG()
                                    << "Token property name parsed from authorization response can't be empty"
                                );

                            BL_CHK_T(
                                false,
                                0U != propertyValue.size(),
                                SecurityException()
                                    << eh::errinfo_is_user_friendly( true ),
                                BL_MSG()
                                    << "Token property value parsed from authorization response can't be empty"
                                );

                            tokenProperties.emplace( std::move( propertyName ), std::move( propertyValue ) );
                        }
                    }
                }

                /*
                 * The code logic below is to extract and verify the status code and throw
                 * an an
                 *
                 * First check if the Status property was parsed successfully, so we can
                 * check the status before we attempt to parse or validate other properties
                 */

                const auto& statusData = uniqueProperties.at( PropertyId::Status );
                const auto& statusIsFound = std::get< 2 >( statusData );

                chkProperty(
                    statusIsFound                       /* condition */,
                    PropertyId::Status                  /* propertyId */,
                    true                                /* isRequired */
                    );

                int status;
                int statusCategory;

                chkProperty(
                    utils::try_lexical_convert< int >(
                        std::get< 3 /* value */ >( statusData ),
                        status
                        )                               /* condition */,
                    PropertyId::Status
                    );

                const auto& statusCategoryData = uniqueProperties.at( PropertyId::StatusCategory );
                const auto& statusCategoryIsFound = std::get< 2 >( statusCategoryData );

                chkProperty(
                    ! m_config -> successStatusCategoryIsSet() || statusCategoryIsFound,
                    PropertyId::StatusCategory
                    );

                if( statusCategoryIsFound )
                {
                    chkProperty(
                        utils::try_lexical_convert< int >(
                            std::get< 3 /* value */ >( statusCategoryData ),
                            statusCategory
                            )                           /* condition */,
                        PropertyId::StatusCategory
                        );
                }

                bool statusOk = m_config -> successStatus() == status;

                if( ! statusOk && m_config -> otherSuccessStatuses().size() )
                {
                    /*
                     * Check the other success statuses if any
                     */

                    for( const auto& otherSuccessStatus : m_config -> otherSuccessStatuses() )
                    {
                        if( otherSuccessStatus == status )
                        {
                            statusOk = true;
                            break;
                        }
                    }
                }

                if( statusOk && m_config -> successStatusCategoryIsSet() && statusCategoryIsFound )
                {
                    /*
                     * If the status is success and status category is found make sure it
                     * matches the required success status category
                     */

                    statusOk = m_config -> successStatusCategory() == statusCategory;
                }

                if( ! statusOk )
                {
                    /*
                     * The request has failed - build an exception with all the error details
                     * and throw it
                     */

                    SecurityException exception;
                    MessageBuffer messageBuffer;

                    exception << eh::errinfo_service_status( status );

                    messageBuffer
                        << "Authorization service failed with the following error details: "
                        << "[status='"
                        << status;

                    if( statusCategoryIsFound )
                    {
                        exception << eh::errinfo_service_status_category( statusCategory );

                        messageBuffer
                            << "'; statusCategory='"
                            << statusCategory;
                    }

                    const auto& statusMessageData = uniqueProperties.at( PropertyId::StatusMessage );

                    if( std::get< 2 /* isFound */ >( statusMessageData ) )
                    {
                        const auto& statusMessage = std::get< 3 /* value */ >( statusMessageData );

                        exception << eh::errinfo_service_status_message( statusMessage );

                        messageBuffer
                            << "'; message='"
                            << statusMessage;
                    }

                    messageBuffer
                        << "']";

                    BL_THROW( exception, messageBuffer );
                }

                /*
                 * Make sure that all other properties that were required were parsed out
                 * successfully (e.g. SID)
                 */

                for( const auto& pair : uniqueProperties )
                {
                    const auto& propertyData = pair.second;

                    const auto& isRequired = std::get< 1 >( propertyData );
                    const auto& isFound = std::get< 2 >( propertyData );

                    chkProperty(
                        ! isRequired || isFound         /* condition */,
                        pair.first                      /* propertyId */,
                        true                            /* isRequired */
                        );
                }

                /*
                 * If we are here that means the request was successful and all required
                 * properties were extracted successfully
                 *
                 * At this point just check to extract the updated authentication token if such
                 * is available
                 */

                om::ObjPtr< data::DataBlock > updatedAuthenticationToken;

                if( m_config -> isTokenBinary() )
                {
                    const auto& updatedBinaryTokenData =
                        uniqueProperties.at( PropertyId::UpdatedBinaryToken );

                    if( std::get< 2 /* isFound */ >( updatedBinaryTokenData ) )
                    {
                        const auto tokenBinaryData =
                            SerializationUtils::base64DecodeVector(
                                std::get< 3 /* value */ >( updatedBinaryTokenData )
                                );

                        chkProperty(
                            tokenBinaryData.size() > 0U,
                            PropertyId::UpdatedBinaryToken
                            );

                        updatedAuthenticationToken =
                            data::DataBlock::get(
                                nullptr                                 /* dataBlocksPool */,
                                tokenBinaryData.size()                  /* capacity */
                                );

                        std::memcpy(
                            updatedAuthenticationToken -> pv(),
                            &tokenBinaryData[ 0 ],
                            tokenBinaryData.size()
                            );

                        updatedAuthenticationToken -> setSize( tokenBinaryData.size() );
                    }
                }
                else
                {
                    if( m_config -> isTokenMultiProperties() )
                    {
                        if( tokenProperties.size() )
                        {
                            cpp::SafeOutputStringStream oss;

                            bool isNotFirst = false;

                            for( const auto& pair : tokenProperties )
                            {
                                if( isNotFirst )
                                {
                                    oss << ';';
                                }

                                oss
                                    << pair.first       /* name */
                                    << '='
                                    << pair.second      /* value */
                                    ;

                                isNotFirst = true;
                            }

                            updatedAuthenticationToken =
                                AuthorizationCache::createAuthenticationToken( oss.str() );
                        }
                    }
                    else
                    {
                        const auto& updatedTextTokenData =
                            uniqueProperties.at( PropertyId::UpdatedTextToken );

                        if( std::get< 2 /* isFound */ >( updatedTextTokenData ) )
                        {
                            const auto& updatedTextToken =
                                std::get< 3 /* value */ >( updatedTextTokenData );

                            chkProperty(
                                updatedTextToken.size() > 0U,
                                PropertyId::UpdatedTextToken
                                );

                            updatedAuthenticationToken =
                                AuthorizationCache::createAuthenticationToken( updatedTextToken );
                        }
                    }
                }

                return SecurityPrincipal::createInstance(
                    std::move( std::get< 3 /* value */ >( uniqueProperties.at( PropertyId::Sid ) ) ),
                    std::move( std::get< 3 /* value */ >( uniqueProperties.at( PropertyId::GivenName ) ) ),
                    std::move( std::get< 3 /* value */ >( uniqueProperties.at( PropertyId::FamilyName ) ) ),
                    std::move( std::get< 3 /* value */ >( uniqueProperties.at( PropertyId::Email ) ) ),
                    std::move( std::get< 3 /* value */ >( uniqueProperties.at( PropertyId::TypeId ) ) ),
                    std::move( updatedAuthenticationToken )
                    );
            }
        };

    } // security

} // bl

#endif /* __BL_SECURITY_AUTHORIZATIONSERVICEREST_H_ */
