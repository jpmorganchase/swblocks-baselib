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

#ifndef __BL_MESSAGING_MESSAGINGCLIENTOBJECTDISPATCH_H_
#define __BL_MESSAGING_MESSAGINGCLIENTOBJECTDISPATCH_H_

#include <baselib/messaging/MessagingCommonTypes.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( MessagingClientObjectDispatch, "22edc658-8269-4471-a86a-f6b626b97baf" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The core messaging dispatch callback interface (at object level)
         */

        class MessagingClientObjectDispatch : public om::Disposable
        {
            BL_DECLARE_INTERFACE( MessagingClientObjectDispatch )

        public:

            typedef cpp::function
            <
                void (
                    SAA_in              const uuid_t&                                   targetPeerId,
                    SAA_in              const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                    SAA_in_opt          const om::ObjPtr< Payload >&                    payload
                    )
            >
            callback_t;


            virtual void pushMessage(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) = 0;

            virtual bool isConnected() const NOEXCEPT = 0;

            /*
             * A small convenience wrapper which takes const reference for the completion callback
             * and makes a copy to invoke the main interface
             *
             * It can be used when rvalue reference is not available and / or one wants to use
             * cpp::bind for this function (since bind doesn't support rvalue references)
             */

            void pushMessageCopyCallback(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              const CompletionCallback&                       completionCallback = CompletionCallback()
                )
            {
                pushMessage( targetPeerId, brokerProtocol, payload, cpp::copy( completionCallback ) );
            }
        };

        /**
         * @brief Implements the core messaging object dispatch callback interface on top of C++ callback
         */

        template
        <
            typename E = void
        >
        class MessagingClientObjectDispatchFromCallbackT : public MessagingClientObjectDispatch
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE(
                MessagingClientObjectDispatchFromCallbackT,
                MessagingClientObjectDispatch
                )

        public:

            typedef MessagingClientObjectDispatch::callback_t                           callback_t;

        protected:

            const callback_t                                                            m_callback;

            std::atomic< bool >                                                         m_disposed;

            MessagingClientObjectDispatchFromCallbackT( SAA_in callback_t&& callback )
                :
                m_callback( BL_PARAM_FWD( callback ) ),
                m_disposed( false )
            {
                BL_ASSERT( m_callback );
            }

        public:

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                m_disposed = true;
            }

            virtual void pushMessage(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< BrokerProtocol >&             brokerProtocol,
                SAA_in_opt              const om::ObjPtr< Payload >&                    payload,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) OVERRIDE
            {
                /*
                 * Note: the completionCallback is expected to always be empty here
                 */

                BL_UNUSED( completionCallback );
                BL_ASSERT( ! completionCallback );

                if( ! m_disposed )
                {
                    m_callback( targetPeerId, brokerProtocol, payload );
                }
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return ! m_disposed;
            }
        };

        typedef om::ObjectImpl< MessagingClientObjectDispatchFromCallbackT<> > MessagingClientObjectDispatchFromCallback;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCLIENTOBJECTDISPATCH_H_ */
