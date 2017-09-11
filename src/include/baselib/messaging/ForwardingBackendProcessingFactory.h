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

#ifndef __BL_MESSAGING_FORWARDINGBACKENDPROCESSINGFACTORY_H_
#define __BL_MESSAGING_FORWARDINGBACKENDPROCESSINGFACTORY_H_

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
        template
        <
            typename STREAM
        >
        class ForwardingBackendProcessingFactory
        {
            BL_DECLARE_STATIC( ForwardingBackendProcessingFactory )

        public:

            typedef std::vector< om::ObjPtrDisposable< MessagingClientBlock > >         block_clients_list_t;
            typedef RotatingMessagingClientBlockDispatch                                block_dispatch_impl_t;

            typedef MessagingClientFactory< STREAM >                                    client_factory_t;
            typedef typename client_factory_t::async_wrapper_t                          async_wrapper_t;
            typedef typename client_factory_t::connection_establisher_t                 connection_establisher_t;

            typedef data::datablocks_pool_type                                          datablocks_pool_type;

            typedef std::vector
            <
                std::pair
                <
                    om::ObjPtr< connection_establisher_t >      /* inboundConnection */,
                    om::ObjPtr< connection_establisher_t >      /* outboundConnection */
                >
            >
            connections_list_t;

            typedef cpp::function
            <
                om::ObjPtr< BackendProcessing > (
                    SAA_in          const uuid_t&                                       peerId,
                    SAA_in          block_clients_list_t&&                              blockClients,
                    SAA_in          om::ObjPtr< BackendProcessing >&&                   receiverBackend,
                    SAA_in          om::ObjPtr< async_wrapper_t >&&                     asyncWrapperReceiver,
                    SAA_in          om::ObjPtr< block_dispatch_impl_t >&&               outgoingBlockChannel,
                    SAA_in          om::ObjPtr< tasks::TaskControlTokenRW >&&           controlToken
                    )
            >
            create_backend_callback_t;

            static auto create(
                SAA_in          const create_backend_callback_t&                        createBackendCallback,
                SAA_in          om::ObjPtr< BackendProcessing >&&                       receiverBackend,
                SAA_in          const os::port_t                                        defaultInboundPort,
                SAA_in_opt      om::ObjPtr< tasks::TaskControlTokenRW >&&               controlToken,
                SAA_in          const uuid_t&                                           peerId,
                SAA_in          const std::size_t                                       noOfConnections,
                SAA_in          std::vector< std::string >&&                            endpoints,
                SAA_in          const om::ObjPtr< datablocks_pool_type >&               dataBlocksPool,
                SAA_in_opt      const std::size_t                                       threadsCount = 0U,
                SAA_in_opt      const std::size_t                                       maxConcurrentTasks = 0U,
                SAA_in_opt      const bool                                              waitAllToConnect = false
                )
                -> om::ObjPtr< BackendProcessing >
            {
                using namespace bl;
                using namespace messaging;

                /*
                 * First create the connections as we want to make sure that once the backend is created
                 * it is ready to accept requests
                 */

                const auto expandedEndpoints =
                    MessagingUtils::expandEndpoints( noOfConnections, BL_PARAM_FWD( endpoints ) );

                connections_list_t                          connections;
                std::vector< std::string >                  hosts;
                std::vector< os::port_t >                   inboundPorts;

                tasks::scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                    {
                        eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                        for( const auto& endpoint : expandedEndpoints )
                        {
                            std::string host;
                            os::port_t inboundPort;

                            if( ! net::tryParseEndpoint( endpoint, host, inboundPort ) )
                            {
                                host = endpoint;
                                inboundPort = defaultInboundPort;
                            }

                            auto inboundConnection =
                                connection_establisher_t::template createInstance(
                                    cpp::copy( host ),
                                    inboundPort,
                                    false /* logExceptions */
                                    );

                            const os::port_t outboundPort = inboundPort + 1U;

                            auto outboundConnection =
                                connection_establisher_t::template createInstance(
                                    cpp::copy( host ),
                                    outboundPort,
                                    false /* logExceptions */
                                    );

                            eq -> push_back( om::qi< tasks::Task >( inboundConnection ) );
                            eq -> push_back( om::qi< tasks::Task >( outboundConnection ) );

                            connections.push_back(
                                std::make_pair( std::move( inboundConnection ), std::move( outboundConnection ) )
                                );

                            hosts.push_back( std::move( host ) );
                            inboundPorts.push_back( inboundPort );
                        }

                        eq -> flushNoThrowIfFailed();
                    }
                    );

                bool isOneConnected = false;

                for( const auto& connectionsPair : connections )
                {
                    if( ! connectionsPair.first -> isFailed() && ! connectionsPair.second -> isFailed() )
                    {
                        isOneConnected = true;
                        break;
                    }
                }

                BL_CHK_USER_FRIENDLY(
                    false,
                    isOneConnected,
                    BL_MSG()
                        << "The backend can't connect to any of the endpoints provided"
                    );

                if( ! controlToken )
                {
                    controlToken = tasks::SimpleTaskControlTokenImpl::createInstance< tasks::TaskControlTokenRW >();
                }

                auto asyncWrapper = om::lockDisposable(
                    client_factory_t::createAsyncWrapperFromBackend(
                        receiverBackend,
                        threadsCount,
                        maxConcurrentTasks,
                        om::copy( dataBlocksPool ),
                        om::copyAs< tasks::TaskControlToken >( controlToken.get() )
                        )
                    );

                BL_ASSERT( expandedEndpoints.size() == connections.size() );
                BL_ASSERT( expandedEndpoints.size() == hosts.size() );
                BL_ASSERT( expandedEndpoints.size() == inboundPorts.size() );

                block_clients_list_t blockClients;

                for( std::size_t i = 0U, count = expandedEndpoints.size(); i < count; ++i )
                {
                    const auto inboundPort = inboundPorts[ i ];
                    const auto outboundPort = inboundPort + 1U;

                    auto& connectionsPair = connections[ i ];

                    const bool isFailed =
                        connectionsPair.first -> isFailed() || connectionsPair.second -> isFailed();

                    auto client = om::lockDisposable(
                        MessagingClientObjectFactory::createFromBackendTcp< MessagingClientBlock >(
                            om::copy( receiverBackend ),
                            om::copy( asyncWrapper ),
                            dataBlocksPool,
                            peerId,
                            hosts[ i ],
                            inboundPort,
                            outboundPort,
                            isFailed ? nullptr : std::move( connectionsPair.first )  /* inboundConnection */,
                            isFailed ? nullptr : std::move( connectionsPair.second ) /* outboundConnection */
                            )
                        );

                    blockClients.push_back( om::copy( client ) );

                    client.detachAsObjPtr();
                }

                /*
                 * At this point before we create and return the backend object we need to wait
                 * until all messaging client objects are fully connected as we don't want to start
                 * receiving connections before we are ready to forward the calls to the real
                 * broker / backend
                 *
                 * We going to wait for certain period of time and then bail if at least one
                 * connection can't be established within that period of time
                 */

                const long maxWaitInMilliseconds = 60L * 1000L;     /* 60 seconds */
                const long pollingIntervalInMilliseconds = 100L;

                auto retries = 0L;
                const auto maxRetries = maxWaitInMilliseconds / pollingIntervalInMilliseconds;

                bool atLeastOneIsConnected = false;
                bool allClientsAreConnected = true;

                for( ;; )
                {
                    atLeastOneIsConnected = false;
                    allClientsAreConnected = true;

                    for( const auto& client : blockClients )
                    {
                        if( client -> isConnected() )
                        {
                            atLeastOneIsConnected = true;

                            if( ! waitAllToConnect )
                            {
                                break;
                            }
                        }
                        else
                        {
                            allClientsAreConnected = false;
                        }
                    }

                    if( allClientsAreConnected || retries >= maxRetries )
                    {
                        break;
                    }

                    os::sleep( time::milliseconds( pollingIntervalInMilliseconds ) );

                    ++retries;
                }

                if( ! atLeastOneIsConnected )
                {
                    BL_THROW_USER_FRIENDLY(
                        UnexpectedException(),
                        BL_MSG()
                            << "Cannot establish connectivity with the messaging broker"
                        );
                }

                auto outgoingBlockChannel = block_dispatch_impl_t::createInstance( blockClients );

                auto forwardingBackend = createBackendCallback(
                    peerId,
                    std::move( blockClients ),
                    om::copy( receiverBackend ),
                    om::copy( asyncWrapper ),
                    std::move( outgoingBlockChannel ),
                    std::move( controlToken )
                    );

                asyncWrapper.detachAsObjPtr();

                return forwardingBackend;
            }
        };

        typedef ForwardingBackendProcessingFactory
        <
            tasks::TcpSslSocketAsyncBase /* STREAM */
        >
        ForwardingBackendProcessingFactorySsl;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_FORWARDINGBACKENDPROCESSINGFACTORY_H_ */
