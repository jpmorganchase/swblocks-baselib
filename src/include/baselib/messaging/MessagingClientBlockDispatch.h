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

#ifndef __BL_MESSAGING_MESSAGINGCLIENTBLOCKDISPATCH_H_
#define __BL_MESSAGING_MESSAGINGCLIENTBLOCKDISPATCH_H_

#include <baselib/messaging/MessagingCommonTypes.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( MessagingClientBlockDispatch, "5a63fac2-dd9a-4412-92ff-a4678dcd6e55" )

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The core messaging dispatch callback interface (at block level)
         */

        class MessagingClientBlockDispatch : public om::Disposable
        {
            BL_DECLARE_INTERFACE( MessagingClientBlockDispatch )

        public:

            typedef cpp::function
            <
                void (
                    SAA_in              const uuid_t&                                   targetPeerId,
                    SAA_in              const om::ObjPtr< data::DataBlock >&            dataBlock
                    )
            >
            callback_t;

            virtual void pushBlock(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&            dataBlock,
                SAA_in_opt              CompletionCallback&&                            completionCallback = CompletionCallback()
                ) = 0;

            virtual bool isConnected() const NOEXCEPT = 0;

            /**
             * @brief An stable id which identifies this channel; if the channel is re-established / re-connected
             * this id should change, so the caller can detect these changes
             *
             * If this function returns nil() that means that a stable channel id cannot be maintained by this
             * implementation
             */

            virtual uuid_t channelId() const NOEXCEPT = 0;

            /**
             * @brief Returns if data blocks should be copied before pushed in the async queue or not
             * (in the latter case they are assumed to be owned by the caller)
             */

            virtual bool isNoCopyDataBlocks() const NOEXCEPT = 0;

            /**
             * @brief Sets if data blocks should be copied before pushed in the async queue or not
             * (in the latter case they are assumed to be owned by the caller)
             */

            virtual void isNoCopyDataBlocks( SAA_in bool isNoCopyDataBlocks ) NOEXCEPT = 0;

            /*
             * A small convenience wrapper which takes const reference for the completion callback
             * and makes a copy to invoke the main interface
             *
             * It can be used when rvalue reference is not available and / or one wants to use
             * cpp::bind for this function (since bind doesn't support rvalue references)
             */

            void pushBlockCopyCallback(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&            dataBlock,
                SAA_in_opt              const CompletionCallback&                       completionCallback = CompletionCallback()
                )
            {
                pushBlock( targetPeerId, dataBlock, cpp::copy( completionCallback ) );
            }
        };

        /**
         * @brief Implements the core messaging block dispatch callback interface on top of C++ callback
         */

        template
        <
            typename E = void
        >
        class MessagingClientBlockDispatchFromCallbackT : public MessagingClientBlockDispatch
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE(
                MessagingClientBlockDispatchFromCallbackT,
                MessagingClientBlockDispatch
                )

        public:

            typedef MessagingClientBlockDispatch::callback_t                            callback_t;

        protected:

            const uuid_t                                                                m_channelId;
            const callback_t                                                            m_callback;

            std::atomic< bool >                                                         m_disposed;

            MessagingClientBlockDispatchFromCallbackT( SAA_in callback_t&& callback )
                :
                m_channelId( uuids::create() ),
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

            virtual void pushBlock(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&            dataBlock,
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
                    m_callback( targetPeerId, dataBlock );
                }
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return ! m_disposed;
            }

            virtual uuid_t channelId() const NOEXCEPT OVERRIDE
            {
                return m_channelId;
            }

            virtual bool isNoCopyDataBlocks() const NOEXCEPT OVERRIDE
            {
                return false;
            }

            virtual void isNoCopyDataBlocks( SAA_in bool isNoCopyDataBlocks ) NOEXCEPT OVERRIDE
            {
                BL_UNUSED( isNoCopyDataBlocks );

                BL_RIP_MSG( "MessagingClientBlockDispatchFromCallbackT:: Unsupported operation: isNoCopyDataBlocks" );
            }
        };

        typedef om::ObjectImpl< MessagingClientBlockDispatchFromCallbackT<> > MessagingClientBlockDispatchFromCallback;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCLIENTBLOCKDISPATCH_H_ */
