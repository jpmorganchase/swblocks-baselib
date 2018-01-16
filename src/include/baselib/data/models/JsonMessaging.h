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

#ifndef __BL_DATA_MODELS_JSONMESSAGING_H_
#define __BL_DATA_MODELS_JSONMESSAGING_H_

#include <baselib/data/DataModelObjectDefs.h>
#include <baselib/core/EnumUtils.h>

#include <baselib/data/models/ErrorHandling.h>

namespace bl
{
    namespace dm
    {
        namespace messaging
        {
            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( MessageType, "MessageType",
                ( ( AsyncRpcDispatch, "AsyncRpcDispatch" ) )
                ( ( AsyncRpcAcknowledgment, "AsyncRpcAcknowledgment" ) )
                ( ( AsyncNotification, "AsyncNotification" ) )
                ( ( BackendAssociateTargetPeerId, "BackendAssociateTargetPeerId" ) )
                ( ( BackendDissociateTargetPeerId, "BackendDissociateTargetPeerId" ) )
                )

            /*
             * @brief Class AuthenticationToken
             */

            BL_DM_DEFINE_CLASS_BEGIN( AuthenticationToken )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( type )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( data )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( type )
                    BL_DM_IMPL_PROPERTY( data )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( AuthenticationToken )

            BL_DM_DEFINE_PROPERTY( AuthenticationToken, type )
            BL_DM_DEFINE_PROPERTY( AuthenticationToken, data )

            /*
             * @brief Class SecurityPrincipal
             */

            BL_DM_DEFINE_CLASS_BEGIN( SecurityPrincipal )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( sid )
                BL_DM_DECLARE_STRING_PROPERTY( givenName )
                BL_DM_DECLARE_STRING_PROPERTY( familyName )
                BL_DM_DECLARE_STRING_PROPERTY( email )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( sid )
                    BL_DM_IMPL_PROPERTY( givenName )
                    BL_DM_IMPL_PROPERTY( familyName )
                    BL_DM_IMPL_PROPERTY( email )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( SecurityPrincipal )

            BL_DM_DEFINE_PROPERTY( SecurityPrincipal, sid )
            BL_DM_DEFINE_PROPERTY( SecurityPrincipal, givenName )
            BL_DM_DEFINE_PROPERTY( SecurityPrincipal, familyName )
            BL_DM_DEFINE_PROPERTY( SecurityPrincipal, email )

            /*
             * @brief Class PrincipalIdentityInfo
             */

            BL_DM_DEFINE_CLASS_BEGIN( PrincipalIdentityInfo )

                BL_DM_DECLARE_COMPLEX_PROPERTY( authenticationToken, bl::dm::messaging::AuthenticationToken )
                BL_DM_DECLARE_COMPLEX_PROPERTY( securityPrincipal, bl::dm::messaging::SecurityPrincipal )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( authenticationToken )
                    BL_DM_IMPL_PROPERTY( securityPrincipal )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( PrincipalIdentityInfo )

            BL_DM_DEFINE_PROPERTY( PrincipalIdentityInfo, authenticationToken )
            BL_DM_DEFINE_PROPERTY( PrincipalIdentityInfo, securityPrincipal )

            /*
             * @brief Class BrokerProtocol
             */

            BL_DM_DEFINE_CLASS_BEGIN( BrokerProtocol )

                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( messageType )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( messageId )
                BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( conversationId )
                BL_DM_DECLARE_STRING_PROPERTY( sourcePeerId )
                BL_DM_DECLARE_STRING_PROPERTY( targetPeerId )
                BL_DM_DECLARE_COMPLEX_PROPERTY( principalIdentityInfo, bl::dm::messaging::PrincipalIdentityInfo )
                BL_DM_DECLARE_COMPLEX_PROPERTY( passThroughUserData, bl::dm::Payload )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( messageType )
                    BL_DM_IMPL_PROPERTY( messageId )
                    BL_DM_IMPL_PROPERTY( conversationId )
                    BL_DM_IMPL_PROPERTY( sourcePeerId )
                    BL_DM_IMPL_PROPERTY( targetPeerId )
                    BL_DM_IMPL_PROPERTY( principalIdentityInfo )
                    BL_DM_IMPL_PROPERTY( passThroughUserData )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( BrokerProtocol )

            BL_DM_DEFINE_PROPERTY( BrokerProtocol, messageType )
            BL_DM_DEFINE_PROPERTY( BrokerProtocol, messageId )
            BL_DM_DEFINE_PROPERTY( BrokerProtocol, conversationId )
            BL_DM_DEFINE_PROPERTY( BrokerProtocol, sourcePeerId )
            BL_DM_DEFINE_PROPERTY( BrokerProtocol, targetPeerId )
            BL_DM_DEFINE_PROPERTY( BrokerProtocol, principalIdentityInfo )
            BL_DM_DEFINE_PROPERTY( BrokerProtocol, passThroughUserData )


            /*
             * @brief The definitions of these placeholder types is no different than bl::dm::Payload
             * so we simply typedef them to bl::dm::Payload
             */

            typedef bl::dm::Payload Payload;
            typedef bl::dm::Payload AsyncRpcRequest;
            typedef bl::dm::Payload NotificationData;

            /*
             * @brief Class AsyncRpcResponse
             */

            BL_DM_DEFINE_CLASS_BEGIN( AsyncRpcResponse )

                BL_DM_DECLARE_COMPLEX_PROPERTY( serverErrorJson, bl::dm::ServerErrorJson )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( serverErrorJson )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( AsyncRpcResponse )

            BL_DM_DEFINE_PROPERTY( AsyncRpcResponse, serverErrorJson )

            /*
             * @brief Class AsyncRpcPayload
             */

            BL_DM_DEFINE_CLASS_BEGIN( AsyncRpcPayload )

                BL_DM_DECLARE_COMPLEX_PROPERTY( asyncRpcRequest, bl::dm::messaging::AsyncRpcRequest )
                BL_DM_DECLARE_COMPLEX_PROPERTY( asyncRpcResponse, bl::dm::messaging::AsyncRpcResponse )
                BL_DM_DECLARE_COMPLEX_PROPERTY( notificationData, bl::dm::messaging::NotificationData )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( asyncRpcRequest )
                    BL_DM_IMPL_PROPERTY( asyncRpcResponse )
                    BL_DM_IMPL_PROPERTY( notificationData )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( AsyncRpcPayload )

            BL_DM_DEFINE_PROPERTY( AsyncRpcPayload, asyncRpcRequest )
            BL_DM_DEFINE_PROPERTY( AsyncRpcPayload, asyncRpcResponse )
            BL_DM_DEFINE_PROPERTY( AsyncRpcPayload, notificationData )

        } // messaging

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_JSONMESSAGING_H_ */
