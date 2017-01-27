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

#ifndef __BL_MESSAGING_MESSAGINGSERVICESOBJECT_H_
#define __BL_MESSAGING_MESSAGINGSERVICESOBJECT_H_

#include <baselib/messaging/MessagingCommonTypes.h>
#include <baselib/messaging/MessagingClientObjectDispatch.h>
#include <baselib/messaging/MessagingClientBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( MessagingClientObject, "a0580813-d05b-417a-9fef-a4df4a80a3dc" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The core messaging services interface (at object level)
         */

        class MessagingClientObject : public MessagingClientBlock
        {
            BL_DECLARE_INTERFACE( MessagingClientObject )

        public:

            virtual auto outgoingObjectChannel() const NOEXCEPT -> const om::ObjPtr< MessagingClientObjectDispatch >& = 0;
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGSERVICESOBJECT_H_ */
