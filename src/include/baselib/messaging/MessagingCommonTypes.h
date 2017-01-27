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

#ifndef __BL_MESSAGING_MESSAGINGCOMMONTYPES_H_
#define __BL_MESSAGING_MESSAGINGCOMMONTYPES_H_

#include <baselib/tasks/TaskBase.h>

#include <baselib/data/models/JsonMessaging.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /*
         * Types from tasks::*
         */

        using tasks::CompletionCallback;
        using tasks::ScheduleCallback;

        /*
         * Types from bl::dm::messaging::*
         */

        using bl::dm::messaging::MessageType;

        using bl::dm::messaging::SecurityPrincipal;
        using bl::dm::messaging::AuthenticationToken;
        using bl::dm::messaging::PrincipalIdentityInfo;
        using bl::dm::messaging::BrokerProtocol;

        using bl::dm::messaging::Payload;
        using bl::dm::messaging::AsyncRpcRequest;
        using bl::dm::messaging::AsyncRpcResponse;
        using bl::dm::messaging::NotificationData;
        using bl::dm::messaging::AsyncRpcPayload;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCOMMONTYPES_H_ */
