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

#ifndef __BL_MESSAGING_MESSAGINGSERVICESBLOCK_H_
#define __BL_MESSAGING_MESSAGINGSERVICESBLOCK_H_

#include <baselib/messaging/MessagingClientBlockDispatch.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( MessagingClientBlock, "12cfedb1-33ab-4e0b-b217-441b40966901" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The core messaging services interface (at block level)
         */

        class MessagingClientBlock : public om::Disposable
        {
            BL_DECLARE_INTERFACE( MessagingClientBlock )

        public:

            virtual auto outgoingBlockChannel() const NOEXCEPT -> const om::ObjPtr< MessagingClientBlockDispatch >& = 0;

            virtual bool isConnected() const NOEXCEPT = 0;
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGSERVICESBLOCK_H_ */
