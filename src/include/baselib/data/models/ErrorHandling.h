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

#ifndef __BL_DATA_MODELS_ERRORHANDLING_H_
#define __BL_DATA_MODELS_ERRORHANDLING_H_

#include <baselib/data/DataModelObjectDefs.h>

namespace bl
{
    namespace dm
    {
        /*
         * @brief Class ExceptionProperties
         */

        BL_DM_DEFINE_CLASS_BEGIN( ExceptionProperties )

            BL_DM_DECLARE_INT_PROPERTY( errNo )
            BL_DM_DECLARE_STRING_PROPERTY( fileName )
            BL_DM_DECLARE_STRING_PROPERTY( fileOpenMode )
            BL_DM_DECLARE_STRING_PROPERTY( message )
            BL_DM_DECLARE_STRING_PROPERTY( timeThrown )
            BL_DM_DECLARE_STRING_PROPERTY( functionName )
            BL_DM_DECLARE_INT_PROPERTY( systemCode )
            BL_DM_DECLARE_STRING_PROPERTY( categoryName )
            BL_DM_DECLARE_INT_PROPERTY( errorCode )
            BL_DM_DECLARE_STRING_PROPERTY( errorCodeMessage )
            BL_DM_DECLARE_BOOL_PROPERTY( isExpected )
            BL_DM_DECLARE_STRING_PROPERTY( taskInfo )
            BL_DM_DECLARE_STRING_PROPERTY( hostName )
            BL_DM_DECLARE_STRING_PROPERTY( serviceName )
            BL_DM_DECLARE_STRING_PROPERTY( endpointAddress )
            BL_DM_DECLARE_INT_PROPERTY( endpointPort )
            BL_DM_DECLARE_STRING_PROPERTY( httpUrl )
            BL_DM_DECLARE_STRING_PROPERTY( httpRedirectUrl )
            BL_DM_DECLARE_INT_PROPERTY( httpStatusCode )
            BL_DM_DECLARE_STRING_PROPERTY( httpResponseHeaders )
            BL_DM_DECLARE_STRING_PROPERTY( httpRequestDetails )
            BL_DM_DECLARE_STRING_PROPERTY( parserFile )
            BL_DM_DECLARE_INT_PROPERTY( parserLine )
            BL_DM_DECLARE_INT_PROPERTY( parserColumn )
            BL_DM_DECLARE_STRING_PROPERTY( parserReason )
            BL_DM_DECLARE_STRING_PROPERTY( externalCommandOutput )
            BL_DM_DECLARE_INT_PROPERTY( externalCommandExitCode )
            BL_DM_DECLARE_STRING_PROPERTY( stringValue )
            BL_DM_DECLARE_BOOL_PROPERTY( isUserFriendly )
            BL_DM_DECLARE_BOOL_PROPERTY( sslIsVerifyFailed )
            BL_DM_DECLARE_INT_PROPERTY( sslIsVerifyError )
            BL_DM_DECLARE_STRING_PROPERTY( sslIsVerifyErrorMessage )
            BL_DM_DECLARE_STRING_PROPERTY( sslIsVerifyErrorString )
            BL_DM_DECLARE_STRING_PROPERTY( sslIsVerifySubjectName )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( errNo )
                BL_DM_IMPL_PROPERTY( fileName )
                BL_DM_IMPL_PROPERTY( fileOpenMode )
                BL_DM_IMPL_PROPERTY( message )
                BL_DM_IMPL_PROPERTY( timeThrown )
                BL_DM_IMPL_PROPERTY( functionName )
                BL_DM_IMPL_PROPERTY( systemCode )
                BL_DM_IMPL_PROPERTY( categoryName )
                BL_DM_IMPL_PROPERTY( errorCode )
                BL_DM_IMPL_PROPERTY( errorCodeMessage )
                BL_DM_IMPL_PROPERTY( isExpected )
                BL_DM_IMPL_PROPERTY( taskInfo )
                BL_DM_IMPL_PROPERTY( hostName )
                BL_DM_IMPL_PROPERTY( serviceName )
                BL_DM_IMPL_PROPERTY( endpointAddress )
                BL_DM_IMPL_PROPERTY( endpointPort )
                BL_DM_IMPL_PROPERTY( httpUrl )
                BL_DM_IMPL_PROPERTY( httpRedirectUrl )
                BL_DM_IMPL_PROPERTY( httpStatusCode )
                BL_DM_IMPL_PROPERTY( httpResponseHeaders )
                BL_DM_IMPL_PROPERTY( httpRequestDetails )
                BL_DM_IMPL_PROPERTY( parserFile )
                BL_DM_IMPL_PROPERTY( parserLine )
                BL_DM_IMPL_PROPERTY( parserColumn )
                BL_DM_IMPL_PROPERTY( parserReason )
                BL_DM_IMPL_PROPERTY( externalCommandOutput )
                BL_DM_IMPL_PROPERTY( externalCommandExitCode )
                BL_DM_IMPL_PROPERTY( stringValue )
                BL_DM_IMPL_PROPERTY( isUserFriendly )
                BL_DM_IMPL_PROPERTY( sslIsVerifyFailed )
                BL_DM_IMPL_PROPERTY( sslIsVerifyError )
                BL_DM_IMPL_PROPERTY( sslIsVerifyErrorMessage )
                BL_DM_IMPL_PROPERTY( sslIsVerifyErrorString )
                BL_DM_IMPL_PROPERTY( sslIsVerifySubjectName )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( ExceptionProperties )

        BL_DM_DEFINE_PROPERTY( ExceptionProperties, errNo )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, fileName )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, fileOpenMode )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, message )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, timeThrown )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, functionName )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, systemCode )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, categoryName )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, errorCode )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, errorCodeMessage )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, isExpected )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, taskInfo )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, hostName )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, serviceName )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, endpointAddress )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, endpointPort )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, httpUrl )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, httpRedirectUrl )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, httpStatusCode )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, httpResponseHeaders )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, httpRequestDetails )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, parserFile )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, parserLine )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, parserColumn )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, parserReason )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, externalCommandOutput )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, externalCommandExitCode )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, stringValue )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, isUserFriendly )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, sslIsVerifyFailed )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, sslIsVerifyError )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, sslIsVerifyErrorMessage )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, sslIsVerifyErrorString )
        BL_DM_DEFINE_PROPERTY( ExceptionProperties, sslIsVerifySubjectName )

        /*
         * @brief Class ServerErrorResult
         *
         * Note that the message property is for a user friendly message and it can
         * be different than the exceptionMessage and message will be the same as
         * exceptionMessage only if the exception is user friendly such
         */

        BL_DM_DEFINE_CLASS_BEGIN( ServerErrorResult )

            BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( message )
            BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( exceptionType )
            BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( exceptionMessage )
            BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( exceptionFullDump )
            BL_DM_DECLARE_COMPLEX_PROPERTY( exceptionProperties, bl::dm::ExceptionProperties )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( message )
                BL_DM_IMPL_PROPERTY( exceptionType )
                BL_DM_IMPL_PROPERTY( exceptionMessage )
                BL_DM_IMPL_PROPERTY( exceptionFullDump )
                BL_DM_IMPL_PROPERTY( exceptionProperties )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( ServerErrorResult )

        BL_DM_DEFINE_PROPERTY( ServerErrorResult, message )
        BL_DM_DEFINE_PROPERTY( ServerErrorResult, exceptionType )
        BL_DM_DEFINE_PROPERTY( ServerErrorResult, exceptionMessage )
        BL_DM_DEFINE_PROPERTY( ServerErrorResult, exceptionFullDump )
        BL_DM_DEFINE_PROPERTY( ServerErrorResult, exceptionProperties )

        /*
         * @brief Class ServerErrorJson
         */

        BL_DM_DEFINE_CLASS_BEGIN( ServerErrorJson )

            BL_DM_DECLARE_COMPLEX_PROPERTY( result, bl::dm::ServerErrorResult )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( result )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( ServerErrorJson )

        /*
         * @brief Class GraphQLError
         */

        BL_DM_DEFINE_CLASS_BEGIN( GraphQLError )

            BL_DM_DECLARE_STRING_PROPERTY( message )
            BL_DM_DECLARE_STRING_PROPERTY( errorType )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( message )
                BL_DM_IMPL_PROPERTY( errorType )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( GraphQLError )

        BL_DM_DEFINE_PROPERTY( GraphQLError, message )
        BL_DM_DEFINE_PROPERTY( GraphQLError, errorType )

        /*
         * @brief Class ServerErrorGraphQL
         */

        BL_DM_DEFINE_CLASS_BEGIN( ServerErrorGraphQL )

            BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( errors, bl::dm::GraphQLError )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( errors )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( ServerErrorGraphQL )

        BL_DM_DEFINE_PROPERTY( ServerErrorGraphQL, errors )

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_ERRORHANDLING_H_ */
