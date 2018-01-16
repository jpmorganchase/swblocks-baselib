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

#ifndef __BL_MESSAGING_MESSAGINGCLIENTFACTORY_H_
#define __BL_MESSAGING_MESSAGINGCLIENTFACTORY_H_

#include <baselib/messaging/MessagingClientImpl.h>

#include <baselib/messaging/MessagingClientBackendProcessing.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>

#include <baselib/tasks/SimpleTaskControlToken.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The messaging client factory
         */

        template
        <
            typename STREAM
        >
        class MessagingClientFactory
        {
            BL_DECLARE_STATIC( MessagingClientFactory )

        public:

            typedef om::ObjectImpl< detail::MessagingClientImpl< STREAM > >             messaging_client_t;

            typedef STREAM                                                              stream_t;
            typedef MessagingClientBlockDispatch                                        block_dispatch_t;

            typedef typename messaging_client_t::acceptor_incoming_t                    acceptor_incoming_t;
            typedef typename messaging_client_t::acceptor_outgoing_t                    acceptor_outgoing_t;
            typedef typename messaging_client_t::async_wrapper_t                        async_wrapper_t;
            typedef typename messaging_client_t::backend_interface_t                    backend_interface_t;

            typedef typename messaging_client_t::stream_ref                             stream_ref;
            typedef typename messaging_client_t::connection_establisher_t               connection_establisher_t;

            typedef typename messaging_client_t::sender_connection_t                    sender_connection_t;
            typedef typename messaging_client_t::receiver_connection_t                  receiver_connection_t;
            typedef typename messaging_client_t::receiver_state_t                       receiver_state_t;

            typedef typename sender_connection_t::notify_callback_t                     notify_callback_t;

        protected:

            static auto createEstablishedConnection(
                SAA_in              const om::ObjPtr< tasks::ExecutionQueue >&          eq,
                SAA_in              const std::string&                                  host,
                SAA_in              const unsigned short                                port
                )
                -> om::ObjPtr< connection_establisher_t >
            {
                auto connection = connection_establisher_t::template createInstance( cpp::copy( host ), port );

                const auto task = om::qi< tasks::Task >( connection );

                eq -> push_back( task );
                eq -> waitForSuccess( task );

                return connection;
            }

            static void verifyConnections(
                SAA_in              const std::string&                                  host,
                SAA_in              const unsigned short                                inboundPort,
                SAA_in_opt          const unsigned short                                outboundPort,
                SAA_in_opt          const om::ObjPtr< connection_establisher_t >&       inboundConnection,
                SAA_in_opt          const om::ObjPtr< connection_establisher_t >&       outboundConnection
                )
            {
                if( inboundConnection )
                {
                    BL_CHK(
                        false,
                        host == inboundConnection -> endpointHost(),
                        BL_MSG()
                            << "Actual inbound connection host does not match the input"
                        );

                    BL_CHK(
                        false,
                        inboundPort == inboundConnection -> endpointPort(),
                        BL_MSG()
                            << "Actual inbound connection port does not match the input"
                        );
                }

                if( outboundConnection )
                {
                    const auto outboundPortResolved = outboundPort ? outboundPort : inboundPort + 1U;

                    BL_CHK(
                        false,
                        host == outboundConnection -> endpointHost(),
                        BL_MSG()
                            << "Actual inbound connection host does not match the input"
                        );

                    BL_CHK(
                        false,
                        outboundPortResolved == outboundConnection -> endpointPort(),
                        BL_MSG()
                            << "Actual inbound connection port does not match the input"
                        );
                }
            }

            static auto createFromConnections(
                SAA_in              const bool                                          ownsBackend,
                SAA_in_opt          om::ObjPtr< tasks::ExecutionQueue >&&               eq,
                SAA_in              const uuid_t&                                       peerId,
                SAA_in              om::ObjPtr< BackendProcessing >&&                   backend,
                SAA_in              om::ObjPtr< async_wrapper_t >&&                     asyncWrapper,
                SAA_in              const std::string&                                  host,
                SAA_in              const unsigned short                                inboundPort,
                SAA_in_opt          const unsigned short                                outboundPort,
                SAA_in_opt          om::ObjPtr< connection_establisher_t >&&            inboundConnection,
                SAA_in_opt          om::ObjPtr< connection_establisher_t >&&            outboundConnection,
                SAA_in_opt          om::ObjPtr< data::datablocks_pool_type >&&          dataBlocksPool =
                    data::datablocks_pool_type::createInstance()
                )
                -> om::ObjPtr< block_dispatch_t >
            {
                using namespace bl::tasks;
                using namespace bl::messaging;
                using namespace bl::data;

                om::ObjPtrDisposable< tasks::ExecutionQueue > eqLocal;

                if( ! eq )
                {
                    eqLocal = om::lockDisposable(
                        ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepNone )
                        );

                    eq = om::copy( eqLocal );
                }

                auto receiverState =
                    receiver_state_t::template createInstance( dataBlocksPool, asyncWrapper );

                auto guard = BL_SCOPE_GUARD(
                    {
                        if( eqLocal )
                        {
                            eqLocal -> forceFlushNoThrow();
                        }
                    }
                    );

                om::ObjPtr< sender_connection_t > sender;
                om::ObjPtr< receiver_connection_t > receiver;

                if( inboundConnection )
                {
                    sender = sender_connection_t::template createInstance(
                        notify_callback_t(),
                        inboundConnection -> detachStream(),
                        dataBlocksPool,
                        peerId
                        );

                    eq -> push_back( om::qi< Task >( sender ) );
                }

                if( outboundConnection )
                {
                    receiver = receiver_connection_t::template createInstance( receiverState, peerId );
                    receiver -> attachStream( outboundConnection -> detachStream() );

                    eq -> push_back( om::qi< Task >( receiver ) );
                }

                auto result = messaging_client_t::template createInstance< block_dispatch_t >(
                    std::move( dataBlocksPool ),
                    peerId,
                    std::move( sender ),
                    cpp::copy( host ),
                    inboundPort,
                    std::move( receiver ),
                    std::move( receiverState ),
                    cpp::copy( host ),
                    outboundPort ? outboundPort : inboundPort + 1U,
                    std::move( eq )                                         /* eqConnections */,
                    ownsBackend ? std::move( asyncWrapper ) : nullptr,
                    ownsBackend ? std::move( backend ) : nullptr,
                    std::move( eqLocal )
                    );

                eqLocal.detachAsObjPtr();
                guard.dismiss();

                return result;
            }

         public:

            static auto createClientBackendProcessingFromBlockDispatch(
                SAA_in              om::ObjPtr< block_dispatch_t >&&                    target
                )
                -> om::ObjPtr< BackendProcessing >
            {
                return MessagingClientBackendProcessing::createInstance< BackendProcessing >(
                    BL_PARAM_FWD( target )
                    );
            }

            static auto createAsyncWrapperFromBackend(
                SAA_in              const om::ObjPtr< BackendProcessing >&              backend,
                SAA_in_opt          const std::size_t                                   threadsCount,
                SAA_in_opt          const std::size_t                                   maxConcurrentTasks,
                SAA_in_opt          om::ObjPtr< data::datablocks_pool_type >&&          dataBlocksPool =
                    data::datablocks_pool_type::createInstance(),
                SAA_in_opt          om::ObjPtr< tasks::TaskControlToken >&&             controlToken =
                    tasks::SimpleTaskControlTokenImpl::createInstance< tasks::TaskControlToken >()
                )
                -> om::ObjPtr< async_wrapper_t >
            {
                return async_wrapper_t::template createInstance(
                    om::copy( backend ) /* writeBackend */,
                    om::copy( backend ) /* readBackend */,
                    threadsCount,
                    controlToken,
                    maxConcurrentTasks,
                    dataBlocksPool
                    );
            }

            static auto createEstablishedConnections(
                SAA_in              const std::string&                                  host,
                SAA_in              const unsigned short                                inboundPort,
                SAA_in              const unsigned short                                outboundPort
                )
                -> std::pair
                <
                    om::ObjPtr< connection_establisher_t > /* inboundConnection */,
                    om::ObjPtr< connection_establisher_t > /* outboundConnection */
                >
            {
                om::ObjPtr< connection_establisher_t > inboundConnection;
                om::ObjPtr< connection_establisher_t > outboundConnection;

                tasks::scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                    {
                        inboundConnection = createEstablishedConnection( eq, host, inboundPort );
                        outboundConnection = createEstablishedConnection( eq, host, outboundPort );
                    }
                    );

                return std::make_pair( std::move( inboundConnection ), std::move( outboundConnection ) );
            }

            static auto createWithSmartDefaults(
                SAA_in_opt          om::ObjPtr< tasks::ExecutionQueue >&&               eq,
                SAA_in              const uuid_t&                                       peerId,
                SAA_in              om::ObjPtr< BackendProcessing >&&                   backend,
                SAA_in              om::ObjPtr< async_wrapper_t >&&                     asyncWrapper,
                SAA_in              const std::string&                                  host,
                SAA_in              const unsigned short                                inboundPort,
                SAA_in_opt          const unsigned short                                outboundPort = 0U,
                SAA_in_opt          om::ObjPtr< connection_establisher_t >&&            inboundConnection = nullptr,
                SAA_in_opt          om::ObjPtr< connection_establisher_t >&&            outboundConnection = nullptr,
                SAA_in_opt          om::ObjPtr< data::datablocks_pool_type >&&          dataBlocksPool =
                    data::datablocks_pool_type::createInstance()
                )
                -> om::ObjPtr< block_dispatch_t >
            {
                verifyConnections( host, inboundPort, outboundPort, inboundConnection, outboundConnection );

                return createFromConnections(
                    false /* ownsBackend */,
                    BL_PARAM_FWD( eq ),
                    peerId,
                    BL_PARAM_FWD( backend ),
                    BL_PARAM_FWD( asyncWrapper ),
                    host,
                    inboundPort,
                    outboundPort,
                    BL_PARAM_FWD( inboundConnection ),
                    BL_PARAM_FWD( outboundConnection ),
                    BL_PARAM_FWD( dataBlocksPool )
                    );
            }

            static auto createWithSmartDefaults(
                SAA_in              const uuid_t&                                       peerId,
                SAA_in              om::ObjPtr< block_dispatch_t >&&                    target,
                SAA_in              const std::string&                                  host,
                SAA_in              const unsigned short                                inboundPort,
                SAA_in_opt          const unsigned short                                outboundPort = 0U,
                SAA_in_opt          om::ObjPtr< connection_establisher_t >&&            inboundConnection = nullptr,
                SAA_in_opt          om::ObjPtr< connection_establisher_t >&&            outboundConnection = nullptr,
                SAA_in_opt          const std::size_t                                   threadsCount = 0U,
                SAA_in_opt          const std::size_t                                   maxConcurrentTasks = 0U,
                SAA_in_opt          om::ObjPtr< data::datablocks_pool_type >&&          dataBlocksPool =
                    data::datablocks_pool_type::createInstance(),
                SAA_in_opt          om::ObjPtr< tasks::TaskControlToken >&&             controlToken =
                    tasks::SimpleTaskControlTokenImpl::createInstance< tasks::TaskControlToken >()
                )
                -> om::ObjPtr< block_dispatch_t >
            {
                verifyConnections( host, inboundPort, outboundPort, inboundConnection, outboundConnection );

                auto backend = om::lockDisposable(
                    createClientBackendProcessingFromBlockDispatch( BL_PARAM_FWD( target ) )
                    );

                auto asyncWrapper = om::lockDisposable(
                    createAsyncWrapperFromBackend(
                        backend,
                        threadsCount,
                        maxConcurrentTasks,
                        om::copy( dataBlocksPool ),
                        BL_PARAM_FWD( controlToken )
                        )
                    );

                auto result = createFromConnections(
                    true                        /* ownsBackend */,
                    nullptr                     /* eq */,
                    peerId,
                    om::copy( backend ),
                    om::copy( asyncWrapper ),
                    host,
                    inboundPort,
                    outboundPort,
                    BL_PARAM_FWD( inboundConnection ),
                    BL_PARAM_FWD( outboundConnection ),
                    BL_PARAM_FWD( dataBlocksPool )
                    );

                backend.detachAsObjPtr();
                asyncWrapper.detachAsObjPtr();

                return result;
            }
        };

        typedef MessagingClientFactory< tasks::TcpSslSocketAsyncBase /* STREAM */ > MessagingClientFactorySsl;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_MESSAGINGCLIENTFACTORY_H_ */
