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

#ifndef __BL_MESSAGING_PROXYBROKERBACKENDPROCESSINGFACTORY_H_
#define __BL_MESSAGING_PROXYBROKERBACKENDPROCESSINGFACTORY_H_

#include <baselib/messaging/ForwardingBackendProcessingFactory.h>
#include <baselib/messaging/MessagingUtils.h>
#include <baselib/messaging/MessagingClientObject.h>
#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/BrokerErrorCodes.h>
#include <baselib/messaging/AsyncBlockDispatcher.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>
#include <baselib/messaging/MessagingClientFactory.h>

#include <baselib/tasks/TasksUtils.h>

#include <baselib/data/models/JsonMessaging.h>
#include <baselib/data/DataBlock.h>

#include <baselib/core/LoggableCounter.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        namespace detail
        {
            /**
             * @brief class ProxyBrokerBackendProcessing - an implementation of a messaging broker proxy backend
             */

            template
            <
                typename STREAM
            >
            class ProxyBrokerBackendProcessing :
                public BackendProcessingBase,
                public AcceptorNotify
            {
                BL_DECLARE_OBJECT_IMPL( ProxyBrokerBackendProcessing )

                BL_QITBL_BEGIN()
                    BL_QITBL_ENTRY_CHAIN_BASE( BackendProcessingBase )
                    BL_QITBL_ENTRY( AcceptorNotify )
                BL_QITBL_END( BackendProcessing )

            public:

                enum : long
                {
                    ASSOCIATE_PEERID_TIMER_IN_SECONDS = 5L,
                };

                typedef data::DataBlock                                                     DataBlock;

                typedef tasks::Task                                                         Task;
                typedef tasks::ExternalCompletionTaskImpl                                   ExternalCompletionTaskImpl;

                typedef tasks::ExecutionQueue                                               ExecutionQueue;
                typedef tasks::ExecutionQueueImpl                                           ExecutionQueueImpl;

                typedef MessagingClientBlockDispatch                                        block_dispatch_t;
                typedef AsyncBlockDispatcher                                                dispatcher_t;

                typedef std::vector< om::ObjPtrDisposable< MessagingClientBlock > >         block_clients_list_t;
                typedef RotatingMessagingClientBlockDispatch                                block_dispatch_impl_t;

                typedef MessagingClientFactory< STREAM >                                    client_factory_t;
                typedef typename client_factory_t::async_wrapper_t                          async_wrapper_t;

            protected:

                template
                <
                    typename E2 = void
                >
                class SharedStateT : public om::DisposableObjectBase
                {
                protected:

                    typedef SharedStateT< E2 >                                              this_type;

                    typedef LoggableCounterDefaultImpl                                      counter_object_t;

                    enum : std::size_t
                    {
                        /*
                         * Caching of 5MB of small blocks per backend instance seems like
                         * an appropriate default
                         */

                        MAX_MEMORY_FOR_ASSOCIATE_BLOCKS_IN_KB = 5U * 1024U,
                    };

                    enum : long
                    {
                        PRUNE_CHECK_INTERVAL_IN_SECONDS = 2L * ASSOCIATE_PEERID_TIMER_IN_SECONDS,
                    };

                    enum : long
                    {
                        PRUNE_INTERVAL_IN_SECONDS = 3L * PRUNE_CHECK_INTERVAL_IN_SECONDS,
                    };

                    struct ClientInfo
                    {
                        std::unordered_set< uuid_t >                                        configuredChannelIds;
                    };

                    template
                    <
                        typename E3 = void
                    >
                    class DataBlockReferenceT : public om::Object
                    {
                        BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( DataBlockReferenceT, om::Object )

                    protected:

                        DataBlockReferenceT(
                            SAA_in          om::ObjPtr< data::datablocks_pool_type >&&      dataBlocksPool,
                            SAA_in          om::ObjPtr< counter_object_t >&&                objectsCounter
                            ) NOEXCEPT
                            :
                            m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool) ),
                            m_objectsCounter( BL_PARAM_FWD( objectsCounter) )
                        {
                        }

                        ~DataBlockReferenceT() NOEXCEPT
                        {
                            dehydrate();
                        }

                    public:

                        /*
                         * freed( ... ) getter/setter is the interface to make the object pool-able
                         */

                        bool freed() const NOEXCEPT
                        {
                            return m_freed;
                        }

                        void freed( SAA_in const bool freed ) NOEXCEPT
                        {
                            m_freed = freed;
                        }

                        auto dataBlock() const NOEXCEPT -> const om::ObjPtr< DataBlock >&
                        {
                            return m_dataBlock;
                        }

                        void hydrate( SAA_in om::ObjPtr< DataBlock >&& dataBlock ) NOEXCEPT
                        {
                            dehydrate();

                            m_dataBlock = BL_PARAM_FWD( dataBlock );
                        }

                        void dehydrate() NOEXCEPT
                        {
                            if( m_dataBlock )
                            {
                                m_dataBlocksPool -> put( std::move( m_dataBlock ) );
                                m_dataBlock.reset();
                                ( void ) m_objectsCounter -> decrement();
                            }
                        }

                        const om::ObjPtr< data::datablocks_pool_type >                      m_dataBlocksPool;
                        const om::ObjPtr< counter_object_t >                                m_objectsCounter;
                        om::ObjPtr< DataBlock >                                             m_dataBlock;
                        cpp::ScalarTypeIniter< bool >                                       m_freed;
                    };

                    typedef om::ObjectImpl< DataBlockReferenceT<> >                         DataBlockReference;
                    typedef om::ObjPtrCopyable< DataBlockReference >                        datablock_reference_ptr;

                    typedef om::ObjectImpl
                    <
                        SimplePool< om::ObjPtr< DataBlockReference > >
                    >
                    datablock_references_pool_t;

                    typedef std::unordered_map< uuid_t /* peerId */, ClientInfo >           clients_state_map_t;
                    typedef std::unordered_map< uuid_t, block_dispatch_t* >                 channels_state_map_t;
                    typedef std::unordered_map< uuid_t /* peerId */, time::ptime >          clients_prune_state_map_t;

                    const uuid_t                                                            m_peerId;
                    const block_clients_list_t                                              m_blockClients;
                    const om::ObjPtrDisposable< BackendProcessing >                         m_backendReceiver;
                    const om::ObjPtrDisposable< async_wrapper_t >                           m_asyncWrapperReceiver;
                    const om::ObjPtrDisposable< block_dispatch_impl_t >                     m_outgoingBlockChannel;
                    const om::ObjPtr< tasks::TaskControlTokenRW >                           m_controlToken;
                    const om::ObjPtrDisposable< ExecutionQueue >                            m_eqTasks;

                    const om::ObjPtr< data::datablocks_pool_type >                          m_smallDataBlocksPool;
                    const om::ObjPtr< datablock_references_pool_t >                         m_smallDataBlockReferencesPool;
                    const cpp::ScalarTypeIniter< std::size_t >                              m_smallBlockSize;
                    const cpp::ScalarTypeIniter< std::size_t >                              m_maxNoOfSmallBlocks;
                    const om::ObjPtr< counter_object_t >                                    m_noOfSmallBlocksAllocated;

                    /*
                     * This is the state to maintain bookkeeping of the registered / associated peer ids
                     */

                    os::mutex                                                               m_lock;
                    om::ObjPtr< om::Proxy >                                                 m_hostServices;
                    clients_state_map_t                                                     m_clientsState;
                    channels_state_map_t                                                    m_channelsState;
                    cpp::ScalarTypeIniter< bool >                                           m_wasFullyDisconnected;
                    clients_prune_state_map_t                                               m_clientsPruneState;
                    time::time_duration                                                     m_clientsPruneCheckInterval;
                    time::time_duration                                                     m_clientsPruneInterval;
                    time::ptime                                                             m_timeLastClientsPruneCheck;

                    /*
                     * Note that this variable needs to be last and after it has been initialized no
                     * other operation that can throw should be executed
                     */

                    cpp::ScalarTypeIniter< bool >                                           m_isDisposed;

                    SharedStateT(
                        SAA_in          const uuid_t&                                       peerId,
                        SAA_in          block_clients_list_t&&                              blockClients,
                        SAA_in          om::ObjPtr< BackendProcessing >&&                   backendReceiver,
                        SAA_in          om::ObjPtr< async_wrapper_t >&&                     asyncWrapperReceiver,
                        SAA_in          om::ObjPtr< block_dispatch_impl_t >&&               outgoingBlockChannel,
                        SAA_in          om::ObjPtr< tasks::TaskControlTokenRW >&&           controlToken,
                        SAA_in_opt      const std::size_t                                   maxNoOfSmallBlocks = 0U,
                        SAA_in_opt      const std::size_t                                   minSmallBlocksDeltaToLog = 0U
                        ) NOEXCEPT
                        :
                        m_peerId( peerId ),
                        m_blockClients( BL_PARAM_FWD( blockClients ) ),
                        m_backendReceiver( BL_PARAM_FWD( backendReceiver ) ),
                        m_asyncWrapperReceiver( BL_PARAM_FWD( asyncWrapperReceiver ) ),
                        m_outgoingBlockChannel( BL_PARAM_FWD( outgoingBlockChannel ) ),
                        m_controlToken( BL_PARAM_FWD( controlToken ) ),
                        m_eqTasks(
                            ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepNone )
                            ),
                        m_smallDataBlocksPool( data::datablocks_pool_type::createInstance() ),
                        m_smallDataBlockReferencesPool( datablock_references_pool_t::createInstance() ),
                        m_smallBlockSize( getSmallBlockSize() ),
                        m_maxNoOfSmallBlocks(
                            maxNoOfSmallBlocks ?
                                maxNoOfSmallBlocks
                                :
                                ( 1024U * MAX_MEMORY_FOR_ASSOCIATE_BLOCKS_IN_KB ) / m_smallBlockSize
                            ),
                        m_noOfSmallBlocksAllocated(
                            counter_object_t::createInstance(
                                "proxy backend small blocks counter" /* name */,
                                minSmallBlocksDeltaToLog ?
                                    minSmallBlocksDeltaToLog : getDeltaToLog( m_maxNoOfSmallBlocks )
                                )
                            ),
                        m_channelsState( getChannelsState( m_blockClients ) ),
                        m_clientsPruneCheckInterval( time::seconds( PRUNE_CHECK_INTERVAL_IN_SECONDS ) ),
                        m_clientsPruneInterval( time::seconds( PRUNE_INTERVAL_IN_SECONDS ) ),
                        m_timeLastClientsPruneCheck( time::microsec_clock::universal_time() )
                    {
                        /*
                         * All data blocks will be owned by the caller and there is no need to make
                         * a copy of it in the client
                         *
                         * In fact it would be incorrect to make a copy in the client as the client
                         * is going to try to allocate the blocks via its data blocks pool, but we
                         * will be allocating blocks with two different sizes from two different
                         * data pools, so if we don't set this we will get an ASSERT that the
                         * capacity value provided does not match
                         */

                        for( const auto& client : m_blockClients )
                        {
                            const auto& blockChannel = client -> outgoingBlockChannel();

                            blockChannel -> isNoCopyDataBlocks( true );
                        }

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Proxy backend: the small block size is "
                                << m_smallBlockSize.value()
                                << "; maximum # of small blocks is "
                                << m_maxNoOfSmallBlocks
                                << "; minimum small blocks delta to log is "
                                << m_noOfSmallBlocksAllocated -> minDeltaToLog()
                            );

                        BL_ASSERT( m_maxNoOfSmallBlocks > 5U );
                    }

                    ~SharedStateT() NOEXCEPT
                    {
                        /*
                         * Last ditch chance to call dispose in case it wasn't called (which would be a bug)
                         */

                        BL_ASSERT( m_isDisposed );
                        BL_ASSERT( ! m_hostServices );

                        disposeInternal();
                    }

                    static std::size_t getSmallBlockSize()
                    {
                        const auto brokerProtocol = MessagingUtils::createAssociateProtocolMessage(
                            uuids::create()     /* physicalTargetPeerId */,
                            uuids::create()     /* logicalPeerId */
                            );

                        const auto protocolDataString =
                            dm::DataModelUtils::getDocAsPackedJsonString( brokerProtocol );

                        /*
                         * Block size should be 130% larger than the minimum / default required size and then
                         * also aligned on 0.5K
                         */

                        return alignedOf( ( 130U * protocolDataString.size() ) / 100U, 512U );
                    }

                    static std::size_t getDeltaToLog( SAA_in const std::size_t maxNoOfSmallBlocks ) NOEXCEPT
                    {
                        const auto deltaToLog = maxNoOfSmallBlocks / 20U;

                        return std::max< std::size_t >( deltaToLog, counter_object_t::MIN_DELTA_DEFAULT );
                    }

                    void disposeInternal() NOEXCEPT
                    {
                        BL_NOEXCEPT_BEGIN()

                        /*
                         * DisposeInternal() is protected API and it does not hold the lock
                         *
                         * It is expected that it will be called when the lock is already held
                         */

                        if( m_isDisposed )
                        {
                            return;
                        }

                        m_eqTasks -> forceFlushNoThrow();

                        m_outgoingBlockChannel -> dispose();

                        m_asyncWrapperReceiver -> dispose();

                        m_backendReceiver -> dispose();

                        for( const auto& client : m_blockClients )
                        {
                            client -> dispose();
                        }

                        m_eqTasks -> dispose();

                        if( m_hostServices )
                        {
                            m_hostServices -> disconnect();
                            m_hostServices.reset();
                        }

                        m_isDisposed = true;

                        BL_NOEXCEPT_END()
                    }

                    bool isFullyDisconnected()
                    {
                        if( m_outgoingBlockChannel -> isConnected() )
                        {
                            if( m_wasFullyDisconnected )
                            {
                                m_wasFullyDisconnected = false;

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Proxy backend was reconnected to the actual backend"
                                    );
                            }
                        }
                        else
                        {
                            if( ! m_wasFullyDisconnected )
                            {
                                m_wasFullyDisconnected = true;

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Proxy backend was disconnected from the actual backend"
                                    );

                                /*
                                 * When the proxy backend is fully disconnected from the actual
                                 * backend it should request the server to stop
                                 */

                                m_controlToken -> requestCancel();
                            }
                        }

                        return m_wasFullyDisconnected;
                    }

                    static auto getChannelsState( SAA_in const block_clients_list_t& clients ) -> channels_state_map_t
                    {
                        channels_state_map_t channelsState;

                        for( const auto& client : clients )
                        {
                            const auto& blockChannel = client -> outgoingBlockChannel();

                            const auto channelId = blockChannel -> channelId();

                            BL_CHK(
                                uuids::nil(),
                                channelId,
                                BL_MSG()
                                    << "Implementations which do not provide stable channel id are not supported"
                                );

                            channelsState[ channelId ] = blockChannel.get();
                        }

                        return channelsState;
                    }

                    void onPeerIdAssociateTaskCompletedCallback(
                        SAA_inout               const std::exception_ptr&                       eptr,
                        SAA_in                  const uuid_t&                                   channelId,
                        SAA_in                  const uuid_t&                                   peerId
                        )
                    {
                        if( eptr )
                        {
                            BrokerErrorCodes::rethrowIfNotExpectedException( eptr );

                            return;
                        }

                        BL_MUTEX_GUARD( m_lock );

                        auto& clientState = m_clientsState[ peerId ];

                        if( clientState.configuredChannelIds.insert( channelId ).second /* inserted */ )
                        {
                            BL_LOG(
                                Logging::trace(),
                                BL_MSG()
                                    << "A channel id "
                                    << channelId
                                    << " was configured for peer id "
                                    << peerId
                                );
                        }
                    }

                    auto peerIdAssociateTaskContinuationCallback(
                        SAA_inout               Task*                                           associateMessageTask,
                        SAA_in                  const bool                                      asMessagePrefix,
                        SAA_in                  const om::ObjPtrCopyable< Task >&               continuationTask,
                        SAA_in                  const uuid_t&                                   channelId,
                        SAA_in                  const uuid_t&                                   peerId,
                        SAA_in                  const datablock_reference_ptr&                  dataBlockReference
                        )
                        -> om::ObjPtr< Task >
                    {
                        /*
                         * Before we do anything we need to make sure that we return the data block
                         * in the pool after it has been dehydrated
                         */

                        BL_ASSERT( dataBlockReference );
                        dataBlockReference -> dehydrate();
                        m_smallDataBlockReferencesPool -> put( om::copy( dataBlockReference ) );

                        if( asMessagePrefix && associateMessageTask -> isFailed() )
                        {
                            /*
                             * If the associate message task has failed this is part of an associate message
                             * pipeline (i.e. asMessagePrefix=true means that it is followed by a regular message)
                             * then the error will be passed back to the message sender of the message and we
                             * should simply terminate the task tree here (by returning nullptr)
                             *
                             * Otherwise the associate message was generated internally by the timer (i.e.
                             * asMessagePrefix=false) then there is nobody to return the error to in which case we
                             * should simply proceed and if it has failed with an error that is not expected then
                             * we should log the error
                             * (see the implementation of onPeerIdAssociateTaskCompletedCallback above)
                             */

                            return nullptr;
                        }

                        auto taskImpl = tasks::SimpleTaskImpl::createInstance(
                            cpp::bind(
                                &this_type::onPeerIdAssociateTaskCompletedCallback,
                                om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                associateMessageTask -> exception()    /* eptr */,
                                channelId,
                                peerId
                                )
                            );

                        taskImpl -> setContinuationCallback(
                            [ continuationTask, peerId ]( SAA_inout Task* associateCompletionTask )
                                -> om::ObjPtr< Task >
                            {
                                const auto eptr = associateCompletionTask -> exception();

                                if( eptr )
                                {
                                    /*
                                     * If an error has occurred with the associate completion task we
                                     * just report it and we don't schedule the continuation task (if any)
                                     */

                                    BL_LOG_MULTILINE(
                                        Logging::debug(),
                                        BL_MSG()
                                            << "An exception occurred while trying to associate the following peerId "
                                            << peerId
                                            << " :\n"
                                            << eh::diagnostic_information( eptr )
                                        );

                                    return nullptr;
                                }

                                return om::copy( continuationTask );
                            }
                            );

                        return om::moveAs< Task >( taskImpl );
                    }

                    auto createAnAssociateMessageNoLock( SAA_in const uuid_t& peerId ) const
                        -> om::ObjPtr< BrokerProtocol >
                    {
                        return MessagingUtils::createAssociateProtocolMessage(
                            m_peerId    /* physicalTargetPeerId */,
                            peerId      /* logicalPeerId */
                            );
                    }

                    auto getAssociateMessageDataBlock(
                        SAA_in                  const bool                                          tryOnly,
                        SAA_in                  const om::ObjPtr< BrokerProtocol >&                 associateMessage
                        ) const
                        -> datablock_reference_ptr
                    {
                        const auto noOfBlocksAllocated = m_noOfSmallBlocksAllocated -> increment();

                        auto g = BL_SCOPE_GUARD(
                            m_noOfSmallBlocksAllocated -> decrement();
                            );

                        if( tryOnly )
                        {
                            if( noOfBlocksAllocated >= m_maxNoOfSmallBlocks.value() )
                            {
                                return nullptr;
                            }
                        }

                        auto dataBlock = MessagingUtils::serializeObjectsToBlock(
                            associateMessage,
                            nullptr                         /* payload */,
                            m_smallDataBlocksPool,
                            m_smallBlockSize                /* capacity */
                            );

                        auto dataBlockReference = m_smallDataBlockReferencesPool -> tryGet();

                        if( ! dataBlockReference )
                        {
                            dataBlockReference = DataBlockReference::createInstance(
                                om::copy( m_smallDataBlocksPool ),
                                om::copy( m_noOfSmallBlocksAllocated )
                                );
                        }

                        dataBlockReference -> hydrate( std::move( dataBlock ) );
                        g.dismiss();

                        return dataBlockReference;
                    }

                    auto createAssociateMessageTaskNoLock(
                        SAA_in                  const datablock_reference_ptr&                      dataBlockReference,
                        SAA_in                  block_dispatch_t*                                   blockDispatch,
                        SAA_in                  tasks::ContinuationCallback&&                       continuationCallback
                        ) const
                        -> om::ObjPtr< Task >
                    {
                        auto associateTaskImpl = ExternalCompletionTaskImpl::createInstance(
                            cpp::bind(
                                &block_dispatch_t::pushBlockCopyCallback,
                                om::ObjPtrCopyable< block_dispatch_t >::acquireRef( blockDispatch ),
                                uuids::create() /* targetPeerId - not relevant */,
                                om::ObjPtrCopyable< DataBlock >( dataBlockReference -> dataBlock() ),
                                _1 /* completionCallback */
                                )
                            );

                        associateTaskImpl -> setContinuationCallback( BL_PARAM_FWD( continuationCallback ) );

                        return om::moveAs< Task >( associateTaskImpl );
                    }

                    /**
                     * @brief returns true if it should continue to be called
                     */

                    bool configureMissingChannelIdAssociationsNoLock(
                        SAA_in                  const uuid_t&                                       peerId,
                        SAA_inout               ClientInfo&                                         clientState,
                        SAA_inout_opt           time::time_duration*                                duration
                        )
                    {
                        bool requestTimeout = false;

                        auto& configuredChannelIds = clientState.configuredChannelIds;

                        /*
                         * First prune the channel ids that are no longer active
                         */

                        std::vector< uuid_t > toDelete;

                        for( const auto& channelId : configuredChannelIds )
                        {
                            const auto pos = m_channelsState.find( channelId );

                            if(
                                pos == m_channelsState.end() ||
                                ! pos -> second /* blockDispatch */ -> isConnected()
                                )
                            {
                                toDelete.push_back( channelId );
                            }
                        }

                        for( const auto& channelId : toDelete )
                        {
                            configuredChannelIds.erase( channelId );

                            BL_LOG(
                                Logging::trace(),
                                BL_MSG()
                                    << "A channel id "
                                    << channelId
                                    << " was removed from the configured channels for peer id "
                                    << peerId
                                );
                        }

                        /*
                         * Now check which channel ids are not configured yet and schedule associate
                         * messages for each such channel
                         */

                        om::ObjPtr< BrokerProtocol > associateMessage;

                        for( const auto& pair : m_channelsState )
                        {
                            const auto& channelId = pair.first;

                            auto* blockDispatch = pair.second;

                            if(
                                cpp::contains( configuredChannelIds, channelId ) ||
                                ! blockDispatch -> isConnected()
                                )
                            {
                                continue;
                            }

                            if( ! associateMessage )
                            {
                                associateMessage = createAnAssociateMessageNoLock( peerId );
                            }

                            const auto dataBlockReference =
                                getAssociateMessageDataBlock( true /* tryOnly */, associateMessage );

                            if( ! dataBlockReference )
                            {
                                /*
                                 * If we were not able to allocate data blocks that means too many
                                 * blocks are in flight and we should stop for now
                                 *
                                 * The work will continue during the next cycle (which will now be
                                 * reduced to 100 ms - see 100L below)
                                 */

                                if( duration )
                                {
                                    *duration = time::milliseconds( 100L );
                                }

                                requestTimeout = true;

                                break;
                            }

                            m_eqTasks -> push_back(
                                createAssociateMessageTaskNoLock(
                                    dataBlockReference,
                                    blockDispatch,
                                    cpp::bind(
                                        &this_type::peerIdAssociateTaskContinuationCallback,
                                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                        _1                              /* finishedTask */,
                                        false                           /* asMessagePrefix */,
                                        nullptr                         /* continuationTask */,
                                        channelId,
                                        peerId,
                                        dataBlockReference
                                        )
                                    )
                                );
                        }

                        return requestTimeout;
                    }

                    auto getActivePeerIdsNoLock() const -> std::unordered_set< uuid_t >
                    {
                        os::mutex_unique_lock guard;

                        const auto blockDispatcher =
                            m_hostServices -> tryAcquireRef< dispatcher_t >( dispatcher_t::iid(), &guard );

                        BL_CHK(
                            false,
                            nullptr != blockDispatcher,
                            BL_MSG()
                                << "Host services do not support the required block dispatcher service (iid): "
                                << dispatcher_t::iid()
                            );

                        return blockDispatcher -> getAllActiveQueuesIds();
                    }

                    void checkToPruneStaleClientsStateNoLock()
                    {
                        if( ! m_hostServices )
                        {
                            return;
                        }

                        const auto elapsed =
                            time::microsec_clock::universal_time() - m_timeLastClientsPruneCheck;

                        /*
                         * Note that we need separate m_clientsPruneCheckInterval and the check below
                         * because this function can be called very often in the case when onTimer is
                         * scheduled often due to the small blocks limit being reached
                         */

                        if( elapsed < m_clientsPruneCheckInterval )
                        {
                            return;
                        }

                        const auto activePeerIds = getActivePeerIdsNoLock();

                        /*
                         * First make sure that all active ids are removed from the pending
                         * prune list (these are clients which became inactive, but then they
                         * got resurrected within the prune interval)
                         *
                         * Note that if they become inactive again later that would reset
                         * their timer
                         */

                        for( const auto& peerId : activePeerIds )
                        {
                            m_clientsPruneState.erase( peerId );
                        }

                        /*
                         * The inactive list will now be everything in the client state map
                         * that is not in the active peer ids set
                         */

                        std::vector< uuid_t > inactivePeerIds;

                        for( const auto& pair : m_clientsState )
                        {
                            const auto& peerId = pair.first;

                            if( ! cpp::contains( activePeerIds, peerId ) )
                            {
                                inactivePeerIds.push_back( peerId );
                            }
                        }

                        /*
                         * Now simply process the inactive peer ids and check if they are stale
                         * enough and should be pruned
                         */

                        for( const auto& peerId : inactivePeerIds )
                        {
                            const auto now = time::microsec_clock::universal_time();

                            const auto pair = m_clientsPruneState.emplace( peerId, now );

                            if( pair.second /* inserted */ )
                            {
                                /*
                                 * This is a new inactive peer id - should not be processed this time
                                 */

                                continue;
                            }

                            const auto& pos = pair.first;
                            const auto& timeStampInactiveSince = pos -> second;

                            if( ( now - timeStampInactiveSince ) < m_clientsPruneInterval )
                            {
                                /*
                                 * Not ok to prune yet
                                 */

                                continue;
                            }

                            /*
                             * This is a stale client and should be pruned now
                             *
                             * Note that we will not send dissociate messages to the broker, but we
                             * also in the future will not try to refresh the association when
                             * there are state changes
                             *
                             * When the broker reboots that state will also be reset in the broker
                             * (or if this client connects from another proxy or directly to the
                             * broker)
                             */

                            m_clientsPruneState.erase( peerId );
                            m_clientsState.erase( peerId );
                        }

                        const auto emptyClientInfo = ClientInfo();

                        for( const auto& peerId : activePeerIds )
                        {
                            const auto pair = m_clientsState.emplace( peerId, emptyClientInfo );

                            if( pair.second /* inserted */ )
                            {
                                /*
                                 * An active peer id was found that is not captured by the client
                                 * state
                                 *
                                 * This normally should not happen, but could be a result of weird
                                 * race condition where the client state is deleted based on stale
                                 * logic and then the code to register it did not execute
                                 *
                                 * This logic will protect against such scenario
                                 */

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Proxy backend: an active peer id "
                                        << peerId
                                        << " was found that is not registered in the client state, re-registering..."
                                    );
                            }
                        }

                        m_timeLastClientsPruneCheck = time::microsec_clock::universal_time();
                    }

                public:

                    void setHostServices( SAA_in om::ObjPtr< om::Proxy >&& hostServices ) NOEXCEPT
                    {
                        BL_NOEXCEPT_BEGIN()

                        BL_MUTEX_GUARD( m_lock );

                        m_hostServices = BL_PARAM_FWD( hostServices );

                        BL_NOEXCEPT_END()
                    }

                    void setClientsPruneIntervals(
                        SAA_in                  time::time_duration&&                           clientsPruneCheckInterval,
                        SAA_in                  time::time_duration&&                           clientsPruneInterval
                        ) NOEXCEPT
                    {
                        BL_NOEXCEPT_BEGIN()

                        BL_MUTEX_GUARD( m_lock );

                        BL_ASSERT( clientsPruneInterval >= clientsPruneCheckInterval );

                        m_clientsPruneCheckInterval = BL_PARAM_FWD( clientsPruneCheckInterval );
                        m_clientsPruneInterval = BL_PARAM_FWD( clientsPruneInterval );

                        BL_NOEXCEPT_END()
                    }

                    void getCurrentState(
                        SAA_in_opt              std::unordered_set< uuid_t >*                   activeClients,
                        SAA_in_opt              std::unordered_set< uuid_t >*                   pendingPrune
                        )
                    {
                        BL_MUTEX_GUARD( m_lock );

                        if( activeClients )
                        {
                            activeClients -> clear();

                            for( const auto& pair : m_clientsState )
                            {
                                activeClients -> insert( pair.first /* peerId */ );
                            }
                        }

                        if( pendingPrune )
                        {
                            pendingPrune -> clear();

                            for( const auto& pair : m_clientsPruneState )
                            {
                                pendingPrune -> insert( pair.first /* peerId */ );
                            }
                        }
                    }

                    auto backendReceiver() const NOEXCEPT -> const om::ObjPtrDisposable< BackendProcessing >&
                    {
                        return m_backendReceiver;
                    }

                    auto onTimer() -> time::time_duration
                    {
                        BL_MUTEX_GUARD( m_lock );

                        auto durationDefault = time::seconds( ASSOCIATE_PEERID_TIMER_IN_SECONDS );

                        if( m_isDisposed )
                        {
                            /*
                             * The object has been disposed; nothing to do more here
                             */

                            return durationDefault;
                        }

                        auto duration = m_clientsPruneCheckInterval < durationDefault ?
                            m_clientsPruneCheckInterval : durationDefault;

                        checkToPruneStaleClientsStateNoLock();

                        {
                            /*
                             * Check if channels have changed in which case we need to check
                             * if we need to re-associate the client peer ids
                             */

                            auto channelsState = getChannelsState( m_blockClients );

                            bool channelsStateChanged = channelsState.size() != m_channelsState.size();

                            if( ! channelsStateChanged )
                            {
                                for( const auto& pair : channelsState )
                                {
                                    if( ! cpp::contains( m_channelsState, pair.first /* channelId */ ) )
                                    {
                                        channelsStateChanged = true;

                                        break;
                                    }
                                }
                            }

                            if( channelsStateChanged )
                            {
                                m_channelsState = std::move( channelsState );

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Channels state has changed"
                                    );
                            }
                        }

                        if( isFullyDisconnected() )
                        {
                            return duration;
                        }

                        for( auto& pair : m_clientsState )
                        {
                            const bool requestTimeout = configureMissingChannelIdAssociationsNoLock(
                                pair.first      /* peerId */,
                                pair.second     /* clientState */,
                                &duration
                                );

                            if( requestTimeout )
                            {
                                break;
                            }
                        }

                        return duration;
                    }

                    /*
                     * Disposable implementation
                     */

                    virtual void dispose() NOEXCEPT OVERRIDE
                    {
                        BL_NOEXCEPT_BEGIN()

                        BL_MUTEX_GUARD( m_lock );

                        disposeInternal();

                        BL_NOEXCEPT_END()
                    }

                    auto createBackendProcessingTask(
                        SAA_in_opt              const uuid_t&                                   sourcePeerId,
                        SAA_in_opt              const uuid_t&                                   targetPeerId,
                        SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                        )
                        -> om::ObjPtr< Task >
                    {
                        BL_MUTEX_GUARD( m_lock );

                        /*
                         * First check to update the broker protocol message with the original source
                         * and target peer ids if they are not specified by the client (which can only
                         * happen for associate / dissociate messages)
                         *
                         * This will ensure that the final target peer of the message will receive the
                         * correct source peer id (instead of the peer id of the proxy connection)
                         *
                         * TODO: note that this code which serializes and de-serializes the broker
                         * protocol message must be done as part of the task instead of executing here
                         * which can be on one of the non-blocking I/O threads, but this re-factoring
                         * will be done as part of a subsequent change (this parsing operation should
                         * be fast enough to not impact the server even if it is loaded)
                         */

                        const auto messagesPair =
                            MessagingUtils::deserializeBlockToObjects( data, true /* brokerProtocolOnly */ );

                        BL_ASSERT( nullptr == messagesPair.second /* payload */ );

                        const auto& brokerProtocol = messagesPair.first;

                        MessagingUtils::updateBrokerProtocolMessageInBlock(
                            brokerProtocol,
                            data,
                            sourcePeerId,
                            targetPeerId,
                            true                /* skipUpdateIfUnchanged */
                            );

                        /*
                         * At this point we are ready to create the task that will dispatch the message
                         * to the destination
                         */

                        const auto blockDispatch = m_outgoingBlockChannel -> getNextDispatch();

                        auto task = ExternalCompletionTaskImpl::createInstance< Task >(
                            cpp::bind(
                                &block_dispatch_t::pushBlockCopyCallback,
                                om::ObjPtrCopyable< block_dispatch_t >::acquireRef( blockDispatch.get() ),
                                targetPeerId,
                                om::ObjPtrCopyable< data::DataBlock >( data ),
                                _1 /* completionCallback */
                                )
                            );

                        const auto pos = m_clientsState.find( targetPeerId );

                        if( pos == m_clientsState.end() )
                        {
                            /*
                             * The target peer id is unknown to the proxy backend, so it is
                             * either a peer which has connected directly to the real broker
                             * or a peer which is unknown / invalid
                             *
                             * In either case we simply forward the message to the real broker
                             * for handling and processing further
                             */

                            return task;
                        }

                        /*
                         * The targetPeerId is a peer which has connected already through
                         * the proxy backend
                         *
                         * Let's check if the peer id has been associated already and if
                         * not then create an associate message task and queue up the backend
                         * processing task behind the associate message task
                         */

                        auto& clientState = pos -> second;

                        const auto& configuredChannelIds = clientState.configuredChannelIds;

                        const auto channelId = blockDispatch -> channelId();

                        if( ! cpp::contains( configuredChannelIds, channelId ) )
                        {
                            /*
                             * This channel id is not configured yet
                             *
                             * We need to create an associate message task and schedule it ahead of the
                             * backend processing task
                             *
                             * This should generally happen rarely as it can only be a race condition
                             * where one client is connecting and trying to send message immediately
                             * to another client which has connected, but has not registered yet with
                             * the real backend
                             */

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "A message was about to be sent to a target peer id "
                                    << targetPeerId
                                    << " that has connected via the proxy backend, but it is not configured yet"
                                );

                            const auto dataBlockReference = getAssociateMessageDataBlock(
                                false /* tryOnly */,
                                createAnAssociateMessageNoLock( targetPeerId /* peerId */ )
                                );

                            task = createAssociateMessageTaskNoLock(
                                dataBlockReference,
                                blockDispatch.get(),
                                cpp::bind(
                                    &this_type::peerIdAssociateTaskContinuationCallback,
                                    om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                    _1                                          /* finishedTask */,
                                    true                                        /* asMessagePrefix */,
                                    om::ObjPtrCopyable< Task >( task )          /* continuationTask */,
                                    channelId,
                                    pos -> first                                /* peerId (the client peer id) */,
                                    dataBlockReference
                                    )
                                );
                        }

                        return task;
                    }

                    bool peerConnectedNotify(
                        SAA_in                  const uuid_t&                                       peerId,
                        SAA_in_opt              CompletionCallback&&                                completionCallback
                        )
                    {
                        BL_UNUSED( completionCallback );

                        BL_MUTEX_GUARD( m_lock );

                        auto& clientState = m_clientsState[ peerId ];

                        if( ! clientState.configuredChannelIds.empty() )
                        {
                            /*
                             * A peer which has connected before is re-connecting
                             */

                            clientState.configuredChannelIds.clear();

                            BL_LOG(
                                Logging::trace(),
                                BL_MSG()
                                    << "All configured channel ids for peer id "
                                    << peerId
                                    << " were removed"
                                );
                        }

                        if( ! isFullyDisconnected() )
                        {
                            /*
                             * For this scenario we can simply ignore the return value
                             */

                            ( void ) configureMissingChannelIdAssociationsNoLock(
                                peerId,
                                clientState,
                                nullptr /* duration */
                                );
                        }

                        /*
                         * Schedule a task to perform the registration asynchronously and return false to
                         * indicate to the caller that the operation was performed synchronously and that
                         * the callback will not be called
                         */

                        return false;
                    }
                };

                typedef om::ObjectImpl< SharedStateT<> >                                    SharedState;

                typedef ProxyBrokerBackendProcessing< STREAM >                              this_type;
                typedef BackendProcessingBase                                               base_type;

                using base_type::m_lock;
                using base_type::m_hostServices;

                const om::ObjPtrDisposable< SharedState >                                   m_state;
                tasks::SimpleTimer                                                          m_timer;

                ProxyBrokerBackendProcessing(
                    SAA_in          const uuid_t&                                           peerId,
                    SAA_in          block_clients_list_t&&                                  blockClients,
                    SAA_in          om::ObjPtr< BackendProcessing >&&                       backendReceiver,
                    SAA_in          om::ObjPtr< async_wrapper_t >&&                         asyncWrapperReceiver,
                    SAA_in          om::ObjPtr< block_dispatch_impl_t >&&                   outgoingBlockChannel,
                    SAA_in          om::ObjPtr< tasks::TaskControlTokenRW >&&               controlToken,
                    SAA_in_opt      const std::size_t                                       maxNoOfSmallBlocks = 0U,
                    SAA_in_opt      const std::size_t                                       minSmallBlocksDeltaToLog = 0U
                    )
                    :
                    m_state(
                        SharedState::createInstance(
                            peerId,
                            BL_PARAM_FWD( blockClients ),
                            BL_PARAM_FWD( backendReceiver ),
                            BL_PARAM_FWD( asyncWrapperReceiver ),
                            BL_PARAM_FWD( outgoingBlockChannel ),
                            BL_PARAM_FWD( controlToken ),
                            maxNoOfSmallBlocks,
                            minSmallBlocksDeltaToLog
                            )
                        ),
                    m_timer(
                        cpp::bind(
                            &SharedState::onTimer,
                            om::ObjPtrCopyable< SharedState >::acquireRef( m_state.get() )
                            ),
                        time::seconds( ASSOCIATE_PEERID_TIMER_IN_SECONDS )      /* defaultDuration */,
                        time::seconds( 0L )                                     /* initDelay */
                        )
                {
                }

                auto createBackendProcessingTaskInternal(
                    SAA_in                  const OperationId                               operationId,
                    SAA_in                  const CommandId                                 commandId,
                    SAA_in                  const uuid_t&                                   sessionId,
                    SAA_in                  const uuid_t&                                   chunkId,
                    SAA_in_opt              const uuid_t&                                   sourcePeerId,
                    SAA_in_opt              const uuid_t&                                   targetPeerId,
                    SAA_in_opt              const om::ObjPtrCopyable< DataBlock >&          data
                    )
                    -> om::ObjPtr< tasks::Task >
                {
                    {
                        BL_MUTEX_GUARD( m_lock );

                        base_type::validateParameters( operationId, commandId, sessionId, chunkId );
                    }

                    /*
                     * The processing of incoming blocks is to simply dispatch them to the real backend
                     * via the outgoing block channel
                     */

                    return m_state -> createBackendProcessingTask( sourcePeerId, targetPeerId, data );
                }

            public:

                void setClientsPruneIntervals(
                    SAA_in                  time::time_duration&&                           clientsPruneCheckInterval,
                    SAA_in                  time::time_duration&&                           clientsPruneInterval
                    ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( m_lock );

                    m_state -> setClientsPruneIntervals(
                        BL_PARAM_FWD( clientsPruneCheckInterval ),
                        BL_PARAM_FWD( clientsPruneInterval )
                        );

                    m_timer.runNow();

                    BL_NOEXCEPT_END()
                }

                void getCurrentState(
                    SAA_in_opt              std::unordered_set< uuid_t >*                   activeClients,
                    SAA_in_opt              std::unordered_set< uuid_t >*                   pendingPrune
                    )
                {
                    BL_MUTEX_GUARD( m_lock );

                    m_state -> getCurrentState( activeClients, pendingPrune );
                }

                /*
                 * Disposable implementation
                 */

                virtual void dispose() NOEXCEPT OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( m_lock );

                    m_timer.stop();

                    m_state -> dispose();

                    base_type::disposeInternal();

                    BL_NOEXCEPT_END()
                }

                /*
                 * BackendProcessing implementation
                 */

                virtual bool autoBlockDispatching() const NOEXCEPT OVERRIDE
                {
                    return false;
                }

                virtual void setHostServices( SAA_in om::ObjPtr< om::Proxy >&& hostServices ) NOEXCEPT OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    base_type::setHostServices( om::copy( hostServices ) );

                    /*
                     * Make sure we also forward the host services to the receiver backend
                     * (which needs then to dispatch blocks via the AsyncBlockDispatcher interface)
                     */

                    m_state -> setHostServices( om::copy( hostServices ) );
                    m_state -> backendReceiver() -> setHostServices( BL_PARAM_FWD( hostServices ) );

                    BL_NOEXCEPT_END()
                }

                virtual auto createBackendProcessingTask(
                    SAA_in                  const OperationId                               operationId,
                    SAA_in                  const CommandId                                 commandId,
                    SAA_in                  const uuid_t&                                   sessionId,
                    SAA_in                  const uuid_t&                                   chunkId,
                    SAA_in_opt              const uuid_t&                                   sourcePeerId,
                    SAA_in_opt              const uuid_t&                                   targetPeerId,
                    SAA_in_opt              const om::ObjPtr< DataBlock >&                  data
                    )
                    -> om::ObjPtr< tasks::Task > OVERRIDE
                {
                    return tasks::SimpleTaskWithContinuation::createInstance< tasks::Task >(
                        cpp::bind(
                            &this_type::createBackendProcessingTaskInternal,
                            om::ObjPtrCopyable< this_type, BackendProcessing >::acquireRef( this ),
                            operationId,
                            commandId,
                            sessionId,
                            chunkId,
                            sourcePeerId,
                            targetPeerId,
                            om::ObjPtrCopyable< DataBlock >( data )
                            )
                        );
                }

                /*
                 * AcceptorNotify implementation
                 */

                virtual bool peerConnectedNotify(
                    SAA_in                  const uuid_t&                                       peerId,
                    SAA_in_opt              CompletionCallback&&                                completionCallback
                    )
                    OVERRIDE
                {
                    return m_state -> peerConnectedNotify( peerId, BL_PARAM_FWD( completionCallback ) );
                }

                virtual bool peerDisconnectedNotify(
                    SAA_in                  const uuid_t&                                       peerId,
                    SAA_in_opt              CompletionCallback&&                                completionCallback
                    )
                    OVERRIDE
                {
                    BL_UNUSED( peerId );
                    BL_UNUSED( completionCallback );

                    /*
                     * Nothing to do for this notification - just return 'false' to indicate that the
                     * call completed synchronously and that the completion callback won't be invoked
                     */

                    return false;
                }
            };

            /**
             * @brief class ProxyBrokerBackendProcessing - an implementation of a messaging broker proxy backend
             */

            template
            <
                typename STREAM
            >
            class ProxyBlocksReceiverBackendProcessing : public BackendProcessingBase
            {
                BL_CTR_DEFAULT( ProxyBlocksReceiverBackendProcessing, protected )
                BL_DECLARE_OBJECT_IMPL( ProxyBlocksReceiverBackendProcessing )

            protected:

                typedef ProxyBlocksReceiverBackendProcessing< STREAM >                      this_type;
                typedef BackendProcessingBase                                               base_type;

                typedef AsyncBlockDispatcher                                                dispatcher_t;
                typedef data::DataBlock                                                     DataBlock;

                using base_type::m_lock;

                auto createBackendProcessingTaskInternal(
                    SAA_in                  const OperationId                               operationId,
                    SAA_in                  const CommandId                                 commandId,
                    SAA_in                  const uuid_t&                                   sessionId,
                    SAA_in                  const uuid_t&                                   chunkId,
                    SAA_in_opt              const uuid_t&                                   sourcePeerId,
                    SAA_in_opt              const uuid_t&                                   targetPeerId,
                    SAA_in_opt              const om::ObjPtrCopyable< DataBlock >&          data
                    )
                    -> om::ObjPtr< tasks::Task >
                {
                    {
                        BL_MUTEX_GUARD( m_lock );

                        base_type::validateParameters( operationId, commandId, sessionId, chunkId );
                    }

                    /*
                     * Simply forward the call to the host services if connected and if the host supports
                     * the AsyncBlockDispatcher interface
                     *
                     * Note that invokeHostService acquires the lock, so this call needs to be made
                     * without holding the lock
                     *
                     * Note that targetPeerId is not used as the real targetPeerId is part of the
                     * message and it needs to be extracted from there, so we need to parse the
                     * broker protocol message part
                     */

                    BL_UNUSED( sourcePeerId );
                    BL_UNUSED( targetPeerId );

                    const auto messagesPair =
                        MessagingUtils::deserializeBlockToObjects( data, true /* brokerProtocolOnly */ );

                    BL_ASSERT( nullptr == messagesPair.second /* payload */ );

                    const auto& brokerProtocol = messagesPair.first;

                    const auto actualTargetPeerId =
                        uuids::string2uuid( brokerProtocol -> targetPeerId() );

                    return base_type::invokeHostService
                        <
                            dispatcher_t                                        /* SERVICE */,
                            om::ObjPtr< tasks::Task >                           /* RETURN */
                        >
                        (
                            cpp::bind(
                                &dispatcher_t::createDispatchTask,
                                _1                                              /* [SERVICE*] */,
                                cpp::cref( actualTargetPeerId ),
                                cpp::cref( data )
                                )
                        );
                }

            public:

                /*
                 * BackendProcessing implementation
                 */

                virtual bool autoBlockDispatching() const NOEXCEPT OVERRIDE
                {
                    return false;
                }

                virtual auto createBackendProcessingTask(
                    SAA_in                  const OperationId                               operationId,
                    SAA_in                  const CommandId                                 commandId,
                    SAA_in                  const uuid_t&                                   sessionId,
                    SAA_in                  const uuid_t&                                   chunkId,
                    SAA_in_opt              const uuid_t&                                   sourcePeerId,
                    SAA_in_opt              const uuid_t&                                   targetPeerId,
                    SAA_in_opt              const om::ObjPtr< DataBlock >&                  data
                    )
                    -> om::ObjPtr< tasks::Task > OVERRIDE
                {
                    return tasks::SimpleTaskWithContinuation::createInstance< tasks::Task >(
                        cpp::bind(
                            &this_type::createBackendProcessingTaskInternal,
                            om::ObjPtrCopyable< this_type, BackendProcessing >::acquireRef( this ),
                            operationId,
                            commandId,
                            sessionId,
                            chunkId,
                            sourcePeerId,
                            targetPeerId,
                            om::ObjPtrCopyable< DataBlock >( data )
                            )
                        );
                }
            };

        } // detail

        template
        <
            typename STREAM
        >
        class ProxyBrokerBackendProcessingFactory
        {
            BL_DECLARE_STATIC( ProxyBrokerBackendProcessingFactory )

            typedef ForwardingBackendProcessingFactory< STREAM >						factory_impl_t;

        public:

            typedef om::ObjectImpl
            <
                detail::ProxyBrokerBackendProcessing< STREAM >
            >
            proxy_backend_t;

            typedef om::ObjectImpl
            <
                detail::ProxyBlocksReceiverBackendProcessing< STREAM >
            >
            receiver_backend_t;

            typedef typename factory_impl_t::block_clients_list_t                       block_clients_list_t;
            typedef typename factory_impl_t::block_dispatch_impl_t                      block_dispatch_impl_t;

            typedef typename factory_impl_t::client_factory_t                           client_factory_t;
            typedef typename factory_impl_t::async_wrapper_t                            async_wrapper_t;
            typedef typename factory_impl_t::connection_establisher_t                   connection_establisher_t;

            typedef typename factory_impl_t::datablocks_pool_type                       datablocks_pool_type;

            static auto create(
                SAA_in          const os::port_t                                        defaultInboundPort,
                SAA_in_opt      om::ObjPtr< tasks::TaskControlTokenRW >&&               controlToken,
                SAA_in          const uuid_t&                                           peerId,
                SAA_in          const std::size_t                                       noOfConnections,
                SAA_in          std::vector< std::string >&&                            endpoints,
                SAA_in          const om::ObjPtr< datablocks_pool_type >&               dataBlocksPool,
                SAA_in_opt      const std::size_t                                       threadsCount = 0U,
                SAA_in_opt      const std::size_t                                       maxConcurrentTasks = 0U,
                SAA_in_opt      const bool                                              waitAllToConnect = false,
                SAA_in_opt      const std::size_t                                       maxNoOfSmallBlocks = 0U,
                SAA_in_opt      const std::size_t                                       minSmallBlocksDeltaToLog = 0U
                )
                -> om::ObjPtr< BackendProcessing >
            {
                using namespace bl;
                using namespace messaging;

                auto receiverBackend = om::lockDisposable(
                    receiver_backend_t::template createInstance< BackendProcessing >()
                    );

                auto proxyBackend = factory_impl_t::create(
                    [ & ](
                        SAA_in          const uuid_t&                                       peerId,
                        SAA_in          block_clients_list_t&&                              blockClients,
                        SAA_in          om::ObjPtr< BackendProcessing >&&                   receiverBackend,
                        SAA_in          om::ObjPtr< async_wrapper_t >&&                     asyncWrapper,
                        SAA_in          om::ObjPtr< block_dispatch_impl_t >&&               outgoingBlockChannel,
                        SAA_in          om::ObjPtr< tasks::TaskControlTokenRW >&&           controlToken
                        )
                        -> om::ObjPtr< BackendProcessing >
                    {
                        return proxy_backend_t::template createInstance< BackendProcessing >(
                            peerId,
                            BL_PARAM_FWD( blockClients ),
                            BL_PARAM_FWD( receiverBackend ),
                            BL_PARAM_FWD( asyncWrapper ),
                            BL_PARAM_FWD( outgoingBlockChannel ),
                            BL_PARAM_FWD( controlToken ),
                            maxNoOfSmallBlocks,
                            minSmallBlocksDeltaToLog
                            );
                    }                                       /* createBackendCallback */,
                    om::copy( receiverBackend ),
                    defaultInboundPort,
                    BL_PARAM_FWD( controlToken ),
                    peerId,
                    noOfConnections,
                    BL_PARAM_FWD( endpoints ),
                    dataBlocksPool,
                    threadsCount,
                    maxConcurrentTasks,
                    waitAllToConnect
                    );

                receiverBackend.detachAsObjPtr();

                return proxyBackend;
            }
        };

        typedef ProxyBrokerBackendProcessingFactory
        <
            tasks::TcpSslSocketAsyncBase /* STREAM */
        >
        ProxyBrokerBackendProcessingFactorySsl;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_PROXYBROKERBACKENDPROCESSINGFACTORY_H_ */
