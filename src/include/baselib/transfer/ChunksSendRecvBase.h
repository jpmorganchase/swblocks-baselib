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

#ifndef __BL_TRANSFER_CHUNKSSENDRECVBASE_H_
#define __BL_TRANSFER_CHUNKSSENDRECVBASE_H_

#include <baselib/transfer/ChunksSendRecvIncludes.h>
#include <baselib/core/ErrorHandling.h>

#include <queue>

namespace bl
{
    namespace transfer
    {
        enum : std::size_t
        {
            DEFAULT_NO_OF_CONNECTIONS = 16
        };

        /**
         * @brief class ChunksSendRecvBase
         *
         * This is the base class for the chunks transmitter and receiver units which will
         * be used to send and receive the chunks over the network respectively
         */

        template
        <
            typename STREAM
        >
        class ChunksSendRecvBase :
            public DataTransferUnitBase< STREAM >
        {
        public:

            typedef DataTransferUnitBase< STREAM >                                          base_type;
            typedef typename base_type::context_t                                           context_t;
            typedef tasks::TcpConnectionEstablisherConnectorImpl< STREAM >                  connector_t;
            typedef tasks::TcpBlockTransferClientConnectionT< STREAM >                      transfer_connection_t;

        protected:

            using base_type::m_eqWorkerTasks;

        private:

            BL_DECLARE_OBJECT_IMPL( ChunksSendRecvBase )

        protected:

            class BlockTransferConnection :
                public transfer_connection_t
            {
                BL_DECLARE_OBJECT_IMPL( BlockTransferConnection )

            protected:

                typedef transfer_connection_t                                                   base_type;

                cpp::ScalarTypeIniter< bool >                                                   m_isBootstrapped;
                std::string                                                                     m_endpointId;

                BlockTransferConnection(
                    SAA_in                  const typename base_type::CommandId                 commandId,
                    SAA_in                  const uuid_t&                                       peerId,
                    SAA_in                  const om::ObjPtr< data::datablocks_pool_type >&     dataBlocksPool
                    )
                    :
                    base_type( commandId, peerId, dataBlocksPool )
                {
                }

            public:

                void markCompleted( SAA_in_opt const std::exception_ptr& exception ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( base_type::m_lock );

                    base_type::m_state = base_type::Completed;
                    base_type::m_exception = exception;

                    BL_NOEXCEPT_END()
                }

                std::string endpointId() const
                {
                    std::string result;

                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( base_type::m_lock );

                    result = m_endpointId;

                    BL_NOEXCEPT_END()

                    return result;
                }

                void endpointId( SAA_in std::string&& endpointId ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( base_type::m_lock );

                    m_endpointId.swap( endpointId );

                    BL_NOEXCEPT_END()
                }

                bool isBootstrapped() const NOEXCEPT
                {
                    return m_isBootstrapped;
                }

                void isBootstrapped( SAA_in const bool isBootstrapped ) NOEXCEPT
                {
                    m_isBootstrapped = isBootstrapped;
                }
            };

            typedef om::ObjectImpl< BlockTransferConnection >                               transfer_task_t;

            const om::ObjPtr< EndpointSelector >                                            m_endpointSelector;
            const om::ObjPtr< EndpointCircularIterator >                                    m_reconnectIterator;
            uuid_t                                                                          m_peerId;

            cpp::ScalarTypeIniter< std::uint64_t >                                          m_totalBlocks;
            cpp::ScalarTypeIniter< std::uint64_t >                                          m_totalDataSize;
            cpp::ScalarTypeIniter< bool >                                                   m_connectionsScheduled;
            cpp::ScalarTypeIniter< bool >                                                   m_statsLogged;
            cpp::ScalarTypeIniter< bool >                                                   m_isDroppedConnection;
            om::ObjPtr< connector_t >                                                       m_reconnectTask;
            std::queue< std::pair< uuid_t, om::ObjPtr< data::DataBlock > > >                m_postponedDataChunks;
            os::mutex                                                                       m_postponedDataChunksLock;

            ChunksSendRecvBase(
                SAA_in              const om::ObjPtr< EndpointSelector >&                   endpointSelector,
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              std::string&&                                           taskName,
                SAA_in              const std::size_t                                       tasksPoolSize = DEFAULT_NO_OF_CONNECTIONS
                )
                :
                base_type( context, BL_PARAM_FWD( taskName ), tasksPoolSize ),
                m_endpointSelector( om::copy( endpointSelector ) ),
                m_reconnectIterator( endpointSelector -> createIterator() ),
                m_peerId( uuids::nil() )
            {
                /*
                 * Note: m_peerId == nil() means tracking peer sessions is disabled by default
                 */
            }

            /**
             * @brief The derived class should determine if it safe to attempt re-connecting if
             * a connection is lost / broken
             *
             * Normally connections which are just reading should be safe to reconnect always
             * (and move to another node), but connection which are pushing out data can only
             * be reconnected if the tracking of peer sessions is disabled
             * (i.e. peerId == uuids::nil())
             */

            virtual bool isSafeToReconnect() const NOEXCEPT = 0;
            virtual void tidyUpTaskOnFailure( SAA_in const om::ObjPtr< transfer_task_t >& transfer ) = 0;
            virtual auto getCommandId() const NOEXCEPT -> typename transfer_task_t::CommandId = 0;

            static void startConnection(
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              const om::ObjPtr< EndpointCircularIterator >&           endpointIterator,
                SAA_in              const om::ObjPtr< tasks::ExecutionQueue >&              eq,
                SAA_inout_opt       om::ObjPtr< connector_t >*                              saveTo = nullptr
                )
            {
                using namespace tasks;

                const auto connector = connector_t::createInstance(
                    cpp::copy( endpointIterator -> host() ),
                    endpointIterator -> port()
                    );

                const auto connectorTask = om::qi< Task >( connector.get() );

                if( saveTo )
                {
                    om::copy( connector ).swap( *saveTo );
                }

                const auto endpointId = connector -> endpointId();

                auto stream = context -> tryGetConnection( endpointId );

                if( stream )
                {
                    /*
                     * We have a ready connection in the connections pool which we can reuse
                     */

                    connector -> attachStream( std::move( stream ) );

                    eq -> push_back( connectorTask, true /* dontSchedule */ );
                }
                else
                {
                    /*
                     * There is no available connection in the connection pool - start
                     * establishing one
                     */

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "ChunksSendRecvBase: begin establishing connection with endpoint "
                            << endpointIterator -> host()
                            << ":"
                            << endpointIterator -> port()
                        );

                    eq -> push_back( connectorTask );
                }
            }

            static void startConnectingTasks(
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              const om::ObjPtr< EndpointCircularIterator >&           endpointIterator,
                SAA_in              const std::size_t                                       tasksPoolSize,
                SAA_in              const om::ObjPtr< tasks::ExecutionQueue >&              eq
                )
            {
                using namespace tasks;

                /*
                 * Begin establishing all connections
                 */

                for( std::size_t i = 0; i < tasksPoolSize; ++i )
                {
                    ( void ) endpointIterator -> selectNext();

                    startConnection( context, endpointIterator, eq );
                }
            }

            om::ObjPtr< tasks::Task > getSuccessfulTopTask( SAA_in const bool force )
            {
                using namespace tasks;

                auto taskTransfer = m_eqWorkerTasks -> top( false /* wait */ );

                if( ! taskTransfer )
                {
                    /*
                     * None of the connections have been established yet or there is
                     * no task ready to accept this chunk (i.e. all tasks are busy)
                     */

                    return nullptr;
                }

                if( force )
                {
                    /*
                     * If force is specified that means we're unwinding due to exception
                     *
                     * No need to check if the task has failed in this case
                     */

                    return taskTransfer;
                }

                /*
                 * If the task is being stopped we don't need to proceed with the remaining
                 * logic and retry / reconnect when the task has failed, etc
                 */

                base_type::chk2ThrowIfStopped();

                /*
                 * Check to throw if the task has failed
                 */

                if( taskTransfer -> isFailed() )
                {
                    const auto exception = taskTransfer -> exception();

                    BL_ASSERT( exception );

                    if( m_reconnectTask )
                    {
                        /*
                         * We're waiting for the top task to be reconnected
                         */

                        BL_ASSERT( m_isDroppedConnection );

                        return nullptr;
                    }

                    if( ! m_isDroppedConnection )
                    {
                        try
                        {
                            cpp::safeRethrowException( exception );
                        }
                        catch( ServerErrorException& )
                        {
                            /*
                             * Server side errors should not be retried
                             */

                            throw;
                        }
                        catch( eh::system_error& e )
                        {
                            if( ! isSafeToReconnect() )
                            {
                                throw;
                            }

                            if( asio::error::operation_aborted == e.code() )
                            {
                                /*
                                 * Don't try to retry if the exception is cancellation
                                 */

                                throw;
                            }

                            /*
                             * Everything that comes eh::system_error in this context
                             * we assume is client side connection issue and we can retry
                             */
                        }

                        m_isDroppedConnection = true;
                    }

                    /*
                     * Here we should try to re-connect and if not possible
                     * then re-throw
                     */

                    if( ! m_reconnectIterator -> canRetry() )
                    {
                        if( ! m_reconnectIterator -> selectNext() )
                        {
                            /*
                             * The retry count has been exhausted for all nodes - time to bail
                             */

                            BL_THROW(
                                ServerNoConnectionException()
                                    << eh::errinfo_nested_exception_ptr( exception ),
                                BL_MSG()
                                    << "An error has occurred while trying to connect to a blob server node"
                                );
                        }
                    }

                    if( ! m_reconnectIterator -> canRetryNow() )
                    {
                        /*
                         * Looks like we must wait for the timeout to pass
                         */

                        return nullptr;
                    }

                    const auto transfer = om::qi< transfer_task_t >( taskTransfer );

                    /*
                     * The transfer task whose connection was dropped cannot be treated as authenticated anymore.
                     */

                    transfer -> isAuthenticated( false );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Lost connection with a blob server node '"
                            << transfer -> endpointId()
                            << "'; attempting to reconnect now..."
                        );

                    startConnection(
                        base_type::m_context,
                        m_reconnectIterator,
                        base_type::m_eqChildTasks,
                        &m_reconnectTask /* saveTo */
                        );

                    /*
                     * Move to the next for further reconnect attempts
                     */

                    m_reconnectIterator -> selectNext();

                    /*
                     * Notify the caller that (s)he must wait now
                     */

                    return nullptr;
                }

                return taskTransfer;
            }

            virtual bool canAcceptReadyTask() OVERRIDE
            {
                /*
                 * We are always ready to accept the connected sockets as they come connected
                 */

                return true;
            }

            void tidyUpTransferTaskOnFailure( SAA_in const om::ObjPtr< transfer_task_t >& transfer )
            {
                if( transfer -> getBlockType() == tasks::BlockTransferDefs::BlockType::Authentication )
                {
                    transfer -> setChunkId( uuids::nil() );

                    return;
                }

                tidyUpTaskOnFailure( transfer );
            }

            virtual bool pushReadyTask( SAA_in const om::ObjPtrCopyable< tasks::Task >& task ) OVERRIDE
            {
                using namespace tasks;

                if( om::areEqual( m_reconnectTask, task.get() ) )
                {
                    if( ! m_reconnectTask -> isFailed() )
                    {
                        m_reconnectIterator -> resetRetry();

                        BL_LOG(
                            Logging::info(),
                            BL_MSG()
                                << "Re-connected successfully to '"
                                << m_reconnectTask -> endpointId()
                                << "'"
                            );

                        auto taskTransfer = m_eqWorkerTasks -> top( false /* wait */ );

                        BL_ASSERT( taskTransfer );
                        BL_ASSERT( taskTransfer -> exception() );

                        const auto transfer = om::qi< transfer_task_t >( taskTransfer );

                        transfer -> attachStream( m_reconnectTask -> detachStream() );
                        transfer -> endpointId( m_reconnectTask -> endpointId() );

                        if( transfer -> isBootstrapped() )
                        {
                            /*
                             * If the task is already bootstrapped it either has already
                             * data attached to it which it wasn't able to transmit if
                             * we're sending data or invalid data if we're receiving data
                             *
                             * To handle the second case we need to call tidyUpTaskOnFailure()
                             * to discard the invalid data and reset the data block pointer
                             * otherwise when we reschedule it we will send invalid data
                             * instead of requesting download which will corrupt the blob store
                            *
                             * We also need to reschedule the command id before the task is
                             * re-started because the command id is always reset back to
                             * NoCommand upon task completion
                             *
                             * In this case we need to reschedule it
                             */

                            tidyUpTransferTaskOnFailure( transfer );

                            if( isAuthenticationRequired() )
                            {
                                scheduleAuthenticationTask( transfer );
                            }
                            else
                            {
                                m_eqWorkerTasks -> push_back( taskTransfer );
                            }
                        }
                        else
                        {
                            transfer -> markCompleted( nullptr );
                            transfer -> isBootstrapped( true );
                        }

                        m_isDroppedConnection = false;
                    }

                    m_reconnectTask.reset();

                    return true;
                }

                /*
                 * Just steal the connected socket and create a block transfer task
                 *
                 * If the connection task has succeeded we mark the task as bootstrapped
                 * (i.e. connected successfully initially)
                 *
                 * If the connection task has failed we save the exception pointer, so
                 * it will be retried according to the general reconnect logic
                 *
                 * We also remember the endpoint, so we can use it for logging purpose
                 */

                const auto establishedConnection = om::qi< connector_t >( task );
                const auto isFailedConnection = establishedConnection -> isFailed();

                const auto transfer =
                    transfer_task_t::template createInstance< transfer_task_t >(
                        transfer_task_t::CommandId::NoCommand,
                        m_peerId,
                        base_type::m_context -> dataBlocksPool()
                        );

                transfer -> attachStream( establishedConnection -> detachStream() );
                transfer -> endpointId( establishedConnection -> endpointId() );


                if( isFailedConnection )
                {
                    const auto exception = establishedConnection -> exception();

                    BL_ASSERT( exception );
                    transfer -> markCompleted( exception );
                }
                else
                {
                    transfer -> isBootstrapped( true );
                }

                if( isFailedConnection || ! isAuthenticationRequired() )
                {
                    m_eqWorkerTasks -> push_back( om::qi< Task >( transfer ), true /* dontSchedule */ );
                }
                else
                {
                    scheduleAuthenticationTask( transfer );
                }

                return true;
            }

            virtual bool allowPushingOfFailedChildTasks() const NOEXCEPT OVERRIDE
            {
                /*
                 * We're going to handle the failures in pushReadyTask()
                 */

                return true;
            }

            virtual time::time_duration chk2LoopUntilShutdownFinished() NOEXCEPT OVERRIDE
            {
                const auto duration = base_type::chk2LoopUntilShutdownFinished();

                if( duration.is_special() )
                {
                    logStats();
                }

                return duration;
            }

            om::ObjPtr< transfer_task_t > chk2ReturnChunkInThePool(
                SAA_in              const om::ObjPtr< tasks::Task >&                        taskTransfer
                )
            {
                auto transfer = om::qi< transfer_task_t >( taskTransfer );

                /*
                 * Check to return the old chunk into the pool (if any)
                 */

                auto oldChunk = transfer -> detachChunkData();

                if( oldChunk )
                {
                    /*
                     * This chunk was successfully transmitted
                     */

                    if( transfer -> getBlockType() != tasks::BlockTransferDefs::BlockType::Authentication )
                    {
                        ++m_totalBlocks;
                        m_totalDataSize += oldChunk -> size();
                        base_type::m_context -> dataBlocksPool() -> put( std::move( oldChunk ) );
                    }
                }

                return transfer;
            }

            bool isAuthenticationRequired() const
            {
                return base_type::m_context -> getAuthenticationDataBlock() != nullptr;
            }

            void scheduleAuthenticationTask( SAA_in const om::ObjPtr< transfer_task_t >& transfer )
            {
                BL_ASSERT( transfer );
                BL_ASSERT( ! transfer -> isAuthenticated() );
                BL_ASSERT( isAuthenticationRequired() );

                using namespace tasks;

                if( transfer -> getChunkId() != uuids::nil() )
                {
                    /*
                     * If we are here it means that the previous connection was dropped and the
                     * transfer task uses a new one, which has to be authenticated again.
                     * Unfortunately, the transfer task already contains the payload - we need
                     * to store it in temporary data structure in order to send authentication request
                     * first. The payload will be picked up later and retransmittted. Note: for send
                     * request both chunkId and chunkData are populated, the latter is not for delete
                     * request.
                     */

                    BL_MUTEX_GUARD( m_postponedDataChunksLock );

                    m_postponedDataChunks.emplace(
                        cpp::copy( transfer -> getChunkId() ),
                        om::copy( transfer -> getChunkData() )
                        );
                }

                transfer -> clientVersion(
                    tasks::detail::CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2
                    );

                transfer -> setCommandInfo(
                    transfer_task_t::CommandId::SendChunk,
                    uuids::nil() /* chunkId */,
                    om::copy( base_type::m_context -> getAuthenticationDataBlock() ),
                    BlockTransferDefs::BlockType::Authentication
                    );

                base_type::m_eqWorkerTasks -> push_back( om::qi< Task >( transfer ) );
            }

        public:

            void enablePeerSessionsTracking() NOEXCEPT
            {
                BL_ASSERT( tasks::Task::Created == base_type::m_state );

                m_peerId = uuids::create();
            }

            void logStats() NOEXCEPT
            {
                if( m_statsLogged )
                {
                    return;
                }

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "\nChunksSendRecvBase::logStats():\n"
                        << "\n    taskName: "
                        << base_type::m_name
                        << "\n    totalBlocks: "
                        << m_totalBlocks
                        << "\n    totalDataSize: "
                        << m_totalDataSize
                        << "\n"
                    );

                m_statsLogged = true;
            }
        };

    } // transfer

} // bl

#endif /* __BL_TRANSFER_CHUNKSSENDRECVBASE_H_ */
