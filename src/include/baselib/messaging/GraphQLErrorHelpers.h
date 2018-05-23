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

#ifndef __BL_MESSAGING_GRAPHQLERRORHELPERS_H_
#define __BL_MESSAGING_GRAPHQLERRORHELPERS_H_

#include <baselib/data/eh/ServerErrorHelpers.h>
#include <baselib/data/DataModelObject.h>
#include <baselib/data/DataModelObjectDefs.h>
#include <baselib/data/models/ErrorHandling.h>

#include <baselib/core/CPP.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <baselib/messaging/BrokerErrorCodes.h>

namespace bl
{
    namespace messaging
    {
        template
        <
            typename E = void
        >
        class GraphQLErrorHelpersT
        {
        public:

            static auto getServerErrorAsGraphQL(
                SAA_in      const std::exception_ptr&             eptr,
                SAA_in_opt  const eh::void_exception_callback_t&  cb = dm::ServerErrorHelpers::defaultEhCallback()
                )
                -> std::string
            {
                return dm::DataModelUtils::getDocAsPrettyJsonString(
                    createServerErrorGraphQLObject( eptr, cb )
                    );
            }

        private:

            static auto createServerErrorGraphQLObject(
                SAA_in      const std::exception_ptr&                       eptr,
                SAA_in      const eh::void_exception_callback_t&            callback
                )
                -> om::ObjPtr< dm::ServerErrorGraphQL >
            {
                using namespace dm;

                auto errorGraphQL = ServerErrorGraphQL::createInstance();

                const auto error = ServerErrorHelpers::createServerErrorResultObject( eptr, callback );

                auto result = GraphQLErrorMessage::createInstance();

                result -> errorType( error -> exceptionType() );

                const auto errorCode =
                    static_cast< eh::errc::errc_t >( error -> exceptionProperties() -> errorCode() );

                const auto errorCodeObj = eh::errc::make_error_code( errorCode );

                const auto expected = BrokerErrorCodes::tryGetExpectedErrorMessage( errorCodeObj );

                auto message = expected.empty() ? error -> message() : expected;

                if( errorCode != eh::errc::errc_t::success )
                {
                    message.append( " (error code "  );
                    message.append( std::to_string( errorCode ) );
                    message.append( ")" );
                }

                result -> message( std::move( message ) );

                errorGraphQL -> errorsLvalue().push_back( std::move( result ) );

                return errorGraphQL;
            }
        };

        typedef GraphQLErrorHelpersT<> GraphQLErrorHelpers;

    } //messaging

} //bl

#endif /* __BL_MESSAGING_GRAPHQLERRORHELPERS_H_ */
