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

#ifndef __BL_DATA_MODELS_SERVICESCONFIG_H_
#define __BL_DATA_MODELS_SERVICESCONFIG_H_

#include <baselib/data/DataModelObjectDefs.h>

namespace bl
{
    namespace dm
    {
        namespace config
        {
            /*
             * @brief Class AuthorizationServiceRestConfig
             */

            BL_DM_DEFINE_CLASS_BEGIN( AuthorizationServiceRestConfig )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( tokenType )
                BL_DM_DECLARE_BOOL_REQUIRED_PROPERTY( isTokenBinary )
                BL_DM_DECLARE_BOOL_PROPERTY( isTokenMultiProperties )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( host )
                BL_DM_DECLARE_INT_REQUIRED_PROPERTY( port )
                BL_DM_DECLARE_INT_REQUIRED_PROPERTY( successStatus )
                BL_DM_DECLARE_INT_PROPERTY( successStatusCategory )
                BL_DM_DECLARE_SIMPLE_VECTOR_PROPERTY( otherSuccessStatuses, int, get_int )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( httpAction )
                BL_DM_DECLARE_STRING_PROPERTY( urlPathTemplate )
                BL_DM_DECLARE_STRING_PROPERTY( contentTemplateBase64 )
                BL_DM_DECLARE_STRING_PROPERTY( contentTemplateFilePath )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( contentType )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( regexSid )
                BL_DM_DECLARE_STRING_PROPERTY( regexGivenName )
                BL_DM_DECLARE_STRING_PROPERTY( regexFamilyName )
                BL_DM_DECLARE_STRING_PROPERTY( regexEmail )
                BL_DM_DECLARE_STRING_PROPERTY( regexTypeId )
                BL_DM_DECLARE_STRING_PROPERTY( regexUpdatedTokenProperty )
                BL_DM_DECLARE_STRING_PROPERTY( regexUpdatedTextToken )
                BL_DM_DECLARE_STRING_PROPERTY( regexUpdatedBinaryToken )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( regexStatus )
                BL_DM_DECLARE_STRING_PROPERTY( regexStatusCategory )
                BL_DM_DECLARE_STRING_PROPERTY( regexStatusMessage )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( tokenType )
                    BL_DM_IMPL_PROPERTY( isTokenBinary )
                    BL_DM_IMPL_PROPERTY( isTokenMultiProperties )
                    BL_DM_IMPL_PROPERTY( host )
                    BL_DM_IMPL_PROPERTY( port )
                    BL_DM_IMPL_PROPERTY( successStatus )
                    BL_DM_IMPL_PROPERTY( successStatusCategory )
                    BL_DM_IMPL_PROPERTY( otherSuccessStatuses )

                    BL_DM_IMPL_PROPERTY( httpAction )
                    BL_DM_IMPL_PROPERTY( urlPathTemplate )
                    BL_DM_IMPL_PROPERTY( contentTemplateBase64 )
                    BL_DM_IMPL_PROPERTY( contentTemplateFilePath )
                    BL_DM_IMPL_PROPERTY( contentType )

                    BL_DM_IMPL_PROPERTY( regexSid )
                    BL_DM_IMPL_PROPERTY( regexGivenName )
                    BL_DM_IMPL_PROPERTY( regexFamilyName )
                    BL_DM_IMPL_PROPERTY( regexEmail )
                    BL_DM_IMPL_PROPERTY( regexTypeId )
                    BL_DM_IMPL_PROPERTY( regexUpdatedTokenProperty )
                    BL_DM_IMPL_PROPERTY( regexUpdatedTextToken )
                    BL_DM_IMPL_PROPERTY( regexUpdatedBinaryToken )
                    BL_DM_IMPL_PROPERTY( regexStatus )
                    BL_DM_IMPL_PROPERTY( regexStatusCategory )
                    BL_DM_IMPL_PROPERTY( regexStatusMessage )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( AuthorizationServiceRestConfig )

            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, tokenType )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, isTokenBinary )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, isTokenMultiProperties )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, host )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, port )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, successStatus )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, successStatusCategory )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, otherSuccessStatuses )

            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, httpAction )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, urlPathTemplate )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, contentTemplateBase64 )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, contentTemplateFilePath )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, contentType )

            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexSid )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexGivenName )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexFamilyName )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexEmail )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexTypeId )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexUpdatedTokenProperty )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexUpdatedTextToken )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexUpdatedBinaryToken )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexStatus )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexStatusCategory )
            BL_DM_DEFINE_PROPERTY( AuthorizationServiceRestConfig, regexStatusMessage )

        } // config

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_SERVICESCONFIG_H_ */
