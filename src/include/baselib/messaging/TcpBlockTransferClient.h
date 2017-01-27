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

#ifndef __BL_MESSAGING_TCPBLOCKTRANSFERCLIENT_H_
#define __BL_MESSAGING_TCPBLOCKTRANSFERCLIENT_H_

#include <baselib/messaging/TcpBlockTransferCommon.h>
#include <baselib/messaging/BrokerErrorCodes.h>
#include <baselib/messaging/AcceptorNotify.h>

namespace bl
{
    namespace tasks
    {
        /**************************************************************************************************************
         */

        /**
         * @brief TcpBlockTransferClientConnection - The block transfer client connection task
         */

        template
        <
            typename STREAM
        >
        class TcpBlockTransferClientConnectionT :
            public TcpBlockTransferConnectionBase< STREAM >
        {
            BL_DECLARE_OBJECT_IMPL( TcpBlockTransferClientConnectionT )

        public:

            enum class CommandId : std::size_t
            {
                NoCommand,
                SendChunk,
                ReceiveChunk,
                RemoveChunk,
                FlushPeerSessions,
            };

        protected:

            typedef TcpBlockTransferClientConnectionT< STREAM >                             this_type;
            typedef TcpBlockTransferConnectionBase< STREAM >                                base_type;
            typedef typename STREAM::stream_ref                                             stream_ref;

            typedef typename base_type::CommandBlock                                        CommandBlock;

            using base_type::m_cmdBuffer;
            using base_type::m_chunkId;
            using base_type::untilCanceled;
            using base_type::getStream;

            const om::ObjPtr< data::datablocks_pool_type >                                  m_dataBlocksPool;
            data::DataBlock*                                                                m_dataRawPtr;
            om::ObjPtr< data::DataBlock >                                                   m_dataLocalCopy;
            cpp::ScalarTypeIniter< BlockTransferDefs::BlockType >                           m_blockType;

            CommandId                                                                       m_commandId;
            uuid_t                                                                          m_targetPeerId;
            cpp::ScalarTypeIniter< bool >                                                   m_clientVersionNegotiated;
            std::uint32_t                                                                   m_clientVersion;
            cpp::ScalarTypeIniter< bool >                                                   m_protocolOperationsOnly;
            cpp::ScalarTypeIniter< bool >                                                   m_isAuthenticated;

            TcpBlockTransferClientConnectionT(
                SAA_in                  const typename this_type::CommandId                 commandId,
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in                  const om::ObjPtr< data::datablocks_pool_type >&     dataBlocksPool,
                SAA_in_opt              const BlockTransferDefs::BlockType                  blockType =
                    BlockTransferDefs::BlockType::Normal
                )
                :
                base_type( peerId ),
                m_dataBlocksPool( om::copy( dataBlocksPool ) ),
                m_dataRawPtr( nullptr ),
                m_blockType( blockType ),
                m_commandId( commandId ),
                m_targetPeerId( uuids::nil() ),
                m_clientVersion(
                    BlockTransferDefs::BlockType::Normal != blockType ?
                        CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2 :
                        CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1
                    )
            {
                base_type::m_name = "TcpTask_BlockTransferClientConnection";

                chkBlockTypeIsSupported( m_blockType );
            }

            void chkBlockTypeIsSupported( SAA_in const BlockTransferDefs::BlockType blockType )
            {
                if(
                    BlockTransferDefs::BlockType::Normal != blockType &&
                    CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2 != m_clientVersion
                    )
                {
                    BL_THROW(
                        ArgumentException(),
                        BL_MSG()
                            << "This block type requires V2 of the blob server protocol"
                        );
                }

                if( blockType >= BlockTransferDefs::BlockType::Count )
                {
                    BL_THROW(
                        ArgumentException(),
                        BL_MSG()
                            << "Invalid block type was specified"
                        );
                }
            }

            static void chkProtocolDataSize( SAA_in_opt data::DataBlock* dataRawPtr )
            {
                if( dataRawPtr == nullptr)
                {
                    return;
                }

                const auto protocolDataOffset = dataRawPtr -> offset1();

                BL_CHK(
                    false,
                    protocolDataOffset <= std::numeric_limits< std::uint32_t >::max(),
                    BL_MSG()
                        << "The value of protocolDataOffset does not fit into std::uint32_t"
                    );

                if( protocolDataOffset )
                {
                    BL_CHK(
                        false,
                        protocolDataOffset <= dataRawPtr -> size(),
                        BL_MSG()
                            << "The value of protocolDataOffset is larger than the data block size"
                        );
                }
            }

            void setCommandInfoStateInternal(
                SAA_in                  const typename this_type::CommandId             commandId,
                SAA_in_opt              const uuid_t&                                   chunkId,
                SAA_in_opt              const BlockTransferDefs::BlockType              blockType,
                SAA_in_opt              data::DataBlock*                                dataRawPtr
                )
            {
                chkBlockTypeIsSupported( blockType );

                chkProtocolDataSize( dataRawPtr );

                m_commandId = commandId;
                m_blockType = blockType;

                if( CommandId::NoCommand == m_commandId || CommandId::FlushPeerSessions == m_commandId )
                {
                    /*
                     * These commands require the chunk id to be uuids::nil()
                     */

                    m_chunkId = uuids::nil();
                }
                else
                {
                    /*
                     * This is a real send/recv/remove command and the chunk id should be
                     * set accordingly:
                     *
                     * -- if the block type is 'Normal' and the chunk id provided is the
                     *    same as BlockTransferDefs::chunkIdDefault() then we will set it
                     *    to uuids::nil() / invalid and the expectation is that it will
                     *    be set to a valid value prior executing the task
                     *
                     * -- if the block type is not 'Normal' then we simply force it to be
                     *    BlockTransferDefs::chunkIdDefault() which is what the chunk id
                     *    is expected to be for the other block types
                     */

                    if( m_blockType == BlockTransferDefs::BlockType::Normal )
                    {
                        m_chunkId =
                            chunkId == BlockTransferDefs::chunkIdDefault() ? uuids::nil() : chunkId;
                    }
                    else
                    {
                        m_chunkId = BlockTransferDefs::chunkIdDefault();
                    }
                }

                /*
                 * By default the target peer id is the remote peer id
                 */

                m_targetPeerId = base_type::m_remotePeerId;
            }

            void getBlock( SAA_in const decltype( CommandBlock::chunkSize ) size )
            {
                if( ! m_dataRawPtr )
                {
                    m_dataLocalCopy = data::DataBlock::get( m_dataBlocksPool );
                    m_dataRawPtr = m_dataLocalCopy.get();
                }

                BL_CHK(
                    false,
                    size <= m_dataRawPtr -> capacity(),
                    BL_MSG()
                        << "Invalid chunk size (larger than the data block capacity) : "
                        << size
                        );

                m_dataRawPtr -> setSize( size );
            }

            uuid_t getOutgoingPeerId( SAA_in const std::uint16_t cntrlCode ) const
            {
                /*
                 * If we are negotiating the protocol version send the peerId otherwise we
                 * send the target peer id
                 */

                return CommandBlock::CntrlCodeSetProtocolVersion == cntrlCode ?
                    base_type::m_peerId : m_targetPeerId;
            }

            bool validatePayload()
            {
                /*
                 * Validate the expected data expected for each command id
                 */

                const auto isNilChunkId = ( m_chunkId == uuids::nil() );

                switch( m_commandId )
                {
                    default:
                        return false;

                    case CommandId::NoCommand:
                        return isNilChunkId && ! m_dataRawPtr;

                    case CommandId::SendChunk:
                        return ! isNilChunkId && m_dataRawPtr;

                    case CommandId::ReceiveChunk:
                        return ! isNilChunkId;

                    case CommandId::RemoveChunk:
                        return ! isNilChunkId && ! m_dataRawPtr;

                    case CommandId::FlushPeerSessions:
                        return isNilChunkId && ! m_dataRawPtr;
                }
            }

            void chk4ServerErrorsClient()
            {
                if( m_cmdBuffer.flags & CommandBlock::ErrBit )
                {
                    std::string message = "Unexpected server error has occurred";

                    if( m_cmdBuffer.errorCode )
                    {
                        const int errorNo = static_cast< int >( m_cmdBuffer.errorCode );

                        eh::error_code ec( errorNo, eh::generic_category() );

                        if( ec.value() == eh::errc::make_error_code( eh::errc::protocol_not_supported ).value() )
                        {
                            message = "Server error has occurred: client protocol version is not set or not supported";
                        }
                        else
                        {
                            message = "Server error has occurred: " + ec.message();
                        }

                        BL_THROW(
                            ServerErrorException()
                                << eh::errinfo_errno( errorNo )
                                << eh::errinfo_error_code( ec ),
                            BL_MSG()
                                << message
                            );
                    }

                    BL_THROW(
                        ServerErrorException(),
                        BL_MSG()
                            << message
                        );
                }
                else
                {
                    BL_CHK(
                        false,
                        0U == m_cmdBuffer.errorCode,
                        BL_MSG()
                            << "Message received with error code, but error flag is not set"
                        );
                }
            }

            void sendCommandPacket(
                SAA_in                  const std::uint16_t                             cntrlCode,
                SAA_in                  const bool                                      isTerminationPacket
                )
            {
                /*
                 * This will be called only from initial requests so error code and
                 * flags should always be clear
                 */

                BL_ASSERT( ! m_cmdBuffer.flags );
                BL_ASSERT( ! m_cmdBuffer.errorCode );

                m_cmdBuffer.cntrlCode = cntrlCode;
                m_cmdBuffer.peerId = getOutgoingPeerId( cntrlCode );

                m_cmdBuffer.host2Network();

                asio::async_write(
                    getStream(),
                    asio::buffer( &m_cmdBuffer, sizeof( m_cmdBuffer ) ),
                    untilCanceled(),
                    cpp::bind(
                        &this_type::onCommandAckRead,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        sizeof( m_cmdBuffer )                                /* bytesExpected */,
                        cntrlCode                                            /* cntrlCodeExpected */,
                        isTerminationPacket,
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );
            }

            void scheduleSessionFlushData()
            {
                BL_ASSERT( m_chunkId == uuids::nil() );

                sendCtrlCode( CommandBlock::CntrlCodePeerSessionsDataFlushRequest );
            }

            void scheduleSendData()
            {
                BL_ASSERT( m_dataRawPtr && m_dataRawPtr -> size() );
                m_cmdBuffer.chunkSize = ( std::uint32_t ) m_dataRawPtr -> size();

                sendCtrlCode( CommandBlock::CntrlCodePutDataBlock );
            }

            void scheduleRecvData()
            {
                sendCtrlCode( CommandBlock::CntrlCodeGetDataBlockSize );
            }

            void scheduleRemoveData()
            {
                /*
                 * Note: by default we want to ignore the 'not found' errors
                 *
                 * They will be reported on the server and investigated by
                 * operate and dev teams
                 */

                m_cmdBuffer.data.blockInfo.flags = CommandBlock::IgnoreIfNotFound;

                sendCtrlCode( CommandBlock::CntrlCodeRemoveDataBlock );
            }

            void sendCtrlCode( SAA_in const std::uint16_t ctrlCode )
            {
                /*
                 * The only termination packet from this point is the case where
                 * ctrlCode == CommandBlock::CntrlCodePeerSessionsDataFlushRequest
                 */

                const bool isTerminationPacket =
                    (
                        CommandBlock::CntrlCodePeerSessionsDataFlushRequest == ctrlCode ||
                        CommandBlock::CntrlCodeRemoveDataBlock == ctrlCode
                    );

                m_cmdBuffer.chunkId = m_chunkId;
                m_cmdBuffer.data.blockInfo.blockType = m_blockType;

                if( CommandBlock::CntrlCodePutDataBlock == ctrlCode )
                {
                    chkProtocolDataSize( m_dataRawPtr );

                    /*
                     * Note: this cast below is safe because the value of protocolDataOffset will be
                     * validated by chkProtocolDataSize call above to ensure that it fits into std::uint32_t
                     */

                    m_cmdBuffer.data.blockInfo.protocolDataOffset = static_cast< std::uint32_t >( m_dataRawPtr -> offset1() );
                }

                sendCommandPacket( ctrlCode, isTerminationPacket );
            }

            void startCommandInternal()
            {
                m_cmdBuffer = CommandBlock();

                switch( m_commandId )
                {
                    default:
                        BL_ASSERT( false && "Invalid command id" );
                        break;

                    case CommandId::SendChunk:
                        scheduleSendData();
                        break;

                    case CommandId::ReceiveChunk:
                        scheduleRecvData();
                        break;

                    case CommandId::RemoveChunk:
                        scheduleRemoveData();
                        break;

                    case CommandId::FlushPeerSessions:
                        scheduleSessionFlushData();
                        break;
                }
            }

            void scheduleVersionSetCommand( SAA_in const bool isTerminationPacket )
            {
                m_cmdBuffer = CommandBlock();

                m_cmdBuffer.data.version.value = m_clientVersion;

                sendCommandPacket( CommandBlock::CntrlCodeSetProtocolVersion, isTerminationPacket );
            }

            void startCommand()
            {
                /*
                 * If the task is scheduled with CommandId::NoCommand it will just ping the server to
                 * re-negotiate on the protocol version and also serve as a 'ping' type of command which
                 * can be executed periodically for long lived connections to keep the connection alive
                 * and not get it terminated by fire walls and the like
                 */

                if( m_commandId != CommandId::NoCommand && m_clientVersionNegotiated )
                {
                    startCommandInternal();
                }
                else
                {
                    scheduleVersionSetCommand( m_commandId == CommandId::NoCommand /* isTerminationPacket */ );
                }
            }

            void onCommandAckRead(
                SAA_in                  const std::size_t                               bytesExpected,
                SAA_in                  const std::uint16_t                             cntrlCodeExpected,
                SAA_in                  const bool                                      terminationPacket,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                detail::chkPartialDataTransfer( bytesExpected == bytesTransferred );

                readAck( cntrlCodeExpected, terminationPacket );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void readAck(
                SAA_in                  const std::uint16_t                             cntrlCodeExpected,
                SAA_in                  const bool                                      terminationPacket
                )
            {
                m_cmdBuffer = CommandBlock();

                asio::async_read(
                    getStream(),
                    asio::buffer( &m_cmdBuffer, sizeof( m_cmdBuffer ) ),
                    untilCanceled(),
                    cpp::bind(
                        &this_type::onCommandAckReceived,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        cntrlCodeExpected,
                        terminationPacket,
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );
            }

            void onCommandAckReceived(
                SAA_in                  const std::uint16_t                             cntrlCodeExpected,
                SAA_in                  const bool                                      terminationPacket,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                bool taskReady = false;

                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                handleAckPacket( cntrlCodeExpected, bytesTransferred );

                if( terminationPacket )
                {
                    BL_ASSERT(
                        CommandBlock::CntrlCodeSetProtocolVersion ||
                        CommandBlock::CntrlCodePutDataBlock == cntrlCodeExpected ||
                        CommandBlock::CntrlCodePeerSessionsDataFlushRequest == cntrlCodeExpected ||
                        CommandBlock::CntrlCodeRemoveDataBlock == cntrlCodeExpected
                        );

                    if( CommandBlock::CntrlCodePeerSessionsDataFlushRequest == cntrlCodeExpected )
                    {
                        /*
                         * This was a flush request nothing special to do further
                         */

                        BL_ASSERT( m_chunkId == uuids::nil() );
                    }

                    if( CommandBlock::CntrlCodeRemoveDataBlock == cntrlCodeExpected )
                    {
                        BL_ASSERT( m_chunkId != uuids::nil() );
                    }

                    if( CommandBlock::CntrlCodeSetProtocolVersion == cntrlCodeExpected )
                    {
                        BL_ASSERT( m_chunkId == uuids::nil() );

                        m_clientVersionNegotiated = true;
                    }

                    if( CommandBlock::CntrlCodePutDataBlock == cntrlCodeExpected )
                    {
                        if( m_blockType == BlockTransferDefs::BlockType::Authentication )
                        {
                            isAuthenticated( true );
                        }
                        else
                        {
                            ++base_type::m_noOfBlocksTransferred;
                        }
                    }

                    taskReady = true;
                }
                else
                {
                    BL_ASSERT( CommandBlock::CntrlCodePeerSessionsDataFlushRequest != cntrlCodeExpected );
                    BL_ASSERT( CommandBlock::CntrlCodeRemoveDataBlock != cntrlCodeExpected );

                    if( CommandBlock::CntrlCodeSetProtocolVersion == cntrlCodeExpected )
                    {
                        /*
                         * If this was a set protocol version command then this command is
                         * usually initiated in the context of a user command and in this
                         * case we should just continue with the user initiated command and
                         * of course keep the task in not ready state
                         */

                        m_clientVersionNegotiated = true;

                        startCommand();
                    }
                    else
                    {
                        /*
                         * This was a request to send/recv data - proceed
                         * and keep the task as not ready
                         */

                        sendRecvData();
                    }
                }

                BL_TASKS_HANDLER_END_NOTREADY()

                if( taskReady )
                {
                    base_type::notifyReady();
                }
            }

            void handleAckPacket(
                SAA_in                  const std::uint16_t                             cntrlCodeExpected,
                SAA_in                  const std::size_t                               bytesTransferred
                )
            {
                detail::chkPartialDataTransfer( sizeof( m_cmdBuffer ) == bytesTransferred );

                m_cmdBuffer.network2Host();

                BL_CHK(
                    false,
                    (
                        cntrlCodeExpected == m_cmdBuffer.cntrlCode &&
                        ( m_cmdBuffer.flags & CommandBlock::AckBit )
                    ),
                    BL_MSG()
                        << "Invalid control code was sent from the server"
                    );

                chk4ServerErrorsClient();

                /*
                 * Now that the acknowledgment and error bits are verified we should
                 * clear them out immediately
                 */

                m_cmdBuffer.flags &= ~( CommandBlock::AckBit | CommandBlock::ErrBit );

                if(
                    CommandBlock::CntrlCodeSetProtocolVersion == m_cmdBuffer.cntrlCode &&
                    m_cmdBuffer.peerId != base_type::m_remotePeerId &&
                    ! base_type::isNonNullAndSame( m_cmdBuffer.peerId, base_type::m_peerId ) &&
                    ! base_type::isNonNullAndSame( m_cmdBuffer.peerId, m_targetPeerId )
                    )
                {
                    /*
                     * The remote peer id is obtained during the client protocol negotiation message
                     *
                     * We only update it if it is different than the one we already obtained previously,
                     * but we also make sure that if it is not nil() then it is different than
                     * base_type::m_peerId and m_targetPeerId
                     */

                    base_type::m_remotePeerId = m_cmdBuffer.peerId;
                }
            }

            void sendRecvData()
            {
                BL_ASSERT( m_chunkId != uuids::nil() );

                if( CommandBlock::CntrlCodePutDataBlock == m_cmdBuffer.cntrlCode )
                {
                    /*
                     * We're sending data
                     */

                    BL_ASSERT( m_dataRawPtr );

                    asio::async_write(
                        getStream(),
                        asio::buffer( m_dataRawPtr -> begin(), m_dataRawPtr -> size() ),
                        untilCanceled(),
                        cpp::bind(
                            &this_type::onCommandAckRead,
                            om::ObjPtrCopyable< this_type >::acquireRef( this ),
                            m_dataRawPtr -> size()                                      /* bytesExpected */,
                            CommandBlock::CntrlCodePutDataBlock                         /* cntrlCodeExpected */,
                            true                                                        /* terminationPacket */,
                            asio::placeholders::error,
                            asio::placeholders::bytes_transferred
                            )
                        );
                }
                else if( CommandBlock::CntrlCodeGetDataBlockSize == m_cmdBuffer.cntrlCode )
                {
                    /*
                     * The data block size was received and acknowledged
                     *
                     * Let's now request the data itself
                     */

                    requestData();
                }
                else if( CommandBlock::CntrlCodeGetDataBlock == m_cmdBuffer.cntrlCode )
                {
                    /*
                     * We're receiving data
                     */

                    BL_ASSERT( m_dataRawPtr );
                    BL_ASSERT( m_cmdBuffer.chunkSize == m_dataRawPtr -> size() );

                    asio::async_read(
                        getStream(),
                        asio::buffer( m_dataRawPtr -> begin(), m_dataRawPtr -> size() ),
                        untilCanceled(),
                        cpp::bind(
                            &this_type::onTransferCompleted,
                            om::ObjPtrCopyable< this_type >::acquireRef( this ),
                            m_dataRawPtr -> size(),
                            asio::placeholders::error,
                            asio::placeholders::bytes_transferred
                            )
                        );
                }
                else
                {
                    BL_ASSERT( false && "Invalid control code in sendRecvData() call" );
                }
            }

            void requestData()
            {
                BL_CHK(
                    false,
                    m_cmdBuffer.chunkSize && m_cmdBuffer.chunkSize < base_type::MAX_CHUNK_SIZE,
                    BL_MSG()
                        << "Invalid chunk size was received from the network"
                    );

                getBlock( m_cmdBuffer.chunkSize );

                BL_ASSERT( 0U == m_cmdBuffer.flags );
                BL_ASSERT( 0U == m_cmdBuffer.errorCode );

                m_cmdBuffer.chunkId = m_chunkId;

                sendCommandPacket( CommandBlock::CntrlCodeGetDataBlock, false /* isTerminationPacket */ );
            }

            void onTransferCompleted(
                SAA_in                  const std::size_t                               bytesExpected,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                detail::chkPartialDataTransfer( bytesExpected == bytesTransferred );

                ++base_type::m_noOfBlocksTransferred;

                BL_TASKS_HANDLER_END()
            }

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT OVERRIDE
            {
                if( base_type::isExpectedException( eptr, exception, ec ) )
                {
                    return true;
                }

                if( eptr )
                {
                    /*
                     * Server error exceptions are considered expected and should not be dumped in
                     * the logs here as these are expected to be handled (or logged) up in the stack
                     */

                    try
                    {
                        cpp::safeRethrowException( eptr );
                    }
                    catch( ServerErrorException& )
                    {
                        return true;
                    }
                    catch( std::exception& )
                    {
                        /*
                         * For the other exceptions just do nothing and
                         * fall through which will return 'false'
                         */
                    }
                }

                return false;
            }

            virtual auto onTaskStoppedNothrow( SAA_in_opt const std::exception_ptr& eptrIn ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                /*
                 * After the task is executed successfully we reset back the command id to
                 * NoCommand, but if the task fails we want to keep the command id, so it
                 * can be retried
                 *
                 * The command id is expected to be set always before the task is rescheduled
                 * with new data
                 */

                m_commandId = CommandId::NoCommand;

                return base_type::onTaskStoppedNothrow( eptrIn );
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                if( m_protocolOperationsOnly )
                {
                    /*
                     * We have been requested to execute the protocol operations only
                     * (i.e. handshake and / or shutdown) - just delegate this to the
                     * STREAM base class
                     */

                    base_type::scheduleProtocolOperations( eq );
                    return;
                }

                /*
                 * Save the thread pool, create the timer and schedule it immediately
                 */

                BL_ASSERT( eq );
                base_type::ensureChannelIsOpen();

                BL_UNUSED( eq );

                /*
                 * Validate command id and the data expected for each command
                 */

                BL_ASSERT( validatePayload() );

                startCommand();
            }

            virtual void onStreamChanging( SAA_in const stream_ref& /* streamNew */ ) NOEXCEPT OVERRIDE
            {
                /*
                 * If the connection is changing we need to re-negotiate the version with the server
                 */

                m_clientVersionNegotiated = false;
                base_type::m_remotePeerId = uuids::nil();
            }

        public:

            bool isClientVersionNegotiated() const NOEXCEPT
            {
                return m_clientVersionNegotiated;
            }

            bool isAuthenticated() const NOEXCEPT
            {
                return m_isAuthenticated;
            }

            void isAuthenticated( SAA_in const bool isAuthenticated ) NOEXCEPT
            {
                m_isAuthenticated = isAuthenticated;
            }

            const om::ObjPtr< data::DataBlock >& getChunkData() const NOEXCEPT
            {
                return m_dataLocalCopy;
            }

            const data::DataBlock* getChunkDataPtr() const NOEXCEPT
            {
                return m_dataRawPtr;
            }

            void setChunkData( SAA_in const om::ObjPtr< data::DataBlock >& chunkData ) NOEXCEPT
            {
                m_dataLocalCopy = om::copy( chunkData );
                m_dataRawPtr = m_dataLocalCopy.get();
            }

            om::ObjPtr< data::DataBlock > detachChunkData() NOEXCEPT
            {
                om::ObjPtr< data::DataBlock > chunkData( std::move( m_dataLocalCopy ) );

                BL_ASSERT( ! m_dataLocalCopy );
                m_dataRawPtr = nullptr;

                return chunkData;
            }

            void setChunkId( SAA_in const uuid_t& chunkId )
            {
                setCommandInfoStateInternal(
                    m_commandId,
                    chunkId,
                    m_blockType,
                    m_dataRawPtr
                    );
            }

            auto getBlockType() const NOEXCEPT -> BlockTransferDefs::BlockType
            {
                return m_blockType;
            }

            void setBlockType( SAA_in const BlockTransferDefs::BlockType blockType )
            {
                setCommandInfoStateInternal(
                    m_commandId,
                    m_chunkId,
                    blockType,
                    m_dataRawPtr
                    );
            }

            auto getCommandId() const NOEXCEPT -> CommandId
            {
                return m_commandId;
            }

            void setCommandId( SAA_in const typename this_type::CommandId commandId )
            {
                setCommandInfoStateInternal(
                    commandId,
                    m_chunkId,
                    m_blockType,
                    m_dataRawPtr
                    );
            }

            const uuid_t& targetPeerId() const NOEXCEPT
            {
                return m_targetPeerId;
            }

            void targetPeerId( SAA_in const uuid_t& targetPeerId ) NOEXCEPT
            {
                m_targetPeerId = targetPeerId;
            }

            void setCommandInfo(
                SAA_in                  const typename this_type::CommandId             commandId,
                SAA_in_opt              const uuid_t&                                   chunkId = uuids::nil(),
                SAA_in_opt              om::ObjPtr< data::DataBlock >&&                 chunkData = nullptr,
                SAA_in_opt              const BlockTransferDefs::BlockType              blockType =
                    BlockTransferDefs::BlockType::Normal
                )
            {
                setCommandInfoStateInternal(
                    commandId,
                    chunkId,
                    blockType,
                    m_dataLocalCopy.get() /* dataRawPtr */
                    );

                m_dataLocalCopy = BL_PARAM_FWD( chunkData );
                m_dataRawPtr = m_dataLocalCopy.get();
            }

            void setCommandInfoRawPtr(
                SAA_in                  const typename this_type::CommandId             commandId,
                SAA_in_opt              const uuid_t&                                   chunkId = uuids::nil(),
                SAA_in_opt              data::DataBlock*                                dataRawPtr = nullptr,
                SAA_in_opt              const BlockTransferDefs::BlockType              blockType =
                    BlockTransferDefs::BlockType::Normal
                )
            {
                setCommandInfoStateInternal( commandId, chunkId, blockType, dataRawPtr );

                m_dataLocalCopy.reset();
                m_dataRawPtr = dataRawPtr;
            }

            std::uint32_t clientVersion() const NOEXCEPT
            {
                return m_clientVersion;
            }

            void clientVersion( SAA_in const std::uint32_t clientVersion )
            {
                BL_CHK_ARG(
                    clientVersion == CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1 ||
                    clientVersion == CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2,
                    "clientVersion"
                    );

                m_clientVersion = clientVersion;

                /*
                 * Reset back m_clientVersionNegotiated to false so the version can be re-negotiated
                 * again with the server
                 *
                 * We also reset m_remotePeerId as it needs to be re-negotiated again as part of the
                 * client version negotiation message
                 */

                m_clientVersionNegotiated = false;
                base_type::m_remotePeerId = uuids::nil();
            }

            bool protocolOperationsOnly() const NOEXCEPT
            {
                return m_protocolOperationsOnly;
            }

            void protocolOperationsOnly( SAA_in const bool protocolOperationsOnly ) NOEXCEPT
            {
                m_protocolOperationsOnly = protocolOperationsOnly;
            }
        };

        template
        <
            typename STREAM
        >
        using TcpBlockTransferClientConnectionImpl = om::ObjectImpl< TcpBlockTransferClientConnectionT< STREAM > >;

        /**************************************************************************************************************
         */

        /**
         * @brief class TcpBlockServerOutgoingBackendState - The shared state between TcpBlockServerOutgoing and
         * TcpBlockTransferClientAutoPushConnection and BrokerDispatchingBackendProcessing
         */

        template
        <
            typename E = void
        >
        class TcpBlockServerOutgoingBackendStateT : public om::ObjectDefaultBase
        {
            BL_CTR_DEFAULT( TcpBlockServerOutgoingBackendStateT, protected )

        protected:

            typedef messaging::MessageBlockCompletionQueue                                  queue_t;
            typedef cpp::SortedVectorHelper< om::ObjPtr< queue_t > >                        helper_t;
            typedef helper_t::vector_t                                                      vector_t;

            struct PeerInfo
            {
                vector_t                                                                    activeQueues;
                vector_t                                                                    unconfirmedQueues;
                cpp::ScalarTypeIniter< vector_t::size_type >                                currentPos;
            };

            helper_t                                                                        m_helper;
            std::unordered_map< bl::uuid_t, PeerInfo >                                      m_peersInfo;
            std::unordered_map< om::ObjPtr< queue_t >, bl::uuid_t >                         m_connections2PeerId;
            os::mutex                                                                       m_lock;

            PeerInfo& getPeerInfo( SAA_in const bl::uuid_t& remotePeerId )
            {
                const auto pos = m_peersInfo.find( remotePeerId );

                BL_CHK(
                    false,
                    pos != m_peersInfo.end(),
                    BL_MSG()
                        << "Attempting to operate on a queue which has no peer info available"
                    );

                return pos -> second;
            }

            void verifyRegisteredQueue(
                SAA_in                  const bl::uuid_t&                                   remotePeerId,
                SAA_in                  const om::ObjPtr< queue_t >&                        queue,
                SAA_in                  const bool                                          unregister = false
                )
            {
                const auto pos = m_connections2PeerId.find( queue );

                BL_CHK(
                    false,
                    pos != m_connections2PeerId.end(),
                    BL_MSG()
                        << "Attempting to operate on a queue which is not registered"
                    );

                BL_CHK(
                    false,
                    pos -> second == remotePeerId,
                    BL_MSG()
                        << "Attempting to operate on a queue with unexpected peerId "
                        << remotePeerId
                    );

                auto& peerInfo = getPeerInfo( remotePeerId );

                if( unregister )
                {
                    m_helper.erase( peerInfo.activeQueues, queue );
                    m_helper.erase( peerInfo.unconfirmedQueues, queue );

                    m_connections2PeerId.erase( pos );
                }
            }

        public:

            void registerQueue(
                SAA_in                  const bl::uuid_t&                                   remotePeerId,
                SAA_in                  om::ObjPtr< queue_t >&&                             queue
                )
            {
                BL_MUTEX_GUARD( m_lock );

                BL_CHK(
                    false,
                    remotePeerId != uuids::nil(),
                    BL_MSG()
                        << "A queue cannot be registered because the remote peer id is not available"
                    );

                BL_CHK(
                    true,
                    cpp::contains( m_connections2PeerId, queue ),
                    BL_MSG()
                        << "A queue is attempting to register in the backend twice"
                    );

                auto& peerInfo = m_peersInfo[ remotePeerId ];
                auto& activeQueues = peerInfo.activeQueues;

                /*
                 * All active connections are now going to be requested to do heartbeat and confirm
                 * that they are alive by moving them into the unconfirmed list and making this
                 * single connection being registered to be the only one for a short period of time
                 *
                 * We assume the other connections will confirm themselves quickly and join the
                 * active list which is being iterated in round robin fashion
                 */

                while( ! activeQueues.empty() )
                {
                    auto& localQueue = activeQueues.back();

                    localQueue -> requestHeartbeat();

                    const auto pair = m_helper.insert( peerInfo.unconfirmedQueues, std::move( localQueue ) );

                    BL_CHK(
                        false,
                        pair.second,
                        BL_MSG()
                            << "A queue is already registered in the unconfirmed list"
                        );

                    BL_ASSERT( nullptr == localQueue );

                    activeQueues.erase( activeQueues.end() - 1 );
                }

                BL_ASSERT( activeQueues.empty() );

                peerInfo.currentPos = 0;

                BL_VERIFY( m_helper.insert( activeQueues, om::copy( queue ) ).second );

                auto g = BL_SCOPE_GUARD(
                    {
                        activeQueues.clear();
                    }
                    );

                BL_VERIFY( m_connections2PeerId.emplace( std::move( queue ), remotePeerId ).second );

                g.dismiss();
            }

            void confirmQueue(
                SAA_in                  const bl::uuid_t&                                   remotePeerId,
                SAA_in                  const om::ObjPtr< queue_t >&                        queue
                )
            {
                BL_MUTEX_GUARD( m_lock );

                verifyRegisteredQueue( remotePeerId, queue );

                /*
                 * Simply move the queue from the unconfirmed list into the confirmed list
                 */

                auto& peerInfo = m_peersInfo.at( remotePeerId );

                auto pos = m_helper.find( peerInfo.unconfirmedQueues, queue );

                BL_CHK(
                    false,
                    pos != peerInfo.unconfirmedQueues.end(),
                    BL_MSG()
                        << "Attempting to confirm a queue which is already confirmed"
                    );

                BL_VERIFY( m_helper.insert( peerInfo.activeQueues, std::move( *pos ) ).second );

                peerInfo.unconfirmedQueues.erase( pos );
            }

            void unregisterQueue(
                SAA_in                  const bl::uuid_t&                                   remotePeerId,
                SAA_in                  const om::ObjPtr< queue_t >&                        queue
                )
            {
                BL_MUTEX_GUARD( m_lock );

                verifyRegisteredQueue( remotePeerId, queue, true /* unregister */ );
            }

            auto tryGetQueue( SAA_in const bl::uuid_t& remotePeerId ) -> om::ObjPtr< queue_t >
            {
                om::ObjPtr< queue_t > result;

                {
                    BL_MUTEX_GUARD( m_lock );

                    /*
                     * We just rotate the active queues in a round robin fashion
                     */

                    const auto pos = m_peersInfo.find( remotePeerId );

                    if( pos != m_peersInfo.end() )
                    {
                        auto& peerInfo = pos -> second;
                        auto& activeQueues = peerInfo.activeQueues;

                        if( ! activeQueues.empty() )
                        {
                            if( peerInfo.currentPos >= activeQueues.size() )
                            {
                                peerInfo.currentPos = 0U;
                            }

                            result = om::copy( activeQueues[ peerInfo.currentPos ] );

                            ++peerInfo.currentPos;
                        }
                    }
                }

                return result;
            }

            auto activeTasksCount() const -> std::size_t
            {
                BL_MUTEX_GUARD( m_lock );

                return m_connections2PeerId.size();
            }

            auto getAllActiveQueuesIds() -> std::unordered_set< uuid_t >
            {
                std::unordered_set< uuid_t > result;

                BL_MUTEX_GUARD( m_lock );

                result.reserve( m_peersInfo.size() );

                for( const auto& peerInfoPair : m_peersInfo )
                {
                    const auto& peerInfo = peerInfoPair.second;

                    if( peerInfo.activeQueues.size() || peerInfo.unconfirmedQueues.size() )
                    {
                        result.insert( peerInfoPair.first /* peerId */ );
                    }
                }

                return result;
            }
        };

        typedef om::ObjectImpl< TcpBlockServerOutgoingBackendStateT<> > TcpBlockServerOutgoingBackendState;

        /**************************************************************************************************************
         */

        /**
         * @brief class TcpBlockTransferClientAutoPushConnection - An auto push client connection implementation
         *
         * It will be implemented as a wrapper task which will have a queue and wait for blocks to be sent to it
         * and then push them out to the server (in this case the server will typically be the messaging client)
         *
         * When there are no blocks there will be periodic timer to check if the connection is still alive by
         * sending heartbeat / ping messages
         */

        template
        <
            typename STREAM
        >
        class TcpBlockTransferClientAutoPushConnectionT :
            public WrapperTaskBase,
            public messaging::MessageBlockCompletionQueue
        {
        public:

            typedef tasks::WrapperTaskBase                                                  base_type;
            typedef TcpBlockTransferClientAutoPushConnectionT< STREAM >                     this_type;
            typedef messaging::MessageBlockCompletionQueue                                  queue_t;

            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( TcpBlockTransferClientAutoPushConnectionT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY_CHAIN_BASE( base_type )
                BL_QITBL_ENTRY( messaging::MessageBlockCompletionQueue )
            BL_QITBL_END( Task )

        public:

            enum : std::size_t
            {
                BLOCK_QUEUE_SIZE                        = 128U,
                DEFAULT_HEARTBEAT_INTERVAL_IN_SECONDS   = 30U,
            };

            enum class NotifyEventId
            {
                Register,
                Confirm,
                Unregister,
            };

            typedef cpp::function
            <
                void (
                    SAA_in              const NotifyEventId                                 evendId,
                    SAA_in              const om::ObjPtr< queue_t >&                        queue
                    )
            >
            notify_callback_t;

        protected:

            struct DataBlockInfo
            {
                uuid_t                                                                      targetPeerId;
                om::ObjPtrCopyable< data::DataBlock >                                       dataBlock;
                CompletionCallback                                                          callback;
            };

            typedef tasks::TcpBlockTransferClientConnectionImpl< STREAM >                   connection_t;

            cpp::circular_buffer< DataBlockInfo >                                           m_pendingQueue;
            const notify_callback_t                                                         m_notifyCallback;
            const om::ObjPtr< connection_t >                                                m_connectionImpl;
            const om::ObjPtr< Task >                                                        m_connectionTask;
            const om::ObjPtr< Task >                                                        m_heartbeatTask;

            cpp::ScalarTypeIniter< bool >                                                   m_stopWasRequested;
            cpp::ScalarTypeIniter< bool >                                                   m_heartbeatWasRequested;
            time::ptime                                                                     m_lastSuccessfulHeartbeat;
            cpp::ScalarTypeIniter< bool >                                                   m_activated;
            std::exception_ptr                                                              m_originalException;
            cpp::ScalarTypeIniter< bool >                                                   m_taskTerminated;

            TcpBlockTransferClientAutoPushConnectionT(
                SAA_in_opt              notify_callback_t&&                                 notifyCallback,
                SAA_in                  typename STREAM::stream_ref&&                       connectedStream,
                SAA_in                  const om::ObjPtr< data::datablocks_pool_type >&     dataBlocksPool,
                SAA_in                  const uuid_t&                                       peerId,
                SAA_in_opt              time::time_duration&&                               heartbeatInterval
                    = time::seconds( DEFAULT_HEARTBEAT_INTERVAL_IN_SECONDS )
                )
                :
                m_pendingQueue( BLOCK_QUEUE_SIZE ),
                m_notifyCallback( BL_PARAM_FWD( notifyCallback ) ),
                m_connectionImpl(
                    connection_t::createInstance(
                        connection_t::CommandId::NoCommand,
                        peerId,
                        dataBlocksPool,
                        tasks::BlockTransferDefs::BlockType::Normal
                        )
                    ),
                m_connectionTask( om::qi< tasks::Task >( m_connectionImpl ) ),
                m_heartbeatTask(
                    SimpleTimerTask::createInstance< tasks::Task >(
                        []() -> bool
                        {
                            return false;
                        },
                        time::neg_infin                         /* duration */,
                        BL_PARAM_FWD( heartbeatInterval )       /* initDelay */
                        )
                    ),
                m_lastSuccessfulHeartbeat( time::neg_infin )
            {
                m_connectionImpl -> attachStream( BL_PARAM_FWD( connectedStream ) );
                m_connectionImpl -> clientVersion( tasks::detail::CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2 );

                m_wrappedTask = om::copy( m_connectionTask );
            }

            ~TcpBlockTransferClientAutoPushConnectionT() NOEXCEPT
            {
                /*
                 * When this task is destroyed m_pendingQueue should always be empty otherwise it means that
                 * some blocks were pushed before the task was started, but then the task was never started
                 * which means they will never be delivered and their callbacks will never be called which
                 * can cause some external completion tasks to never finish (if they are waiting on the
                 * callbacks to be called) which of course is not safe as it can potentially cause a hang
                 */

                if( ! m_pendingQueue.empty() )
                {
                    BL_RIP_MSG( "TcpBlockTransferClientAutoPushConnectionT is destroyed with pending blocks" );
                }
            }

            void scheduleNow()
            {
                if( m_wrappedTask == m_heartbeatTask )
                {
                    /*
                     * We are currently running the heartbeat timer task let's cancel it
                     * so a heartbeat or data block can be re-scheduled immediately
                     */

                    m_heartbeatTask -> requestCancel();
                }
            }

        public:

            std::uint64_t noOfBlocksTransferred() const NOEXCEPT
            {
                return m_connectionImpl -> noOfBlocksTransferred();
            }

            auto connection() const NOEXCEPT -> const om::ObjPtr< connection_t >&
            {
                return m_connectionImpl;
            }

            bool isRemotePeerIdAvailable() const NOEXCEPT
            {
                return m_connectionImpl -> isRemotePeerIdAvailable();
            }

            auto lastSuccessfulHeartbeat() const -> time::ptime
            {
                time::ptime lastSuccessfulHeartbeat;

                {
                    BL_MUTEX_GUARD( m_lock );

                    lastSuccessfulHeartbeat = m_lastSuccessfulHeartbeat;
                }

                return lastSuccessfulHeartbeat;
            }

            virtual void scheduleNothrow(
                SAA_in                  const std::shared_ptr< ExecutionQueue >&    eq,
                SAA_in                  cpp::void_callback_noexcept_t&&             callbackReady
                ) NOEXCEPT OVERRIDE
            {
                if( m_taskTerminated )
                {
                    BL_RIP_MSG( "This task can't be rescheduled after it has been terminated" );
                }

                base_type::scheduleNothrow( eq, BL_PARAM_FWD( callbackReady ) );
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                m_stopWasRequested = true;

                BL_NOEXCEPT_END()

                base_type::requestCancel();
            }

            virtual om::ObjPtr< tasks::Task > continuationTask() OVERRIDE
            {
                auto task = base_type::handleContinuationForward();

                if( task )
                {
                    return task;
                }

                /*
                 * If the wrapper task is neither m_connectionTask nor m_heartbeatTask then
                 * it is the shutdown task and the exception should be obtained from the
                 * original exception
                 */

                auto exception = m_originalException ? m_originalException : this_type::exception();

                DataBlockInfo blockInfoNotify;
                cpp::circular_buffer< DataBlockInfo > queueNotify( BLOCK_QUEUE_SIZE );

                bool safeToContinue = true;

                {
                    BL_MUTEX_GUARD( m_lock );

                    do
                    {
                        bool terminatedGracefully = false;

                        /*
                         * If the task is externally canceled that means the acceptor is shutting down and we
                         * should indeed cancel (i.e. no more continuations)
                         *
                         * Otherwise we will schedule a continuation depending on which task is currently executing
                         */

                        if( m_stopWasRequested && ! exception )
                        {
                            terminatedGracefully = true;

                            exception = std::make_exception_ptr(
                                SystemException::create(
                                    asio::error::operation_aborted,
                                    BL_SYSTEM_ERROR_DEFAULT_MSG
                                    )
                                );
                        }

                        if( exception && ! messaging::BrokerErrorCodes::isExpectedException( exception ) )
                        {
                            if( ! terminatedGracefully )
                            {
                                BL_LOG_MULTILINE(
                                    Logging::trace(),
                                    BL_MSG()
                                        << "Unexpected exception occurred in TcpBlockTransferClientAutoPushConnection:\n"
                                        << eh::diagnostic_information( exception )
                                    );
                            }

                            if( m_activated )
                            {
                                if( m_notifyCallback )
                                {
                                    m_notifyCallback( NotifyEventId::Unregister, om::copyAs< queue_t >( this ) );
                                }

                                m_activated = false;
                            }

                            /*
                             * The task is about to terminate and we need to make sure
                             * that the socket from the connection task is closed
                             * promptly regardless of the lifetime of the task object
                             *
                             * This way the remote party should be notified promptly
                             * and can terminate
                             *
                             * If there is protocol which requires shutdown (e.g. SSL)
                             * then we call the isCloseStreamOnTaskFinish( true )
                             * setter and schedule the protocol shutdown operation;
                             * otherwise we simply close the socket via calling
                             * m_connectionImpl -> chkToCloseSocket()
                             *
                             * This code is necessary because the actual lifetime of
                             * the task cannot be controlled and when the acceptor is
                             * stopped the task can still be held in the internal data
                             * structures of the server and if there are client(s)
                             * connecting and waiting on the socket (e.g. on SSL
                             * handshake) they can hang forever
                             */

                            if( m_connectionImpl -> isShutdownNeeded() )
                            {
                                m_originalException = exception;

                                m_connectionImpl -> setCommandInfo( connection_t::CommandId::NoCommand );
                                m_connectionImpl -> protocolOperationsOnly( true );
                                m_connectionImpl -> isCloseStreamOnTaskFinish( true );

                                m_wrappedTask = om::copy( m_connectionTask );

                                return om::copyAs< Task >( this );
                            }
                            else
                            {
                                m_connectionImpl -> chkToCloseSocket();
                            }

                            if( ! m_pendingQueue.empty() )
                            {
                                queueNotify.swap( m_pendingQueue );
                            }

                            safeToContinue = false;

                            break;
                        }

                        if( exception && ! terminatedGracefully )
                        {
                            BL_LOG_MULTILINE(
                                Logging::trace(),
                                BL_MSG()
                                    << "Expected exception occurred in TcpBlockTransferClientAutoPushConnection:\n"
                                    << eh::diagnostic_information( exception )
                                );
                        }

                        if( m_wrappedTask == m_connectionTask )
                        {
                            const auto* dataPtr = m_connectionImpl -> getChunkDataPtr();

                            if( dataPtr )
                            {
                                /*
                                 * A normal block message has been sent successfully
                                 */

                                BL_ASSERT( ! m_pendingQueue.empty() );
                                BL_ASSERT( dataPtr == m_pendingQueue.front().dataBlock.get() );

                                blockInfoNotify = std::move( m_pendingQueue.front() );
                                m_pendingQueue.pop_front();
                            }
                            else
                            {
                                BL_ASSERT( ! exception );

                                BL_LOG(
                                    Logging::trace(),
                                    BL_MSG()
                                        << "Heartbeat was sent successfully to remote peer with id "
                                        << str::quoteString(
                                            uuids::uuid2string( m_connectionImpl -> remotePeerId() )
                                            )
                                    );
                            }

                            if( ! exception )
                            {
                                /*
                                 * Regardless if we sent a block or this was a pure heartbeat message
                                 * we always update m_lastSuccessfulHeartbeat and check if we need to
                                 * call the notify callback / activate the task
                                 */

                                BL_ASSERT( m_connectionImpl -> remotePeerId() != uuids::nil() );

                                m_lastSuccessfulHeartbeat = time::microsec_clock::universal_time();

                                if( ! m_activated )
                                {
                                    if( m_notifyCallback )
                                    {
                                        m_notifyCallback( NotifyEventId::Register, om::copyAs< queue_t >( this ) );
                                    }

                                    m_activated = true;
                                }

                                if( m_activated && m_heartbeatWasRequested )
                                {
                                    if( m_notifyCallback )
                                    {
                                        m_notifyCallback( NotifyEventId::Confirm, om::copyAs< queue_t >( this ) );
                                    }

                                    m_heartbeatWasRequested = false;
                                }
                            }
                        }

                        BL_ASSERT( m_activated );

                        if( m_pendingQueue.empty() )
                        {
                            m_connectionImpl -> setCommandInfo( connection_t::CommandId::NoCommand );

                            /*
                             * If there are no blocks pending we check if the current task was
                             * m_connectionTask that means we have either just sent a block or
                             * a heartbeat and we need to wait for the heartbeat interval or
                             * if the current task was m_heartbeatTask then we need to schedule
                             * a connection task to send a heartbeat
                             */

                            m_wrappedTask =
                                om::copy( m_wrappedTask == m_connectionTask ? m_heartbeatTask : m_connectionTask );
                        }
                        else
                        {
                            const auto& blockInfo = m_pendingQueue.front();

                            m_connectionImpl -> setCommandInfoRawPtr(
                                connection_t::CommandId::SendChunk,
                                uuids::create()                                /* chunkId */,
                                blockInfo.dataBlock.get()                      /* dataRawPtr */,
                                BlockTransferDefs::BlockType::Normal           /* blockType */
                                );

                            m_connectionImpl -> targetPeerId( blockInfo.targetPeerId );

                            /*
                             * If we are here that means we have a block to send
                             *
                             * Simply check to schedule m_connectionTask if it is
                             * not already the one that is scheduled
                             */

                            if( m_wrappedTask != m_connectionTask )
                            {
                                m_wrappedTask = om::copy( m_connectionTask );
                            }
                        }
                    }
                    while( false );

                    if( ! safeToContinue )
                    {
                         m_taskTerminated = true;
                    }
                }

                /*
                 * Callbacks are always called outside of holding any locks
                 */

                if( ! queueNotify.empty() )
                {
                    for( const auto& blockInfo : queueNotify )
                    {
                        if( blockInfo.callback )
                        {
                            blockInfo.callback( exception );
                        }
                    }
                }

                if( blockInfoNotify.callback )
                {
                    blockInfoNotify.callback( exception );
                }

                return safeToContinue ? om::copyAs< Task >( this ) : nullptr;
            }

            virtual void requestHeartbeat() OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                m_heartbeatWasRequested = true;

                scheduleNow();
            }

            virtual bool tryScheduleBlock(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  om::ObjPtr< data::DataBlock >&&                     dataBlock,
                SAA_in                  CompletionCallback&&                                callback
                ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                /*
                 * The current logic is that the task is considered connected and ready to accept
                 * data blocks even before it has started executing, so basically it is ready to
                 * accept blocks as soon as it is created (with the assumption that it will start
                 * executing and flush these blocks sooner or later)
                 *
                 * However if the task has started executing it can only accept data blocks
                 * until it has been terminated which means we should only check for
                 * m_taskTerminated which is set when the task is about to finish or in the
                 * process of executing its last SSL shutdown execution (if the connection was SSL)
                 */

                BL_CHK_T_USER_FRIENDLY(
                    true,
                    m_taskTerminated,
                    NotSupportedException()
                        << eh::errinfo_error_uuid( messaging::uuiddefs::ErrorUuidNotConnectedToBroker() ),
                    "Messaging client is not connected to messaging broker"
                    );

                if( m_pendingQueue.full() )
                {
                    return false;
                }

                DataBlockInfo blockInfo;

                blockInfo.targetPeerId = targetPeerId;
                blockInfo.dataBlock = BL_PARAM_FWD( dataBlock );
                blockInfo.callback = BL_PARAM_FWD( callback );

                m_pendingQueue.push_back( std::move( blockInfo ) );

                scheduleNow();

                return true;
            }
        };

        template
        <
            class STREAM
        >
        using TcpBlockTransferClientAutoPushConnectionImpl =
            om::ObjectImpl< TcpBlockTransferClientAutoPushConnectionT< STREAM > >;

        /**************************************************************************************************************
         */

        /**
         * @brief class TcpBlockServerOutgoing - The block server implementation for outgoing data (the acceptor)
         */

        template
        <
            typename STREAM,
            typename SERVERPOLICY = TcpServerPolicyDefault
        >
        class TcpBlockServerOutgoingT :
            public TcpServerBase< STREAM, SERVERPOLICY >
        {
            BL_DECLARE_OBJECT_IMPL( TcpBlockServerOutgoingT )

        public:

            typedef TcpBlockServerOutgoingT< STREAM, SERVERPOLICY >                             this_type;
            typedef TcpServerBase< STREAM, SERVERPOLICY >                                       base_type;
            typedef STREAM                                                                      stream_t;
            typedef TcpBlockTransferClientAutoPushConnectionImpl< STREAM >                      connection_t;
            typedef typename connection_t::NotifyEventId                                        NotifyEventId;
            typedef typename connection_t::queue_t                                              queue_t;
            typedef TcpBlockServerOutgoingBackendState                                          backend_state_t;

        protected:

            const om::ObjPtr< data::datablocks_pool_type >                                      m_dataBlocksPool;
            const time::time_duration                                                           m_heartbeatInterval;
            const om::ObjPtr< backend_state_t >                                                 m_backendState;
            const uuid_t                                                                        m_peerId;

            TcpBlockServerOutgoingT(
                SAA_in              const om::ObjPtr< TaskControlTokenRW >&                     controlToken,
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
                base_type( controlToken, BL_PARAM_FWD( host ), port, privateKeyPem, certificatePem ),
                m_dataBlocksPool( om::copy( dataBlocksPool ) ),
                m_heartbeatInterval( BL_PARAM_FWD( heartbeatInterval ) ),
                m_backendState( TcpBlockServerOutgoingBackendState::createInstance() ),
                m_peerId( peerId )
            {
            }

            static void cancelConnectionCallback(
                SAA_in              const om::ObjPtrCopyable< connection_t >                    connectionTaskImpl
                )
            {
                connectionTaskImpl -> requestCancel();
            }

            static auto notifyTaskCompletedContinuationCallback(
                SAA_in              const om::ObjPtrCopyable< connection_t >                    connectionTaskImpl,
                SAA_in              Task*                                                       finishedTask
                )
                -> om::ObjPtr< Task >
            {
                /*
                 * If the notify task failed we request the connection to be closed
                 */

                if( finishedTask -> isFailed() )
                {
                    BL_LOG_MULTILINE(
                        Logging::trace(),
                        BL_MSG()
                            << "peerConnectedNotify() call failed with the following exception:\n"
                            << eh::diagnostic_information( finishedTask -> exception() )
                        );

                    return SimpleTaskImpl::createInstance< Task >(
                        cpp::bind( &this_type::cancelConnectionCallback, connectionTaskImpl )
                        );
                }

                return nullptr;
            }

            static void notifyCallback(
                SAA_in_opt          const om::ObjPtrCopyable< om::Proxy >                       hostServices,
                SAA_in_opt          const om::ObjPtrCopyable< om::Proxy >                       executionServices,
                SAA_in              const om::ObjPtrCopyable< backend_state_t >                 backendState,
                SAA_in              const NotifyEventId                                         evendId,
                SAA_in              const om::ObjPtr< queue_t >&                                queue
                )
            {
                const auto taskImpl = om::qi< connection_t >( queue );

                const auto remotePeerId = taskImpl -> connection() -> remotePeerId();

                BL_ASSERT( remotePeerId != uuids::nil() );

                switch( evendId )
                {
                    default:
                        BL_ASSERT( false );
                        break;

                    case NotifyEventId::Register:
                        backendState -> registerQueue( remotePeerId, om::copy( queue ) );
                        break;

                    case NotifyEventId::Confirm:
                        backendState -> confirmQueue( remotePeerId, queue );
                        break;

                    case NotifyEventId::Unregister:
                        backendState -> unregisterQueue( remotePeerId, queue );
                        break;
                }

                if( NotifyEventId::Register == evendId && hostServices && executionServices )
                {
                    os::mutex_unique_lock guardNotify;
                    os::mutex_unique_lock guardExecutionServices;

                    typedef messaging::AcceptorNotify notify_t;

                    const auto acceptorNotify =
                        hostServices -> tryAcquireRef< notify_t >( notify_t::iid(), &guardNotify );

                    const auto executionQueue =
                        executionServices -> tryAcquireRef< ExecutionQueue >(
                            ExecutionQueue::iid(),
                            &guardExecutionServices
                            );

                    if( acceptorNotify && executionQueue )
                    {
                        /*
                         * If we are here that means that host services and execution services were
                         * provided and they are not disconnected yet
                         *
                         * We can now schedule the notify task and the respective continuation task
                         * which will cancel the connection task if peerConnectedNotify() fails
                         *
                         * Note that while doing this we are holding the locks, so the notify and
                         * execution services can't be disconnected
                         */

                        const auto notifyTaskImpl = ExternalCompletionTaskIfImpl::createInstance(
                            cpp::bind(
                                &notify_t::peerConnectedNotifyCopyCallback,
                                om::ObjPtrCopyable< notify_t >::acquireRef( acceptorNotify.get() ),
                                remotePeerId,
                                _1 /* completionCallback */
                                )
                            );

                        notifyTaskImpl -> setContinuationCallback(
                            cpp::bind(
                                &this_type::notifyTaskCompletedContinuationCallback,
                                om::ObjPtrCopyable< connection_t >::acquireRef( taskImpl.get() ) /* connectionTaskImpl */,
                                _1 /* finishedTask */
                                )
                            );

                        executionQueue -> push_back( om::qi< Task >( notifyTaskImpl ) );
                    }
                }
            }

            virtual om::ObjPtr< Task > createConnection( SAA_inout typename STREAM::stream_ref&& connectedStream ) OVERRIDE
            {
                return connection_t::template createInstance< Task >(
                    cpp::bind(
                        &this_type::notifyCallback,
                        om::ObjPtrCopyable< om::Proxy >( base_type::m_hostServices ),
                        om::ObjPtrCopyable< om::Proxy >( base_type::m_executionServices ),
                        om::ObjPtrCopyable< backend_state_t >::acquireRef( m_backendState.get() ),
                        _1,
                        _2
                        ),
                    BL_PARAM_FWD( connectedStream ),
                    m_dataBlocksPool,
                    m_peerId,
                    cpp::copy( m_heartbeatInterval )
                    );
            }

        public:

            auto backendState() const NOEXCEPT -> const om::ObjPtr< backend_state_t >&
            {
                return m_backendState;
            }
        };

    } // tasks

} // bl

#endif /* __BL_MESSAGING_TCPBLOCKTRANSFERCLIENT_H_ */
