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

#ifndef __BL_MESSAGING_FORWARDINGBACKENDSHAREDSTATE_H_
#define __BL_MESSAGING_FORWARDINGBACKENDSHAREDSTATE_H_

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
        class ForwardingBackendSharedState : public om::DisposableObjectBase
        {
        public:

            typedef std::vector< om::ObjPtrDisposable< MessagingClientBlock > >     block_clients_list_t;
            typedef RotatingMessagingClientBlockDispatch                            block_dispatch_impl_t;

            typedef MessagingClientFactory< STREAM >                                client_factory_t;
            typedef typename client_factory_t::async_wrapper_t                      async_wrapper_t;

        protected:

            const uuid_t                                                            m_peerId;
            const block_clients_list_t                                              m_blockClients;
            const om::ObjPtrDisposable< BackendProcessing >                         m_backendReceiver;
            const om::ObjPtrDisposable< async_wrapper_t >                           m_asyncWrapperReceiver;
            const om::ObjPtrDisposable< block_dispatch_impl_t >                     m_outgoingBlockChannel;
            const om::ObjPtr< tasks::TaskControlTokenRW >                           m_controlToken;

            /*
             * This is the state to maintain bookkeeping of the registered / associated peer ids
             */

            os::mutex                                                               m_lock;
            om::ObjPtr< om::Proxy >                                                 m_hostServices;

            /*
             * Note that this variable needs to be last and after it has been initialized no
             * other operation that can throw should be executed
             */

            cpp::ScalarTypeIniter< bool >                                           m_isDisposed;

            ForwardingBackendSharedState(
                SAA_in          const uuid_t&                                       peerId,
                SAA_in          block_clients_list_t&&                              blockClients,
                SAA_in          om::ObjPtr< BackendProcessing >&&                   backendReceiver,
                SAA_in          om::ObjPtr< async_wrapper_t >&&                     asyncWrapperReceiver,
                SAA_in          om::ObjPtr< block_dispatch_impl_t >&&               outgoingBlockChannel,
                SAA_in          om::ObjPtr< tasks::TaskControlTokenRW >&&           controlToken
                ) NOEXCEPT
                :
                m_peerId( peerId ),
                m_blockClients( BL_PARAM_FWD( blockClients ) ),
                m_backendReceiver( BL_PARAM_FWD( backendReceiver ) ),
                m_asyncWrapperReceiver( BL_PARAM_FWD( asyncWrapperReceiver ) ),
                m_outgoingBlockChannel( BL_PARAM_FWD( outgoingBlockChannel ) ),
                m_controlToken( BL_PARAM_FWD( controlToken ) )
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
            }

            ~ForwardingBackendSharedState() NOEXCEPT
            {
                /*
                 * Last ditch chance to call dispose in case it wasn't called (which would be a bug)
                 */

                BL_ASSERT( m_isDisposed );
                BL_ASSERT( ! m_hostServices );

                disposeInternal( true /* markAsDisposed */ );
            }

            void disposeInternal( SAA_in const bool markAsDisposed ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                /*
                 * disposeInternal() is protected API and it does not hold the lock
                 *
                 * It is expected that it will be called when the lock is already held
                 */

                if( m_isDisposed )
                {
                    return;
                }

                m_outgoingBlockChannel -> dispose();

                m_asyncWrapperReceiver -> dispose();

                m_backendReceiver -> dispose();

                for( const auto& client : m_blockClients )
                {
                    client -> dispose();
                }

                if( m_hostServices )
                {
                    m_hostServices -> disconnect();
                    m_hostServices.reset();
                }

                if( markAsDisposed )
                {
                    m_isDisposed = true;
                }

                BL_NOEXCEPT_END()
            }

        public:

            void setHostServices( SAA_in om::ObjPtr< om::Proxy >&& hostServices ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                m_hostServices = BL_PARAM_FWD( hostServices );

                BL_NOEXCEPT_END()
            }

            auto backendReceiver() const NOEXCEPT -> const om::ObjPtrDisposable< BackendProcessing >&
            {
                return m_backendReceiver;
            }

            auto createBlockDispatchingTask(
                SAA_in_opt              MessagingClientBlockDispatch*                   blockDispatch,
                SAA_in_opt              const uuid_t&                                   targetPeerId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                )
                -> om::ObjPtr< tasks::Task >
            {
                om::ObjPtr< MessagingClientBlockDispatch > defaultDispatch;

                if( ! blockDispatch )
                {
                    defaultDispatch = m_outgoingBlockChannel -> getNextDispatch();
                    blockDispatch = defaultDispatch.get();
                }

                return tasks::ExternalCompletionTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &MessagingClientBlockDispatch::pushBlockCopyCallback,
                        om::ObjPtrCopyable< MessagingClientBlockDispatch >::acquireRef( blockDispatch ),
                        targetPeerId,
                        om::ObjPtrCopyable< data::DataBlock >( data ),
                        _1 /* completionCallback */
                        )
                    );
            }

            /*
             * Disposable implementation
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                disposeInternal( true /* markAsDisposed */ );

                BL_NOEXCEPT_END()
            }
        };

    } // messaging

} // bl

#endif /* __BL_MESSAGING_FORWARDINGBACKENDSHAREDSTATE_H_ */
