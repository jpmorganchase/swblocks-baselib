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

#ifndef __BL_MESSAGING_ACCEPTORNOTIFY_H_
#define __BL_MESSAGING_ACCEPTORNOTIFY_H_

#include <baselib/tasks/TaskBase.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( AcceptorNotify, "47556a22-736b-4d7d-885c-012e01d25ac9" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief class AcceptorNotify - an acceptor notify interface
         */

        class AcceptorNotify : public om::Object
        {
            BL_DECLARE_INTERFACE( AcceptorNotify )

        public:

            bool peerConnectedNotifyCopyCallback(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              const tasks::CompletionCallback&                    completionCallback
                )
            {
                return peerConnectedNotify( peerId, cpp::copy( completionCallback ) );
            }

            virtual bool peerConnectedNotify(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              tasks::CompletionCallback&&                         completionCallback
                )
                = 0;

            virtual bool peerDisconnectedNotify(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              tasks::CompletionCallback&&                         completionCallback
                )
                = 0;
        };

    } // messaging

} // bl


#endif /* __BL_MESSAGING_ACCEPTORNOTIFY_H_ */
