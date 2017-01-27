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

#ifndef __BL_MESSAGING_MESSAGINGCLIENTBLOCKDISPATCHLOCAL_H_
#define __BL_MESSAGING_MESSAGINGCLIENTBLOCKDISPATCHLOCAL_H_

#include <baselib/messaging/MessagingClientBlockDispatch.h>

#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/TaskBase.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief Simple local dispatcher implementation of MessagingClientBlockDispatch
         */

        template
        <
            typename E = void
        >
        class MessagingClientBlockDispatchLocalT  : public MessagingClientBlockDispatch
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE( MessagingClientBlockDispatchLocalT, MessagingClientBlockDispatch )

        protected:

            typedef MessagingClientBlockDispatchLocalT< E >                             this_type;

            enum : std::size_t
            {
                /*
                 * Limit the # of executing tasks on the processing side to ensure that the general thread pool is
                 * never clogged with these tasks
                 */

                MAX_TASKS_EXECUTING = 4,
            };

            const om::ObjPtr< data::datablocks_pool_type >                              m_dataBlocksPool;
            const uuid_t                                                                m_channelId;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                               m_queue;
            om::ObjPtrDisposable< MessagingClientBlockDispatch >                        m_receiver;

            MessagingClientBlockDispatchLocalT(
                SAA_in                  om::ObjPtr< MessagingClientBlockDispatch >&&    receiver,
                SAA_in_opt              om::ObjPtr< data::datablocks_pool_type >&&      dataBlocksPool = nullptr
                )
                :
                m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) ),
                m_channelId( uuids::create() )
            {
                m_queue = tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                    tasks::ExecutionQueue::OptionKeepNone,
                    MAX_TASKS_EXECUTING
                    );

                m_receiver = BL_PARAM_FWD( receiver );
            }

            void dispatchBlockInternal(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtrCopyable< data::DataBlock >&    dataBlock
                )
            {
                m_receiver -> pushBlock( targetPeerId, om::copy( dataBlock ) );

                if( m_dataBlocksPool )
                {
                    m_dataBlocksPool -> put( om::copy( dataBlock ) );
                }
            }

        public:

            void flush()
            {
                m_queue -> flush();
            }

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                tasks::ExecutionQueue::disposeQueue( m_queue );

                if( m_receiver )
                {
                    m_receiver -> dispose();
                    m_receiver.reset();
                }

                BL_NOEXCEPT_END()
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

                /*
                 * Just make a copy of the data block and push it in the queue for processing
                 */

                const auto newDataBlock = data::DataBlock::copy( dataBlock, m_dataBlocksPool );

                m_queue -> push_back(
                    tasks::SimpleTaskImpl::createInstance< tasks::Task >(
                        cpp::bind(
                            &this_type::dispatchBlockInternal,
                            om::ObjPtrCopyable< this_type >::acquireRef( this ),
                            targetPeerId,
                            om::ObjPtrCopyable< data::DataBlock >( newDataBlock )
                            ),
                        "LocalMessageDispatcher" /* taskName */
                        )
                    );
            }

            virtual bool isConnected() const NOEXCEPT OVERRIDE
            {
                return true;
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

        typedef om::ObjectImpl< MessagingClientBlockDispatchLocalT<> > MessagingClientBlockDispatchLocal;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCLIENTBLOCKDISPATCHLOCAL_H_ */
