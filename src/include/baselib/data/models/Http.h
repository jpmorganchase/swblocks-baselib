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

#ifndef __BL_DATA_MODELS_HTTP_H_
#define __BL_DATA_MODELS_HTTP_H_

#include <baselib/data/DataModelObjectDefs.h>
#include <baselib/core/EnumUtils.h>

namespace bl
{
    namespace dm
    {
        namespace http
        {
            /*
             * @brief Class HttpRequestMetadata
             */

            BL_DM_DEFINE_CLASS_BEGIN( HttpRequestMetadata )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( method )
                BL_DM_DECLARE_STRING_PROPERTY( urlPath )
                BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( headers, bl::dm::NameValueStringsPair )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( method )
                    BL_DM_IMPL_PROPERTY( urlPath )
                    BL_DM_IMPL_PROPERTY( headers )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( HttpRequestMetadata )

            BL_DM_DEFINE_PROPERTY( HttpRequestMetadata, method )
            BL_DM_DEFINE_PROPERTY( HttpRequestMetadata, urlPath )
            BL_DM_DEFINE_PROPERTY( HttpRequestMetadata, headers )

            /*
             * @brief Class HttpResponseMetadata
             */

            BL_DM_DEFINE_CLASS_BEGIN( HttpResponseMetadata )

                BL_DM_DECLARE_INT_REQUIRED_PROPERTY( statusCode )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( contentType )
                BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( headers, bl::dm::NameValueStringsPair )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( statusCode )
                    BL_DM_IMPL_PROPERTY( contentType )
                    BL_DM_IMPL_PROPERTY( headers )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( HttpResponseMetadata )

            BL_DM_DEFINE_PROPERTY( HttpResponseMetadata, statusCode )
            BL_DM_DEFINE_PROPERTY( HttpResponseMetadata, contentType )
            BL_DM_DEFINE_PROPERTY( HttpResponseMetadata, headers )

        } // http

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_HTTP_H_ */
