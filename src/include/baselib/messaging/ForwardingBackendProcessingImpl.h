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

#ifndef __BL_MESSAGING_FORWARDINGBACKENDPROCESSINGIMPL_H_
#define __BL_MESSAGING_FORWARDINGBACKENDPROCESSINGIMPL_H_

#include <baselib/messaging/ForwardingBackendProcessingFactory.h>
#include <baselib/messaging/ForwardingBackendSharedState.h>
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
             * @brief class ForwardingBackendProcessing - an implementation of a message forwarding backend
             */

            template
            <
                typename STREAM
            >
            class ForwardingBackendProcessing : public BackendProcessingBase
            {
                BL_DECLARE_OBJECT_IMPL( ForwardingBackendProcessing )

            public:

                typedef ForwardingBackendSharedState< STREAM >                              fb_shared_state_t;

                typedef data::DataBlock                                                     DataBlock;

                typedef tasks::Task                                                         Task;
                typedef tasks::ExternalCompletionTaskImpl                                   ExternalCompletionTaskImpl;

                typedef tasks::ExecutionQueue                                               ExecutionQueue;
                typedef tasks::ExecutionQueueImpl                                           ExecutionQueueImpl;

                typedef MessagingClientBlockDispatch                                        block_dispatch_t;
                typedef AsyncBlockDispatcher                                                dispatcher_t;

                typedef typename fb_shared_state_t::block_clients_list_t                    block_clients_list_t;
                typedef typename fb_shared_state_t::block_dispatch_impl_t                   block_dispatch_impl_t;

                typedef typename fb_shared_state_t::client_factory_t                        client_factory_t;
                typedef typename fb_shared_state_t::async_wrapper_t                         async_wrapper_t;

            protected:

                typedef om::ObjectImpl< fb_shared_state_t >                                 SharedState;

                typedef ForwardingBackendProcessing< STREAM >                               this_type;
                typedef BackendProcessingBase                                               base_type;

                using base_type::m_lock;
                using base_type::m_hostServices;

                const om::ObjPtrDisposable< SharedState >                                   m_state;

                ForwardingBackendProcessing(
                    SAA_in          const uuid_t&                                           peerId,
                    SAA_in          block_clients_list_t&&                                  blockClients,
                    SAA_in          om::ObjPtr< BackendProcessing >&&                       backendReceiver,
                    SAA_in          om::ObjPtr< async_wrapper_t >&&                         asyncWrapperReceiver,
                    SAA_in          om::ObjPtr< block_dispatch_impl_t >&&                   outgoingBlockChannel,
                    SAA_in          om::ObjPtr< tasks::TaskControlTokenRW >&&               controlToken
                    )
                    :
                    m_state(
                        SharedState::createInstance(
                            peerId,
                            BL_PARAM_FWD( blockClients ),
                            BL_PARAM_FWD( backendReceiver ),
                            BL_PARAM_FWD( asyncWrapperReceiver ),
                            BL_PARAM_FWD( outgoingBlockChannel ),
                            BL_PARAM_FWD( controlToken )
                            )
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
                     *
                     * blockDispatch=nullptr means use the next available in the round robin (getNextDispatch)
                     */

                    BL_UNUSED( sourcePeerId );

                    return m_state -> createBlockDispatchingTask( nullptr /* blockDispatch */, targetPeerId, data );
                }

            public:

                /*
                 * Disposable implementation
                 */

                virtual void dispose() NOEXCEPT OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( m_lock );

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

                virtual bool isConnected() const NOEXCEPT OVERRIDE
                {
                    /*
                     * If the outgoing channel is disconnected the backend itself is
                     * considered fully disconnected
                     */

                    return m_state -> outgoingBlockChannel() -> isConnected();
                }
            };

            /**
             * @brief class ForwardingReceiverBackendProcessing - an implementation of a messaging forwarding
             * receiver backend
             */

            template
            <
                typename STREAM
            >
            class ForwardingReceiverBackendProcessing : public BackendProcessingBase
            {
                BL_CTR_DEFAULT( ForwardingReceiverBackendProcessing, protected )
                BL_DECLARE_OBJECT_IMPL( ForwardingReceiverBackendProcessing )

            protected:

                typedef ForwardingReceiverBackendProcessing< STREAM >                       this_type;
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
                     */

                    BL_UNUSED( sourcePeerId );

                    return base_type::invokeHostService
                        <
                            dispatcher_t                                        /* SERVICE */,
                            om::ObjPtr< tasks::Task >                           /* RETURN */
                        >
                        (
                            cpp::bind(
                                &dispatcher_t::createDispatchTask,
                                _1                                              /* [SERVICE*] */,
                                cpp::cref( targetPeerId ),
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
        class ForwardingBackendProcessingFactoryDefault
        {
            BL_DECLARE_STATIC( ForwardingBackendProcessingFactoryDefault )

            typedef ForwardingBackendProcessingFactory< STREAM >						factory_impl_t;

        public:

            typedef om::ObjectImpl
            <
                detail::ForwardingBackendProcessing< STREAM >
            >
            forwarding_backend_t;

            typedef om::ObjectImpl
            <
                detail::ForwardingReceiverBackendProcessing< STREAM >
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
                SAA_in_opt      const bool                                              waitAllToConnect = false
                )
                -> om::ObjPtr< BackendProcessing >
            {
                using namespace bl;
                using namespace messaging;

                auto receiverBackend = om::lockDisposable(
                    receiver_backend_t::template createInstance< BackendProcessing >()
                    );

                auto forwardingBackend = factory_impl_t::create(
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
                        return forwarding_backend_t::template createInstance< BackendProcessing >(
                            peerId,
                            BL_PARAM_FWD( blockClients ),
                            BL_PARAM_FWD( receiverBackend ),
                            BL_PARAM_FWD( asyncWrapper ),
                            BL_PARAM_FWD( outgoingBlockChannel ),
                            BL_PARAM_FWD( controlToken )
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

                return forwardingBackend;
            }
        };

        typedef ForwardingBackendProcessingFactoryDefault
        <
            tasks::TcpSslSocketAsyncBase /* STREAM */
        >
        ForwardingBackendProcessingFactoryDefaultSsl;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_FORWARDINGBACKENDPROCESSINGIMPL_H_ */
