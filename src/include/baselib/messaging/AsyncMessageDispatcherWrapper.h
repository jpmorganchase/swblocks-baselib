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

#ifndef __BL_MESSAGING_ASYNCMESSAGEDISPATCHERWRAPPER_H_
#define __BL_MESSAGING_ASYNCMESSAGEDISPATCHERWRAPPER_H_

#include <baselib/messaging/AsyncExecutorWrapperBlocks.h>

namespace bl
{
    namespace detail
    {
        /**
         * @brief class AsyncMessageDispatcherWrapperSharedState - state object that will be used to maintain the shared
         * state between the AsyncMessageDispatcherWrapper object and the operation states it manages
         */

        template
        <
            typename E = void
        >
        class AsyncMessageDispatcherWrapperSharedStateT : public AsyncSharedStateBlocks
        {
        public:

            typedef AsyncSharedStateBlocks::datablock_callback_t                            datablock_callback_t;
            typedef messaging::BackendProcessing                                            backend_interface_t;

        protected:

            typedef AsyncSharedStateBlocks                                                  base_type;

            const om::ObjPtr< messaging::BackendProcessing >                                m_backend;

            AsyncMessageDispatcherWrapperSharedStateT(
                SAA_in              om::ObjPtr< data::datablocks_pool_type >&&              dataBlocksPool,
                SAA_in              om::ObjPtr< messaging::BackendProcessing >&&            backend,
                SAA_in_opt          om::ObjPtr< tasks::TaskControlToken >&&                 controlToken = nullptr,
                SAA_in_opt          datablock_callback_t&&                                  authenticationCallback =
                    datablock_callback_t(),
                SAA_in_opt          datablock_callback_t&&                                  serverStateCallback =
                    datablock_callback_t()
                )
                :
                base_type(
                    BL_PARAM_FWD( dataBlocksPool ),
                    BL_PARAM_FWD( controlToken ),
                    BL_PARAM_FWD( authenticationCallback ),
                    BL_PARAM_FWD( serverStateCallback )
                    ),
                m_backend( BL_PARAM_FWD( backend ) )
            {
            }

        public:

            auto backend() const NOEXCEPT -> const om::ObjPtr< messaging::BackendProcessing >&
            {
                return m_backend;
            }
        };

        typedef om::ObjectImpl< AsyncMessageDispatcherWrapperSharedStateT<> > AsyncMessageDispatcherWrapperSharedState;

    } // detail

    /**
     * @brief class AsyncMessageDispatcherWrapper - an implementation of async message dispatcher on
     * top of the sync MessageDispatcher interface
     *
     * For more information see the description of AsyncExecutorWrapperBase class
     */

    template
    <
        typename E = void
    >
    class AsyncMessageDispatcherWrapperT :
        public AsyncExecutorWrapperBlocks< detail::AsyncMessageDispatcherWrapperSharedState >
    {
    public:

        typedef detail::AsyncMessageDispatcherWrapperSharedState                            shared_state_t;
        typedef shared_state_t::backend_interface_t                                         backend_interface_t;
        typedef shared_state_t::OperationId                                                 OperationId;
        typedef shared_state_t::CommandId                                                   CommandId;
        typedef shared_state_t::datablock_callback_t                                        datablock_callback_t;
        typedef AsyncExecutorWrapperBlocks< shared_state_t >                                base_type;
        typedef typename base_type::AsyncOperationStateBlockBase                            AsyncOperationStateBlockBase;

        /**
         * @brief class AsyncOperationState - this object holds the state for an async operation that
         * is already in progress
         */

        template
        <
            typename E2 = void
        >
        class AsyncOperationStateImplT : public AsyncOperationStateBlockBase
        {
            BL_CTR_DEFAULT( AsyncOperationStateImplT, protected )
            BL_DECLARE_OBJECT_IMPL( AsyncOperationStateImplT )

        private:

            typedef AsyncOperationStateBlockBase                                            base_type;

        public:

            /**************************************************************************************
             * AsyncOperationState implementation
             */

            virtual om::ObjPtr< tasks::Task > createTask() OVERRIDE
            {
                switch( m_operationId.value() )
                {
                    default:
                        return nullptr;

                    case OperationId::Get:
                    case OperationId::Put:
                        BL_ASSERT( m_chunkId != uuids::nil() );
                        break;

                    case OperationId::Command:
                        {
                            switch( m_commandId.value() )
                            {
                                default:
                                    BL_RT_ASSERT( false, "Invalid command id" );
                                    break;

                                case CommandId::FlushPeerSessions:
                                    BL_ASSERT( m_chunkId == uuids::nil() );
                                    break;

                                case CommandId::Remove:
                                    BL_ASSERT( m_chunkId != uuids::nil() );
                                    break;
                            }
                        }
                        break;
                }

                /*
                 * If we are here then the operation id is either Get, Put or Command
                 */

                return base_type::impl() -> backend() -> createBackendProcessingTask(
                    m_operationId,
                    m_commandId,
                    m_sessionId,
                    m_chunkId,
                    m_sourcePeerId,
                    m_targetPeerId,
                    m_data
                    );
            }

            virtual void execute() OVERRIDE
            {
                switch( m_operationId.value() )
                {
                    default:
                        BL_THROW(
                            NotSupportedException(),
                            BL_MSG()
                                << "The requested operation "
                                << static_cast< std::uint16_t >( m_operationId.value() )
                                << " is not supported by the backend"
                            );
                        break;

                    case OperationId::Alloc:
                    case OperationId::SecureAlloc:
                    case OperationId::SecureDiscard:
                    case OperationId::AuthenticateClient:
                    case OperationId::GetServerState:
                        base_type::execute();
                        break;
                }
            }
        };

        typedef om::ObjectImpl< AsyncOperationStateImplT<> > AsyncOperationStateImpl;

    protected:

        AsyncMessageDispatcherWrapperT(
            SAA_in                  const om::ObjPtr< messaging::BackendProcessing >&   writeBackend,
            SAA_in_opt              const om::ObjPtr< messaging::BackendProcessing >&   readBackend,
            SAA_in_opt              const std::size_t                                   threadsCount = 0U,
            SAA_in_opt              const om::ObjPtr< tasks::TaskControlToken >&        controlToken = nullptr,
            SAA_in_opt              const std::size_t                                   maxConcurrentTasks = 0U,
            SAA_in_opt              const om::ObjPtr< data::datablocks_pool_type >&     dataBlocksPool = nullptr,
            SAA_in_opt              datablock_callback_t&&                              authenticationCallback =
                datablock_callback_t(),
            SAA_in_opt              datablock_callback_t&&                              serverStateCallback =
                datablock_callback_t()
            )
            :
            base_type(
                shared_state_t::createInstance(
                    dataBlocksPool ?
                        om::copy( dataBlocksPool ) :
                        data::datablocks_pool_type::createInstance( "[data blocks]" ),
                    om::copy( writeBackend ),
                    om::copy( controlToken ),
                    BL_PARAM_FWD( authenticationCallback ),
                    BL_PARAM_FWD( serverStateCallback )
                ),
                threadsCount,
                maxConcurrentTasks
                )
        {
            BL_CHK(
                false,
                nullptr == readBackend || readBackend == writeBackend,
                BL_MSG()
                    << "Read and write backend cannot be different"
                );
        }

    public:

        /*
         * The server of a messaging backend should not stop on unexpected backend error
         */

        virtual bool stopServerOnUnexpectedBackendError() const NOEXCEPT OVERRIDE
        {
            return false;
        }

        template
        <
            typename T = AsyncOperationState
        >
        om::ObjPtr< T > createOperationState(
            SAA_in                  const OperationId                                   operationId,
            SAA_in_opt              const uuid_t&                                       sessionId,
            SAA_in_opt              const uuid_t&                                       chunkId,
            SAA_in_opt              const uuid_t&                                       sourcePeerId,
            SAA_in_opt              const uuid_t&                                       targetPeerId
            )
        {
            const auto operationState =
                base_type::template createOperationStateInternal< AsyncOperationStateImpl >(
                    operationId,
                    sessionId,
                    chunkId,
                    sourcePeerId,
                    targetPeerId,
                    CommandId::None
                    );

            return om::qi< T >( operationState );
        }
    };

    typedef om::ObjectImpl< AsyncMessageDispatcherWrapperT<> > AsyncMessageDispatcherWrapper;

} // bl

#endif /* __BL_MESSAGING_ASYNCMESSAGEDISPATCHERWRAPPER_H_ */

