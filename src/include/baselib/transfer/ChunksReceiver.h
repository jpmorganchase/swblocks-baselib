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

#ifndef __BL_TRANSFER_CHUNKSRECEIVER_H_
#define __BL_TRANSFER_CHUNKSRECEIVER_H_

#include <baselib/transfer/ChunksReceiverDeleterBase.h>
#include <baselib/transfer/ChunksSendRecvIncludes.h>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class ChunksReceiver
         *
         * This is the chunks receiver which will receive the chunks from the network
         *
         * The input is implicit and the connecting will start immediately when the
         * observable is scheduled for execution
         */

        template
        <
            typename STREAM
        >
        class ChunksReceiverT :
            public ChunksReceiverDeleterBase< STREAM >
        {
        public:

            typedef ChunksReceiverDeleterBase< STREAM >                                     base_type;
            typedef typename base_type::context_t                                           context_t;
            typedef typename base_type::transfer_task_t                                     transfer_task_t;

        private:

            BL_DECLARE_OBJECT_IMPL( ChunksReceiverT )

        protected:

            ChunksReceiverT(
                SAA_in              const om::ObjPtr< EndpointSelector >&                   endpointSelector,
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                SAA_in              const std::size_t                                       tasksPoolSize = DEFAULT_NO_OF_CONNECTIONS
                )
                :
                base_type( endpointSelector, context, "success:Chunks_Receiver" /* taskName */, fsmd, tasksPoolSize )
            {
            }

            virtual auto getCommandId() const NOEXCEPT -> typename transfer_task_t::CommandId OVERRIDE
            {
                return transfer_task_t::CommandId::ReceiveChunk;
            }

            virtual bool handleReadyTaskImpl( SAA_in const om::ObjPtr< transfer_task_t >& transfer ) OVERRIDE
            {
                const auto& block = transfer -> getChunkData();

                if( block )
                {
                    data::DataChunkBlock chunkInfo;

                    chunkInfo.chunkId = transfer -> getChunkId();
                    BL_ASSERT( uuids::nil() != chunkInfo.chunkId );
                    chunkInfo.data = block;

                    if( ! base_type::notifyOnNext( cpp::any( chunkInfo ) ) )
                    {
                        /*
                         * We could not push the data block into the next processing unit
                         * events queue
                         */

                        return false;
                    }

                    /*
                     * The data was pushed successfully into the next processing unit queue
                     * and thus we must detach it (since we no longer own it in the sense
                     * that the data will be used / read from a different unit). The data
                     * block will be returned in the pool for reuse once it has been
                     * processed and discarded by the last processing unit in the pipeline.
                     */

                    ++base_type::m_totalBlocks;
                    base_type::m_totalDataSize += block -> size();

                    transfer -> detachChunkData();
                    transfer -> setChunkId( uuids::nil() );
                }

                return true;
            }
        };

        typedef ChunksReceiverT< tasks::TcpSocketAsyncBase >        ChunksReceiver;
        typedef ChunksReceiverT< tasks::TcpSslSocketAsyncBase >     SslChunksReceiver;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_CHUNKSRECEIVER_H_ */
