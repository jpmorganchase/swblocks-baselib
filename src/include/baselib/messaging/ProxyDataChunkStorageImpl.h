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

#ifndef __BL_MESSAGING_PROXYDATACHUNKSTORAGEIMPL_H_
#define __BL_MESSAGING_PROXYDATACHUNKSTORAGEIMPL_H_

#include <baselib/messaging/TcpBlockTransferClient.h>
#include <baselib/messaging/DataChunkStorage.h>

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TaskControlToken.h>

#include <baselib/core/OS.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/EndpointSelector.h>
#include <baselib/core/BaseIncludes.h>

#include <unordered_set>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief class ProxyDataChunkStorageImplT - implementation of the data::DataChunkStorage
         * interface for a proxy caching store backed by other persistent store
         */

        template
        <
            typename STREAM
        >
        class ProxyDataChunkStorageImplT :
            public data::DataChunkStorage
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( ProxyDataChunkStorageImplT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( om::Disposable )
                BL_QITBL_ENTRY( data::DataChunkStorage )
            BL_QITBL_END( om::Disposable )

        protected:

            typedef tasks::TcpBlockTransferClientConnectionImpl< STREAM >                           transfer_task_t;
            typedef tasks::TcpConnectionEstablisherConnectorImpl< STREAM >                          connector_t;

            struct ClientWrapperHolder
            {
                om::ObjPtr< transfer_task_t >                                                       connection;
                om::ObjPtr< tasks::Task >                                                           connectionTask;
                std::string                                                                         endpointId;

                ~ClientWrapperHolder() NOEXCEPT
                {
                    if( connection && false == endpointId.empty() )
                    {
                        BL_LOG(
                            Logging::info(),
                            BL_MSG()
                                << "Object "
                                << connection.get()
                                << " is disconnecting from '"
                                << endpointId
                                << "'"
                            );
                    }
                }
            };

            typedef tasks::TaskControlToken                                                         TaskControlToken;

            const om::ObjPtr< EndpointSelector >                                                    m_endpointSelector;
            const om::ObjPtr< EndpointCircularIterator >                                            m_connectIterator;
            const om::ObjPtr< EndpointCircularIterator >                                            m_reconnectIterator;
            const om::ObjPtr< data::DataChunkStorage >                                              m_storage;
            const om::ObjPtr< data::datablocks_pool_type >                                          m_dataBlocksPool;
            const om::ObjPtr< TaskControlToken >                                                    m_controlToken;
            const uuid_t                                                                            m_peerId;
            om::ObjPtrDisposable< tasks::ExecutionQueue >                                           m_workers;

            mutable os::thread_specific_ptr< ClientWrapperHolder >                                  g_tlsClient;
            mutable os::mutex                                                                       m_lock;

            mutable std::unordered_set< uuid_t >                                                    m_cachedChunks;
            mutable os::shared_mutex                                                                m_cacheLock;
            mutable os::mutex                                                                       m_chunkCacheWriteLock;

            ProxyDataChunkStorageImplT(
                SAA_in                  const om::ObjPtr< EndpointSelector >&                       endpointSelector,
                SAA_in_opt              const om::ObjPtr< data::DataChunkStorage >&                 storage = nullptr,
                SAA_in_opt              const om::ObjPtr< data::datablocks_pool_type >&             dataBlocksPool = nullptr,
                SAA_in_opt              const om::ObjPtr< TaskControlToken >&                       controlToken = nullptr
                )
                :
                m_endpointSelector( om::copy( endpointSelector ) ),
                m_connectIterator( endpointSelector -> createIterator() ),
                m_reconnectIterator( endpointSelector -> createIterator() ),
                /*
                 * m_storage can be nullptr which indicates proxy only mode (i.e. no persistent caching)
                 */
                m_storage( om::copy( storage ) ),
                m_dataBlocksPool(
                    dataBlocksPool ?
                        om::copy( dataBlocksPool ) :
                        data::datablocks_pool_type::createInstance()
                    ),
                m_controlToken( om::copy( controlToken ) ),
                m_peerId( uuids::nil() ),
                m_workers(
                    om::lockDisposable(
                        tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                            tasks::ExecutionQueue::OptionKeepNone
                        )
                    )
                )
            {
                if( m_storage )
                {
                    /*
                     * If caching is enabled we set the max load factor to 30%
                     * and pre-allocate the hash table to one million buckets
                     *
                     * This should be enough to avoid rehashing for a while,
                     * but if rehash is needed it would block everything for
                     * a bit
                     */

                    m_cachedChunks.max_load_factor( 0.3f );
                    m_cachedChunks.rehash( 1000000U );
                }
            }

            ~ProxyDataChunkStorageImplT() NOEXCEPT
            {
                BL_RT_ASSERT(
                    nullptr == m_workers,
                    "ProxyDataChunkStorageImplT::~ProxyDataChunkStorageImplT: object has not been disposed properly"
                    );
            }

            void chk4Cancel() const
            {
                if( m_controlToken && m_controlToken -> isCanceled() )
                {
                    BL_CHK_EC_USER_FRIENDLY(
                        asio::error::operation_aborted,
                        BL_MSG()
                            << "Can't connect to blob server because shutdown is already in progress"
                        );
                }
            }

            auto reconnect(
                SAA_in                  const std::exception_ptr&                                   eptr,
                SAA_in                  const std::string&                                          endpointId
                ) const -> om::ObjPtr< connector_t >
            {
                BL_MUTEX_GUARD( m_lock );

                try
                {
                    cpp::safeRethrowException( eptr );
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
                    if( asio::error::operation_aborted == e.code() )
                    {
                        /*
                         * Don't try to retry if the exception is cancellation
                         */

                        throw;
                    }

                    /*
                     * Everything else that comes eh::system_error in this context
                     * we assume is client side connection issue and we can retry
                     */
                }

                om::ObjPtr< connector_t > connector;

                for( ;; )
                {
                    chk4Cancel();

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
                                    << eh::errinfo_nested_exception_ptr( eptr ),
                                BL_MSG()
                                    << "An error has occurred while trying to connect to a blob server node"
                                );
                        }
                    }

                    time::time_duration duration;

                    if( ! m_reconnectIterator -> canRetryNow( &duration ) )
                    {
                        /*
                         * Looks like we must wait for the timeout to pass
                         */

                        os::sleep( duration );
                    }

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Lost connection with a blob server node '"
                            << endpointId
                            << "'; attempting to reconnect now..."
                        );

                    connector =
                        connector_t::createInstance(
                            cpp::copy( m_reconnectIterator -> host() ),
                            m_reconnectIterator -> port()
                            );

                    const auto connectorTask = om::qi< tasks::Task >( connector );

                    m_workers -> push_back( connectorTask );
                    m_workers -> wait( connectorTask );

                    if( connector -> isFailed() )
                    {
                        /*
                         * Move to the next for further reconnect attempts
                         */

                        m_reconnectIterator -> selectNext();
                        continue;
                    }

                    m_reconnectIterator -> resetRetry();
                    break;
                }

                return connector;
            }

            auto createClient( SAA_in ClientWrapperHolder* current ) const -> cpp::SafeUniquePtr< ClientWrapperHolder >
            {
                chk4Cancel();

                om::ObjPtr< connector_t > connector;
                bool initialConnect = false;

                if( current )
                {
                    connector = reconnect( current -> connection -> exception(), current -> endpointId );
                }
                else
                {
                    initialConnect = true;

                    /*
                     * This is the initial connection attempt
                     *
                     * In this case we use m_connectIterator which simply iterates over all
                     * nodes in round robin fashion
                     */

                    std::string         host;
                    unsigned short      port;

                    {
                        BL_MUTEX_GUARD( m_lock );

                        ( void ) m_connectIterator -> selectNext();

                        host = m_connectIterator -> host();
                        port = m_connectIterator -> port();
                    }

                    connector = connector_t::createInstance( std::move( host ), port );

                    const auto connectorTask = om::qi< tasks::Task >( connector );

                    m_workers -> push_back( connectorTask );
                    m_workers -> wait( connectorTask );

                    if( connector -> isFailed() )
                    {
                        connector = reconnect( connector -> exception(), connector -> endpointId() );
                    }
                }

                /*
                 * reconnect(...) only returns a successful connection or throws
                 */

                BL_ASSERT( ! connector -> isFailed() );

                auto endpointId = connector -> endpointId();

                auto client =
                    cpp::SafeUniquePtr< ClientWrapperHolder >::attach( new ClientWrapperHolder );

                client -> connection =
                    transfer_task_t::template createInstance< transfer_task_t >(
                        transfer_task_t::CommandId::NoCommand,
                        m_peerId,
                        m_dataBlocksPool
                        );

                client -> connection -> attachStream( connector -> detachStream() );
                client -> connectionTask = om::qi< tasks::Task >( client -> connection );
                client -> endpointId.swap( endpointId );

                BL_LOG(
                    Logging::info(),
                    BL_MSG()
                        << "Object "
                        << client -> connection.get()
                        << (
                                initialConnect ?
                                    " has connected successfully to '" :
                                    " has re-connected successfully to '"
                            )
                        << client -> endpointId
                        << "'"
                    );

                return client;
            }

            ClientWrapperHolder* tlsClient( SAA_in const bool force = false ) const
            {
                if( force || nullptr == g_tlsClient.get() )
                {
                    g_tlsClient.reset( createClient( g_tlsClient.get() ).release() );
                }

                return g_tlsClient.get();
            }

            void executeCommand(
                SAA_in                  const typename transfer_task_t::CommandId                   commandId,
                SAA_in_opt              const uuid_t&                                               chunkId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&                        data
                ) const
            {
                /*
                 * If local persistent caching is enabled and the command is ReceiveChunk then check
                 * if the chunk is cached and get it from the cache if it is available there
                 *
                 * The SendChunk/RemoveChunk commands do not work with the cache
                 */

                if( m_storage && transfer_task_t::CommandId::ReceiveChunk == commandId )
                {
                    bool inCache = false;

                    {
                        os::shared_lock< decltype( m_cacheLock ) > sharedLock( m_cacheLock );

                        const auto pos = m_cachedChunks.find( chunkId );

                        inCache = ( pos != m_cachedChunks.end() );
                    }

                    if( inCache )
                    {
                        /*
                         * The chunk is already cached - just load it from there and return
                         */

                        BL_ASSERT( m_storage );
                        m_storage -> load( uuids::nil() /* sessionId */, chunkId, data );

                        return;
                    }
                }

                const auto* client = tlsClient();

                for( ;; )
                {
                    /*
                     * Just request the command to be executed on the connection
                     */

                    client -> connection -> setCommandInfoRawPtr( commandId, chunkId, data.get() );

                    m_workers -> push_back( client -> connectionTask );
                    m_workers -> wait( client -> connectionTask );

                    if( client -> connectionTask -> isFailed() )
                    {
                        /*
                         * Force to reconnect if possible and then try again
                         *
                         * If it can't reconnect it will throw and we're going to
                         * exit loop
                         */

                        client = tlsClient( true /* force */ );
                        continue;
                    }

                    break;
                }

                /*
                 * Reset the task to keep things tidy
                 */

                client -> connection -> setCommandInfoRawPtr(
                    transfer_task_t::CommandId::NoCommand,
                    uuids::nil(),
                    nullptr
                    );

                /*
                 * If local persistent caching is enabled and the command is ReceiveChunk then check
                 * to put the chunk data in the cache before it is returned to the caller
                 *
                 * While we are adding it to the cache we should upgrade the shared lock to
                 * unique / exclusive such
                 *
                 * The SendChunk/RemoveChunk commands do not work with the cache
                 */

                if( m_storage && transfer_task_t::CommandId::ReceiveChunk == commandId )
                {
                    bool inCache = false;

                    {
                        os::shared_lock< decltype( m_cacheLock ) > sharedLock( m_cacheLock );

                        const auto pos = m_cachedChunks.find( chunkId );

                        inCache = ( pos != m_cachedChunks.end() );
                    }

                    if( ! inCache )
                    {
                        /*
                         * We are not holding the RW lock while writing the data, but we acquire
                         * another mutex to ensure all writes to the cache are serialized, but
                         * yet not blocking readers on the cache lock (m_cacheLock)
                         *
                         * After we acquire the cache writer lock (which is mutex) we again check
                         * if the chunk was populated in the cache by some other call that was
                         * queued up before us and ignore if it was already populated otherwise we
                         * execute the save operation
                         *
                         * After the save operation is completed only then we acquire the cache
                         * lock again to update it
                         *
                         * Note also that any exceptions that occur during the chunk save
                         * operation will be logged and ignored, so they don't interfere with
                         * the request if for some reason the persistent cache stops working
                         */

                        BL_MUTEX_GUARD( m_chunkCacheWriteLock );

                        {
                            os::shared_lock< decltype( m_cacheLock ) > sharedLock( m_cacheLock );

                            const auto pos = m_cachedChunks.find( chunkId );

                            inCache = ( pos != m_cachedChunks.end() );
                        }

                        if( ! inCache )
                        {
                            utils::tryCatchLog(
                                "Saving chunk in the cache failed with an exception",
                                [ & ]() -> void
                                {
                                    m_storage -> save( uuids::nil() /* sessionId */, chunkId, data );

                                    {
                                        os::unique_lock< decltype( m_cacheLock ) > exclusiveLock( m_cacheLock );

                                        m_cachedChunks.insert( chunkId );
                                    }
                                },
                                [ & ]() -> void
                                {
                                    /*
                                     * If proxy caching is enabled and we fail to write to the cache (due to disk
                                     * space issue or other reason) we don't swallow the error, but re-throw, so
                                     * the server will exit and allow for alerts to be triggered and the issue
                                     * investigated and addressed by operate team
                                     *
                                     * We don't want the proxy server with caching enabled to continue running if
                                     * the local data store cache is unavailable
                                     */

                                    throw;
                                }
                                );
                        }
                    }
                }
            }

        public:

            /*
             * Implementation of data::DataChunkStorage (load/save/remove/flushPeerSessions) +
             * implementation of om::Disposable (dispose)
             */

            virtual void load(
                SAA_in                  const uuid_t&                                               sessionId,
                SAA_in                  const uuid_t&                                               chunkId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&                        data
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );
                BL_ASSERT( chunkId != uuids::nil() );

                executeCommand( transfer_task_t::CommandId::ReceiveChunk, chunkId, data );
            }

            virtual void save(
                SAA_in                  const uuid_t&                                               sessionId,
                SAA_in                  const uuid_t&                                               chunkId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&                        data
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );
                BL_ASSERT( chunkId != uuids::nil() );

                executeCommand( transfer_task_t::CommandId::SendChunk, chunkId, data );
            }

            virtual void remove(
                SAA_in                  const uuid_t&                                               sessionId,
                SAA_in                  const uuid_t&                                               chunkId
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );
                BL_ASSERT( chunkId != uuids::nil() );

                executeCommand( transfer_task_t::CommandId::RemoveChunk, chunkId, nullptr /* data */ );
            }

            virtual void flushPeerSessions( SAA_in const uuid_t& peerId ) OVERRIDE
            {
                BL_UNUSED( peerId );

                /*
                 * NOP for now
                 */
            }

            virtual void dispose() OVERRIDE
            {
                if( m_workers )
                {
                    BL_ASSERT( 0U == m_workers -> getQueueSize( tasks::ExecutionQueue::Pending ) );
                    BL_ASSERT( 0U == m_workers -> getQueueSize( tasks::ExecutionQueue::Executing ) );
                }

                tasks::ExecutionQueue::disposeQueue( m_workers );
            }
        };

        typedef om::ObjectImpl< ProxyDataChunkStorageImplT< tasks::TcpSocketAsyncBase > >           ProxyDataChunkStorageImpl;
        typedef om::ObjectImpl< ProxyDataChunkStorageImplT< tasks::TcpSslSocketAsyncBase > >        SslProxyDataChunkStorageImpl;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_PROXYDATACHUNKSTORAGEIMPL_H_ */
