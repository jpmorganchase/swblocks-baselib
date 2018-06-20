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

#ifndef __BL_DATA_MODELS_FUNCTIONS_
#define __BL_DATA_MODELS_FUNCTIONS_

#include <baselib/data/models/JsonMessaging.h>
#include <baselib/data/DataModelObjectDefs.h>

namespace bl
{
    namespace dm
    {
        /**
         * @brief A type definition to represent a request context object
         */

        BL_DM_DEFINE_CLASS_BEGIN_IMPL_PARTIAL( FunctionContext, bl::dm::DataModelObject )

            BL_DM_DECLARE_COMPLEX_PROPERTY( securityPrincipal, bl::dm::messaging::SecurityPrincipal )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( securityPrincipal )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( FunctionContext )

        BL_DM_DEFINE_PROPERTY( FunctionContext, securityPrincipal )

        /**
         * @brief Generic class used to invoke APIs and functions across boundaries
         * (e.g. native <-> java, rest, GraphQL, etc)
         */

        BL_DM_DEFINE_CLASS_BEGIN( FunctionInputData )

            BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( functionId )
            BL_DM_DECLARE_CUSTOM_PROPERTY( arguments )
            BL_DM_DECLARE_COMPLEX_PROPERTY( functionContext, bl::dm::FunctionContext )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( functionId )
                BL_DM_IMPL_PROPERTY( arguments )
                BL_DM_IMPL_PROPERTY( functionContext )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( FunctionInputData )

        BL_DM_DEFINE_PROPERTY( FunctionInputData, functionId )
        BL_DM_DEFINE_PROPERTY( FunctionInputData, arguments )
        BL_DM_DEFINE_PROPERTY( FunctionInputData, functionContext )

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_FUNCTIONS_ */
