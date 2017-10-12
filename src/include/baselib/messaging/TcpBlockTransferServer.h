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

#ifndef __BL_MESSAGING_TCPBLOCKTRANSFERSERVER_H_
#define __BL_MESSAGING_TCPBLOCKTRANSFERSERVER_H_

#include <baselib/messaging/TcpBlockTransferCommon.h>

namespace bl
{
    namespace tasks
    {
        namespace detail
        {
            /*********************************************************************************************
             * Server state object
             */

            /**
             * @brief - A state object that is used to pass state from the block transfer server to
             * the block server connection objects
             */

            template
            <
                typename ASYNCWRAPPER
            >
            class BlockTransferServerStateT : public om::ObjectDefaultBase
            {
            protected:

                const om::ObjPtr< data::datablocks_pool_type >                                  m_dataBlocksPool;
                const om::ObjPtr< ASYNCWRAPPER >                                                m_asyncWrapper;

                BlockTransferServerStateT(
                    SAA_in              const om::ObjPtr< data::datablocks_pool_type >&         dataBlocksPool,
                    SAA_in              const om::ObjPtr< ASYNCWRAPPER >&                       asyncWrapper
                    )
                    :
                    m_dataBlocksPool( om::copy( dataBlocksPool ) ),
                    m_asyncWrapper( om::copy( asyncWrapper ) )
                {
                }

            public:

                auto dataBlocksPool() const NOEXCEPT -> const om::ObjPtr< data::datablocks_pool_type >&
                {
                    return m_dataBlocksPool;
                }

                auto asyncWrapper() const NOEXCEPT -> const om::ObjPtr< ASYNCWRAPPER >&
                {
                    return m_asyncWrapper;
                }
            };

            template
            <
                typename ASYNCWRAPPER
            >
            using BlockTransferServerStateImpl = om::ObjectImpl< BlockTransferServerStateT< ASYNCWRAPPER > >;

        } // detail

        /*************************************************************************************************
         * Server connection
         */

        /**
         * @brief TcpBlockTransferServerConnection
         */

        template
        <
            typename STREAM,
            typename ASYNCWRAPPER
        >
        class TcpBlockTransferServerConnection :
            public TcpBlockTransferConnectionBase< STREAM >
        {
            BL_CTR_DEFAULT( TcpBlockTransferServerConnection, protected )
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( TcpBlockTransferServerConnection )

        protected:

            typedef TcpBlockTransferServerConnection< STREAM, ASYNCWRAPPER >            this_type;
            typedef TcpBlockTransferConnectionBase< STREAM >                            base_type;
            typedef typename STREAM::stream_ref                                         stream_ref;

            typedef typename ASYNCWRAPPER::AsyncOperationStateImpl                      AsyncOperationStateImpl;
            typedef typename ASYNCWRAPPER::OperationId                                  OperationId;
            typedef typename ASYNCWRAPPER::CommandId                                    CommandId;

            typedef detail::BlockTransferServerStateImpl< ASYNCWRAPPER >                BlockTransferServerState;

        public:

            typedef typename base_type::CommandBlock                                    CommandBlock;
            typedef cpp::function
                <
                    bool (
                        SAA_in      const BlockTransferDefs::BlockType                  blockType,
                        SAA_in      const std::uint16_t                                 cntrlCode
                        )
                >
                isauthenticationrequired_callback_t;

        protected:

            using base_type::m_cmdBuffer;
            using base_type::untilCanceled;

            typedef cpp::function
                <
                    void (
                        SAA_in                  const om::ObjPtrCopyable< this_type >&  thisObj,
                        SAA_in                  const bool                              newCommand,
                        SAA_in                  const std::size_t                       bytesExpected,
                        SAA_in                  const eh::error_code&                   ec,
                        SAA_in                  const std::size_t                       bytesTransferred
                        ) NOEXCEPT
                >
                callback_t;

            typedef cpp::function
                <
                    void (
                        SAA_in                  const om::ObjPtrCopyable< this_type >&  thisObj,
                        SAA_in                  const AsyncOperation::Result&           result
                        ) NOEXCEPT
                >
                async_result_callback_t;

            const om::ObjPtr< BlockTransferServerState >                                m_serverState;

            om::ObjPtr< AsyncOperation >                                                m_operation;
            om::ObjPtr< AsyncOperationStateImpl >                                       m_operationState;
            cpp::ScalarTypeIniter< BlockTransferDefs::BlockType >                       m_operationBlockType;
            cpp::ScalarTypeIniter< std::uint32_t >                                      m_operationProtocolDataSize;
            cpp::ScalarTypeIniter< bool >                                               m_operationDataValid;

            uuid_t                                                                      m_connectedSessionId;
            cpp::ScalarTypeIniter< std::uint32_t >                                      m_clientProtocolVersion;
            cpp::ScalarTypeIniter< bool >                                               m_isFatalServerError;
            cpp::ScalarTypeIniter< bool >                                               m_isClientAuthenticated;
            cpp::ScalarTypeIniter< bool >                                               m_skipShutdownContinuation;
            cpp::ScalarTypeIniter< bool >                                               m_forceShutdownContinuation;
            isauthenticationrequired_callback_t                                         m_isAuthenticationRequiredCallback;

            TcpBlockTransferServerConnection(
                SAA_in                  const om::ObjPtr< BlockTransferServerState >&   serverState,
                SAA_in_opt              const uuid_t&                                   peerId = uuids::nil(),
                SAA_in_opt              isauthenticationrequired_callback_t&&           isAuthenticationRequiredCallback =
                    isauthenticationrequired_callback_t()
                )
                :
                base_type( peerId ),
                m_serverState( om::copy( serverState ) ),
                m_connectedSessionId( uuids::create() ),
                m_isAuthenticationRequiredCallback( std::move( isAuthenticationRequiredCallback ) )
            {
                base_type::m_name = "TcpTask_BlockTransferServerConnection";

                /*
                 * Request the socket to be closed when the task finishes which will ensure that
                 * the client task will receive error promptly and also that protocol shutdown
                 * is also executed (if any)
                 */

                base_type::isCloseStreamOnTaskFinish( true );
            }

            ~TcpBlockTransferServerConnection() NOEXCEPT
            {
                BL_WARN_NOEXCEPT_BEGIN()

                releaseOperation();

                chk2DisconnectSession();

                BL_WARN_NOEXCEPT_END( "~TcpBlockTransferServerConnection" )
            }

            void checkToPrintExceptionInfo(
                SAA_in                  const bool                                      isFatal,
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           errorCode
                )
            {
                const bool* isExpected = eh::get_error_info< eh::errinfo_is_expected >( exception );

                if( isExpected && *isExpected )
                {
                    /*
                     * This is an expected exception - no need to dump anything in the logs
                     */

                    return;
                }

                if( this_type::isExpectedException( eptr, exception, errorCode ) )
                {
                    /*
                     * This is an expected exception - no need to dump anything in the logs
                     */

                    return;
                }

                const char* message =
                    isFatal ?
                        "\nFatal server exception has occurred (server will initiate shutdown):\n" :
                        "\nNon-fatal server exception has occurred (but could be fatal for the client):\n";

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << message
                        << eh::diagnostic_information( exception )
                        << "ControlCode='"
                        << m_cmdBuffer.cntrlCode
                        << "'; Flags='"
                        << m_cmdBuffer.flags
                        << "'"
                        << "\nSessionId is '"
                        << uuids::uuid2string( m_connectedSessionId )
                        << "'"
                        << "\nChunkId in control buffer is '"
                        << uuids::uuid2string( m_cmdBuffer.chunkId )
                        << "'"
                        << "\nChunkId in connection object is '"
                        << uuids::uuid2string( base_type::m_chunkId )
                        << "'"
                        << "\nChunk size is '"
                        << ( ( m_operationState && m_operationState -> data() ) ? m_operationState -> data() -> size() : 0U )
                        << "'\n"
                    );
            }

            void chk4ServerErrors( SAA_in const cpp::void_callback_t& cb )
            {
                /*
                 * Some specific exceptions will be propagated as server errors
                 * to the client and will terminate the client and these will
                 * have to be thrown as ServerErrorException
                 *
                 * All other exceptions will be considered that something is
                 * wrong with the service or some other fatal situation which
                 * will require to take down the server so the client
                 * can re-connect to a different one in the case
                 * the error situation is transient and / or node specific
                 */

                std::uint32_t errorValue = 0U;

                try
                {
                    cb();
                }
                catch( ServerErrorException& e )
                {
                    /*
                     * Exception is benign from server perspective, but fatal from client
                     * perspective -- propagate to the client
                     */

                    const int* errorNo = eh::get_error_info< eh::errinfo_errno >( e );

                    if( errorNo )
                    {
                        errorValue = *errorNo;
                    }

                    const auto* errorCode = eh::get_error_info< eh::errinfo_error_code >( e );

                    if( ! errorValue )
                    {
                        if( errorCode && errorCode -> category() == eh::generic_category() )
                        {
                            errorValue = errorCode -> value();
                        }
                    }

                    checkToPrintExceptionInfo( false /* isFatal */, std::current_exception(), e, errorCode );

                    /*
                     * Check for some specific server errors to be ignored
                     */

                    if(
                        errorValue &&
                        CommandBlock::CntrlCodeRemoveDataBlock == m_cmdBuffer.cntrlCode &&
                        ( CommandBlock::IgnoreIfNotFound & m_cmdBuffer.data.blockInfo.flags ) &&
                        eh::errc::make_error_code( eh::errc::no_such_file_or_directory ) ==
                            eh::error_code( errorValue, eh::generic_category() )
                        )
                    {
                        errorValue = 0U;
                    }
                }
                catch( eh::system_error& e )
                {
                    /*
                     * Anything else other than asio::error::operation_aborted coming from
                     * the server will be considered fatal for the server
                     */

                    const eh::error_code errorCode = e.code();

                    m_isFatalServerError = ( asio::error::operation_aborted != errorCode );

                    checkToPrintExceptionInfo( m_isFatalServerError, std::current_exception(), e, &errorCode );

                    throw;
                }
                catch( std::exception& e )
                {
                    /*
                     * Remember that we have encountered a fatal server error before we
                     * rethrow the exception and let it propagate to close the connection
                     * and kill the blob server
                     */

                    m_isFatalServerError = true;

                    const auto* errorCode = eh::get_error_info< eh::errinfo_error_code >( e );

                    checkToPrintExceptionInfo( m_isFatalServerError, std::current_exception(), e, errorCode );

                    throw;
                }

                if( errorValue )
                {
                    /*
                     * An error was encountered which needs to be propagated to the client
                     */

                    m_cmdBuffer.flags |= CommandBlock::ErrBit;
                    m_cmdBuffer.errorCode = errorValue;
                }
            }

            void chk2DisconnectSession()
            {
                m_connectedSessionId = uuids::nil();
            }

            const uuid_t& sessionId()
            {
                return m_connectedSessionId;
            }

            void doChkAsyncResult( SAA_in const AsyncOperation::Result& result )
            {
                BL_TASKS_HANDLER_CHK_ASYNC_RESULT_IMPL_THROW( result )
            }

            bool chkAsyncResult( SAA_in const AsyncOperation::Result& result )
            {
                chk4ServerErrors(
                    [ &result, this ]
                    {
                        doChkAsyncResult( result );
                    }
                    );

                if( m_cmdBuffer.flags & CommandBlock::ErrBit )
                {
                    /*
                     * An error was found which needs to be reported to the client
                     */

                    scheduleResponseCommand( true /* newCommand */ );

                    return false;
                }

                return true;
            }

            void releaseOperation() NOEXCEPT
            {
                if( m_operation )
                {
                    m_serverState -> asyncWrapper() -> asyncExecutor() -> releaseOperation( m_operation );
                    BL_ASSERT( ! m_operation );

                    m_operationDataValid = false;
                }

                m_operationState.reset();
                m_operationBlockType = BlockTransferDefs::BlockType::Normal;
                m_operationProtocolDataSize = 0U;
            }

            void createOperationInternal(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in_opt              const uuid_t&                                   sourcePeerId,
                SAA_in_opt              const uuid_t&                                   targetPeerId
                )
            {
                releaseOperation();

                m_operationState =
                    m_serverState -> asyncWrapper() -> template createOperationState< AsyncOperationStateImpl >(
                        operationId,
                        sessionId,
                        chunkId,
                        sourcePeerId,
                        targetPeerId
                        );

                m_operation = m_serverState -> asyncWrapper() -> asyncExecutor() -> createOperation(
                    om::qi< AsyncOperationState >( m_operationState )
                    );

                m_operationDataValid = false;

                /*
                 * Always capture and remember the block type from the command block when we start
                 * a brand new operation
                 */

                m_operationBlockType = m_cmdBuffer.data.blockInfo.blockType;
                m_operationProtocolDataSize = m_cmdBuffer.data.blockInfo.protocolDataOffset;

                BL_ASSERT( ! m_operationState -> data() );
            }

            void createOperation(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in_opt              const uuid_t&                                   sourcePeerId,
                SAA_in_opt              const uuid_t&                                   targetPeerId
                )
            {
                createOperationInternal( operationId, chunkId, sessionId(), sourcePeerId, targetPeerId );
            }

            void scheduleReadCommand( SAA_in const bool newCommand )
            {
                if( newCommand )
                {
                    releaseOperation();
                }

                asio::async_read(
                    base_type::getStream(),
                    asio::buffer( &m_cmdBuffer, sizeof( m_cmdBuffer ) ),
                    untilCanceled(),
                    cpp::bind(
                        &this_type::onCommandRead,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );
            }

            void scheduleResponseCommand(
                SAA_in                  const bool                                      newCommand,
                SAA_in                  const callback_t&                               callback =
                    &this_type::onTransferCompleted
                )
            {
                m_cmdBuffer.flags |= CommandBlock::AckBit;

                /*
                 * Make sure we return our own peer id when we are doing version negotiation exchange
                 *
                 * Otherwise base_type::m_cmdBuffer.peerId is the target peer id which is typically
                 * the remote peer id for the server task
                 */

                if(
                    CommandBlock::CntrlCodeGetProtocolVersion == m_cmdBuffer.cntrlCode ||
                    CommandBlock::CntrlCodeSetProtocolVersion == m_cmdBuffer.cntrlCode
                    )
                {
                    m_cmdBuffer.peerId = base_type::m_peerId;
                }
                else
                {
                    m_cmdBuffer.peerId = base_type::m_remotePeerId;
                }

                m_cmdBuffer.host2Network();

                asio::async_write(
                    base_type::getStream(),
                    asio::buffer( &m_cmdBuffer, sizeof( m_cmdBuffer ) ),
                    untilCanceled(),
                    cpp::bind(
                        callback,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        newCommand,
                        sizeof( m_cmdBuffer ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );
            }

            void onCommandRead(
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                if( asio::error::eof == ec )
                {
                    /*
                     * The connection was closed by the peer after the last
                     * command. Ensure the bytes transferred is zero.
                     */

                    detail::chkPartialDataTransfer( 0U == bytesTransferred );
                }

                BL_TASKS_HANDLER_END_NOTREADY()

                if( asio::error::eof == ec )
                {
                    /*
                     * The connection was closed by the peer after the last
                     * command. The task is done.
                     */

                    base_type::notifyReady();
                    return;
                }

                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                /*
                 * If we don't have an error than the data read must be
                 * exactly the size of a command
                 */

                detail::chkPartialDataTransfer( sizeof( m_cmdBuffer ) == bytesTransferred );

                m_cmdBuffer.network2Host();

                BL_LOG(
                    Logging::trace(),
                    BL_MSG()
                        << "Blob server connection "
                        << this
                        << " received command "
                        << m_cmdBuffer.cntrlCode
                        << "; chunkId is '"
                        << m_cmdBuffer.chunkId
                        << "'; and chunkSize is "
                        << m_cmdBuffer.chunkSize
                        << "; flags are "
                        << m_cmdBuffer.flags
                    );

                if( ! m_clientProtocolVersion )
                {
                    /*
                     * If the client protocol version isn't set we only allow
                     * get/set version commands to be executed
                     */

                    switch( m_cmdBuffer.cntrlCode )
                    {
                        default:

                            /*
                             * Commands cannot be accepted until the client protocol version is set
                             */

                            scheduleErrorResponse( eh::errc::make_error_code( eh::errc::protocol_not_supported ) );
                            return;

                        case CommandBlock::CntrlCodeGetProtocolVersion:
                        case CommandBlock::CntrlCodeSetProtocolVersion:

                            /*
                             * Fall through
                             */

                            break;
                    }
                }

                /*
                 * If it is a data block control code let's validate the other relevant information
                 */

                if( m_isAuthenticationRequiredCallback &&
                    m_isAuthenticationRequiredCallback(
                        m_cmdBuffer.data.blockInfo.blockType,
                        m_cmdBuffer.cntrlCode
                        ) &&
                    ! isClientAuthenticated() )
                {
                    scheduleErrorResponse( eh::errc::make_error_code( eh::errc::permission_denied ) );

                    return;
                }

                auto ok = true;

                switch( m_cmdBuffer.cntrlCode )
                {
                    default:
                        break;

                    case CommandBlock::CntrlCodeGetDataBlockSize:
                    case CommandBlock::CntrlCodeGetDataBlock:
                    case CommandBlock::CntrlCodePutDataBlock:
                    case CommandBlock::CntrlCodeRemoveDataBlock:
                        {
                            if( m_cmdBuffer.data.blockInfo.blockType >= BlockTransferDefs::BlockType::Count )
                            {
                                ok = false;

                                BL_LOG(
                                    Logging::warning(),
                                        BL_MSG()
                                            << "TcpBlockTransferServerConnection::onCommandRead(): invalid block type '"
                                            << static_cast< std::uint16_t >( m_cmdBuffer.data.blockInfo.blockType )
                                            << "' read in command buffer"
                                        );
                            }
                            else if( m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::Normal )
                            {
                                if( m_cmdBuffer.chunkId == uuids::nil() )
                                {
                                    ok = false;

                                    BL_LOG(
                                        Logging::warning(),
                                            BL_MSG()
                                                << "TcpBlockTransferServerConnection::onCommandRead(): invalid chunk id '"
                                                << m_cmdBuffer.chunkId
                                                << "' read in command buffer"
                                            );
                                }
                            }
                            else if( m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::Authentication )
                            {
                                BL_ASSERT( m_cmdBuffer.chunkId == BlockTransferDefs::chunkIdDefault() );

                                if( m_cmdBuffer.cntrlCode != CommandBlock::CntrlCodePutDataBlock )
                                {
                                    ok = false;

                                    BL_LOG(
                                        Logging::warning(),
                                            BL_MSG()
                                                << "TcpBlockTransferServerConnection::onCommandRead(): invalid control code '"
                                                << m_cmdBuffer.cntrlCode
                                                << "' for an authentication type block"
                                            );
                                }
                                else if( ! m_serverState -> asyncWrapper() -> authenticationCallback() )
                                {
                                    /*
                                     * This is an authentication block, but the backend does not support authentication
                                     */

                                    m_isClientAuthenticated = false;
                                    scheduleErrorResponse( eh::errc::make_error_code( eh::errc::function_not_supported ) );

                                    return;
                                }
                            }
                            else if( m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::ServerState )
                            {
                                BL_ASSERT( m_cmdBuffer.chunkId == BlockTransferDefs::chunkIdDefault() );

                                if(
                                    m_cmdBuffer.cntrlCode != CommandBlock::CntrlCodeGetDataBlockSize &&
                                    m_cmdBuffer.cntrlCode != CommandBlock::CntrlCodeGetDataBlock
                                    )
                                {
                                    ok = false;

                                    BL_LOG(
                                        Logging::warning(),
                                            BL_MSG()
                                                << "TcpBlockTransferServerConnection::onCommandRead(): invalid control code '"
                                                << m_cmdBuffer.cntrlCode
                                                << "' for a server state type block"
                                            );
                                }
                            }
                            else if( m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::TransferOnly )
                            {
                                BL_ASSERT( m_cmdBuffer.chunkId == BlockTransferDefs::chunkIdDefault() );
                            }
                        }
                        break;
                }

                if( ! ok )
                {
                    /*
                     * The block validation has failed, inform the client that the request is invalid
                     */

                    scheduleErrorResponse( eh::errc::make_error_code( eh::errc::invalid_argument ) );
                    return;
                }

                switch( m_cmdBuffer.cntrlCode )
                {
                    default:
                        {
                            BL_LOG(
                                Logging::warning(),
                                BL_MSG()
                                    << "TcpBlockTransferServerConnection::onCommandRead(): invalid control code: "
                                    << m_cmdBuffer.cntrlCode
                                );

                            scheduleErrorResponse( eh::errc::make_error_code( eh::errc::invalid_argument ) );
                            return;
                        }
                        break;

                    case CommandBlock::CntrlCodeGetProtocolVersion:
                        scheduleGetSetProtocolVersion();
                        break;

                    case CommandBlock::CntrlCodeSetProtocolVersion:
                        scheduleGetSetProtocolVersion();
                        break;

                    case CommandBlock::CntrlCodeRemoveDataBlock:
                    case CommandBlock::CntrlCodeGetDataBlockSize:
                        scheduleGetSizeorRemoveRequest();
                        break;

                    case CommandBlock::CntrlCodeGetDataBlock:
                        scheduleGetRequest();
                        break;

                    case CommandBlock::CntrlCodePutDataBlock:
                        schedulePutRequest();
                        break;

                    case CommandBlock::CntrlCodePeerSessionsDataFlushRequest:
                        clientSessionsDataFlush();
                        break;
                }

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void scheduleErrorResponse( SAA_in const eh::error_code& ec )
            {
                base_type::setErrorCode( ec );

                scheduleResponseCommand( true /* newCommand */ );
            }

            void scheduleGetSetProtocolVersion()
            {
                BL_ASSERT(
                    CommandBlock::CntrlCodeGetProtocolVersion == m_cmdBuffer.cntrlCode ||
                    CommandBlock::CntrlCodeSetProtocolVersion == m_cmdBuffer.cntrlCode
                    );

                if( CommandBlock::CntrlCodeSetProtocolVersion == m_cmdBuffer.cntrlCode )
                {
                    if( m_cmdBuffer.data.version.value > CommandBlock::BLOB_TRANSFER_PROTOCOL_SERVER_VERSION )
                    {
                        /*
                         * The client version is newer than the server and thus we don't support it
                         *
                         * In the client protocol version is older then it is ok because the changes
                         * are normally backward compatible
                         */

                        base_type::setErrorCode(  eh::errc::make_error_code( eh::errc::protocol_not_supported ) );
                    }
                    else
                    {
                        m_clientProtocolVersion = m_cmdBuffer.data.version.value;
                    }
                }
                else
                {
                    m_cmdBuffer.data.version.value = CommandBlock::BLOB_TRANSFER_PROTOCOL_SERVER_VERSION;
                }

                if(
                    0 == ( m_cmdBuffer.flags & CommandBlock::ErrBit ) &&
                    m_cmdBuffer.peerId != base_type::m_remotePeerId &&
                    ! base_type::isNonNullAndSame( m_cmdBuffer.peerId, base_type::m_peerId )
                    )
                {
                    /*
                     * The remote peer id is obtained during the client protocol negotiation message
                     *
                     * We only update it if it is different than the one we already obtained previously,
                     * but we also make sure that if it is not nil() then it is different than
                     * base_type::m_peerId
                     */

                    base_type::m_remotePeerId = m_cmdBuffer.peerId;
                }

                scheduleResponseCommand( true /* newCommand */ );
            }

            void scheduleGetSizeorRemoveRequest()
            {
                BL_ASSERT(
                    CommandBlock::CntrlCodeGetDataBlockSize == m_cmdBuffer.cntrlCode ||
                    CommandBlock::CntrlCodeRemoveDataBlock == m_cmdBuffer.cntrlCode
                    );

                if( CommandBlock::CntrlCodeRemoveDataBlock == m_cmdBuffer.cntrlCode )
                {
                    /*
                     * Note that for OperationId::Command / CommandId::Remove this check needs to be done
                     * against m_cmdBuffer.data.blockInfo.blockType instead of m_operationBlockType
                     * because there is no operation bootstrapped yet and m_operationBlockType is not
                     * set yet (and never will be)
                     */

                    if( m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::TransferOnly )
                    {
                        scheduleResponseCommand( true /* newCommand */ );
                    }
                    else
                    {
                        createOperation(
                            OperationId::Command,
                            m_cmdBuffer.chunkId,
                            base_type::m_remotePeerId               /* sourcePeerId */,
                            m_cmdBuffer.peerId           /* targetPeerId */
                            );

                        m_operationState -> commandId( CommandId::Remove );

                        m_serverState -> asyncWrapper() -> asyncExecutor() -> asyncBegin(
                            m_operation,
                            cpp::bind(
                                &this_type::onAsyncCommandCompleted,
                                om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                _1
                                )
                            );
                    }
                }
                else
                {
                    if( chk2LoadChunk() )
                    {
                        /*
                         * If the chunk was loaded synchronously we simply respond
                         * to the CntrlCodeGetDataBlockSize request immediately
                         *
                         * Otherwise it will be responded later in the async callback
                         */

                        respond2GetSize();
                    }
                }
            }

            void respond2GetSize()
            {
                m_cmdBuffer.chunkSize =
                    ( std::uint32_t )
                    (
                        ( m_operationState -> data() && m_operationDataValid && 0 == ( m_cmdBuffer.flags & CommandBlock::ErrBit ) ) ?
                            m_operationState -> data() -> size() :
                            0U
                    );

                /*
                 * newCommand should be false because of the following reason:
                 *
                 * Get size is normally called just prior to calling immediately
                 * to get the data, so in this case we don't want to reset the
                 * async operation to avoid the unnecessary second load of the
                 * data -- essentially the last chunk loaded is cached between
                 * the CntrlCodeGetDataBlockSize and CntrlCodeGetDataBlock calls
                 */

                scheduleResponseCommand( false /* newCommand */ );
            }

            void onAsyncCommandCompleted( SAA_in const AsyncOperation::Result& result ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                if( ! chkAsyncResult( result ) )
                {
                    /*
                     * Client side error has occurred - we can't proceed
                     */

                    return;
                }

                scheduleResponseCommand( true /* newCommand */ );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void onChunkAllocated(
                SAA_in              const decltype( CommandBlock::chunkSize )           size,
                SAA_in              const cpp::void_callback_t&                         postAllocCallback,
                SAA_in              const AsyncOperation::Result&                       result
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                if( ! chkAsyncResult( result ) )
                {
                    /*
                     * Client side error has occurred - we can't proceed
                     */

                    return;
                }

                BL_CHK(
                    false,
                    nullptr != m_operationState -> data(),
                    BL_MSG()
                        << "Data block can't be nullptr post successful async alloc operation"
                    );

                BL_CHK(
                    false,
                    size <= m_operationState -> data() -> capacity(),
                    BL_MSG()
                        << "Invalid chunk size (larger than the data block capacity) : "
                        << size
                        );

                if( m_operationBlockType.value() == BlockTransferDefs::BlockType::TransferOnly )
                {
                    /*
                     * Since there won't be load operation in this case (to set the size correctly)
                     * we need to set it here to something and we are going to set it to max value
                     * - i.e. m_operationState -> data() -> capacity()
                     */

                    m_operationState -> data() -> setSize( m_operationState -> data() -> capacity() );
                }
                else
                {
                    m_operationState -> data() -> setSize( size );
                }

                postAllocCallback();

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void handleChunkLoaded(
                SAA_in              const std::uint32_t                                 chunkSizeExpected,
                SAA_in              const AsyncOperation::Result&                       result
                )
            {
                if( ! chkAsyncResult( result ) )
                {
                    /*
                     * Client side error has occurred - we can't proceed
                     */

                    return;
                }

                detail::chkChunkSize( m_operationState -> data() && m_operationState -> data() -> size() );

                base_type::m_chunkId = m_cmdBuffer.chunkId;
                m_operationDataValid = true;

                BL_LOG(
                    Logging::trace(),
                    BL_MSG()
                        << "Blob server connection "
                        << this
                        << " loaded chunk '"
                        << base_type::m_chunkId
                        << "'; and chunkSize is "
                        << m_operationState -> data() -> size()
                        << "; data block is "
                        << m_operationState -> data().get()
                        << "; operationDataValid is "
                        << m_operationDataValid
                    );

                if( chunkSizeExpected )
                {
                    /*
                     * This is a CntrlCodeGetDataBlock request
                     */

                    BL_ASSERT( m_operationDataValid );
                    BL_ASSERT( m_operationState -> data() );

                    BL_CHK(
                        false,
                        chunkSizeExpected == m_operationState -> data() -> size(),
                        BL_MSG()
                            << "TcpBlockTransferServerConnection::onCommandRead(): invalid size "
                            << m_operationState -> data() -> size()
                            << " for chunk: '"
                            << m_cmdBuffer.chunkId
                            << "' in a GET chunk command; expected size is "
                            << chunkSizeExpected
                        );

                    scheduleResponseCommand( true /* newCommand */, &this_type::onGetDataAck );
                }
                else
                {
                    /*
                     * This is a CntrlCodeGetDataBlockSize request
                     */

                    respond2GetSize();
                }
            }

            void onChunkLoadedNothrow(
                SAA_in              const std::uint32_t                                 chunkSizeExpected,
                SAA_in              const AsyncOperation::Result&                       result
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                handleChunkLoaded( chunkSizeExpected, result );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void scheduleAsyncLoad( SAA_in const std::uint32_t chunkSizeExpected )
            {
                m_operationState -> operationId(
                    m_operationBlockType.value() == BlockTransferDefs::BlockType::ServerState ?
                        OperationId::GetServerState : OperationId::Get
                    );

                 m_serverState -> asyncWrapper() -> asyncExecutor() -> asyncBegin(
                    m_operation,
                    cpp::bind(
                        &this_type::onChunkLoadedNothrow,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        chunkSizeExpected,
                        _1 /* result */
                        )
                    );
            }

            /**
             * @brief Returns true if the chunk was loaded synchronously and false
             * otherwise
             */

            bool chk2LoadChunk( SAA_in const std::uint32_t chunkSizeExpected = 0U )
            {
                if( m_cmdBuffer.chunkId != base_type::m_chunkId || false == m_operationDataValid.value() )
                {
                    /*
                     * Note that because createOperation has not been called yet we need to use
                     * m_cmdBuffer.data.blockInfo.blockType instead of m_operationBlockType
                     */

                    const bool isTransferOnlyBlock =
                        m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::TransferOnly;

                    /*
                     * If this is TransferOnly block we need to do OperationId::SecureAlloc since
                     * the block will be sent to the client as is without subsequent Load call and
                     * we don't want to expose any / random process memory content to the client
                     */

                    createOperation(
                        isTransferOnlyBlock ? OperationId::SecureAlloc : OperationId::Alloc,
                        m_cmdBuffer.chunkId,
                        base_type::m_remotePeerId               /* sourcePeerId */,
                        m_cmdBuffer.peerId           /* targetPeerId */
                        );

                    const auto getPostAllocCallback =
                        [ chunkSizeExpected, isTransferOnlyBlock, this ]() -> cpp::void_callback_t
                    {
                        if( isTransferOnlyBlock )
                        {
                            return cpp::bind(
                                &this_type::handleChunkLoaded,
                                om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                chunkSizeExpected,
                                AsyncOperation::Result()
                                );
                        }
                        else
                        {
                            return cpp::bind(
                                &this_type::scheduleAsyncLoad,
                                om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                chunkSizeExpected
                                );
                        }
                    };

                    m_serverState -> asyncWrapper() -> asyncExecutor() -> asyncBegin(
                        m_operation,
                        cpp::bind(
                            &this_type::onChunkAllocated,
                            om::ObjPtrCopyable< this_type >::acquireRef( this ),
                            0U /* size */,
                            getPostAllocCallback(),
                            _1 /* result */
                            )
                        );

                    /*
                     * The chunk will be loaded asynchronously
                     */

                    return false;
                }

                /*
                 * The chunk is already loaded
                 *
                 * return true to indicate the load happened synchronously
                 */

                BL_LOG(
                    Logging::trace(),
                    BL_MSG()
                        << "Blob server connection "
                        << this
                        << " skipped loaded chunk '"
                        << base_type::m_chunkId
                        << "'; and chunkSize is "
                        << m_operationState -> data() -> size()
                        << "; data block is "
                        << m_operationState -> data().get()
                        << "; operationDataValid is "
                        << m_operationDataValid
                    );

                return true;
            }

            void onTransferCompleted(
                SAA_in                  const bool                                      newCommand,
                SAA_in                  const std::size_t                               bytesExpected,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                detail::chkPartialDataTransfer( bytesExpected == bytesTransferred );

                scheduleReadCommand( newCommand );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void scheduleGetRequest()
            {
                BL_ASSERT( CommandBlock::CntrlCodeGetDataBlock == m_cmdBuffer.cntrlCode );

                if( chk2LoadChunk( m_cmdBuffer.chunkSize /* chunkSizeExpected */ ) )
                {
                    /*
                     * If the chunk was loaded synchronously we simply respond
                     * to the CntrlCodeGetDataBlockSize request immediately
                     *
                     * Otherwise it will be responded later in the async callback
                     */

                    scheduleResponseCommand( true /* newCommand */, &this_type::onGetDataAck );
                }
            }

            void onGetDataAck(
                SAA_in                  const bool                                      newCommand,
                SAA_in                  const std::size_t                               bytesExpected,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                detail::chkPartialDataTransfer( bytesExpected == bytesTransferred );

                BL_ASSERT( m_operationState -> data() && m_operationState -> data() -> size() );

                asio::async_write(
                    base_type::getStream(),
                    asio::buffer( m_operationState -> data() -> begin(), m_operationState -> data() -> size() ),
                    untilCanceled(),
                    cpp::bind(
                        &this_type::onTransferCompleted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        newCommand,
                        m_operationState -> data() -> size(),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void schedulePutRequest()
            {
                BL_ASSERT( CommandBlock::CntrlCodePutDataBlock == m_cmdBuffer.cntrlCode );

                detail::chkChunkSize( 0U != m_cmdBuffer.chunkSize );

                base_type::m_chunkId = m_cmdBuffer.chunkId;

                createOperation(
                    OperationId::Alloc,
                    base_type::m_chunkId,
                    base_type::m_remotePeerId               /* sourcePeerId */,
                    m_cmdBuffer.peerId           /* targetPeerId */
                    );

                const cpp::void_callback_t postAllocCallback =
                    cpp::bind(
                            &this_type::scheduleResponseCommand,
                            om::ObjPtrCopyable< this_type >::acquireRef( this ),
                            false /* newCommand */,
                            &this_type::onPutDataAck
                            );

                m_serverState -> asyncWrapper() -> asyncExecutor() -> asyncBegin(
                    m_operation,
                    cpp::bind(
                        &this_type::onChunkAllocated,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        m_cmdBuffer.chunkSize /* size */,
                        postAllocCallback,
                        _1 /* result */
                        )
                    );
            }

            void onPutDataAck(
                SAA_in                  const bool                                      newCommand,
                SAA_in                  const std::size_t                               bytesExpected,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                BL_UNUSED( newCommand );

                detail::chkPartialDataTransfer( bytesExpected == bytesTransferred );

                m_cmdBuffer.network2Host();

                if( m_cmdBuffer.flags & CommandBlock::ErrBit )
                {
                    /*
                     * This request has failed - go back to reading the next command
                     */

                    scheduleReadCommand( true /* newCommand */ );
                }
                else
                {
                    /*
                     * We're now ready to receive the data
                     */

                    BL_ASSERT(
                        m_operationState -> data() &&
                        m_operationState -> data() -> size() &&
                        m_cmdBuffer.chunkSize == m_operationState -> data() -> size()
                        );

                    asio::async_read(
                        base_type::getStream(),
                        asio::buffer( m_operationState -> data() -> begin(), m_operationState -> data() -> size() ),
                        untilCanceled(),
                        cpp::bind(
                                &this_type::onChunkReceived,
                                om::ObjPtrCopyable< this_type >::acquireRef( this ),
                                asio::placeholders::error,
                                asio::placeholders::bytes_transferred
                            )
                        );
                }

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void clientSessionsDataFlush()
            {
                /*
                 * Note that for OperationId::Command / CommandId::FlushPeerSessions this check needs to be
                 * done against m_cmdBuffer.data.blockInfo.blockType instead of m_operationBlockType
                 * because there is no operation bootstrapped yet and m_operationBlockType is not
                 * set yet (and never will be)
                 */

                if( m_cmdBuffer.data.blockInfo.blockType == BlockTransferDefs::BlockType::TransferOnly )
                {
                    scheduleResponseCommand( true /* newCommand */ );

                    return;
                }

                createOperationInternal(
                    OperationId::Command,
                    uuids::nil()                            /* chunkId */,
                    sessionId(),
                    base_type::m_remotePeerId               /* sourcePeerId */,
                    m_cmdBuffer.peerId           /* targetPeerId */
                    );

                m_operationState -> commandId( CommandId::FlushPeerSessions );

                m_serverState -> asyncWrapper() -> asyncExecutor() -> asyncBegin(
                    m_operation,
                    cpp::bind(
                        &this_type::onAsyncCommandCompleted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        _1
                        )
                    );
            }

            void onChunkDataProcessed( SAA_in const AsyncOperation::Result& result ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                if( ! chkAsyncResult( result ) )
                {
                    /*
                     * Client side error has occurred - we can't proceed
                     */

                    if( BlockTransferDefs::BlockType::Authentication == m_operationBlockType )
                    {
                        m_isClientAuthenticated = false;
                    }

                    return;
                }

                BL_ASSERT( m_cmdBuffer.flags & CommandBlock::AckBit );

                if( BlockTransferDefs::BlockType::Authentication == m_operationBlockType )
                {
                    m_isClientAuthenticated = true;
                }

                ++base_type::m_noOfBlocksTransferred;

                m_cmdBuffer.peerId = base_type::m_remotePeerId;

                m_cmdBuffer.host2Network();

                asio::async_write(
                    base_type::getStream(),
                    asio::buffer( &m_cmdBuffer, sizeof( m_cmdBuffer ) ),
                    untilCanceled(),
                    cpp::bind(
                        &this_type::onTransferCompleted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        true /* newCommand */,
                        sizeof( m_cmdBuffer ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void onChunkReceived(
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                detail::chkPartialDataTransfer(
                    m_operationState -> data() &&
                    m_operationState -> data() -> size() == bytesTransferred
                    );

                /*
                 * Figure out the operation id based on the block type and schedule
                 * the async call
                 */

                auto operationId = OperationId::None;

                switch( m_operationBlockType.value() )
                {
                    default:
                        BL_THROW_EC(
                            eh::errc::make_error_code( eh::errc::invalid_argument ),
                            BL_MSG()
                                << "Invalid block type for send operation"
                            );
                        break;

                    case BlockTransferDefs::BlockType::Normal:
                        operationId = OperationId::Put;
                        m_operationState -> data() -> setOffset1( m_operationProtocolDataSize );
                        break;

                    case BlockTransferDefs::BlockType::Authentication:
                        operationId = OperationId::AuthenticateClient;
                        break;

                    case BlockTransferDefs::BlockType::TransferOnly:
                        operationId = OperationId::SecureDiscard;
                        break;
                }

                m_operationState -> operationId( operationId );

                m_serverState -> asyncWrapper() -> asyncExecutor() -> asyncBegin(
                    m_operation,
                    cpp::bind(
                        &this_type::onChunkDataProcessed,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        _1 /* result */
                        )
                    );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            virtual bool scheduleTaskFinishContinuation( SAA_in_opt const std::exception_ptr& eptrIn = nullptr ) OVERRIDE
            {
                if( m_skipShutdownContinuation )
                {
                    return false;
                }

                return base_type::scheduleTaskFinishContinuation( eptrIn );
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                /*
                 * Save the thread pool, create the timer and schedule it immediately
                 */

                BL_ASSERT( eq );

                base_type::ensureChannelIsOpen();

                BL_UNUSED( eq );

                m_cmdBuffer = CommandBlock();
                base_type::m_chunkId = uuids::nil();
                releaseOperation();

                scheduleReadCommand( true /* newCommand */ );
            }

            virtual void cancelTask() OVERRIDE
            {
                if( ! m_forceShutdownContinuation )
                {
                    /*
                     * If the task is cancelled (and force not requested) then do
                     * not attempt to do shutdown continuation because otherwise it
                     * will get 'hung' for interval up to the server heartbeat time
                     * (since the termination is not initiated from the client task)
                     */

                    m_skipShutdownContinuation = true;
                }

                if( m_operation )
                {
                    m_operation -> cancel();
                    releaseOperation();
                }

                base_type::cancelTask();
            }

            virtual void onStreamChanging( SAA_in const stream_ref& /* streamNew */ ) NOEXCEPT OVERRIDE
            {
                /*
                 * In case of reconnection we need to re-negotiate the client version
                 * and start a new session
                 */

                m_clientProtocolVersion = 0U;
                m_connectedSessionId = uuids::create();
            }

            virtual auto onTaskStoppedNothrow(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_inout_opt           bool*                                       isExpectedException = nullptr
                ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                /*
                 * The only legitimate case where m_operation would need to be
                 * released here (i.e. non-null) is when an exception was thrown
                 * from within an async callback or some other callback after
                 * an async operation was completed
                 *
                 * Note that an exception can't be thrown after an async call is
                 * started unless this operation can be waited on to complete
                 * which implies the callback is ok to be called even after the
                 * task has completed (which normally is not the case)
                 */

                releaseOperation();

                BL_NOEXCEPT_END()

                return base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );
            }

        public:

            bool isFatalServerError() const NOEXCEPT
            {
                return m_isFatalServerError;
            }

            bool isClientAuthenticated() const NOEXCEPT
            {
                return m_isClientAuthenticated;
            }

            bool forceShutdownContinuation() const NOEXCEPT
            {
                return m_forceShutdownContinuation;
            }

            void forceShutdownContinuation( SAA_in const bool force ) NOEXCEPT
            {
                m_forceShutdownContinuation = force;
            }
        };

        template
        <
            typename STREAM,
            typename ASYNCWRAPPER
        >
        using TcpBlockTransferServerConnectionImpl =
            om::ObjectImpl< TcpBlockTransferServerConnection< STREAM, ASYNCWRAPPER > >;

        /*************************************************************************************************
         * The block server implementation (the acceptor)
         */

        template
        <
            typename STREAM,
            typename ASYNCWRAPPER,
            typename SERVERPOLICY = TcpServerPolicyDefault
        >
        class TcpBlockServerT :
            public TcpServerBase< STREAM, SERVERPOLICY >
        {
            BL_DECLARE_OBJECT_IMPL( TcpBlockServerT )

        public:

            typedef TcpServerBase< STREAM, SERVERPOLICY >                           base_type;
            typedef TcpBlockTransferServerConnectionImpl< STREAM, ASYNCWRAPPER >    connection_t;
            typedef ASYNCWRAPPER                                                    async_wrapper_t;
            typedef STREAM                                                          stream_t;
            typedef detail::BlockTransferServerStateImpl< ASYNCWRAPPER >            server_state_t;

            typedef typename connection_t::isauthenticationrequired_callback_t      isauthenticationrequired_callback_t;

        protected:

            const om::ObjPtr< server_state_t >                                      m_serverState;
            const uuid_t                                                            m_peerId;
            const isauthenticationrequired_callback_t                               m_isAuthenticationRequiredCallback;

            TcpBlockServerT(
                SAA_in              const om::ObjPtr< TaskControlTokenRW >&         controlToken,
                SAA_in              const om::ObjPtr< data::datablocks_pool_type >& dataBlocksPool,
                SAA_in              std::string&&                                   host,
                SAA_in              const unsigned short                            port,
                SAA_in              const std::string&                              privateKeyPem,
                SAA_in              const std::string&                              certificatePem,
                SAA_in              const om::ObjPtr< ASYNCWRAPPER >&               asyncWrapper,
                SAA_in_opt          const uuid_t&                                   peerId = uuids::nil(),
                SAA_in_opt          isauthenticationrequired_callback_t&&           isAuthenticationRequiredCallback =
                    isauthenticationrequired_callback_t()
                )
                :
                base_type( controlToken, BL_PARAM_FWD( host ), port, privateKeyPem, certificatePem ),
                m_serverState(
                    server_state_t::createInstance( dataBlocksPool, asyncWrapper )
                    ),
                m_peerId( peerId ),
                m_isAuthenticationRequiredCallback( std::move( isAuthenticationRequiredCallback ) )
            {
            }

            virtual om::ObjPtr< Task > createConnection( SAA_inout typename STREAM::stream_ref&& connectedStream ) OVERRIDE
            {
                auto connection = connection_t::createInstance(
                    m_serverState,
                    m_peerId,
                    cpp::copy( m_isAuthenticationRequiredCallback )
                    );

                connection -> attachStream( BL_PARAM_FWD( connectedStream ) );

                return om::qi< Task >( connection );
            }

            virtual void onTaskTerminated( const om::ObjPtrCopyable< Task >& task ) OVERRIDE
            {
                auto connection = om::qi< connection_t >( task );

                if(
                    connection -> isFatalServerError() &&
                    m_serverState -> asyncWrapper() -> stopServerOnUnexpectedBackendError()
                    )
                {
                    /*
                     * This is a fatal error - we're requesting shutdown of the blob server
                     */

                    base_type::requestCancelInternal();
                }
            }
        };

    } // tasks

} // bl

#endif /* __BL_MESSAGING_TCPBLOCKTRANSFERSERVER_H_ */
