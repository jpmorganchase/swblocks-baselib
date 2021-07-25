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

#ifndef __BL_MESSAGING_ASYNCEXECUTORWRAPPERBLOCKS_H_
#define __BL_MESSAGING_ASYNCEXECUTORWRAPPERBLOCKS_H_

#include <baselib/messaging/DataChunkStorage.h>
#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/BackendProcessing.h>

#include <baselib/async/AsyncExecutorWrapperBase.h>

namespace bl
{
    namespace detail
    {
        /**
         * @brief class AsyncSharedStateBlocks - state object that will be used to maintain the shared
         * state between the AsyncExecutorWrapperBlocks object and the operation states it manages
         */

        template
        <
            typename E = void
        >
        class AsyncSharedStateBlocksT : public AsyncSharedStateBase
        {
        public:

            typedef messaging::BackendProcessing::CommandId                                 CommandId;
            typedef messaging::BackendProcessing::OperationId                               OperationId;

            typedef cpp::function
                <
                    void (
                        SAA_in const om::ObjPtr< data::DataBlock >& dataBlock
                        )
                >
                datablock_callback_t;

        protected:

            typedef AsyncSharedStateBase                                                    base_type;

            const om::ObjPtr< data::datablocks_pool_type >                                  m_dataBlocksPool;
            datablock_callback_t                                                            m_authenticationCallback;
            datablock_callback_t                                                            m_serverStateCallback;
            std::size_t                                                                     m_blockCapacity;

            AsyncSharedStateBlocksT(
                SAA_in              om::ObjPtr< data::datablocks_pool_type >&&              dataBlocksPool,
                SAA_in_opt          om::ObjPtr< tasks::TaskControlToken >&&                 controlToken = nullptr,
                SAA_in_opt          datablock_callback_t&&                                  authenticationCallback =
                    datablock_callback_t(),
                SAA_in_opt          datablock_callback_t&&                                  serverStateCallback =
                    datablock_callback_t()
                )
                :
                base_type( BL_PARAM_FWD( controlToken ) ),
                m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) ),
                m_blockCapacity( data::DataBlock::defaultCapacity() )
            {
                authenticationCallback.swap( m_authenticationCallback );
                serverStateCallback.swap( m_serverStateCallback );
            }

        public:

            auto dataBlocksPool() const NOEXCEPT -> const om::ObjPtr< data::datablocks_pool_type >&
            {
                return m_dataBlocksPool;
            }

            auto authenticationCallback() const NOEXCEPT -> const datablock_callback_t&
            {
                return m_authenticationCallback;
            }

            void authenticationCallback( SAA_in_opt datablock_callback_t&& authenticationCallback ) NOEXCEPT
            {
                authenticationCallback.swap( m_authenticationCallback );
            }

            auto serverStateCallback() const NOEXCEPT -> const datablock_callback_t&
            {
                return m_serverStateCallback;
            }

            void serverStateCallback( SAA_in_opt datablock_callback_t&& serverStateCallback ) NOEXCEPT
            {
                serverStateCallback.swap( m_serverStateCallback );
            }

            auto blockCapacity() const NOEXCEPT -> std::size_t
            {
                return m_blockCapacity;
            }

            void blockCapacity( SAA_in const std::size_t blockCapacity ) NOEXCEPT
            {
                m_blockCapacity = blockCapacity;
            }

            auto allocateBlock() const -> om::ObjPtr< data::DataBlock >
            {
                auto newBlock = m_dataBlocksPool -> tryGet();

                if( ! newBlock )
                {
                    newBlock = data::DataBlock::createInstance( m_blockCapacity );
                }

                BL_ASSERT( newBlock -> capacity() == m_blockCapacity );

                newBlock -> reset();

                return newBlock;
            }

            void deallocateBlock( SAA_in om::ObjPtr< data::DataBlock >&& block ) const NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                BL_ASSERT( block );
                BL_ASSERT( block -> capacity() == m_blockCapacity );

                block -> reset();

                m_dataBlocksPool -> put( BL_PARAM_FWD( block ) );

                BL_NOEXCEPT_END()
            }
        };

        typedef AsyncSharedStateBlocksT<> AsyncSharedStateBlocks;

    } // detail

    /**
     * @brief class AsyncExecutorWrapperBlocks - an implementation of async executor
     * which manages, possibly authenticated, data blocks
     *
     * For more information see the description of AsyncExecutorWrapperBase class
     */

    template
    <
        typename IMPL = detail::AsyncSharedStateBlocks
    >
    class AsyncExecutorWrapperBlocks : public AsyncExecutorWrapperBase< IMPL >
    {
    public:

        enum : char
        {
            SECURE_BLOCKS_FILL_BYTE = 'x',
        };

        typedef typename IMPL::OperationId                                                  OperationId;
        typedef typename IMPL::CommandId                                                    CommandId;
        typedef typename IMPL::datablock_callback_t                                         datablock_callback_t;
        typedef AsyncExecutorWrapperBase< IMPL >                                            base_type;
        typedef typename base_type::AsyncOperationStateBase                                 AsyncOperationStateBase;

        using base_type::impl;

        /**
         * @brief class AsyncOperationState - this object holds the state for an async operation that
         * is already in progress
         */

        template
        <
            typename E2 = void
        >
        class AsyncOperationStateBlockBaseT : public AsyncOperationStateBase
        {
            BL_DECLARE_OBJECT_IMPL( AsyncOperationStateBlockBaseT )

        protected:

            typedef AsyncOperationStateBase                                                 base_type;

        public:

            using base_type::impl;

        protected:

            cpp::ScalarTypeIniter< OperationId >                                            m_operationId;
            om::ObjPtr< data::DataBlock >                                                   m_data;
            uuid_t                                                                          m_sessionId;
            uuid_t                                                                          m_chunkId;
            uuid_t                                                                          m_sourcePeerId;
            uuid_t                                                                          m_targetPeerId;
            cpp::ScalarTypeIniter< CommandId >                                              m_commandId;

            AsyncOperationStateBlockBaseT()
                :
                m_operationId( OperationId::None ),
                m_sessionId( uuids::nil() ),
                m_chunkId( uuids::nil() ),
                m_sourcePeerId( uuids::nil() ),
                m_targetPeerId( uuids::nil() ),
                m_commandId( CommandId::None )
            {
            }

            om::ObjPtr< data::DataBlock > detachData() NOEXCEPT
            {
                om::ObjPtr< data::DataBlock > result( std::move( m_data ) );
                return result;
            }

            virtual void reset() NOEXCEPT
            {
                BL_ASSERT( ! m_data );

                m_operationId = OperationId::None;
                m_sessionId = uuids::nil();
                m_chunkId = uuids::nil();
                m_sourcePeerId = uuids::nil();
                m_targetPeerId = uuids::nil();
                m_commandId = CommandId::None;
            }

            void data( SAA_in om::ObjPtr< data::DataBlock >&& data ) NOEXCEPT
            {
                m_data = BL_PARAM_FWD( data );
            }

            void handleAlloc( SAA_in const bool isSecure )
            {
                if( ! m_data )
                {
                    m_data = impl() -> allocateBlock();
                }

                if( isSecure )
                {
                    std::memset( m_data -> pv(), SECURE_BLOCKS_FILL_BYTE, m_data -> capacity() );
                }
            }

            void handleDataBlockCallback(
                SAA_in          const bool                          allowAllocate,
                SAA_in_opt      const datablock_callback_t&         callback,
                SAA_in          const std::string&                  messagePrefix,
                SAA_in_opt      const eh::errc::errc_t              defaultError = eh::errc::success
                )
            {
                messaging::BackendProcessingBase::chkToWrapInServerErrorAndThrow(
                    [ & ]() -> void
                    {
                        if( ! callback )
                        {
                            BL_THROW_EC(
                                eh::errc::make_error_code( eh::errc::function_not_supported ),
                                BL_MSG()
                                    << messagePrefix
                                    << " is not supported by the backend"
                                    );
                        }

                        if( ! m_data && allowAllocate )
                        {
                            m_data = impl() -> allocateBlock();
                        }

                        if( ! m_data )
                        {
                            BL_THROW_EC(
                                eh::errc::make_error_code( eh::errc::invalid_argument ),
                                BL_MSG()
                                    << "Data block data is required to be sent from the client"
                                    );
                        }

                        callback( m_data );
                    },
                    messagePrefix,
                    defaultError
                    );
            }

        public:

            OperationId operationId() const NOEXCEPT
            {
                return m_operationId;
            }

            void operationId( SAA_in const OperationId operationId ) NOEXCEPT
            {
                m_operationId = operationId;
            }

            const om::ObjPtr< data::DataBlock >& data() const NOEXCEPT
            {
                return m_data;
            }

            const uuid_t& sessionId() const NOEXCEPT
            {
                return m_sessionId;
            }

            void sessionId( SAA_in const uuid_t& sessionId ) NOEXCEPT
            {
                m_sessionId = sessionId;
            }

            const uuid_t& chunkId() const NOEXCEPT
            {
                return m_chunkId;
            }

            void chunkId( SAA_in const uuid_t& chunkId ) NOEXCEPT
            {
                m_chunkId = chunkId;
            }

            const uuid_t& sourcePeerId() const NOEXCEPT
            {
                return m_sourcePeerId;
            }

            void sourcePeerId( SAA_in const uuid_t& sourcePeerId ) NOEXCEPT
            {
                m_sourcePeerId = sourcePeerId;
            }

            const uuid_t& targetPeerId() const NOEXCEPT
            {
                return m_targetPeerId;
            }

            void targetPeerId( SAA_in const uuid_t& targetPeerId ) NOEXCEPT
            {
                m_targetPeerId = targetPeerId;
            }

            CommandId commandId() const NOEXCEPT
            {
                return m_commandId;
            }

            void commandId( SAA_in const CommandId commandId ) NOEXCEPT
            {
                m_commandId = commandId;
            }

            /**************************************************************************************
             * AsyncOperationState implementation
             */

            virtual void execute() OVERRIDE
            {
                switch( m_operationId.value() )
                {
                    default:
                        BL_RT_ASSERT( false, "Invalid operation id" );
                        break;

                    case OperationId::Alloc:
                    case OperationId::SecureAlloc:
                        {
                            handleAlloc( OperationId::SecureAlloc == m_operationId /* isSecure */ );
                        }
                        break;

                    case OperationId::SecureDiscard:
                        {
                            BL_CHK(
                                nullptr,
                                m_data,
                                BL_MSG()
                                    << "Secure discard operation was scheduled without data"
                                );

                            std::memset( m_data -> pv(), SECURE_BLOCKS_FILL_BYTE, m_data -> capacity() );
                        }
                        break;

                    case OperationId::AuthenticateClient:
                        {
                            handleDataBlockCallback(
                                false                                               /* allowAllocate */,
                                impl() -> authenticationCallback(),
                                "Operation authenticate client"                     /* messagePrefix */,
                                eh::errc::permission_denied                         /* defaultError */
                                );
                        }
                        break;

                    case OperationId::GetServerState:
                        {
                            handleDataBlockCallback(
                                true                                                /* allowAllocate */,
                                impl() -> serverStateCallback(),
                                "Operation getting server state"                    /* messagePrefix */
                                );
                        }
                        break;


                    case OperationId::Command:
                        {
                            /*
                             * 'FlushPeerSessions' and 'Remove' can be ignored, but everything else
                             *  should cause RIP if not implemented in the derived classes
                             */

                            switch( m_commandId.value() )
                            {
                                default:
                                    BL_RT_ASSERT( false, "Invalid command id" );
                                    break;

                                case CommandId::FlushPeerSessions:
                                case CommandId::Remove:
                                    break;
                            }
                        }
                        break;
                }
            }

            virtual void releaseResources() NOEXCEPT OVERRIDE
            {
                if( m_data )
                {
                    impl() -> deallocateBlock( detachData() );
                }

                reset();

                base_type::releaseResources();
            }
        };

        typedef AsyncOperationStateBlockBaseT<> AsyncOperationStateBlockBase;

    protected:

        AsyncExecutorWrapperBlocks(
            SAA_in                  om::ObjPtr< IMPL >&&                                impl,
            SAA_in_opt              const std::size_t                                   threadsCount = 0U,
            SAA_in_opt              const std::size_t                                   maxConcurrentTasks = 0U
            )
            :
            base_type( BL_PARAM_FWD( impl ), threadsCount, maxConcurrentTasks )
        {
        }

        template
        <
            typename T
        >
        om::ObjPtr< T > createOperationStateInternal(
            SAA_in                  const OperationId                                   operationId,
            SAA_in_opt              const uuid_t&                                       sessionId,
            SAA_in_opt              const uuid_t&                                       chunkId,
            SAA_in_opt              const uuid_t&                                       sourcePeerId,
            SAA_in_opt              const uuid_t&                                       targetPeerId,
            SAA_in_opt              const CommandId                                     commandId
            )
        {
            auto operationState = impl() -> template tryGetOperationState< T >();

            if( ! operationState )
            {
                operationState = T::template createInstance();
            }

            operationState -> impl( om::copy( impl() ) );

            operationState -> operationId( operationId );
            operationState -> sessionId( sessionId );
            operationState -> chunkId( chunkId );
            operationState -> sourcePeerId( sourcePeerId );
            operationState -> targetPeerId( targetPeerId );
            operationState -> commandId( commandId );

            BL_ASSERT( ! operationState -> data() );

            return operationState;
        }

    public:

        auto authenticationCallback() const NOEXCEPT -> const datablock_callback_t&
        {
            return impl() -> authenticationCallback();
        }

        void authenticationCallback( SAA_in_opt datablock_callback_t&& authenticationCallback ) NOEXCEPT
        {
            impl() -> authenticationCallback( BL_PARAM_FWD( authenticationCallback ) );
        }

        auto serverStateCallback() const NOEXCEPT -> const datablock_callback_t&
        {
            return impl() -> serverStateCallback();
        }

        void serverStateCallback( SAA_in_opt datablock_callback_t&& serverStateCallback ) NOEXCEPT
        {
            impl() -> serverStateCallback( BL_PARAM_FWD( serverStateCallback ) );
        }
    };

} // bl

#endif /* __BL_MESSAGING_ASYNCEXECUTORWRAPPERBLOCKS_H_ */

