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

#ifndef __BL_TRANSFER_CHUNKSDELETER_H_
#define __BL_TRANSFER_CHUNKSDELETER_H_

#include <baselib/transfer/ChunksReceiverDeleterBase.h>
#include <baselib/transfer/ChunksSendRecvIncludes.h>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class ChunksDeleter
         *
         * This is the chunks deleter which will delete the chunks from the data store
         *
         * The input is implicit and the connecting will start immediately when the
         * observable is scheduled for execution
         */

        template
        <
            typename STREAM
        >
        class ChunksDeleterT :
            public ChunksReceiverDeleterBase< STREAM >
        {
        public:

            typedef ChunksReceiverDeleterBase< STREAM >                                     base_type;
            typedef typename base_type::context_t                                           context_t;
            typedef typename base_type::transfer_task_t                                     transfer_task_t;

        private:

            BL_DECLARE_OBJECT_IMPL( ChunksDeleterT )

        protected:

            ChunksDeleterT(
                SAA_in              const om::ObjPtr< EndpointSelector >&                   endpointSelector,
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                SAA_in              const std::size_t                                       tasksPoolSize = DEFAULT_NO_OF_CONNECTIONS
                )
                :
                base_type( endpointSelector, context, "success:Chunks_Deleter" /* taskName */, fsmd, tasksPoolSize )
            {
            }

            virtual auto getCommandId() const NOEXCEPT -> typename transfer_task_t::CommandId OVERRIDE
            {
                return transfer_task_t::CommandId::RemoveChunk;
            }

            virtual bool handleReadyTaskImpl( SAA_in const om::ObjPtr< transfer_task_t >& transfer ) OVERRIDE
            {
                /*
                 * The task that is passed here cannot be failed and it should not have data
                 * block attached to it - this invariant should be enforced by the outer code
                 */

                BL_ASSERT( ! transfer -> isFailed() );
                BL_ASSERT( ! transfer -> getChunkData() );

                if(
                    uuids::nil() != transfer -> getChunkId() &&
                    tasks::Task::Completed == transfer -> getState() &&
                    false == base_type::m_sessionsFlushRequested
                    )
                {
                    /*
                     * Deletion was successful - notify next, count and proceed
                     */

                    if( ! base_type::notifyOnNext( cpp::any( transfer -> getChunkId() ) ) )
                    {
                        /*
                         * The next processing unit is not ready to
                         * process the deletion event
                         */

                        return false;
                    }

                    ++base_type::m_totalBlocks;
                    transfer -> setChunkId( uuids::nil() );
                }

                return true;
            }
        };

        typedef ChunksDeleterT< tasks::TcpSocketAsyncBase >         ChunksDeleter;
        typedef ChunksDeleterT< tasks::TcpSslSocketAsyncBase >      SslChunksDeleter;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_CHUNKSDELETER_H_ */
