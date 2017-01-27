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

#ifndef __BL_MESSAGING_BROKERDISPATCHINGBACKENDPROCESSING_H_
#define __BL_MESSAGING_BROKERDISPATCHINGBACKENDPROCESSING_H_

#include <baselib/messaging/TcpBlockServerMessageDispatcher.h>
#include <baselib/messaging/TcpBlockTransferClient.h>
#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/BackendProcessing.h>
#include <baselib/messaging/BrokerErrorCodes.h>
#include <baselib/messaging/MessageBlockCompletionQueue.h>
#include <baselib/messaging/AsyncBlockDispatcher.h>
#include <baselib/messaging/AcceptorNotify.h>

#include <baselib/tasks/TaskControlToken.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TasksUtils.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/StringUtils.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief class BrokerDispatchingBackendProcessing
         */

        template
        <
            typename ACCEPTOR
        >
        class BrokerDispatchingBackendProcessing :
            public BackendProcessingBase,
            public AsyncBlockDispatcher,
            public AcceptorNotify
        {
            BL_DECLARE_OBJECT_IMPL( BrokerDispatchingBackendProcessing )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY_CHAIN_BASE( BackendProcessingBase )
                BL_QITBL_ENTRY( AsyncBlockDispatcher )
                BL_QITBL_ENTRY( AcceptorNotify )
            BL_QITBL_END( BackendProcessing )

        public:

            typedef ACCEPTOR                                                                    acceptor_t;
            typedef typename ACCEPTOR::connection_t                                             connection_t;

        protected:

            using BackendProcessingBase::m_lock;

            template
            <
                typename E2 = void
            >
            class DispatchingTaskT : public tasks::WrapperTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( DispatchingTaskT )

            protected:

                typedef tasks::WrapperTaskBase                                                  base_type;

                const om::ObjPtr< tasks::Task >                                                 m_sendBlockTask;
                const om::ObjPtr< tasks::Task >                                                 m_processingTask;

                DispatchingTaskT(
                    SAA_in              om::ObjPtr< tasks::Task >&&                             sendBlockTask,
                    SAA_in_opt          om::ObjPtr< tasks::Task >&&                             processingTask
                    )
                    :
                    m_sendBlockTask( BL_PARAM_FWD( sendBlockTask ) ),
                    m_processingTask( BL_PARAM_FWD( processingTask ) )
                {
                    m_wrappedTask = om::copy( m_processingTask ? m_processingTask : m_sendBlockTask );
                }

                virtual om::ObjPtr< tasks::Task > continuationTask() OVERRIDE
                {
                    auto task = base_type::handleContinuationForward();

                    if( task )
                    {
                        return task;
                    }

                    const auto eptr = exception();

                    if( eptr )
                    {
                        tasks::WrapperTaskBase::exception(
                            BackendProcessingBase::chkToRemapToServerError(
                                eptr,
                                "Broker backend operation" /* messagePrefix */
                                )
                            );

                        return nullptr;
                    }

                    if( om::areEqual( m_wrappedTask, m_sendBlockTask ) )
                    {
                        return nullptr;
                    }

                    /*
                     * If we are here that means that the send block task has not been executed yet
                     */

                    m_wrappedTask = om::copy( m_sendBlockTask );

                    return om::copyAs< tasks::Task >( this );
                }
            };

            typedef om::ObjectImpl< DispatchingTaskT<> >                                        DispatchingTask;

            typedef BrokerDispatchingBackendProcessing< ACCEPTOR >                              this_type;

            typedef tasks::TcpBlockServerOutgoingBackendState                                   backend_state_t;

            const om::ObjPtr< ACCEPTOR >                                                        m_acceptor;
            const om::ObjPtr< tasks::Task >                                                     m_acceptorTask;
            const om::ObjPtr< om::Proxy >                                                       m_backendCallback;
            const om::ObjPtr< om::Proxy >                                                       m_acceptorCallback;
            const om::ObjPtrDisposable< AcceptorNotify >                                        m_backendAcceptorNotify;
            const om::ObjPtrDisposable< BackendProcessing >                                     m_processingBackend;

            om::ObjPtrDisposable< tasks::ExecutionQueue >                                       m_eq;

            BrokerDispatchingBackendProcessing(
                SAA_in_opt          om::ObjPtr< BackendProcessing >&&                           processingBackend,
                SAA_in              const om::ObjPtr< tasks::TaskControlTokenRW >&              controlToken,
                SAA_in              const om::ObjPtr< data::datablocks_pool_type >&             dataBlocksPool,
                SAA_in              std::string&&                                               host,
                SAA_in              const unsigned short                                        port,
                SAA_in              const std::string&                                          privateKeyPem,
                SAA_in              const std::string&                                          certificatePem,
                SAA_in              const uuid_t&                                               peerId,
                SAA_in_opt          time::time_duration&&                                       heartbeatInterval
                    = time::seconds( connection_t::DEFAULT_HEARTBEAT_INTERVAL_IN_SECONDS )
                )
                :
                m_acceptor(
                    ACCEPTOR::template createInstance(
                        controlToken,
                        dataBlocksPool,
                        BL_PARAM_FWD( host ),
                        port,
                        privateKeyPem,
                        certificatePem,
                        peerId,
                        BL_PARAM_FWD( heartbeatInterval )
                        )
                    ),
                m_acceptorTask( om::qi< tasks::Task >( m_acceptor ) ),
                m_backendCallback( om::ProxyImpl::createInstance< om::Proxy >() ),
                m_acceptorCallback( om::ProxyImpl::createInstance< om::Proxy >() ),
                m_backendAcceptorNotify(
                    processingBackend ? om::tryQI< AcceptorNotify >( processingBackend ) : nullptr
                    ),
                m_processingBackend( BL_PARAM_FWD( processingBackend ) ),
                m_eq(
                    tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                        tasks::ExecutionQueue::OptionKeepNone
                        )
                    )

            {
                auto g = BL_SCOPE_GUARD(
                    {
                        if( m_processingBackend )
                        {
                            m_processingBackend -> setHostServices( nullptr );
                            m_backendCallback -> disconnect();
                        }

                        m_acceptor -> setHostServices( nullptr );
                        m_acceptorCallback -> disconnect();
                    }
                    );

                if( m_processingBackend )
                {
                    m_backendCallback -> connect( static_cast< om::Disposable* >( this ) );
                    m_processingBackend -> setHostServices( om::copy( m_backendCallback ) );
                }

                m_acceptorCallback -> connect( static_cast< om::Disposable* >( this ) );
                m_acceptor -> setHostServices( om::copy( m_acceptorCallback ) );

                m_eq -> push_back( m_acceptorTask );

                g.dismiss();
            }

            static void scheduleSendBlock(
                SAA_in              const om::ObjPtrCopyable< backend_state_t >                 backendState,
                SAA_in              const uuid_t&                                               targetPeerId,
                SAA_in              const om::ObjPtrCopyable< data::DataBlock >&                dataBlock,
                SAA_in              const tasks::CompletionCallback&                            onReady
                )
            {
                const auto queue = backendState -> tryGetQueue( targetPeerId /* remotePeerId */ );

                BL_CHK_T(
                    false,
                    !! queue,
                    ServerErrorException()
                        << eh::errinfo_error_code( eh::errc::make_error_code( BrokerErrorCodes::TargetPeerNotFound ) )
                        << eh::errinfo_is_expected( true ),
                    BL_MSG()
                        << "Target peer with id "
                        << str::quoteString( uuids::uuid2string( targetPeerId ) )
                        << " is not available"
                    );

                queue -> scheduleBlock(
                    targetPeerId,
                    om::copy( dataBlock ),
                    cpp::copy( onReady )
                    );
            }

        public:

            auto acceptor() const NOEXCEPT -> const om::ObjPtr< ACCEPTOR >&
            {
                return m_acceptor;
            }

            /*
             * BackendProcessing implementation
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                if( ! m_eq )
                {
                    /*
                     * It is already disposed
                     */

                    return;
                }

                utils::tryCatchLog(
                    "Error while shutting down the broker dispatching backend acceptor",
                    [ this ]() -> void
                    {
                        m_acceptorTask -> requestCancel();
                        tasks::waitForSuccessOrCancel( m_eq, m_acceptorTask );
                    }
                    );

                if( m_processingBackend )
                {
                    m_processingBackend -> dispose();
                    m_processingBackend -> setHostServices( nullptr );
                }

                m_acceptor -> setHostServices( nullptr );

                m_backendCallback -> disconnect();
                m_acceptorCallback -> disconnect();

                tasks::ExecutionQueue::disposeQueue( m_eq );

                BL_NOEXCEPT_END()
            }

            virtual auto createBackendProcessingTask(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const CommandId                                 commandId,
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in_opt              const uuid_t&                                   sourcePeerId,
                SAA_in_opt              const uuid_t&                                   targetPeerId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                /*
                 * Wrap this code in chkToWrapInServerErrorAndThrowT in case he task creation call or
                 * something else in this code below fails
                 *
                 * In this case we should return BrokerErrorCodes::AuthorizationFailed error code as
                 * default error code to be used if the exception doesn't carry such already
                 */

                return BackendProcessingBase::chkToWrapInServerErrorAndThrowT< om::ObjPtr< tasks::Task > >(
                    [ & ]() -> om::ObjPtr< tasks::Task >
                    {
                        /*
                         * We are only interested in processing Put commands (the rest should be handled by
                         * the async wrapper class if supported)
                         */

                        if( OperationId::Put != operationId || CommandId::None != commandId )
                        {
                            return nullptr;
                        }

                        if( ! m_processingBackend )
                        {
                            /*
                             * There is no processing backend installed
                             *
                             * We simply dispatch the block "as is" in this case (i.e. assume that
                             * autoBlockDispatching()=true)
                             */

                            return this_type::createDispatchTask( targetPeerId, data );
                        }

                        auto processingTask = m_processingBackend -> createBackendProcessingTask(
                            operationId,
                            commandId,
                            sessionId,
                            chunkId,
                            sourcePeerId,
                            targetPeerId,
                            data
                            );

                        if( m_processingBackend -> autoBlockDispatching() )
                        {
                            /*
                             * The backend has requested auto block dispatching
                             *
                             * Make sure we chain in the block dispatching task
                             */

                            return DispatchingTask::template createInstance< tasks::Task >(
                                createDispatchTask( targetPeerId, data )                            /* sendBlockTask */,
                                std::move( processingTask )
                                );
                        }

                        /*
                         * If we are here autoBlockDispatching()=false and that means the backend will have the
                         * responsibility to handle the block dispatching (if necessary)
                         *
                         * In this case we simply directly return the backend processing task
                         */

                        return processingTask;
                    },
                    "Broker backend operation" /* messagePrefix */,
                    BrokerErrorCodes::AuthorizationFailed
                    );
            }

            /*
             * AsyncBlockDispatcher implementation
             */

            virtual auto getAllActiveQueuesIds() -> std::unordered_set< uuid_t > OVERRIDE
            {
                return m_acceptor -> backendState() -> getAllActiveQueuesIds();
            }

            virtual auto tryGetMessageBlockCompletionQueue( SAA_in const uuid_t& targetPeerId )
                -> om::ObjPtr< MessageBlockCompletionQueue > OVERRIDE
            {
                return m_acceptor -> backendState() -> tryGetQueue( targetPeerId /* remotePeerId */ );
            }

            virtual auto createDispatchTask(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&                data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                return tasks::ExternalCompletionTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &this_type::scheduleSendBlock,
                        om::ObjPtrCopyable< backend_state_t >::acquireRef(
                            m_acceptor -> backendState().get()
                            ),
                        targetPeerId,
                        om::ObjPtrCopyable< data::DataBlock >( data ),
                        _1 /* onReady - the completion callback */
                        )
                    );
            }

            /*
             * AcceptorNotify implementation
             */

            virtual bool peerConnectedNotify(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              tasks::CompletionCallback&&                         completionCallback
                )
                OVERRIDE
            {
                /*
                 * If the backend supports the AcceptorNotify just forward the call to it, otherwise
                 * simply return false (i.e. indicate to the caller that the operation completed
                 * synchronously and no async call was scheduled)
                 */

                if( m_backendAcceptorNotify )
                {
                    return m_backendAcceptorNotify -> peerConnectedNotify(
                        peerId,
                        BL_PARAM_FWD( completionCallback )
                        );
                }

                return false;
            }

            virtual bool peerDisconnectedNotify(
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              tasks::CompletionCallback&&                         completionCallback
                )
                OVERRIDE
            {
                /*
                 * If the backend supports the AcceptorNotify just forward the call to it, otherwise
                 * simply return false (i.e. indicate to the caller that the operation completed
                 * synchronously and no async call was scheduled)
                 */

                if( m_backendAcceptorNotify )
                {
                    return m_backendAcceptorNotify -> peerDisconnectedNotify(
                        peerId,
                        BL_PARAM_FWD( completionCallback )
                        );
                }

                return false;
            }
        };

        template
        <
            typename ACCEPTOR
        >
        using BrokerDispatchingBackendProcessingImplT =
            om::ObjectImpl< BrokerDispatchingBackendProcessing< ACCEPTOR > >;

        typedef BrokerDispatchingBackendProcessingImplT< tasks::TcpBlockServerOutgoingMessageDispatcher >
            BrokerDispatchingBackendProcessingImpl;

        typedef BrokerDispatchingBackendProcessingImplT< tasks::TcpSslBlockServerOutgoingMessageDispatcher >
            SslBrokerDispatchingBackendProcessingImpl;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BROKERDISPATCHINGBACKENDPROCESSING_H_ */
