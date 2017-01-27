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

#ifndef __BL_TRANSFER_CHUNKSTRANSMITTER_H_
#define __BL_TRANSFER_CHUNKSTRANSMITTER_H_

#include <baselib/transfer/ChunksSendRecvBase.h>
#include <baselib/transfer/ChunksSendRecvIncludes.h>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class ChunksTransmitter
         *
         * This is the chunks transmitter which will send the chunks over the network
         *
         * It will delay connecting until the first chunk arrives and then start pushing
         * out the chunks directly as they arrive
         */

        template
        <
            typename STREAM
        >
        class ChunksTransmitterT :
            public ChunksSendRecvBase< STREAM >
        {
        public:

            typedef ChunksSendRecvBase< STREAM >                                            base_type;
            typedef typename base_type::context_t                                           context_t;
            typedef typename base_type::transfer_task_t                                     transfer_task_t;

        private:

            BL_DECLARE_OBJECT_IMPL( ChunksTransmitterT )

        protected:

            cpp::ScalarTypeIniter< bool >                                                   m_sessionsFlushRequested;
            const om::ObjPtr< data::FilesystemMetadataWO >                                  m_fsmd;

            ChunksTransmitterT(
                SAA_in              const om::ObjPtr< EndpointSelector >&                   endpointSelector,
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              const om::ObjPtr< data::FilesystemMetadataWO >&         fsmd,
                SAA_in              const std::size_t                                       tasksPoolSize = DEFAULT_NO_OF_CONNECTIONS
                )
                :
                base_type( endpointSelector, context, "success:Chunk_Transmitter" /* taskName */, tasksPoolSize ),
                m_fsmd( om::copy( fsmd ) )
            {
            }

            virtual auto getCommandId() const NOEXCEPT -> typename transfer_task_t::CommandId OVERRIDE
            {
                return transfer_task_t::CommandId::SendChunk;
            }

            virtual bool isSafeToReconnect() const NOEXCEPT OVERRIDE
            {
                /*
                 * It is only safe to reconnect when the tracking of peer sessions
                 * code is disabled (i.e. uuids::nil() == base_type::m_peerId)
                 *
                 * See description of ChunksSendRecvBase::isSafeToReconnect()
                 * in the base class
                 */

                return ( uuids::nil() == base_type::m_peerId );
            }

            virtual void tidyUpTaskOnFailure( SAA_in const om::ObjPtr< transfer_task_t >& transfer ) OVERRIDE
            {
                /*
                 * Just reset the command id back to what is expected to be upon re-schedule
                 */

                if( m_sessionsFlushRequested )
                {
                    BL_ASSERT( transfer -> getChunkId() == uuids::nil() );
                    BL_ASSERT( ! transfer -> getChunkData() );

                    transfer -> setCommandId( transfer_task_t::CommandId::FlushPeerSessions );
                }
                else
                {
                    BL_ASSERT( transfer -> getChunkId() != uuids::nil() );
                    BL_ASSERT( transfer -> getChunkData() );

                    transfer -> setCommandId( getCommandId() );
                }
            }

            void chk2BeginConnecting()
            {
                base_type::chk2ThrowIfStopped();

                if( ! base_type::m_connectionsScheduled )
                {
                    base_type::startConnectingTasks(
                        base_type::m_context,
                        base_type::m_endpointSelector -> createIterator(),
                        base_type::m_tasksPoolSize,
                        base_type::m_eqChildTasks
                        );

                    base_type::m_connectionsScheduled = true;
                }
            }

            bool chk2ScheduleBlockForTransfer( SAA_in_opt const cpp::any* value )
            {
                using namespace tasks;

                const auto taskTransfer = base_type::getSuccessfulTopTask( false /* force */ );

                if( ! taskTransfer )
                {
                    /*
                     * None of the connections have been established yet or there is
                     * no task ready to accept this chunk (i.e. all tasks are busy)
                     */

                    return false;
                }

                auto result = true;

                const auto transfer = base_type::chk2ReturnChunkInThePool( taskTransfer );

                if( value )
                {
                    /*
                     * Set the new chunk and the chunk id, convert to
                     * network byte order and schedule it
                     */

                    BL_MUTEX_GUARD( base_type::m_postponedDataChunksLock );

                    if( base_type::m_postponedDataChunks.empty() )
                    {
                        auto chunkInfo = cpp::any_cast< data::DataChunkBlock >( *value );

                        transfer -> setCommandInfo(
                            getCommandId(),
                            chunkInfo.chunkId,
                            chunkInfo.data.detachAsUnique()
                            );
                    }
                    else
                    {
                        auto& failedDataChunk = base_type::m_postponedDataChunks.front();

                        transfer -> setCommandInfo(
                            getCommandId(),
                            failedDataChunk.first,
                            om::copy( failedDataChunk.second )
                            );
                        base_type::m_postponedDataChunks.pop();
                        result = false;
                    }
                }
                else
                {
                    /*
                     * value is nullptr which indicates that sending flush command was requested
                     */

                    transfer -> setCommandInfo( transfer_task_t::CommandId::FlushPeerSessions );
                }

                base_type::m_eqWorkerTasks -> push_back( taskTransfer );

                return result;
            }

            void unwindWorkerTasks( SAA_in const bool force )
            {
                if( force && false == base_type::m_eqWorkerTasks -> isEmpty() )
                {
                    base_type::m_eqWorkerTasks -> cancelAll( false /* wait */ );
                }

                for( ;; )
                {
                    const auto taskTransfer = base_type::getSuccessfulTopTask( force );

                    if( taskTransfer )
                    {
                        BL_VERIFY( base_type::m_eqWorkerTasks -> pop( false /* wait */ ) );

                        if(
                            base_type::m_peerId != uuids::nil() &&
                            base_type::m_eqWorkerTasks -> isEmpty() &&
                            false == force &&
                            false == m_sessionsFlushRequested
                            )
                        {
                            /*
                             * Request flush of the session after all tasks have completed, but we
                             * only want to do this if we're not unwinding due to an error
                             */

                            base_type::m_eqWorkerTasks -> push_back( taskTransfer, true /* dontSchedule */ );
                            BL_VERIFY( chk2ScheduleBlockForTransfer( nullptr /* request session flush */ ) );
                            m_sessionsFlushRequested = true;
                        }
                        else
                        {
                            /*
                             * Return the connection and the data block in the respective pool
                             * for further reuse
                             *
                             * Note that the connection is returned only if/when the task is
                             * not in failed state
                             */

                            const auto transfer = base_type::chk2ReturnChunkInThePool( taskTransfer );

                            if( ! transfer -> isFailed() )
                            {
                                const auto endpointId = transfer -> endpointId();
                                base_type::m_context -> putConnection( endpointId, transfer -> detachStream() );
                            }
                        }

                        continue;
                    }

                    break;
                }

                if( ! m_fsmd -> isFinalized() )
                {
                    /*
                     * At this point we know that the session is flushed, so we can commit
                     * the data in the filesystem metadata store
                     */

                    m_fsmd -> finalize();
                }
            }

            virtual bool flushAllPendingTasks() OVERRIDE
            {
                /*
                 * Just flush out all packaging tasks
                 */

                unwindWorkerTasks( base_type::isFailedOrFailing() /* force */ );

                return base_type::m_eqWorkerTasks -> isEmpty();
            }

        public:

            /*
             * A connector point for the chunk arrival events
             */

            bool onChunkArrived( SAA_in const cpp::any& value )
            {
                BL_MUTEX_GUARD( base_type::m_lock );

                if( ! base_type::verifyInputInternal( value ) )
                {
                    return false;
                }

                chk2BeginConnecting();

                return chk2ScheduleBlockForTransfer( &value );
            }
        };

        typedef ChunksTransmitterT< tasks::TcpSocketAsyncBase >         ChunksTransmitter;
        typedef ChunksTransmitterT< tasks::TcpSslSocketAsyncBase >      SslChunksTransmitter;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_CHUNKSTRANSMITTER_H_ */
