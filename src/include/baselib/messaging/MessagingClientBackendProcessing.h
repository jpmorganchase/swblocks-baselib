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

#ifndef __BL_MESSAGING_MESSAGINGCLIENTBACKENDPROCESSING_H_
#define __BL_MESSAGING_MESSAGINGCLIENTBACKENDPROCESSING_H_

#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/BackendProcessing.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/Task.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief An implementation of the BackendProcessing interface on top of the
         * MessagingClientBlockDispatch interface
         */

        template
        <
            typename E = void
        >
        class MessagingClientBackendProcessingT : public BackendProcessingBase
        {
            BL_DECLARE_OBJECT_IMPL( MessagingClientBackendProcessingT )

        protected:

            typedef MessagingClientBackendProcessingT< E >                              this_type;
            typedef BackendProcessingBase                                               base_type;

            const om::ObjPtrDisposable< MessagingClientBlockDispatch >                  m_target;

            MessagingClientBackendProcessingT( SAA_in om::ObjPtrDisposable< MessagingClientBlockDispatch >&& target )
                :
                m_target( BL_PARAM_FWD( target ) )
            {
            }

            void processIncomingBlock(
                SAA_in                  const uuid_t&                                   targetPeerId,
                SAA_in                  const om::ObjPtrCopyable< data::DataBlock >&    dataBlock
                )
            {
                /*
                 * Note: for now we pass empty completion callback as these are typically
                 * expected to be processed either synch or async, but in either case
                 * the completion needs to be handled by the caller downstream as this is
                 * the place where there is enough knowledge about how to handle it
                 */

                m_target -> pushBlock( targetPeerId, dataBlock, CompletionCallback() );
            }

        public:

            typedef BackendProcessing::OperationId                                      OperationId;
            typedef BackendProcessing::CommandId                                        CommandId;

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                base_type::dispose();

                m_target -> dispose();

                BL_NOEXCEPT_END()
            }

            virtual auto createBackendProcessingTask(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const CommandId                                 commandId,
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in_opt              const uuid_t&                                   sourcePeerId,
                SAA_in_opt              const uuid_t&                                   targetPeerId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                BL_UNUSED( sessionId );
                BL_UNUSED( chunkId );
                BL_UNUSED( sourcePeerId );

                /*
                 * We are only interested in processing Put commands (the rest should be handled by
                 * the async wrapper class if supported)
                 */

                if( OperationId::Put != operationId || CommandId::None != commandId )
                {
                    return nullptr;
                }

                return tasks::SimpleTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &this_type::processIncomingBlock,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        targetPeerId,
                        om::ObjPtrCopyable< data::DataBlock >( data )
                        )
                    );
            }
        };

        typedef om::ObjectImpl< MessagingClientBackendProcessingT<> > MessagingClientBackendProcessing;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCLIENTBACKENDPROCESSING_H_ */
