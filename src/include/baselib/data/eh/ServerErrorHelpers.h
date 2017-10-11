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

#ifndef __BL_DATA_EH_SERVERERRORHELPERS_H_
#define __BL_DATA_EH_SERVERERRORHELPERS_H_

#include <baselib/data/DataModelObject.h>
#include <baselib/data/DataModelObjectDefs.h>
#include <baselib/data/models/ErrorHandling.h>

#include <baselib/core/CPP.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace dm
    {
        /**
         * @brief Helper code to handle JSON serialization of exceptions to return server errors
         */

        template
        <
            typename E = void
        >
        class ServerErrorHelpersT
        {
            BL_DECLARE_STATIC( ServerErrorHelpersT )

        protected:

            static auto defaultEhCallback() -> eh::void_exception_callback_t
            {
                return eh::void_exception_callback_t();
            }

        public:

            static auto createServerErrorResultObject(
                SAA_in      const std::exception_ptr&                   eptr,
                SAA_in_opt  const eh::void_exception_callback_t&        exceptionCallback = defaultEhCallback()
                )
                -> om::ObjPtr< ServerErrorResult >
            {
                auto errorResult = ServerErrorResult::createInstance();

                errorResult -> exceptionProperties( ExceptionProperties::createInstance() );

                const auto populateExceptionResult = [ & ]( SAA_in const std::exception& e ) -> void
                {
                    errorResult -> exceptionMessage( e.what() );
                    errorResult -> exceptionFullDump( eh::diagnostic_information( e ) );

                    const auto errorCode = eh::get_error_info< eh::errinfo_error_code >( e );

                    if( errorCode != nullptr )
                    {
                        errorResult -> exceptionProperties() -> systemCode( errorCode -> value() );
                    }

#define BL_DM_SET_SERVER_EXCEPTION_PROPERTY( property, errinfo, selector )                         \
                    {                                                                               \
                        const auto* info = eh::get_error_info< eh::errinfo >( e );          \
                        if( info != nullptr )                                                       \
                        {                                                                           \
                            errorResult -> exceptionProperties() -> property( ( *info ) selector ); \
                        }                                                                           \
                    }                                                                               \

                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( errNo, errinfo_errno,                                         );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( fileName, errinfo_file_name,                                  );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( fileOpenMode, errinfo_file_open_mode,                         );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( message, errinfo_message,                                     );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( timeThrown, errinfo_time_thrown,                              );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( functionName, errinfo_function_name,                          );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( systemCode, errinfo_system_code,                              );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( categoryName, errinfo_category_name,                          );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( errorCode, errinfo_error_code,                                .value() );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( errorCodeMessage, errinfo_error_code_message,                 );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( isExpected, errinfo_is_expected,                              );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( taskInfo, errinfo_task_info,                                  );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( hostName, errinfo_host_name,                                  );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( serviceName, errinfo_service_name,                            );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( endpointAddress, errinfo_endpoint_address,                    );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( endpointPort, errinfo_endpoint_port,                          );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( httpUrl, errinfo_http_url,                                    );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( httpRedirectUrl, errinfo_http_redirect_url,                   );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( httpStatusCode, errinfo_http_status_code,                     );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( httpResponseHeaders, errinfo_http_response_headers,           );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( httpRequestDetails, errinfo_http_request_details,             );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( parserFile, errinfo_parser_file,                              );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( parserLine, errinfo_parser_line,                              );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( parserColumn, errinfo_parser_column,                          );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( parserReason, errinfo_parser_reason,                          );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( externalCommandOutput, errinfo_external_command_output,       );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( externalCommandExitCode, errinfo_external_command_exit_code,  );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( stringValue, errinfo_string_value,                            );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( isUserFriendly, errinfo_is_user_friendly,                     );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( sslIsVerifyFailed, errinfo_ssl_is_verify_failed,              );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( sslIsVerifyError, errinfo_ssl_is_verify_error,                );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( sslIsVerifyErrorMessage, errinfo_ssl_is_verify_error_message, );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( sslIsVerifyErrorString, errinfo_ssl_is_verify_error_string,   );
                    BL_DM_SET_SERVER_EXCEPTION_PROPERTY( sslIsVerifySubjectName, errinfo_ssl_is_verify_subject_name,   );

#undef BL_DM_SET_SERVER_EXCEPTION_PROPERTY
                };

                try
                {
                    try
                    {
                        cpp::safeRethrowException( eptr );
                    }
                    catch( BaseException& e )
                    {
                        errorResult -> exceptionType( e.fullTypeName() );

                        throw;
                    }
                }
                catch( std::exception& e )
                {
                    errorResult -> message( eh::isUserFriendly( e ) ? e.what() : BL_GENERIC_FRIENDLY_UNEXPECTED_MSG );

                    if( errorResult -> exceptionType().empty() )
                    {
                        errorResult -> exceptionType( "std::exception" );
                    }

                    populateExceptionResult( e );

                    if( exceptionCallback )
                    {
                        exceptionCallback( e );
                    }
                }

                return errorResult;
            }

            static auto createServerErrorObject(
                SAA_in      const std::exception_ptr&                   eptr,
                SAA_in_opt  const eh::void_exception_callback_t&        exceptionCallback = defaultEhCallback()
                )
                -> om::ObjPtr< ServerErrorJson >
            {
                auto errorJson = ServerErrorJson::createInstance();

                errorJson -> result( createServerErrorResultObject( eptr, exceptionCallback ) );

                return errorJson;
            }

            static auto createServerErrorGraphQLObject(
                SAA_in      const std::exception_ptr&                   eptr,
                SAA_in_opt  const eh::void_exception_callback_t&        exceptionCallback = defaultEhCallback()
                )
                -> om::ObjPtr< ServerErrorGraphQL >
            {
                auto errorGraphQL = ServerErrorGraphQL::createInstance();

                errorGraphQL -> errorsLvalue().push_back( createServerErrorResultObject( eptr, exceptionCallback ) );

                return errorGraphQL;
            }

            template
            <
                typename EXCEPTIONPROPERTIES,
                typename EXCEPTION
            >
            static auto exceptionFromProperties(
                SAA_in      const om::ObjPtr< EXCEPTIONPROPERTIES >&    exceptionProperties,
                SAA_in      const eh::error_category*                   errorCategory,
                SAA_in      const EXCEPTION&                            exception
                )
                -> std::exception_ptr
            {
                if( errorCategory && exceptionProperties -> errorCodeIsSet() )
                {
                    exception
                        << eh::errinfo_category_name( exceptionProperties -> categoryName() )
                        << eh::errinfo_error_code( eh::error_code( exceptionProperties -> errorCode(), *errorCategory ) )
                        ;
                }

#define BL_DM_SET_EXCEPTION_PROPERTY( errinfo, property ) \
                if( exceptionProperties -> property ## IsSet() ) \
                { \
                    exception << eh::errinfo( exceptionProperties -> property() ); \
                } \

#define BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo, property ) \
                if( ! exceptionProperties -> property().empty() ) \
                { \
                    exception << eh::errinfo( exceptionProperties -> property() ); \
                } \

                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_errno, errNo )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_file_name, fileName )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_file_open_mode, fileOpenMode )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_message, message )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_time_thrown, timeThrown )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_function_name, functionName )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_system_code, systemCode )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_error_code_message, errorCodeMessage )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_is_expected, isExpected )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_task_info, taskInfo )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_host_name, hostName )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_service_name, serviceName )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_endpoint_address, endpointAddress )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_endpoint_port, endpointPort )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_http_url, httpUrl )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_http_redirect_url, httpRedirectUrl )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_http_status_code, httpStatusCode )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_http_response_headers, httpResponseHeaders )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_http_request_details, httpRequestDetails )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_parser_file, parserFile )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_parser_line, parserLine )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_parser_column, parserColumn )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_parser_reason, parserReason )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_external_command_output, externalCommandOutput )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_external_command_exit_code, externalCommandExitCode )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_string_value, stringValue )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_is_user_friendly, isUserFriendly )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_ssl_is_verify_failed, sslIsVerifyFailed )
                BL_DM_SET_EXCEPTION_PROPERTY( errinfo_ssl_is_verify_error, sslIsVerifyError )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_ssl_is_verify_error_message, sslIsVerifyErrorMessage )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_ssl_is_verify_error_string, sslIsVerifyErrorString )
                BL_DM_SET_EXCEPTION_STRING_PROPERTY( errinfo_ssl_is_verify_subject_name, sslIsVerifySubjectName )

#undef BL_DM_SET_EXCEPTION_STRING_PROPERTY

#undef BL_DM_SET_EXCEPTION_PROPERTY

                return std::make_exception_ptr(
                    eh::enable_current_exception(
                        eh::enable_error_info( exception )
                        )
                    );
            }

            template
            <
                typename SERVERERRORJSON
            >
            static auto createExceptionFromObject( SAA_in const om::ObjPtr< SERVERERRORJSON >& serverErrorJson )
                -> std::exception_ptr
            {
                if( serverErrorJson -> result() == nullptr )
                {
                    BL_THROW(
                        ArgumentException(),
                        "ServerErrorJson: 'result' property is not set"
                        );
                }

                const auto& exceptionType = serverErrorJson -> result() -> exceptionType();

                if( exceptionType == "std::exception" )
                {
                    return std::make_exception_ptr(
                        std::runtime_error( serverErrorJson -> result() -> exceptionMessage() ) );
                }

                if( serverErrorJson -> result() -> exceptionProperties() == nullptr )
                {
                    BL_THROW(
                        ArgumentException(),
                        "ServerErrorResult: 'exceptionProperties' property is not set"
                        );
                }

                const auto& exceptionProperties = serverErrorJson -> result() -> exceptionProperties();

                const auto& categoryName = exceptionProperties -> categoryName();

                const eh::error_category* errorCategory = nullptr;

                if( categoryName == "generic" )
                {
                    errorCategory = &eh::generic_category();
                }
                else if( categoryName == "system" )
                {
                    errorCategory = &eh::system_category();
                }
                else if( categoryName == "" )
                {
                    errorCategory = nullptr;
                }
                else
                {
                    BL_THROW(
                        ArgumentException(),
                        BL_MSG()
                            <<"Unknown error category: '"
                            << categoryName
                            <<"'"
                         );
                }

                if( exceptionType == "bl::ArgumentException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, ArgumentException() );
                }
                else if( exceptionType == "bl::ArgumentNullException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, ArgumentNullException() );
                }
                else if( exceptionType == "bl::CacheException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, CacheException() );
                }
                else if( exceptionType == "bl::ExternalCommandException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, ExternalCommandException() );
                }
                else if( exceptionType == "bl::HttpException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, HttpException() );
                }
                else if( exceptionType == "bl::HttpServerException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, HttpServerException() );
                }
                else if( exceptionType == "bl::TimeoutException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, TimeoutException() );
                }
                else if( exceptionType == "bl::JavaException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, JavaException() );
                }
                else if( exceptionType == "bl::JsonException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, JsonException() );
                }
                else if( exceptionType == "bl::NotSupportedException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, NotSupportedException() );
                }
                else if( exceptionType == "bl::ObjectDisconnectedException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, ObjectDisconnectedException() );
                }
                else if( exceptionType == "bl::SecurityException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, SecurityException() );
                }
                else if( exceptionType == "bl::ServerErrorException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, ServerErrorException() );
                }
                else if( exceptionType == "bl::ServerNoConnectionException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, ServerNoConnectionException() );
                }
                else if( exceptionType == "bl::UnexpectedException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, UnexpectedException() );
                }
                else if( exceptionType == "bl::XmlException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, XmlException() );
                }
                else if( exceptionType == "bl::InvalidDataFormatException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, XmlException() );
                }
                else if( exceptionType == "bl::UserMessageException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, UserMessageException() );
                }
                else if( exceptionType == "bl::UserMessageException" )
                {
                    return exceptionFromProperties( exceptionProperties, errorCategory, UserMessageException() );
                }
                else if( exceptionType == "bl::SystemException" )
                {
                    if( ! errorCategory || ! exceptionProperties -> errorCodeIsSet() )
                    {
                        BL_THROW(
                            ArgumentException(),
                            BL_MSG()
                                <<"errorCategory or errorCode properties are not set for SystemException"
                             );
                    }

                    const auto& exceptionMessage = serverErrorJson -> result() -> exceptionMessage();

                    std::string whatPrefix = "";

                    std::size_t pos = exceptionMessage.find( ": " );
                    if ( pos != std::string::npos )
                    {
                        whatPrefix = exceptionMessage.substr( 0, pos );
                    }

                    auto systemException = SystemException::create(
                        eh::error_code( exceptionProperties -> errorCode(), *errorCategory ),
                        whatPrefix
                        );

                    return exceptionFromProperties( exceptionProperties, errorCategory, systemException );
                }
                else
                {
                    /*
                     * Unknown exception type, fall back on UnexpectedException
                     */

                    return exceptionFromProperties( exceptionProperties, errorCategory, UnexpectedException() );
                }
            }

            static auto getServerErrorAsJson(
                SAA_in      const std::exception_ptr&                   eptr,
                SAA_in_opt  const eh::void_exception_callback_t&        exceptionCallback = defaultEhCallback()
                )
                -> std::string
            {
                return DataModelUtils::getDocAsPrettyJsonString( createServerErrorObject( eptr, exceptionCallback ) );
            }

            static auto getServerErrorAsGraphQL(
                SAA_in      const std::exception_ptr&                   eptr,
                SAA_in_opt  const eh::void_exception_callback_t&        exceptionCallback = defaultEhCallback()
                )
                -> std::string
            {
                return DataModelUtils::getDocAsPrettyJsonString(
                    createServerErrorGraphQLObject( eptr, exceptionCallback )
                    );
            }
        };

        typedef ServerErrorHelpersT<> ServerErrorHelpers;

    } // dm

} // bl

#endif /* __BL_DATA_EH_SERVERERRORHELPERS_H_ */
