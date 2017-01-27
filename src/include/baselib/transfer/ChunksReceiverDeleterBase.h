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

#ifndef __BL_TRANSFER_CHUNKSRECEIVERDELETERBASE_H_
#define __BL_TRANSFER_CHUNKSRECEIVERDELETERBASE_H_

#include <baselib/transfer/ChunksSendRecvBase.h>
#include <baselib/transfer/ChunksSendRecvIncludes.h>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class ChunksReceiverDeleterBase
         *
         * This is the base class for the receiver and deleter units both of which will
         * have very similar functionality
         *
         * The input is implicit and the connecting will start immediately when the
         * observable is scheduled for execution
         */

        template
        <
            typename STREAM
        >
        class ChunksReceiverDeleterBase :
            public ChunksSendRecvBase< STREAM >
        {
        public:

            typedef ChunksSendRecvBase< STREAM >                                            base_type;
            typedef typename base_type::context_t                                           context_t;
            typedef typename base_type::transfer_task_t                                     transfer_task_t;

        protected:

            using base_type::m_eqWorkerTasks;

        private:

            BL_DECLARE_OBJECT_IMPL( ChunksReceiverDeleterBase )

            /**
             * @brief Chunks scheduler task
             */

            class ChunksScheduler :
                public tasks::SimpleTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( ChunksScheduler )

            protected:

                enum
                {
                    MAX_CHUNKS_QUEUE_SIZE = 10 * 1024,
                };

                const om::ObjPtr< context_t >                                               m_context;
                const om::ObjPtr< data::FilesystemMetadataRO >                              m_fsmd;
                const std::size_t                                                           m_tasksPoolSize;
                om::ObjPtr< EndpointCircularIterator >                                      m_endpointIterator;

                om::ObjPtr< UuidIterator >                                                  m_chunksIterator;
                cpp::circular_buffer< uuid_t >                                              m_chunksQueue;

                ChunksScheduler(
                    SAA_in          const om::ObjPtr< context_t >&                          context,
                    SAA_in          const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                    SAA_in          const std::size_t                                       tasksPoolSize,
                    SAA_in          const om::ObjPtr< EndpointCircularIterator >&           endpointIterator
                    )
                    :
                    m_context( om::copy( context ) ),
                    m_fsmd( om::copy( fsmd ) ),
                    m_tasksPoolSize( tasksPoolSize ),
                    m_endpointIterator( om::copy( endpointIterator ) ),
                    m_chunksQueue( MAX_CHUNKS_QUEUE_SIZE )
                {
                }

                virtual void onExecute() NOEXCEPT OVERRIDE
                {
                    om::ObjPtr< EndpointCircularIterator >                                  endpointIterator;
                    om::ObjPtr< tasks::ExecutionQueue >                                     eq;

                    BL_TASKS_HANDLER_BEGIN()

                    if( ! isCanceled() )
                    {
                        if( m_endpointIterator )
                        {
                            /*
                             * Called for the first time, so before we do anything we must
                             * first schedule the connect tasks
                             */

                            endpointIterator.swap( m_endpointIterator );
                            eq = om::copy( m_eq.get() );
                        }
                        else
                        {
                            if( ! m_chunksIterator )
                            {
                                m_chunksIterator = m_fsmd -> queryAllChunks();
                            }

                            /*
                             * Try to schedule as much as possible for this slice
                             */

                            while( false == m_chunksQueue.full() && m_chunksIterator -> hasCurrent() )
                            {
                                m_chunksQueue.push_back( m_chunksIterator -> current() );

                                m_chunksIterator -> loadNext();
                            }
                        }
                    }

                    BL_TASKS_HANDLER_END_NOTREADY()

                    BL_TASKS_HANDLER_BEGIN_NOLOCK()

                    /*
                     * Now we're not holding the lock and we can push the connecting tasks
                     * into the queue
                     */

                    if( ( ! isCanceled() ) && endpointIterator )
                    {
                        ChunksSendRecvBase< STREAM >::startConnectingTasks( m_context, endpointIterator, m_tasksPoolSize, eq );
                    }

                    BL_TASKS_HANDLER_END()
                }

            public:

                bool hasMoreWork() const NOEXCEPT
                {
                    if( nullptr == m_chunksIterator )
                    {
                        return true;
                    }

                    return ( m_chunksIterator -> hasCurrent() || false == m_chunksQueue.empty() );
                }

                cpp::circular_buffer< uuid_t >& chunksQueue() NOEXCEPT
                {
                    return m_chunksQueue;
                }
            };

            typedef om::ObjectImpl< ChunksScheduler > ChunksSchedulerImpl;

        protected:

            cpp::ScalarTypeIniter< bool >                                                   m_sessionsFlushRequested;
            const om::ObjPtr< data::FilesystemMetadataRO >                                  m_fsmd;
            om::ObjPtr< ChunksSchedulerImpl >                                               m_scheduler;

            ChunksReceiverDeleterBase(
                SAA_in              const om::ObjPtr< EndpointSelector >&                   endpointSelector,
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              std::string&&                                           taskName,
                SAA_in              const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                SAA_in              const std::size_t                                       tasksPoolSize = DEFAULT_NO_OF_CONNECTIONS
                )
                :
                base_type( endpointSelector, context, std::forward< std::string >( taskName ), tasksPoolSize ),
                m_fsmd( om::copy( fsmd ) )
            {
            }

            virtual bool handleReadyTaskImpl( SAA_in const om::ObjPtr< transfer_task_t >& transfer ) = 0;

            bool handleReadyTask( SAA_in const om::ObjPtr< transfer_task_t >& transfer )
            {
                if( transfer -> getBlockType() == tasks::BlockTransferDefs::BlockType::Authentication )
                {
                    base_type::chk2ReturnChunkInThePool( om::qi< tasks::Task >( transfer ) );

                    return true;
                }

                return handleReadyTaskImpl( transfer );
            }

            virtual bool isSafeToReconnect() const NOEXCEPT OVERRIDE
            {
                /*
                 * It is always safe to reconnect when receiving or deleting data
                 *
                 * See description of ChunksSendRecvBase::isSafeToReconnect()
                 * in the base class
                 */

                return true;
            }

            virtual void tidyUpTaskOnFailure( SAA_in const om::ObjPtr< transfer_task_t >& transfer ) OVERRIDE
            {
                const auto commandId = this -> getCommandId();

                if( transfer_task_t::CommandId::ReceiveChunk == commandId )
                {
                    /*
                     * Just clear up the chunk data and return it to the pool
                     */

                    auto oldChunk = transfer -> detachChunkData();

                    if( oldChunk )
                    {
                        base_type::m_context -> dataBlocksPool() -> put( std::move( oldChunk ) );
                    }
                }

                BL_ASSERT( ! transfer -> getChunkData() );

                if( m_sessionsFlushRequested )
                {
                    BL_ASSERT( transfer -> getChunkId() == uuids::nil() );
                    transfer -> setCommandId( transfer_task_t::CommandId::FlushPeerSessions );
                }
                else
                {
                    BL_ASSERT( transfer -> getChunkId() != uuids::nil() );
                    transfer -> setCommandId( commandId );
                }
            }

            om::ObjPtr< transfer_task_t > waitForReadyTask(
                SAA_in              const bool                                              force,
                SAA_inout           om::ObjPtr< tasks::Task >&                              taskTransfer
                )
            {
                using namespace tasks;

                om::ObjPtr< transfer_task_t > transfer;

                taskTransfer = base_type::getSuccessfulTopTask( force );

                if( force || ( ! taskTransfer ) )
                {
                    /*
                     * None of the connections have been established yet or there is
                     * no task ready to accept this chunk (i.e. all tasks are busy)
                     *
                     * Or the force parameter is set to true to imply that the tasks
                     * are being unwound because of an exception
                     */

                    return transfer;
                }

                transfer = om::qi< transfer_task_t >( taskTransfer );

                if( ! handleReadyTask( transfer ) )
                {
                    transfer.reset();
                }

                return transfer;
            }

            bool chk2ScheduleChunkForDownload( SAA_in const uuid_t& chunkId )
            {
                using namespace tasks;

                om::ObjPtr< Task > taskTransfer;
                const auto transfer = waitForReadyTask( false /* force */, taskTransfer );

                if( ! transfer )
                {
                    /*
                     * No task available to pick up the new request
                     */

                    return false;
                }

                /*
                 * Re-schedule the task for the next chunk download and return true
                 */

                auto result = true;

                {
                    BL_MUTEX_GUARD( base_type::m_postponedDataChunksLock );

                    if( base_type::m_postponedDataChunks.empty() )
                    {
                        transfer -> setCommandInfo( this -> getCommandId(), chunkId );
                    }
                    else
                    {
                        auto& failedDataChunk = base_type::m_postponedDataChunks.front();
                        transfer -> setCommandInfo( this -> getCommandId(), failedDataChunk.first );
                        base_type::m_postponedDataChunks.pop();

                        result = false;
                    }
                }

                m_eqWorkerTasks -> push_back( taskTransfer );

                return result;
            }

            void scheduleSessionFlushTask()
            {
                const auto taskTransfer = m_eqWorkerTasks -> top( false /* wait */ );
                BL_ASSERT( taskTransfer );

                auto transfer = om::qi< transfer_task_t >( taskTransfer );
                BL_ASSERT( ! transfer -> getChunkData() );

                /*
                 * Re-schedule the task for the next chunk download and return true
                 */

                transfer -> setCommandInfo( transfer_task_t::CommandId::FlushPeerSessions );
                m_eqWorkerTasks -> push_back( taskTransfer );
            }

            void unwindWorkerTasks( SAA_in const bool force )
            {
                if( force && false == m_eqWorkerTasks -> isEmpty() )
                {
                    m_eqWorkerTasks -> cancelAll( false /* wait */ );
                }

                for( ;; )
                {
                    om::ObjPtr< tasks::Task > taskTransfer;
                    const auto transfer = waitForReadyTask( force, taskTransfer );

                    if( ! transfer )
                    {
                        break;
                    }

                    const auto taskRemoved = m_eqWorkerTasks -> pop( false /* wait */ );
                    BL_ASSERT( om::areEqual( taskRemoved, taskTransfer ) );

                    if(
                        m_eqWorkerTasks -> isEmpty() &&
                        false == force &&
                        false == m_sessionsFlushRequested
                        )
                    {
                        /*
                         * Request flush of the session after all tasks have completed, but we
                         * only want to do this if we're not unwinding due to an error
                         */

                        m_eqWorkerTasks -> push_back( taskRemoved, true /* dontSchedule */ );

                        scheduleSessionFlushTask();
                        m_sessionsFlushRequested = true;
                    }
                    else
                    {
                        if( ! transfer -> isFailed() )
                        {
                            /*
                             * If the task has not been in failed state we want to
                             * return the connection in the pool for further reuse
                             */

                            const auto endpointId = transfer -> endpointId();
                            base_type::m_context -> putConnection( endpointId, transfer -> detachStream() );
                        }
                    }
                }
            }

            virtual om::ObjPtr< tasks::Task > createSeedingTask() OVERRIDE
            {
                typedef ChunksSchedulerImpl scheduler_t;

                /*
                 * The only seeding task is the scheduler which will keep pumping
                 * chunk download requests until we're done. We also need to
                 * remember the scheduler task, so we can differentiate it from
                 * the connection tasks in the pushReadyTask( ... ) call below
                 */

                m_scheduler = scheduler_t::template createInstance< scheduler_t >(
                    base_type::m_context,
                    m_fsmd,
                    base_type::m_tasksPoolSize,
                    base_type::m_endpointSelector -> createIterator()
                    );

                return om::qi< tasks::Task >( m_scheduler );
            }

            virtual bool pushReadyTask( SAA_in const om::ObjPtrCopyable< tasks::Task >& task ) OVERRIDE
            {
                if( ! om::areEqual( m_scheduler, task.get() ) )
                {
                    /*
                     * If this is not the scheduler task just call the base
                     */

                    return base_type::pushReadyTask( task );
                }

                /*
                 * If we're here that means this is the scheduler task
                 *
                 * We must process as many chunks as we can and then re-schedule
                 * if necessary
                 */

                if( m_scheduler -> isCanceled() )
                {
                    /*
                     * The task was canceled
                     */

                    return true;
                }

                auto& queue = m_scheduler -> chunksQueue();

                for( ;; )
                {
                    if( queue.empty() )
                    {
                        break;
                    }

                    const auto& chunkId = queue.front();

                    if( chk2ScheduleChunkForDownload( chunkId ) )
                    {
                        queue.pop_front();

                        continue;
                    }

                    /*
                    * Cannot schedule more chunks to be downloaded now
                    */

                    break;
                }

                if( ! m_scheduler -> hasMoreWork() )
                {
                    /*
                     * Return true to indicate that we're done and the
                     * scheduler needs not be re-scheduled further
                     */

                    base_type::m_inputDisconnected = true;

                    return true;
                }

                /*
                 * Return false here since the scheduler has more work to do,
                 * so we need to request for it will be re-scheduled again
                 */

                return false;
            }

            virtual bool flushAllPendingTasks() OVERRIDE
            {
                /*
                 * Just flush out all packaging tasks
                 */

                unwindWorkerTasks( base_type::isFailedOrFailing() /* force */ );

                return m_eqWorkerTasks -> isEmpty();
            }
        };

    } // transfer

} // bl

#endif /* __BL_TRANSFER_CHUNKSRECEIVERDELETERBASE_H_ */
