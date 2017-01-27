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

#ifndef __BL_MESSAGING_ASYNCDATACHUNKSTORAGE_H_
#define __BL_MESSAGING_ASYNCDATACHUNKSTORAGE_H_

#include <baselib/messaging/AsyncExecutorWrapperBlocks.h>

namespace bl
{
    namespace detail
    {
        /**
         * @brief class AsyncDataChunkStorageSharedState - state object that will be used to maintain the shared
         * state between the AsyncDataChunkStorage object and the operation states it manages
         */

        template
        <
            typename E = void
        >
        class AsyncDataChunkStorageSharedStateT : public AsyncSharedStateBlocks
        {
        public:

            typedef AsyncSharedStateBlocks::datablock_callback_t                            datablock_callback_t;
            typedef data::DataChunkStorage                                                  backend_interface_t;

        protected:

            typedef AsyncSharedStateBlocks                                                  base_type;

            const om::ObjPtr< data::DataChunkStorage >                                      m_readStorage;
            const om::ObjPtr< data::DataChunkStorage >                                      m_writeStorage;

            AsyncDataChunkStorageSharedStateT(
                SAA_in              om::ObjPtr< data::datablocks_pool_type >&&              dataBlocksPool,
                SAA_in              om::ObjPtr< data::DataChunkStorage >&&                  readStorage,
                SAA_in              om::ObjPtr< data::DataChunkStorage >&&                  writeStorage,
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
                m_readStorage( BL_PARAM_FWD( readStorage ) ),
                m_writeStorage( BL_PARAM_FWD( writeStorage ) )
            {
            }

        public:

            auto readStorage() const NOEXCEPT -> const om::ObjPtr< data::DataChunkStorage >&
            {
                return m_readStorage;
            }

            auto writeStorage() const NOEXCEPT -> const om::ObjPtr< data::DataChunkStorage >&
            {
                return m_writeStorage;
            }
        };

        typedef om::ObjectImpl< AsyncDataChunkStorageSharedStateT<> > AsyncDataChunkStorageSharedState;

    } // detail

    /**
     * @brief class AsyncDataChunkStorage - an implementation of async data storage on
     * top of a sync interface
     *
     * For more information see the description of AsyncExecutorWrapperBase class
     */

    template
    <
        typename E = void
    >
    class AsyncDataChunkStorageT : public AsyncExecutorWrapperBlocks< detail::AsyncDataChunkStorageSharedState >
    {
    public:

        typedef detail::AsyncDataChunkStorageSharedState                                    shared_state_t;
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

            virtual void execute() OVERRIDE
            {
                switch( m_operationId.value() )
                {
                    default:
                        break;

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

                switch( m_operationId.value() )
                {
                    default:
                        BL_RT_ASSERT( false, "Invalid operation id" );
                        break;

                    case OperationId::Alloc:
                    case OperationId::SecureAlloc:
                    case OperationId::SecureDiscard:
                    case OperationId::AuthenticateClient:
                    case OperationId::GetServerState:
                        base_type::execute();
                        break;

                    case OperationId::Get:
                        {
                            handleAlloc( true /* isSecure */ );

                            base_type::impl() -> readStorage() -> load( m_sessionId, m_chunkId, m_data );
                        }
                        break;

                    case OperationId::Put:
                        {
                            BL_CHK(
                                nullptr,
                                m_data,
                                BL_MSG()
                                    << "Save operation was scheduled without data"
                                );

                            base_type::impl() -> writeStorage() -> save( m_sessionId, m_chunkId, m_data );
                        }
                        break;


                    case OperationId::Command:
                        {
                            switch( m_commandId.value() )
                            {
                                default:
                                    BL_RT_ASSERT( false, "Invalid command id" );
                                    break;

                                case CommandId::FlushPeerSessions:
                                    {
                                        BL_ASSERT( ! m_data );

                                        base_type::impl() -> writeStorage() -> flushPeerSessions(
                                            uuids::nil() /* peerId */
                                            );
                                    }
                                    break;

                                case CommandId::Remove:
                                    {
                                        BL_ASSERT( ! m_data );

                                        base_type::impl() -> writeStorage() -> remove( m_sessionId, m_chunkId );
                                    }
                                    break;
                            }
                        }
                        break;
                }
            }
        };

        typedef om::ObjectImpl< AsyncOperationStateImplT<> > AsyncOperationStateImpl;

    protected:

        AsyncDataChunkStorageT(
            SAA_in                  const om::ObjPtr< data::DataChunkStorage >&         writeStorage,
            SAA_in                  const om::ObjPtr< data::DataChunkStorage >&         readStorage,
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
                    om::copy( readStorage ),
                    om::copy( writeStorage ),
                    om::copy( controlToken ),
                    BL_PARAM_FWD( authenticationCallback ),
                    BL_PARAM_FWD( serverStateCallback )
                ),
                threadsCount,
                maxConcurrentTasks
                )
        {
        }

    public:

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

    typedef om::ObjectImpl< AsyncDataChunkStorageT<> > AsyncDataChunkStorage;

} // bl

#endif /* __BL_MESSAGING_ASYNCDATACHUNKSTORAGE_H_ */
