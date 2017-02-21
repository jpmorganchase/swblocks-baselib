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

#include <baselib/reactive/Observer.h>

#include <baselib/messaging/BrokerFacade.h>
#include <baselib/messaging/BrokerDispatchingBackendProcessing.h>
#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/AsyncDataChunkStorage.h>
#include <baselib/messaging/AsyncMessageDispatcherWrapper.h>
#include <baselib/messaging/TcpBlockTransferClient.h>
#include <baselib/messaging/TcpBlockServerDataChunkStorage.h>
#include <baselib/messaging/TcpBlockServerMessageDispatcher.h>
#include <baselib/messaging/MessagingClientBlockDispatchLocal.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>
#include <baselib/messaging/MessagingClientBlock.h>
#include <baselib/messaging/MessagingClientFactory.h>
#include <baselib/messaging/BrokerErrorCodes.h>

#include <baselib/tasks/TasksUtils.h>
#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/SimpleTaskControlToken.h>

#include <baselib/transfer/SendRecvContext.h>

#include <baselib/core/OS.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/Random.h>
#include <baselib/core/BaseIncludes.h>
#include <baselib/core/EndpointSelectorImpl.h>

#include <utests/baselib/MachineGlobalTestLock.h>
#include <utests/baselib/TestTaskUtils.h>
#include <utests/baselib/UtfArgsParser.h>
#include <utests/baselib/UtfCrypto.h>
#include <utests/baselib/Utf.h>

/************************************************************************
 * I/O code tests
 */

namespace
{
    using namespace bl;

    typedef bl::tasks::TcpBlockTransferClientConnectionImpl< bl::tasks::TcpSocketAsyncBase >        connection_t;
    typedef bl::tasks::TcpConnectionEstablisherConnectorImpl< bl::tasks::TcpSocketAsyncBase >       connector_t;
    typedef bl::tasks::TcpConnectionEstablisherConnectorImpl< bl::tasks::TcpSslSocketAsyncBase >    ssl_connector_t;

    template
    <
        typename T
    >
    void chkTaskCompletedOkOrRunning( SAA_in const om::ObjPtr< T >& task )
    {
        if( bl::tasks::Task::Completed != task -> getState() )
        {
            return;
        }

        if( task -> isFailed() )
        {
            bl::cpp::safeRethrowException( task -> exception() );
        }
    }

    template
    <
        typename Acceptor
    >
    void basicAcceptorTest()
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace utest;

        typedef typename Acceptor::async_wrapper_t                                              async_wrapper_t;
        typedef typename async_wrapper_t::backend_interface_t                                   backend_interface_t;

        test::MachineGlobalTestLock lock;

        {
            const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();
            const auto dataBlocksPool = datablocks_pool_type::createInstance();
            const auto backendImpl = BackendImplTestImpl::createInstance();

            const auto asyncWrapper = om::lockDisposable(
                async_wrapper_t::template createInstance(
                    om::qi< backend_interface_t >( backendImpl ) /* writeBackend */,
                    om::qi< backend_interface_t >( backendImpl ) /* readBackend */,
                    test::UtfArgsParser::threadsCount(),
                    om::qi< TaskControlToken >( controlToken ),
                    0U /* maxConcurrentTasks */,
                    dataBlocksPool
                    )
                );

            {
                const auto acceptor = Acceptor::template createInstance(
                    controlToken,
                    dataBlocksPool,
                    "localhost",
                    1234,
                    bl::str::empty() /* privateKeyPem */,
                    bl::str::empty() /* certificatePem */,
                    asyncWrapper
                    );

                UTF_REQUIRE( acceptor );

                {
                    const auto i = om::qi< tasks::Task >( acceptor );
                    UTF_REQUIRE( i );
                }
            }
        }
    }

    template
    <
        typename Acceptor,
        typename Connector
    >
    void simpleConnectAndTransmitDataTest(
        SAA_in      const bool          startConnector = false,
        SAA_in      const bool          isAuthenticationRequired = false
        )
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace utest;

        typedef typename Acceptor::async_wrapper_t                                              async_wrapper_t;
        typedef typename async_wrapper_t::backend_interface_t                                   backend_interface_t;
        typedef bl::tasks::TcpBlockTransferClientConnectionImpl< typename Acceptor::stream_t >  connection_t;

        test::MachineGlobalTestLock lock;

        tasks::scheduleAndExecuteInParallel(
            [ &startConnector, &isAuthenticationRequired ](
                SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq
                ) -> void
            {
                const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();
                const auto dataBlocksPool = datablocks_pool_type::createInstance();
                const auto backendImpl = BackendImplTestImpl::createInstance();

                const auto backend = om::lockDisposable(
                    async_wrapper_t::template createInstance(
                        om::qi< backend_interface_t >( backendImpl ) /* writeBackend */,
                        om::qi< backend_interface_t >( backendImpl ) /* readBackend */,
                        test::UtfArgsParser::threadsCount(),
                        om::qi< TaskControlToken >( controlToken ),
                        0U /* maxConcurrentTasks */,
                        dataBlocksPool
                        )
                    );

                {
                    auto isAuthenticationRequiredCallback = [ isAuthenticationRequired ](
                        SAA_in      const typename BlockTransferDefs::BlockType         blockType,
                        SAA_in      const typename std::uint16_t                        cntrlCode
                        ) -> bool
                    {
                        auto result = false;

                        if( isAuthenticationRequired && blockType == BlockTransferDefs::BlockType::Normal )
                        {
                            switch( cntrlCode )
                            {
                                case tasks::detail::CommandBlock::CntrlCodePutDataBlock:
                                case tasks::detail::CommandBlock::CntrlCodeRemoveDataBlock:
                                    result = true;
                                    break;

                                default:
                                    break;
                            }
                        }

                        return result;
                    };

                    const bl::uuid_t remotePeerId = uuids::create();

                    const auto acceptor = Acceptor::template createInstance< Acceptor >(
                        controlToken,
                        dataBlocksPool,
                        "localhost",
                        28100,
                        test::UtfCrypto::getDefaultServerKey()          /* privateKeyPem */,
                        test::UtfCrypto::getDefaultServerCertificate()  /* certificatePem */,
                        backend,
                        remotePeerId,
                        std::move( isAuthenticationRequiredCallback )
                        );

                    UTF_REQUIRE( acceptor );

                    /*
                     * Start the acceptor and sleep for a couple of seconds to give it a chance to start
                     */

                    const auto taskAcceptor = om::qi< tasks::Task >( acceptor );
                    eq -> push_back( taskAcceptor );

                    try
                    {
                        os::sleep( time::milliseconds( 2000 ) );

                        if( startConnector )
                        {
                            typedef tasks::detail::CommandBlock CommandBlock;

                            const auto connector = Connector::template createInstance< Connector >( "localhost", 28100 );
                            const auto taskConnector = om::qi< tasks::Task >( connector.get() );
                            eq -> push_back( taskConnector );
                            eq -> waitForSuccess( taskConnector );

                            /*
                             * Wait for the server connection to be established
                             */

                            std::size_t retries = 0;
                            const std::size_t maxRetries = 30;
                            om::ObjPtr< typename Acceptor::connection_t > serverConnection;

                            for( ;; )
                            {
                                os::sleep( time::seconds( 1 ) );

                                chkTaskCompletedOkOrRunning( acceptor );
                                const auto serverEndpoints = acceptor -> activeEndpoints();

                                if( serverEndpoints.size() )
                                {
                                    UTF_REQUIRE_EQUAL( serverEndpoints.size(), 1U );

                                    serverConnection =
                                        om::qi< typename Acceptor::connection_t >( serverEndpoints.back() );
                                    chkTaskCompletedOkOrRunning( serverConnection );

                                    break;
                                }

                                if( retries > maxRetries )
                                {
                                    UTF_FAIL( "Connection with server can't be established" );
                                }

                                ++retries;
                            }

                            const auto isServerConnectionAuthenticated = [ & ]() -> bool
                            {
                                return serverConnection -> isClientAuthenticated();
                            };

                            UTF_REQUIRE_EQUAL( serverConnection -> peerId(), remotePeerId );
                            UTF_REQUIRE_EQUAL( serverConnection -> remotePeerId(), uuids::nil() );

                            /*
                             * Test the isExpectedException logic first
                             */

                            {
                                /*
                                 * TODO: clang 3.9.1 seems to have an issue and complains that this is
                                 * unused local type if the connection_base_t is defined inside the
                                 * local class as base_type
                                 *
                                 * This seems to be for now an acceptable workaround, but once the
                                 * compiler issue is fixed we can change the code again
                                 */

                                typedef TcpBlockTransferClientConnectionT< typename Acceptor::stream_t >
                                    connection_base_t;

                                class LocalClientConnection : public connection_base_t
                                {
                                protected:

                                    LocalClientConnection(
                                        SAA_in const om::ObjPtr< data::datablocks_pool_type >& dataBlocksPool
                                        )
                                        :
                                        connection_base_t(
                                        	connection_base_t::CommandId::NoCommand,
                                            uuids::create() /* peerId */,
                                            dataBlocksPool
                                        )
                                    {
                                    }

                                public:

                                    void checkExpectedException(
                                        SAA_in                  const std::exception_ptr&                       eptr,
                                        SAA_in                  const std::exception&                           exception,
                                        SAA_in_opt              const eh::error_code*                           ec
                                        )
                                    {
                                        /*
                                         * TODO: Compiler workaround
                                         * "base_type::" should work below (MSVC and GCC accept it)
                                         * but it fails for Clang3.5 which requires "this ->"
                                         */

                                        UTF_REQUIRE( this -> isExpectedException( eptr, exception, ec ) );
                                    }
                                };

                                typedef om::ObjectImpl< LocalClientConnection > local_connection_t;

                                const auto localConnection = local_connection_t::createInstance( dataBlocksPool );

                                /*
                                 * Test some error codes which are expected to be filtered out
                                 */

                                /*
                                 * On Windows the error codes for connection reset and aborted are different
                                 * and we need to handle these separately
                                 *
                                 * The values for WSAECONNRESET (10054), WSAECONNABORTED (10053) and
                                 * WSAETIMEDOUT (10060) are from here:
                                 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%28v=vs.85%29.aspx
                                 */

                                const auto ecConnectionReset =
                                    os::onUNIX() ?  eh::errc::connection_reset : 10054 /* WSAECONNRESET */;
                                const auto ecBrokenPipe =
                                    os::onUNIX() ?  eh::errc::broken_pipe : 10053 /* WSAECONNABORTED */;
                                const auto ecTimedOut =
                                    os::onUNIX() ?  eh::errc::timed_out : 10060 /* WSAETIMEDOUT */;
                                const auto ecHostUnreachable =
                                    os::onUNIX() ?  eh::errc::host_unreachable : 10065 /* WSAEHOSTUNREACH */;

                                UnexpectedException exception;

                                {
                                    const auto ec = eh::error_code( ecConnectionReset, eh::system_category() );

                                    if( os::onLinux() )
                                    {
                                        UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:104" ) );                                    }
                                    else
                                    {
                                        UTF_REQUIRE_EQUAL(
                                            eh::errorCodeToString( ec ),
                                            os::onUNIX() ? std::string( "system:54" ) : std::string( "system:10054" )
                                            );
                                    }

                                    localConnection -> checkExpectedException( nullptr /* eptr */, exception, &ec );
                                }

                                {
                                    const auto ec = eh::error_code( ecBrokenPipe, eh::system_category() );

                                    UTF_REQUIRE_EQUAL(
                                        eh::errorCodeToString( ec ),
                                        os::onUNIX() ? std::string( "system:32" ) : std::string( "system:10053" )
                                        );

                                    localConnection -> checkExpectedException( nullptr /* eptr */, exception, &ec );
                                }

                                {
                                    const auto ec = eh::error_code( ecTimedOut, eh::system_category() );

                                    if( os::onLinux() )
                                    {
                                        UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:110" ) );                                    }
                                    else
                                    {
                                        UTF_REQUIRE_EQUAL(
                                            eh::errorCodeToString( ec ),
                                            os::onUNIX() ? std::string( "system:60" ) : std::string( "system:10060" )
                                            );
                                    }

                                    localConnection -> checkExpectedException( nullptr /* eptr */, exception, &ec );
                                }

                                {
                                    const auto ec = eh::error_code( ecHostUnreachable, eh::system_category() );

                                    if( os::onLinux() )
                                    {
                                        UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:113" ) );                                    }
                                    else
                                    {
                                        UTF_REQUIRE_EQUAL(
                                            eh::errorCodeToString( ec ),
                                            os::onUNIX() ? std::string( "system:65" ) : std::string( "system:10065" )
                                            );
                                    }

                                    localConnection -> checkExpectedException( nullptr /* eptr */, exception, &ec );
                                }
                            }

                            /*
                             * Now create a client connection and negotiate the client version
                             *
                             * This will trigger the update of the remotePeerId() on both ends
                             */

                            const bl::uuid_t peerId = uuids::create();

                            const auto transfer =
                                connection_t::createInstance(
                                    connection_t::CommandId::NoCommand,
                                    peerId,
                                    dataBlocksPool
                                    );

                            UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                            UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), uuids::nil() );
                            UTF_REQUIRE_EQUAL( transfer -> targetPeerId(), uuids::nil() );

                            transfer -> attachStream( connector -> detachStream() );

                            const auto taskTransfer = om::qi< tasks::Task >( transfer.get() );
                            eq -> push_back( taskTransfer );
                            eq -> waitForSuccess( taskTransfer );

                            auto guard = BL_SCOPE_GUARD(
                                {
                                    if( connection_t::isProtocolHandshakeNeeded )
                                    {
                                        eq -> wait( taskTransfer );

                                        UTF_REQUIRE( transfer -> isShutdownNeeded() );

                                        transfer -> setCommandId( connection_t::CommandId::NoCommand );
                                        transfer -> protocolOperationsOnly( true );

                                        eq -> push_back( taskTransfer );
                                        eq -> waitForSuccess( taskTransfer );

                                        UTF_REQUIRE( transfer -> hasShutdownCompletedSuccessfully() );
                                        UTF_REQUIRE( ! transfer -> isShutdownNeeded() );
                                    }
                                }
                                );

                            UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                            UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), remotePeerId );
                            UTF_REQUIRE_EQUAL( transfer -> targetPeerId(), uuids::nil() );
                            UTF_REQUIRE_EQUAL( serverConnection -> peerId(), remotePeerId );
                            UTF_REQUIRE_EQUAL( serverConnection -> remotePeerId(), peerId );

                            /*
                             * Verify that resetting the version also resets the remotePeerId() and
                             * then after re-negotiating the version again the remotePeerId() is
                             * again obtained correctly
                             */

                            transfer -> clientVersion( CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1 );
                            UTF_REQUIRE( ! transfer -> isClientVersionNegotiated() );
                            UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                            UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), uuids::nil() );
                            UTF_REQUIRE_EQUAL( transfer -> targetPeerId(), uuids::nil() );

                            transfer -> setCommandInfo( connection_t::CommandId::NoCommand );
                            eq -> push_back( taskTransfer );
                            eq -> waitForSuccess( taskTransfer );
                            UTF_REQUIRE( transfer -> isClientVersionNegotiated() );
                            UTF_REQUIRE_EQUAL( transfer -> clientVersion(), CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1 );

                            UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                            UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), remotePeerId );
                            UTF_REQUIRE_EQUAL( transfer -> targetPeerId(), uuids::nil() );
                            UTF_REQUIRE_EQUAL( serverConnection -> peerId(), remotePeerId );
                            UTF_REQUIRE_EQUAL( serverConnection -> remotePeerId(), peerId );

                            const auto authenticateBackend = [ & ]() -> void
                                {
                                    backend -> authenticationCallback(
                                        [ &backendImpl ]( SAA_in const om::ObjPtr< data::DataBlock >& authenticationToken )
                                            -> om::ObjPtr< data::DataBlock >
                                        {
                                            if( ! BackendImplTestImpl::areBlocksEqual( authenticationToken, backendImpl -> getData() ) )
                                            {
                                                BL_THROW_EC(
                                                    eh::errc::make_error_code( eh::errc::permission_denied ),
                                                    BL_MSG()
                                                        << "Authentication failed"
                                                    );
                                            }

                                            return om::copy( authenticationToken );
                                        }
                                        );

                                    transfer -> clientVersion( CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2 );
                                    transfer -> setCommandInfo(
                                        connection_t::CommandId::SendChunk,
                                        uuids::nil() /* chunkId */,
                                        nullptr /* chunkData */,
                                        BlockTransferDefs::BlockType::Authentication
                                        );

                                    transfer -> setChunkData( backendImpl -> getData() );
                                    eq -> push_back( taskTransfer );
                                    eq -> waitForSuccess( taskTransfer );
                                };

                            /*
                             * Now verify send/recv/remove/flush commands
                             */

                            const auto runSendRecvRemoveFlush = [ & ]( SAA_in const bool storageEnabled ) -> void
                            {
                                transfer -> setChunkData( backendImpl -> getData() );

                                UTF_REQUIRE( 0U == backendImpl -> loadCalls() );
                                UTF_REQUIRE( 0U == backendImpl -> saveCalls() );
                                UTF_REQUIRE( 0U == backendImpl -> removeCalls() );
                                UTF_REQUIRE( 0U == backendImpl -> flushCalls() );

                                if( isAuthenticationRequired )
                                {
                                    authenticateBackend();
                                }

                                UTF_REQUIRE_EQUAL( isAuthenticationRequired, isServerConnectionAuthenticated() );

                                const auto getExpected = [ & ]( SAA_in const std::size_t expected ) -> std::size_t
                                {
                                    return storageEnabled ? expected : 0U;
                                };

                                const auto blockType =
                                    storageEnabled ?
                                        BlockTransferDefs::BlockType::Normal :
                                        BlockTransferDefs::BlockType::TransferOnly;

                                const auto verifyBlock =
                                    [ & ]( SAA_in const om::ObjPtr< data::DataBlock >& dataBlock ) -> void
                                {
                                    if( blockType != BlockTransferDefs::BlockType::TransferOnly )
                                    {
                                        return;
                                    }

                                    /*
                                     * This is a TransferOnly block and its size should equal to
                                     * capacity and the content should be the secure fill byte
                                     */

                                    UTF_REQUIRE_EQUAL( dataBlock -> size(), dataBlock -> capacity() );

                                    const auto size = dataBlock -> size();
                                    const auto* data = dataBlock -> begin();

                                    bool blockIsValid = true;

                                    for( std::size_t i = 0; i < size; ++i )
                                    {
                                        if( data[ i ] != async_wrapper_t::SECURE_BLOCKS_FILL_BYTE )
                                        {
                                            blockIsValid = false;
                                            break;
                                        }
                                    }

                                    UTF_REQUIRE( blockIsValid );
                                };

                                /*
                                 * Send the data twice in a row and then request flush
                                 */

                                transfer -> setCommandId( connection_t::CommandId::SendChunk );
                                transfer -> setBlockType( blockType );
                                transfer -> setChunkId( uuids::create() );
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 1U ) == backendImpl -> saveCalls() );

                                transfer -> setCommandId( connection_t::CommandId::SendChunk );
                                transfer -> setBlockType( blockType );
                                transfer -> setChunkId( uuids::create() );
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> saveCalls() );

                                transfer -> setCommandId( connection_t::CommandId::FlushPeerSessions );
                                transfer -> setBlockType( blockType );
                                transfer -> setChunkId( uuids::nil() );
                                transfer -> detachChunkData();
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 1U ) == backendImpl -> flushCalls() );

                                /*
                                 * Let's now receive data twice in a row
                                 */

                                transfer -> setCommandId( connection_t::CommandId::ReceiveChunk );
                                transfer -> setBlockType( blockType );
                                transfer -> detachChunkData();
                                transfer -> setChunkId( uuids::create() );
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 1U ) == backendImpl -> loadCalls() );
                                verifyBlock( transfer -> getChunkData() );

                                transfer -> setCommandId( connection_t::CommandId::ReceiveChunk );
                                transfer -> setBlockType( blockType );
                                transfer -> detachChunkData();
                                transfer -> setChunkId( uuids::create() );
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> loadCalls() );
                                verifyBlock( transfer -> getChunkData() );

                                /*
                                 * Let's now delete data twice in a row and flush again
                                 */

                                transfer -> setCommandId( connection_t::CommandId::RemoveChunk );
                                transfer -> setBlockType( blockType );
                                transfer -> detachChunkData();
                                transfer -> setChunkId( uuids::create() );
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 1U ) == backendImpl -> removeCalls() );

                                transfer -> setCommandId( connection_t::CommandId::RemoveChunk );
                                transfer -> setBlockType( blockType );
                                transfer -> detachChunkData();
                                transfer -> setChunkId( uuids::create() );
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> removeCalls() );

                                transfer -> setCommandId( connection_t::CommandId::FlushPeerSessions );
                                transfer -> setBlockType( blockType );
                                transfer -> setChunkId( uuids::nil() );
                                transfer -> detachChunkData();
                                eq -> push_back( taskTransfer );
                                eq -> waitForSuccess( taskTransfer );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> flushCalls() );

                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> loadCalls() );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> saveCalls() );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> removeCalls() );
                                UTF_REQUIRE( getExpected( 2U ) == backendImpl -> flushCalls() );

                                UTF_REQUIRE_EQUAL( isAuthenticationRequired, isServerConnectionAuthenticated() );
                            };

                            runSendRecvRemoveFlush( true /* storageEnabled */ );

                            /*
                             * Let's test the V2 protocol enable/disable logic for the new commands
                             */

                            if( ! isAuthenticationRequired )
                            {
                                UTF_REQUIRE_EQUAL(
                                    CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1,
                                    transfer -> clientVersion()
                                    );

                                UTF_REQUIRE_THROW_MESSAGE(
                                    transfer -> setBlockType( BlockTransferDefs::BlockType::TransferOnly ),
                                    ArgumentException,
                                    "This block type requires V2 of the blob server protocol"
                                    );

                                UTF_REQUIRE_THROW_MESSAGE(
                                    transfer -> setCommandInfo(
                                        connection_t::CommandId::SendChunk,
                                        uuids::nil() /* chunkId */,
                                        nullptr /* chunkData */,
                                        BlockTransferDefs::BlockType::Authentication
                                        ),
                                    ArgumentException,
                                    "This block type requires V2 of the blob server protocol"
                                    );

                                UTF_REQUIRE_THROW_MESSAGE(
                                    transfer -> setCommandInfoRawPtr(
                                        connection_t::CommandId::SendChunk,
                                        uuids::nil() /* chunkId */,
                                        nullptr /* dataRawPtr */,
                                        BlockTransferDefs::BlockType::Authentication
                                        ),
                                    ArgumentException,
                                    "This block type requires V2 of the blob server protocol"
                                    );

                                transfer -> clientVersion(
                                    CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2
                                    );
                            }

                            UTF_REQUIRE_EQUAL(
                                CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2,
                                transfer -> clientVersion()
                                );

                            /*
                             * Test disabling backend and then the TransferOnly block types
                             */

                            const auto testDisabledCommand = [ & ]( SAA_in const cpp::void_callback_t& callback )
                            {
                                try
                                {
                                    callback();
                                    UTF_FAIL( "This is expected to throw" );
                                }
                                catch( bl::SystemException& e )
                                {
                                    /*
                                     * This is now expected to throw because the backend is disabled
                                     */

                                    BL_LOG_MULTILINE(
                                        bl::Logging::debug(),
                                        BL_MSG()
                                            << "Expected not permitted exception:\n"
                                            << bl::eh::diagnostic_information( e )
                                        );

                                    const auto* errNo = eh::get_error_info< eh::errinfo_errno >( e );
                                    const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );
                                    UTF_REQUIRE( errNo && ec );

                                    eh::error_code ecExpected( *errNo, eh::generic_category() );

                                    UTF_REQUIRE_EQUAL( *ec, ecExpected );
                                    UTF_REQUIRE_EQUAL( *ec, eh::errc::make_error_code( eh::errc::operation_not_permitted ) );
                                }
                            };

                            backendImpl -> resetStats();
                            backendImpl -> setStorageDisabled( true );

                            testDisabledCommand(
                                [ & ]() -> void
                                {
                                    backendImpl -> load(
                                        uuids::nil()                    /* peerId */,
                                        uuids::create()                 /* chunkId */,
                                        backendImpl -> getData()        /* data */
                                        );
                                }
                                );

                            testDisabledCommand(
                                [ & ]() -> void
                                {
                                    backendImpl -> save(
                                        uuids::nil()                    /* peerId */,
                                        uuids::create()                 /* chunkId */,
                                        backendImpl -> getData()        /* data */
                                        );
                                }
                                );

                            testDisabledCommand(
                                [ & ]() -> void
                                {
                                    backendImpl -> remove(
                                        uuids::nil()                    /* peerId */,
                                        uuids::create()                 /* chunkId */
                                        );
                                }
                                );

                            testDisabledCommand(
                                [ & ]() -> void
                                {
                                    backendImpl -> flushPeerSessions( uuids::nil() /* peerId */ );
                                }
                                );

                            UTF_REQUIRE( 0U == backendImpl -> loadCalls() );
                            UTF_REQUIRE( 0U == backendImpl -> saveCalls() );
                            UTF_REQUIRE( 0U == backendImpl -> removeCalls() );
                            UTF_REQUIRE( 0U == backendImpl -> flushCalls() );

                            runSendRecvRemoveFlush( false /* storageEnabled */ );

                            UTF_REQUIRE( 0U == backendImpl -> loadCalls() );
                            UTF_REQUIRE( 0U == backendImpl -> saveCalls() );
                            UTF_REQUIRE( 0U == backendImpl -> removeCalls() );
                            UTF_REQUIRE( 0U == backendImpl -> flushCalls() );

                            /*
                             * Re-enable the backend layer and then test the normal block types again
                             */

                            backendImpl -> resetStats();
                            backendImpl -> setStorageDisabled( false );

                            runSendRecvRemoveFlush( true /* storageEnabled */ );

                            if( isAuthenticationRequired )
                            {
                                backend -> authenticationCallback( typename async_wrapper_t::datablock_callback_t() );
                            }

                            /*
                             * Let's test client authentication here
                             */

                            transfer -> setCommandInfo(
                                connection_t::CommandId::SendChunk,
                                uuids::nil() /* chunkId */,
                                nullptr /* chunkData */,
                                BlockTransferDefs::BlockType::Authentication
                                );
                            transfer -> setChunkData( backendImpl -> getData() );

                            eq -> push_back( taskTransfer );

                            try
                            {
                                eq -> waitForSuccess( taskTransfer );
                                UTF_FAIL( "This is expected to throw" );
                            }
                            catch( bl::ServerErrorException& e )
                            {
                                UTF_REQUIRE( ! isServerConnectionAuthenticated() );

                                BL_LOG_MULTILINE(
                                    bl::Logging::debug(),
                                    BL_MSG()
                                        << "Expected authentication exception:\n"
                                        << bl::eh::diagnostic_information( e )
                                    );

                                const auto* errNo = eh::get_error_info< eh::errinfo_errno >( e );
                                const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );
                                UTF_REQUIRE( errNo && ec );

                                eh::error_code ecExpected( *errNo, eh::generic_category() );

                                UTF_REQUIRE_EQUAL( *ec, ecExpected );
                                UTF_REQUIRE_EQUAL( *ec, eh::errc::make_error_code( eh::errc::function_not_supported ) );
                            }

                            UTF_REQUIRE( ! isServerConnectionAuthenticated() );

                            const auto testFailedAuthentication = [ & ]( SAA_in const eh::error_code& ecThrown ) -> void
                            {
                                backend -> authenticationCallback(
                                    [ & ]( SAA_in const om::ObjPtr< data::DataBlock >& authenticationToken )
                                        -> om::ObjPtr< data::DataBlock >
                                    {
                                        if(
                                            BackendImplTestImpl::areBlocksEqual(
                                                authenticationToken,
                                                backendImpl -> getData()
                                                )
                                            )
                                        {
                                            if( ecThrown )
                                            {
                                                BL_THROW_EC(
                                                    ecThrown,
                                                    BL_MSG()
                                                        << "Authentication failed"
                                                    );
                                            }
                                            else
                                            {
                                                BL_THROW(
                                                    UnexpectedException(),
                                                    BL_MSG()
                                                        << "Unexpected exception during authentication call"
                                                    );
                                            }
                                        }

                                        return om::copy( authenticationToken );
                                    }
                                    );

                                transfer -> setCommandInfo(
                                    connection_t::CommandId::SendChunk,
                                    uuids::nil() /* chunkId */,
                                    nullptr /* chunkData */,
                                    BlockTransferDefs::BlockType::Authentication
                                    );
                                transfer -> setChunkData( backendImpl -> getData() );
                                eq -> push_back( taskTransfer );

                                try
                                {
                                    eq -> waitForSuccess( taskTransfer );
                                    UTF_FAIL( "This is expected to throw" );
                                }
                                catch( bl::ServerErrorException& e )
                                {
                                    BL_LOG_MULTILINE(
                                        bl::Logging::debug(),
                                        BL_MSG()
                                            << "Expected authentication exception:\n"
                                            << bl::eh::diagnostic_information( e )
                                        );

                                    const auto* errNo = eh::get_error_info< eh::errinfo_errno >( e );
                                    const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );
                                    UTF_REQUIRE( errNo && ec );

                                    eh::error_code ecExpected( *errNo, eh::generic_category() );

                                    /*
                                     * If the exception thrown has an error code it should be transmitted
                                     * to the client code properly
                                     *
                                     * If the exception thrown does not have an error code we should still
                                     * get eh::errc::permission_denied error code on the client side
                                     */

                                    if( ecThrown )
                                    {
                                        UTF_REQUIRE_EQUAL( *ec, ecThrown );
                                        UTF_REQUIRE_EQUAL( ecExpected, ecThrown );
                                    }
                                    else
                                    {
                                        const auto ecPermissionDenied =
                                            eh::errc::make_error_code( eh::errc::permission_denied );

                                        UTF_REQUIRE_EQUAL( *ec, ecPermissionDenied );
                                        UTF_REQUIRE_EQUAL( ecExpected, ecPermissionDenied );
                                    }
                                }

                                UTF_REQUIRE( ! isServerConnectionAuthenticated() );
                            };

                            testFailedAuthentication( eh::error_code() );
                            testFailedAuthentication( eh::errc::make_error_code( eh::errc::permission_denied ) );
                            testFailedAuthentication( eh::errc::make_error_code( eh::errc::filename_too_long ) );

                            authenticateBackend();

                            /*
                             * The channel should now be authenticated!
                             */

                            UTF_REQUIRE( isServerConnectionAuthenticated() );

                            /*
                             * Scheduling and executing CommandId::NoCommand command it should work and it
                             * should just re-negotiate the version again (and serve as ping basically)
                             */

                            transfer -> clientVersion( CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1 );
                            UTF_REQUIRE( ! transfer -> isClientVersionNegotiated() );
                            transfer -> setCommandInfo( connection_t::CommandId::NoCommand );
                            eq -> push_back( taskTransfer );
                            eq -> waitForSuccess( taskTransfer );
                            UTF_REQUIRE( transfer -> isClientVersionNegotiated() );
                            UTF_REQUIRE_EQUAL( transfer -> clientVersion(), CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1 );

                            transfer -> clientVersion( CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2 );
                            UTF_REQUIRE( ! transfer -> isClientVersionNegotiated() );
                            transfer -> setCommandInfo( connection_t::CommandId::NoCommand );
                            eq -> push_back( taskTransfer );
                            eq -> waitForSuccess( taskTransfer );
                            UTF_REQUIRE( transfer -> isClientVersionNegotiated() );
                            UTF_REQUIRE_EQUAL( transfer -> clientVersion(), CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2 );

                            guard.runNow();
                        }
                    }
                    catch( std::exception& )
                    {
                        BL_WARN_NOEXCEPT_BEGIN()

                        /*
                         * Shutdown the acceptor gracefully before we
                         * rethrow the exception
                         */

                        cancelAndWaitForSuccess( eq, taskAcceptor );

                        BL_WARN_NOEXCEPT_END( "simpleConnectAndTransmitDataTest" )

                        throw;
                    }

                    /*
                     * Shutdown the acceptor gracefully and exit
                     */

                    cancelAndWaitForSuccess( eq, taskAcceptor );
                }
            }
            );
    }

    template
    <
        typename Connector
    >
    void runClientPerfTest(
        SAA_in              const bl::om::ObjPtr< bl::data::datablocks_pool_type >&     dataBlocksPool,
        SAA_in_opt          std::string&&                                               host = "localhost",
        SAA_in_opt          const unsigned short                                        port = 28100U,
        SAA_in_opt          const std::size_t                                           connectionsCount = 10U,
        SAA_in_opt          const std::size_t                                           totalSizeInMB = 200U
        )
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace utest;

        const auto backendImpl = BackendImplTestImpl::createInstance();

        std::vector< om::ObjPtr< connection_t > > connections;

        const auto eqTransfers = om::lockDisposable(
            tasks::ExecutionQueueImpl::createInstance(
                tasks::ExecutionQueue::OptionKeepAll
                )
            );

        {
            {
                /*
                 * Testing the connection pooling logic in SendRecvContext class
                 */

                using namespace bl::transfer;

                const auto context = SendRecvContext::createInstance(
                    SimpleEndpointSelectorImpl::createInstance< EndpointSelector >( cpp::copy( host ), port )
                    );

                const auto connector = connector_t::createInstance( std::move( host ), port );
                const auto taskConnector = om::qi< tasks::Task >( connector.get() );
                eqTransfers -> push_back( taskConnector );
                eqTransfers -> waitForSuccess( taskConnector );

                auto connectedStream = connector -> detachStream();

                {
                    /*
                     * Test the configure socket logic and code
                     */

                    const auto ok = TcpSocketCommonBase::tryConfigureConnectedStream( *connectedStream );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "The stream was "
                            << ( ok ? "" : "not " )
                            << "configured successfully"
                        );

                    tcp::socket invalid( ThreadPoolDefault::getDefault() -> aioService() );
                    UTF_REQUIRE( ! TcpSocketCommonBase::tryConfigureConnectedStream( invalid ) );
                }

                context -> putConnection( "key", std::move( connectedStream ) );
                os::sleep( time::seconds( 2 ) );

                {
                    auto socket = context -> tryGetConnection( "key" );
                    UTF_REQUIRE( socket );

                    UTF_REQUIRE( ! context -> tryGetConnection( "key" ) );

                    const auto originalTimeout = context -> maxIdleTimeoutInSeconds();

                    context -> maxIdleTimeoutInSeconds( 2L );
                    UTF_REQUIRE_EQUAL( context -> maxIdleTimeoutInSeconds(), 2L );

                    context -> putConnection( "key", std::move( socket ) );
                    os::sleep( time::seconds( 4 ) );

                    /*
                     * The connections would now be expired and discarded
                     */

                    UTF_REQUIRE( ! context -> tryGetConnection( "key" ) );

                    context -> maxIdleTimeoutInSeconds( originalTimeout );
                    UTF_REQUIRE_EQUAL( context -> maxIdleTimeoutInSeconds(), originalTimeout );
                }
            }

            /*
             * First we establish all connections
             */

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Establishing "
                    << connectionsCount
                    << " connections...."
                );

            const bl::uuid_t peerId = uuids::create();

            for( std::size_t i = 0; i< connectionsCount; ++i )
            {
                const auto connector = connector_t::createInstance( std::move( host ), port );
                const auto taskConnector = om::qi< tasks::Task >( connector.get() );
                eqTransfers -> push_back( taskConnector );
                eqTransfers -> waitForSuccess( taskConnector );

                auto transfer = connection_t::createInstance(
                    connection_t::CommandId::NoCommand,
                    peerId,
                    dataBlocksPool
                    );

                transfer -> attachStream( connector -> detachStream() );
                transfer -> setChunkData( backendImpl -> getData() );

                connections.push_back( std::move( transfer ) );
            }

            const auto t1 = bl::time::microsec_clock::universal_time();

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Sending "
                    << totalSizeInMB
                    << " MB ..."
                );

            try
            {
                /*
                 * Let's do simple sequential perf test
                 */

                const std::size_t oneMB = 1024 * 1024;

                BL_ASSERT( backendImpl -> getData() -> size() <= oneMB );
                BL_ASSERT( 0 == ( oneMB % backendImpl -> getData() -> size() ) );

                const std::size_t numberOfBlocks = totalSizeInMB * ( oneMB / backendImpl -> getData() -> size() );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Sending "
                        << numberOfBlocks
                        << " number of "
                        << backendImpl -> getData() -> size()
                        << " blocks"
                    );

                for( std::size_t i = 0; i < numberOfBlocks; ++i )
                {
                    if( ! connections.empty() )
                    {
                        connections.back() -> setCommandId( connection_t::CommandId::SendChunk );
                        connections.back() -> setChunkId( uuids::create() );
                        eqTransfers -> push_back( om::qi< tasks::Task >( connections.back().get() ) );
                        connections.erase( connections.end() - 1 );

                        continue;
                    }

                    const auto taskTransfer = eqTransfers -> top( true /* wait */ );

                    if( taskTransfer -> isFailed() )
                    {
                        cpp::safeRethrowException( taskTransfer -> exception() );
                    }

                    const auto transfer = om::qi< connection_t >( taskTransfer );
                    transfer -> setCommandId( connection_t::CommandId::SendChunk );
                    transfer -> setChunkId( uuids::create() );

                    eqTransfers -> push_back( taskTransfer );
                }

                {
                    eqTransfers -> flush();

                    const auto taskTransfer = eqTransfers -> top( false /* wait */ );
                    BL_ASSERT( taskTransfer );

                    if( taskTransfer -> isFailed() )
                    {
                        cpp::safeRethrowException( taskTransfer -> exception() );
                    }

                    /*
                     * Request flush
                     */

                    const auto transfer = om::qi< connection_t >( taskTransfer );
                    transfer -> setCommandId( connection_t::CommandId::FlushPeerSessions );
                    transfer -> setChunkId( uuids::nil() );
                    transfer -> detachChunkData();

                    eqTransfers -> push_back( taskTransfer );

                    eqTransfers -> flushAndDiscardReady();

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Session was flushed on the server"
                        );
                }
            }
            catch( std::exception& )
            {
                eqTransfers -> forceFlushNoThrow();
                throw;
            }

            const auto duration = bl::time::microsec_clock::universal_time() - t1;
            const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Sending "
                    << totalSizeInMB
                    << " MB took "
                    << durationInSeconds
                    << " seconds; "
                    << "speed is "
                    << ( totalSizeInMB / durationInSeconds )
                    << " MB/s"
                );
        }
    }

    template
    <
        typename Acceptor,
        typename Connector
    >
    void simplePerfTest(
        SAA_in_opt          std::string&&                                               host = "localhost",
        SAA_in_opt          const unsigned short                                        port = 28100U,
        SAA_in_opt          const std::size_t                                           connectionsCount = 10U,
        SAA_in_opt          const std::size_t                                           totalSizeInMB = 200U
        )
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace utest;

        typedef typename Acceptor::async_wrapper_t                                              async_wrapper_t;
        typedef typename async_wrapper_t::backend_interface_t                                   backend_interface_t;

        test::MachineGlobalTestLock lock;

        tasks::scheduleAndExecuteInParallel(
            [ &host, &port, &connectionsCount, &totalSizeInMB ](
                SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq
                ) -> void
            {
                const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();
                const auto dataBlocksPool = datablocks_pool_type::createInstance();
                const auto backendImpl = BackendImplTestImpl::createInstance();

                const auto backend = om::lockDisposable(
                    async_wrapper_t::createInstance(
                        om::qi< backend_interface_t >( backendImpl ) /* writeBackend */,
                        om::qi< backend_interface_t >( backendImpl ) /* readBackend */,
                        test::UtfArgsParser::threadsCount(),
                        om::qi< TaskControlToken >( controlToken ),
                        0U /* maxConcurrentTasks */,
                        dataBlocksPool
                        )
                    );

                {
                    const auto acceptor = Acceptor::template createInstance< Acceptor >(
                        controlToken,
                        dataBlocksPool,
                        "localhost",
                        28100,
                        bl::str::empty() /* privateKeyPem */,
                        bl::str::empty() /* certificatePem */,
                        backend
                        );

                    UTF_REQUIRE( acceptor );

                    /*
                     * Start the acceptor and sleep for a couple of seconds to give it a chance to start
                     */

                    const auto taskAcceptor = om::qi< tasks::Task >( acceptor );
                    eq -> push_back( taskAcceptor );

                    BL_SCOPE_EXIT( cancelAndWaitForSuccess( eq, taskAcceptor ); );

                    /*
                     * For this particular test there is no reliable way except to
                     * use some arbitrary value which is large enough to not
                     * break in the CI and normally
                     */

                    os::sleep( time::milliseconds( 5000 ) );

                    runClientPerfTest< Connector >(
                        dataBlocksPool,
                        std::forward< std::string >( host ),
                        port,
                        connectionsCount,
                        totalSizeInMB
                        );
                }
            }
            );
    }

    template
    <
        typename Acceptor
    >
    void simplePerfStartServer()
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace utest;
        using namespace test;

        typedef typename Acceptor::async_wrapper_t                                              async_wrapper_t;
        typedef typename async_wrapper_t::backend_interface_t                                   backend_interface_t;

        tasks::scheduleAndExecuteInParallel(
            []( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
            {
                const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();
                const auto dataBlocksPool = datablocks_pool_type::createInstance();

                const auto backendImpl = BackendImplTestImpl::createInstance();

                const auto backend = om::lockDisposable(
                    async_wrapper_t::createInstance(
                        om::qi< backend_interface_t >( backendImpl ) /* writeBackend */,
                        om::qi< backend_interface_t >( backendImpl ) /* readBackend */,
                        test::UtfArgsParser::threadsCount(),
                        om::qi< TaskControlToken >( controlToken ),
                        0U /* maxConcurrentTasks */,
                        dataBlocksPool
                        )
                    );

                {
                    const auto acceptor = Acceptor::template createInstance(
                        controlToken,
                        dataBlocksPool,
                        "localhost",
                        28100,
                        bl::str::empty() /* privateKeyPem */,
                        bl::str::empty() /* certificatePem */,
                        backend
                        );

                    UTF_REQUIRE( acceptor );

                    startAcceptor( acceptor, eq );
                }
            }
            );
    }

    namespace
    {
        typedef messaging::BackendProcessing::OperationId                               OperationId;
        typedef messaging::BackendProcessing::CommandId                                 CommandId;

        class LocalBackendProcessing : public messaging::BackendProcessingBase
        {
        protected:

            typedef LocalBackendProcessing                                              this_type;

            const std::size_t                                                           m_dataExpectedOffset;
            const std::string                                                           m_dataProcessed;
            const std::string                                                           m_dataUnprocessed;

            LocalBackendProcessing(
                SAA_in              const std::size_t                                   dataExpectedOffset,
                SAA_in              std::string&&                                       dataProcessed,
                SAA_in              std::string&&                                       dataUnprocessed
                )
                :
                m_dataExpectedOffset( dataExpectedOffset ),
                m_dataProcessed( BL_PARAM_FWD( dataProcessed ) ),
                m_dataUnprocessed( BL_PARAM_FWD( dataUnprocessed ) )
            {
            }

            void processBlock( SAA_in const om::ObjPtrCopyable< data::DataBlock >& dataBlock )
            {
                const auto protocolDataOffsetIn = dataBlock -> offset1();

                UTF_REQUIRE_EQUAL( protocolDataOffsetIn, m_dataExpectedOffset );

                UTF_REQUIRE( ( m_dataExpectedOffset + m_dataProcessed.size() ) == dataBlock -> size() );

                UTF_REQUIRE_EQUAL(
                    0,
                    std::memcmp(
                        dataBlock -> begin() + m_dataExpectedOffset,
                        m_dataUnprocessed.c_str(),
                        m_dataUnprocessed.size()
                        )
                    );

                std::memcpy(
                    dataBlock -> begin() + m_dataExpectedOffset,
                    m_dataProcessed.c_str(),
                    m_dataProcessed.size()
                    );
            }

        public:

            virtual auto createBackendProcessingTask(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const CommandId                                 commandId,
                SAA_in                  const bl::uuid_t&                               sessionId,
                SAA_in                  const bl::uuid_t&                               chunkId,
                SAA_in_opt              const bl::uuid_t&                               sourcePeerId,
                SAA_in_opt              const bl::uuid_t&                               targetPeerId,
                SAA_in_opt              const om::ObjPtr< data::DataBlock >&            data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                BL_UNUSED( operationId );
                BL_UNUSED( commandId );
                BL_UNUSED( sessionId );
                BL_UNUSED( chunkId );
                BL_UNUSED( sourcePeerId );
                BL_UNUSED( targetPeerId );

                return tasks::SimpleTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &this_type::processBlock,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        om::ObjPtrCopyable< data::DataBlock >( data )
                        )
                    );
            }
        };

        typedef om::ObjectImpl< LocalBackendProcessing > LocalBackendProcessingImpl;

    } // __unnamed

    template
    <
        typename DispatchingBackend,
        typename Connector,
        typename AsyncWrapper
    >
    void simpleConnectAndTransmitDataOutgoingTest()
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace utest;

        typedef bl::tasks::detail::BlockTransferServerStateImpl< AsyncWrapper >                 BlockTransferServerState;
        typedef typename AsyncWrapper::backend_interface_t                                      backend_interface_t;

        typedef bl::tasks::TcpBlockTransferServerConnectionImpl
        <
            typename DispatchingBackend::acceptor_t::stream_t,
            AsyncWrapper
        >
        server_connection_t;

        typedef messaging::BrokerErrorCodes BrokerErrorCodes;

        test::MachineGlobalTestLock lock;

        tasks::scheduleAndExecuteInParallel(
            [ ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
            {
                const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();
                const auto dataBlocksPool = datablocks_pool_type::createInstance();
                const auto backendImpl = BackendImplTestImpl::createInstance();

                const auto backend = om::lockDisposable(
                    AsyncWrapper::template createInstance(
                        om::qi< backend_interface_t >( backendImpl ) /* writeBackend */,
                        om::qi< backend_interface_t >( backendImpl ) /* readBackend */,
                        test::UtfArgsParser::threadsCount(),
                        om::qi< TaskControlToken >( controlToken ),
                        0U /* maxConcurrentTasks */,
                        dataBlocksPool
                        )
                    );

                const auto serverState = BlockTransferServerState::createInstance( dataBlocksPool, backend );

                {
                    typedef tasks::detail::CommandBlock                                             CommandBlock;

                    typedef messaging::BackendProcessing::OperationId                               OperationId;
                    typedef messaging::BackendProcessing::CommandId                                 CommandId;

                    /*
                     * Define protocol data patterns for valid, invalid and unprocessed
                     */

                    const std::size_t protocolDataSize = 42U;
                    const std::size_t protocolDataOffset = DataBlock::defaultCapacity() - protocolDataSize;
                    const std::string protocolData( protocolDataSize, 'A' );

                    UTF_REQUIRE_EQUAL( protocolData.size(), protocolDataSize );

                    for( std::size_t i = 0U; i < protocolDataSize; ++i )
                    {
                        UTF_REQUIRE_EQUAL( protocolData[ i ], 'A' );
                    }

                    const std::string protocolDataInvalid( protocolDataSize, 'B' );
                    const std::string protocolDataUnprocessed( protocolDataSize, 'U' );

                    UTF_REQUIRE_EQUAL( protocolData.size(), protocolDataInvalid.size() );
                    UTF_REQUIRE_EQUAL( protocolData.size(), protocolDataUnprocessed.size() );

                    /*
                     * Define some local callbacks to be shared and used below:
                     *
                     * createBlock( ... ) to allocate new block from the pool initialize it accordingly
                     *
                     * onReady( ... ) callback which will mark data block with a special pattern and
                     *     return it to the pool
                     */

                    const auto createBlock = [ & ]( SAA_in const bool unprocessed ) -> om::ObjPtr< data::DataBlock >
                    {
                        auto dataBlock = DataBlock::get( dataBlocksPool );

                        UTF_REQUIRE_EQUAL( DataBlock::defaultCapacity(), dataBlock -> capacity() );

                        dataBlock -> setSize( dataBlock -> capacity() );

                        UTF_REQUIRE( ( protocolDataOffset + protocolData.size() ) == dataBlock -> size() );

                        const auto& dataToCopy = unprocessed ? protocolDataUnprocessed : protocolData;

                        std::memcpy(
                            dataBlock -> begin() + protocolDataOffset,
                            dataToCopy.c_str(),
                            dataToCopy.size()
                            );

                        dataBlock -> setOffset1( protocolDataOffset );

                        return dataBlock;
                    };

                    const auto onReady = [ & ](
                        SAA_in              const om::ObjPtrCopyable< DataBlock >&          dataBlock,
                        SAA_in              const std::exception_ptr&                       eptr
                        )
                        -> void
                    {
                        BL_NOEXCEPT_BEGIN()

                        if( eptr )
                        {
                            cpp::safeRethrowException( eptr );
                        }

                        UTF_REQUIRE_EQUAL( protocolDataOffset, dataBlock -> offset1() );

                        UTF_REQUIRE( ( protocolDataOffset + protocolData.size() ) == dataBlock -> size() );

                        UTF_REQUIRE_EQUAL(
                            0,
                            std::memcmp(
                                dataBlock -> begin() + protocolDataOffset,
                                protocolData.c_str(),
                                protocolData.size()
                                )
                            );

                        std::memcpy(
                            dataBlock -> begin() + protocolDataOffset,
                            protocolDataInvalid.c_str(),
                            protocolDataInvalid.size()
                            );

                        dataBlocksPool -> put( om::copy( dataBlock ) );

                        BL_NOEXCEPT_END()
                    };

                    /*
                     * Create the dispatching backend and run the tests
                     */

                    const long heartbeatIntervalInSeconds = 2L;

                    const auto processingBackend =
                        LocalBackendProcessingImpl::template createInstance< messaging::BackendProcessing >(
                            protocolDataOffset                          /* dataExpectedOffset */,
                            cpp::copy( protocolData )                   /* dataProcessed */,
                            cpp::copy( protocolDataUnprocessed )        /* dataUnprocessed */
                            );

                    const auto peerId = uuids::create();

                    const auto dispatchingBackendImpl = om::lockDisposable(
                        DispatchingBackend::template createInstance< DispatchingBackend >(
                            om::copy( processingBackend ),
                            controlToken,
                            dataBlocksPool,
                            "localhost",
                            28100,
                            test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
                            test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
                            peerId,
                            time::seconds( heartbeatIntervalInSeconds )         /* heartbeatInterval */
                            )
                        );

                    UTF_REQUIRE( dispatchingBackendImpl );

                    const auto& acceptor = dispatchingBackendImpl -> acceptor();

                    /*
                     * Sleep for a couple of seconds to give it a chance to start the server
                     */

                    os::sleep( time::milliseconds( 2000 ) );

                    {
                        {
                            /*
                             * Simply create a large # of connections first to observe the smooth
                             * logging and then shut them down immediately
                             */

                            tasks::scheduleAndExecuteInParallel(
                                [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eqLocal ) -> void
                                {
                                    const std::size_t maxConnections = 220U;

                                    for( std::size_t i = 0U; i < maxConnections; ++i )
                                    {
                                        const auto connector =
                                            Connector::template createInstance( "localhost", 28100 );

                                        const auto taskConnector = om::qi< tasks::Task >( connector );
                                        eqLocal -> push_back( taskConnector );
                                        eqLocal -> waitForSuccess( taskConnector );

                                        const auto peerId = uuids::create();

                                        const auto transfer =
                                            server_connection_t::createInstance( serverState, peerId );

                                        UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                                        UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), uuids::nil() );

                                        transfer -> attachStream( connector -> detachStream() );
                                        eqLocal -> push_back( om::qi< tasks::Task >( transfer ) );
                                    }

                                    os::sleep( time::seconds( 1L ) );

                                    eqLocal -> forceFlushNoThrow();

                                    /*
                                     * Note that after we shutdown the connections on the client side we need
                                     * to wait for at least heartbeatIntervalInSeconds + some extra timeout
                                     * to ensure that the server tasks have sent heartbeat messages and have
                                     * disconnected (after which point we should not have any server tasks
                                     * associated with the acceptor - see UTF_REQUIRE_EQUAL check below)
                                     */

                                    os::sleep( time::seconds( heartbeatIntervalInSeconds ) + time::seconds( 2L ) );

                                    UTF_REQUIRE_EQUAL( acceptor -> activeEndpoints().size(), 0U );
                                }
                                );
                        }

                        const auto connector = Connector::template createInstance< Connector >( "localhost", 28100 );
                        const auto taskConnector = om::qi< tasks::Task >( connector.get() );
                        eq -> push_back( taskConnector );
                        eq -> waitForSuccess( taskConnector );

                        const auto peerId = uuids::create();

                        const auto transfer = server_connection_t::createInstance( serverState, peerId );

                        /*
                         * This will allow to test for successful SSL shutdown as the
                         * default behavior is to not do SSL shutdown if task is canceled
                         */

                        transfer -> forceShutdownContinuation( true );

                        UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                        UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), uuids::nil() );

                        transfer -> attachStream( connector -> detachStream() );
                        const auto taskTransfer = om::qi< tasks::Task >( transfer );
                        eq -> push_back( taskTransfer );

                        /*
                         * The taskTransfer task is now a server style connection and will never terminate
                         * on its own, so we need to cancel it explicitly when we exit the scope
                         */

                        auto guard = BL_SCOPE_GUARD(
                            {
                                if( server_connection_t::isProtocolHandshakeNeeded )
                                {
                                    UTF_REQUIRE( transfer -> isShutdownNeeded() );
                                }

                                BL_NOEXCEPT_BEGIN()

                                try
                                {
                                    cancelAndWaitForSuccess( eq, taskTransfer );
                                }
                                catch( std::exception& e )
                                {
                                    const auto* errorCode = eh::get_error_info< eh::errinfo_error_code >( e );

                                    if(
                                        ! TcpSocketCommonBase::isExpectedSocketException(
                                            true /* isCancelExpected */,
                                            errorCode
                                            )
                                        )
                                    {
                                        throw;
                                    }
                                }

                                BL_NOEXCEPT_END()

                                /*
                                 * Since the task was cancelled by forcefully closing the socket
                                 * it would terminate without having a chance to execute proper
                                 * SSL shutdown sequence (and this is expected)
                                 *
                                 * However since the SSL tasks explicitly handles this
                                 * isShutdownNeeded() should still return false
                                 */

                                UTF_REQUIRE( ! transfer -> hasShutdownCompletedSuccessfully() );
                                UTF_REQUIRE( ! transfer -> isShutdownNeeded() );
                            }
                            );

                        /*
                         * Wait for the server connection to be established
                         */

                        typedef typename DispatchingBackend::acceptor_t             acceptor_t;
                        typedef typename acceptor_t::connection_t                   connection_t;

                        std::size_t retries = 0;
                        const std::size_t maxRetries = 2 * 60;
                        om::ObjPtr< connection_t > serverTask;

                        for( ;; )
                        {
                            os::sleep( time::seconds( 1 ) );

                            chkTaskCompletedOkOrRunning( acceptor );
                            chkTaskCompletedOkOrRunning( transfer );

                            const auto serverEndpoints = acceptor -> activeEndpoints();

                            if( serverEndpoints.size() )
                            {
                                UTF_REQUIRE_EQUAL( serverEndpoints.size(), 1U );

                                serverTask = om::qi< connection_t >( serverEndpoints.back() );

                                chkTaskCompletedOkOrRunning( serverTask );
                                if( serverTask -> lastSuccessfulHeartbeat() != time::neg_infin )
                                {
                                    break;
                                }
                            }

                            if( retries > maxRetries )
                            {
                                UTF_FAIL( "Connection with server can't be established" );
                            }

                            ++retries;
                        }

                        UTF_REQUIRE( serverTask -> lastSuccessfulHeartbeat() != time::neg_infin );

                        const auto& serverConnection = serverTask -> connection();
                        UTF_REQUIRE( serverConnection -> isClientVersionNegotiated() );

                        UTF_REQUIRE_EQUAL( transfer -> peerId(), peerId );
                        UTF_REQUIRE_EQUAL( transfer -> remotePeerId(), serverConnection -> peerId() );
                        UTF_REQUIRE_EQUAL( serverConnection -> remotePeerId(), transfer -> peerId() );

                        UTF_REQUIRE(
                            serverConnection -> targetPeerId() == uuids::nil() ||
                            serverConnection -> targetPeerId() == transfer -> peerId()
                            );

                        UTF_REQUIRE_EQUAL(
                            serverConnection -> clientVersion(),
                            CommandBlock::BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2
                            );

                        /*
                         * Wait until the last heartbeat changes and ensure it has moved forward
                         */

                        const auto waitForNewHeartbeat = [ & ](
                            SAA_in              const time::ptime&                              referenceTimestamp
                            ) -> time::ptime
                        {
                            retries = 0;
                            time::ptime newHeartbeat;

                            for( ;; )
                            {
                                newHeartbeat = serverTask -> lastSuccessfulHeartbeat();

                                if( newHeartbeat > referenceTimestamp )
                                {
                                    break;
                                }

                                os::sleep( time::seconds( 1 ) );

                                ++retries;
                            }

                            return newHeartbeat;
                        };

                        const auto lastSuccessfulHeartbeat = serverTask -> lastSuccessfulHeartbeat();
                        UTF_REQUIRE( waitForNewHeartbeat( lastSuccessfulHeartbeat ) > lastSuccessfulHeartbeat );

                        /*
                         * Send a data block and then verify that the targetPeerId and sourcePeerId
                         * are propagated correctly
                         */

                        auto targetPeerId = uuids::create();
                        const auto sourcePeerId = serverConnection -> peerId();

                        const auto waitForBlocks = [ & ]( SAA_in const std::size_t blocksNo ) -> void
                        {
                            retries = 0;

                            for( ;; )
                            {
                                chkTaskCompletedOkOrRunning( acceptor );
                                chkTaskCompletedOkOrRunning( transfer );
                                chkTaskCompletedOkOrRunning( serverTask );

                                os::sleep( time::seconds( 1 ) );

                                BL_ASSERT( backendImpl -> saveCalls() <= blocksNo );

                                if( backendImpl -> saveCalls() == blocksNo )
                                {
                                    break;
                                }

                                if( retries > maxRetries )
                                {
                                    UTF_FAIL( "Connection with server can't be established" );
                                }

                                ++retries;
                            }
                        };

                        const auto scheduleBlocks = [ & ]( SAA_in const std::size_t noOfBlocks ) -> std::size_t
                        {
                            for( std::size_t i = 0U; i < noOfBlocks; ++i )
                            {
                                const auto dataBlock = createBlock( false /* unprocessed */ );

                                try
                                {
                                    serverTask -> scheduleBlock(
                                        targetPeerId,
                                        om::copy( dataBlock ),
                                        cpp::bind< void /* result_type */ >(
                                            onReady,
                                            om::ObjPtrCopyable< DataBlock >( dataBlock ),
                                            _1 /* onReady - the NOEXCEPT completion callback */
                                            )
                                        );
                                }
                                catch( ServerErrorException& e )
                                {
                                    const auto* ec = e.errorCode();

                                    UTF_REQUIRE( ec );

                                    UTF_REQUIRE(
                                        eh::errc::make_error_code( BrokerErrorCodes::TargetPeerQueueFull ) == *ec
                                        );

                                    return i;
                                }
                            }

                            return noOfBlocks;
                        };

                        std::size_t totalBlocksScheduled = 0U;

                        backendImpl -> setExpectRealData( true );

                        UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );

                        UTF_REQUIRE_EQUAL( scheduleBlocks( 1 ), 1U );
                        totalBlocksScheduled += 1;
                        waitForBlocks( totalBlocksScheduled );

                        UTF_REQUIRE_EQUAL( targetPeerId, backendImpl -> targetPeerId() );
                        UTF_REQUIRE_EQUAL( sourcePeerId, backendImpl -> sourcePeerId() );
                        UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );

                        targetPeerId = uuids::create();

                        UTF_REQUIRE_EQUAL( scheduleBlocks( 1 ), 1U );
                        totalBlocksScheduled += 1;
                        waitForBlocks( totalBlocksScheduled );

                        UTF_REQUIRE_EQUAL( targetPeerId, backendImpl -> targetPeerId() );
                        UTF_REQUIRE_EQUAL( sourcePeerId, backendImpl -> sourcePeerId() );
                        UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );

                        auto noOfBlocksToSchedule = connection_t::BLOCK_QUEUE_SIZE / 2;

                        UTF_REQUIRE_EQUAL( scheduleBlocks( noOfBlocksToSchedule ), noOfBlocksToSchedule );
                        totalBlocksScheduled += noOfBlocksToSchedule;
                        waitForBlocks( totalBlocksScheduled );

                        UTF_REQUIRE_EQUAL( targetPeerId, backendImpl -> targetPeerId() );
                        UTF_REQUIRE_EQUAL( sourcePeerId, backendImpl -> sourcePeerId() );
                        UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );

                        os::sleep( time::seconds( 2L * heartbeatIntervalInSeconds ) );

                        UTF_REQUIRE_EQUAL( targetPeerId, backendImpl -> targetPeerId() );
                        UTF_REQUIRE_EQUAL( sourcePeerId, backendImpl -> sourcePeerId() );
                        UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );

                        typedef cpp::function< std::size_t () > blocks_schedule_callback_t;

                        const auto perfTest = [ & ]( SAA_in const blocks_schedule_callback_t& callback ) -> void
                        {
                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Start sending data ..."
                                );

                            const auto t1 = bl::time::microsec_clock::universal_time();

                            const auto noOfBlocksScheduled = callback();

                            /*
                             * Note that to measure accurately we need to remember the last heartbeat
                             * before we start sending and then measure from t1 until a new heartbeat
                             * but not until the time after the wait because the wait can add up a
                             * second rounding time which can skew the measurement significantly
                             */

                            const auto duration = waitForNewHeartbeat( t1 /* referenceTimestamp */ ) - t1;
                            const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

                            const auto totalSizeInMB =
                                ( noOfBlocksScheduled * DataBlock::defaultCapacity() ) / ( 1024 * 1024.0 );

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Sending "
                                    << totalSizeInMB
                                    << " MB ("
                                    << noOfBlocksScheduled
                                    << " messages) took "
                                    << durationInSeconds
                                    << " seconds; "
                                    << "speed is "
                                    << ( totalSizeInMB / durationInSeconds )
                                    << " MB/s"
                                );

                            UTF_REQUIRE_EQUAL( targetPeerId, backendImpl -> targetPeerId() );
                            UTF_REQUIRE_EQUAL( sourcePeerId, backendImpl -> sourcePeerId() );
                            UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );

                            os::sleep( time::seconds( 2 * heartbeatIntervalInSeconds ) );

                            UTF_REQUIRE_EQUAL( totalBlocksScheduled, backendImpl -> saveCalls() );
                        };

                        /*
                         * Schedule some blocks directly on the acceptor to test raw performance
                         */

                        const auto noOfPerfTestBlocks = connection_t::BLOCK_QUEUE_SIZE * 5;

                        perfTest(
                            [ & ]() -> std::size_t
                            {
                                noOfBlocksToSchedule = noOfPerfTestBlocks;
                                const auto noOfBlocksScheduled = scheduleBlocks( noOfBlocksToSchedule );

                                UTF_REQUIRE( noOfBlocksScheduled <= noOfBlocksToSchedule );
                                totalBlocksScheduled += noOfBlocksScheduled;
                                waitForBlocks( totalBlocksScheduled );

                                return noOfBlocksScheduled;
                            }
                            );

                        /*
                         * Schedule some blocks via the dispatching backend interface to test
                         * this execution path
                         */

                        perfTest(
                            [ & ]() -> std::size_t
                            {
                                const std::size_t noOfBlocksToSchedule = noOfPerfTestBlocks;
                                std::size_t noOfBlocksScheduled = 0U;

                                tasks::scheduleAndExecuteInParallel(
                                    [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eqLocal ) -> void
                                    {
                                        eqLocal -> setOptions( ExecutionQueue::OptionKeepAll );

                                        const auto dispatchingBackend =
                                            om::qi< messaging::BackendProcessing >( dispatchingBackendImpl );

                                        const auto sessionId = uuids::create();
                                        const auto chunkId = uuids::create();

                                        targetPeerId = transfer -> peerId();

                                        std::unordered_map< const Task*, om::ObjPtr< DataBlock > > dataBlocksInProgress;

                                        const auto scheduleNewBlock = [ & ]() -> void
                                        {
                                            const auto dataBlock = createBlock( true /* unprocessed */ );

                                            const auto task = dispatchingBackend -> createBackendProcessingTask(
                                                OperationId::Put,
                                                CommandId::None,
                                                sessionId,
                                                chunkId,
                                                sourcePeerId,
                                                targetPeerId,
                                                dataBlock
                                                );

                                            const auto pair =
                                                dataBlocksInProgress.emplace( task.get(), om::copy( dataBlock ) );

                                            UTF_REQUIRE( pair.second );

                                            eqLocal -> push_back( task );
                                        };

                                        /*
                                         * We should be able to schedule safely at least
                                         * connection_t::BLOCK_QUEUE_SIZE / 2 workers before filling up the queue
                                         */

                                        const std::size_t noOfWorkers = connection_t::BLOCK_QUEUE_SIZE / 2;

                                        for( std::size_t i = 0U; i < noOfWorkers; ++i )
                                        {
                                            scheduleNewBlock();
                                        }

                                        noOfBlocksScheduled += noOfWorkers;

                                        const auto popTask = [ & ]() -> void
                                        {
                                            const auto task = eqLocal -> pop();

                                            const auto eptr = task -> exception();

                                            if( eptr )
                                            {
                                                cpp::safeRethrowException( eptr );
                                            }

                                            const auto pos = dataBlocksInProgress.find( task.get() );
                                            UTF_REQUIRE( pos != std::end( dataBlocksInProgress ) );

                                            /*
                                             * onReady will check the block has the correct
                                             * pattern and then will mark it with a different pattern
                                             * before it gets returned in the data blocks pool
                                             */

                                            onReady( pos -> second, eptr );
                                            dataBlocksInProgress.erase( pos );
                                        };

                                        while( noOfBlocksScheduled < noOfBlocksToSchedule )
                                        {
                                            popTask();

                                            scheduleNewBlock();

                                            ++noOfBlocksScheduled;
                                        }

                                        while( ! eqLocal -> isEmpty() )
                                        {
                                            popTask();
                                        }

                                        totalBlocksScheduled += noOfBlocksScheduled;
                                    }
                                    );

                                return noOfBlocksScheduled;
                            }
                            );

                        os::sleep( time::seconds( 4 ) );

                        guard.runNow();

                        retries = 0;

                        for( ;; )
                        {
                            os::sleep( time::seconds( 1 ) );

                            chkTaskCompletedOkOrRunning( acceptor );

                            if( Task::Completed == serverTask -> getState() )
                            {
                                break;
                            }

                            if( retries > maxRetries )
                            {
                                UTF_FAIL( "The server task did not finish in the expected time" );
                            }

                            ++retries;
                        }

                        UTF_REQUIRE( Task::Completed == serverConnection -> getState() );

                        /*
                         * Since the remote peer task is cancelled by closing the socket
                         * this task would terminate without having a chance to execute
                         * proper shutdown (likely with one of the expected
                         * error codes which happen when the socket is closed abruptly)
                         *
                         * However since shutdown was attempted isShutdownNeeded() should
                         * still return false
                         */

                        UTF_REQUIRE( ! serverConnection -> hasShutdownCompletedSuccessfully() );
                        UTF_REQUIRE( ! serverConnection -> isShutdownNeeded() );
                    }
                }
            }
            );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( IO_BasicTests )
{
    basicAcceptorTest< bl::tasks::TcpBlockServerDataChunkStorage >();
}

UTF_AUTO_TEST_CASE( IO_BasicMessageDispatcherTests )
{
    basicAcceptorTest< bl::tasks::TcpBlockServerMessageDispatcher >();
}

UTF_AUTO_TEST_CASE( IO_SimpleAcceptorStartStopTests )
{
    simpleConnectAndTransmitDataTest< bl::tasks::TcpBlockServerDataChunkStorage, connector_t >();
}

UTF_AUTO_TEST_CASE( IO_SimpleAcceptorStartStopMessageDispatcherTests )
{
    simpleConnectAndTransmitDataTest< bl::tasks::TcpBlockServerMessageDispatcher, connector_t >();
}

UTF_AUTO_TEST_CASE( IO_SimpleConnectAndTransmitDataTests )
{
    simpleConnectAndTransmitDataTest< bl::tasks::TcpBlockServerDataChunkStorage, connector_t >(
        true /* startConnector */
        );
}

UTF_AUTO_TEST_CASE( IO_AuthenticatedConnectAndTransmitDataTests )
{
    simpleConnectAndTransmitDataTest< bl::tasks::TcpBlockServerDataChunkStorage, connector_t >(
        true /* startConnector */,
        true /* isAuthenticationRequired */
        );
}

UTF_AUTO_TEST_CASE( IO_SimpleConnectAndTransmitDataMessageDispatcherTests )
{
    simpleConnectAndTransmitDataTest< bl::tasks::TcpBlockServerMessageDispatcher, connector_t >(
        true /* startConnector */
        );
}

UTF_AUTO_TEST_CASE( IO_SslSimpleConnectAndTransmitDataMessageDispatcherTests )
{
    simpleConnectAndTransmitDataTest< bl::tasks::TcpSslBlockServerMessageDispatcher, ssl_connector_t >(
        true /* startConnector */
        );
}

UTF_AUTO_TEST_CASE( IO_SimpleConnectAndTransmitDataMessageDispatcherOutgoingTests )
{
    simpleConnectAndTransmitDataOutgoingTest<
        bl::messaging::BrokerDispatchingBackendProcessingImpl               /* DispatchingBackend */,
        connector_t,                                                        /* Connector */
        bl::tasks::TcpBlockServerMessageDispatcher::async_wrapper_t         /* AsyncWrapper */
        >();
}

UTF_AUTO_TEST_CASE( IO_SslSimpleConnectAndTransmitDataMessageDispatcherOutgoingTests )
{
    simpleConnectAndTransmitDataOutgoingTest<
        bl::messaging::SslBrokerDispatchingBackendProcessingImpl            /* DispatchingBackend */,
        ssl_connector_t,                                                    /* Connector */
        bl::tasks::TcpSslBlockServerMessageDispatcher::async_wrapper_t      /* AsyncWrapper */
        >();
}

UTF_AUTO_TEST_CASE( IO_SimplePerfTests )
{
    using namespace test;

    simplePerfTest< bl::tasks::TcpBlockServerDataChunkStorage, connector_t >(
        std::string( UtfArgsParser::host() ),
        UtfArgsParser::port(),
        UtfArgsParser::connections(),
        UtfArgsParser::dataSizeInMB()
        );
}

UTF_AUTO_TEST_CASE( IO_SimplePerfMessageDispatcherTests )
{
    using namespace test;

    simplePerfTest< bl::tasks::TcpBlockServerMessageDispatcher, connector_t >(
        std::string( UtfArgsParser::host() ),
        UtfArgsParser::port(),
        UtfArgsParser::connections(),
        UtfArgsParser::dataSizeInMB()
        );
}

UTF_AUTO_TEST_CASE( IO_PerfStartServer )
{
    if( ! test::UtfArgsParser::isServer() )
    {
        return;
    }

    test::MachineGlobalTestLock lock;

    simplePerfStartServer< bl::tasks::TcpBlockServerDataChunkStorage >();
}

UTF_AUTO_TEST_CASE( IO_PerfStartMessageDispatcherServer )
{
    if( ! test::UtfArgsParser::isServer() )
    {
        return;
    }

    test::MachineGlobalTestLock lock;

    simplePerfStartServer< bl::tasks::TcpBlockServerMessageDispatcher >();
}

UTF_AUTO_TEST_CASE( IO_PerfStartClient )
{
    using namespace test;

    if( ! UtfArgsParser::isClient() )
    {
        return;
    }

    const auto dataBlocksPool = bl::data::datablocks_pool_type::createInstance();

    runClientPerfTest< connector_t >(
        dataBlocksPool,
        std::string( UtfArgsParser::host() ),
        UtfArgsParser::port(),
        UtfArgsParser::connections(),
        UtfArgsParser::dataSizeInMB()
        );
}

UTF_AUTO_TEST_CASE( IO_MaxConnectionsTest )
{
    using namespace test;
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace utest;

    if( ! UtfArgsParser::isClient() )
    {
        return;
    }

    tasks::scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
        {
            const auto dataBlocksPool = datablocks_pool_type::createInstance();

            std::vector< om::ObjPtr< connector_t > > connections;

            const std::size_t connectionsCount = UtfArgsParser::connections();

            {
                const auto t1 = bl::time::microsec_clock::universal_time();

                BL_CHK(
                    false,
                    connectionsCount < ( std::size_t ) INT_MAX,
                    BL_MSG()
                        << "Invalid value for connections "
                        << connectionsCount
                    );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Establishing "
                        << connectionsCount
                        << " connections...."
                    );

                for( std::size_t i = 0; i < connectionsCount; ++i )
                {
                    auto connector = connector_t::createInstance(
                        cpp::copy( UtfArgsParser::host() ),
                        UtfArgsParser::port()
                        );

                    const auto taskConnector = om::qi< tasks::Task >( connector.get() );
                    eq -> push_back( taskConnector );

                    connections.push_back( std::move( connector ) );
                }

                eq -> flush();

                const auto duration = bl::time::microsec_clock::universal_time() - t1;
                const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Established "
                        << connectionsCount
                        << " connections took "
                        << durationInSeconds
                        << " seconds; waiting for 5 seconds"
                    );

                os::sleep( time::seconds( 5 ) );
            }

            {
                const auto newBlock = BackendImplTestImpl::initDataBlock();
                newBlock -> setSize( 1024 );

                std::vector< om::ObjPtr< connection_t > > transfers;

                const auto chunkId = uuids::create();

                const std::size_t maxTransfers = 100;
                std::size_t actualTransfers = 0U;

                const auto t1 = bl::time::microsec_clock::universal_time();

                for( std::size_t i = 0; i < maxTransfers; ++i )
                {
                    auto transfer =
                        connection_t::createInstance(
                            connection_t::CommandId::NoCommand,
                            uuids::nil(),
                            dataBlocksPool
                            );

                    const std::size_t rndIndex = std::rand() % ( int ) connectionsCount;

                    const auto pos = connections.begin() + rndIndex;

                    auto& connector = *pos;

                    if( ! connector )
                    {
                        /*
                         * Used already
                         */

                        continue;
                    }

                    ++actualTransfers;

                    transfer -> attachStream( ( *pos ) -> detachStream() );
                    connector.reset();

                    const auto taskTransfer = om::qi< tasks::Task >( transfer.get() );

                    if( 0U == i )
                    {
                        transfer -> setChunkData( newBlock );
                    }
                    else
                    {
                        transfer -> setChunkData( nullptr );
                    }

                    transfer -> setCommandId(
                        0U == i ?
                            connection_t::CommandId::SendChunk :
                            (
                                ( maxTransfers - 1 ) == i ?
                                    connection_t::CommandId::RemoveChunk :
                                    connection_t::CommandId::ReceiveChunk
                            )
                        );

                    transfer -> setChunkId( chunkId );

                    eq -> push_back( taskTransfer );
                    eq -> waitForSuccess( taskTransfer );

                    if( ( maxTransfers - 1 ) != i )
                    {
                        BackendImplTestImpl::verifyData( transfer -> getChunkData() );
                    }

                    transfers.push_back( std::move( transfer ) );
                }

                const auto duration = bl::time::microsec_clock::universal_time() - t1;
                const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Performing "
                        << actualTransfers
                        << " transfers took "
                        << durationInSeconds
                        << " seconds; waiting for 5 seconds"
                    );

                os::sleep( time::seconds( 5 ) );
            }
        }
        );
}

UTF_AUTO_TEST_CASE( IO_BinaryProtocolInvariants )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::tasks::detail;

    CommandBlock command;

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Size of command block is "
            << sizeof( command )
        );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Size of command.data block is "
            << sizeof( command.data )
        );

    /*
     * Ensure that something breaks if the sizes change for some reason
     *
     * These structures and fields sizes should change very deliberately
     * and if some of these invariants have to change legitimately then
     * these values need to be adjusted
     */

    UTF_REQUIRE_EQUAL( sizeof( command ), 72U );
    UTF_REQUIRE_EQUAL( sizeof( command.data ), 28U );

    UTF_REQUIRE_EQUAL( sizeof( command.data.reserved.reserved1 ), sizeof( std::uint32_t ) );
    UTF_REQUIRE_EQUAL( sizeof( command.data.reserved.reserved2 ), sizeof( std::uint32_t ) );
    UTF_REQUIRE_EQUAL( sizeof( command.data.reserved.reserved3 ), sizeof( std::uint16_t ) );
    UTF_REQUIRE_EQUAL( sizeof( command.data.reserved.reserved4 ), sizeof( std::uint16_t ) );

    UTF_REQUIRE_EQUAL(
        sizeof( command.data.version.value ),
        sizeof( command.data.reserved.reserved1 )
        );

    UTF_REQUIRE_EQUAL(
        sizeof( command.data.blockInfo.flags ) + sizeof( command.data.blockInfo.unused ),
        sizeof( command.data.reserved.reserved1 )
        );

    UTF_REQUIRE_EQUAL(
        sizeof( command.data.blockInfo.protocolDataOffset ),
        sizeof( command.data.reserved.reserved2 )
        );

    UTF_REQUIRE_EQUAL(
        sizeof( command.data.blockInfo.blockType ),
        sizeof( command.data.reserved.reserved3 )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader, version ),
        offsetof( CommandBlock::DataHeader, reserved )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader, blockInfo ),
        offsetof( CommandBlock::DataHeader, reserved )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader, raw ),
        offsetof( CommandBlock::DataHeader, reserved )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader::tagVersion, value ),
        offsetof( CommandBlock::DataHeader::tagReserved, reserved1 )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader::tagBlockInfo, flags ),
        offsetof( CommandBlock::DataHeader::tagReserved, reserved1 )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader::tagBlockInfo, protocolDataOffset ),
        offsetof( CommandBlock::DataHeader::tagReserved, reserved2 )
        );

    UTF_REQUIRE_EQUAL(
        offsetof( CommandBlock::DataHeader::tagBlockInfo, blockType ),
        offsetof( CommandBlock::DataHeader::tagReserved, reserved3 )
        );

     UTF_REQUIRE(
        offsetof( CommandBlock::DataHeader::tagBlockInfo, flags ) <
        offsetof( CommandBlock::DataHeader::tagBlockInfo, unused )
        );

     UTF_REQUIRE(
        offsetof( CommandBlock::DataHeader::tagBlockInfo, unused ) <
        offsetof( CommandBlock::DataHeader::tagBlockInfo, protocolDataOffset )
        );

     UTF_REQUIRE(
        offsetof( CommandBlock::DataHeader::tagBlockInfo, protocolDataOffset ) <
        offsetof( CommandBlock::DataHeader::tagBlockInfo, blockType )
        );

    const auto printDataBytes = [ & ]() -> void
    {
        cpp::SafeOutputStringStream os;

        os << "{";
        for( std::size_t i = 0; i < sizeof( command.data.raw.bytes ); ++i )
        {
            if( i )
            {
                os << ", ";
            }

            os
                << "["
                << i
                << " : "
                << static_cast< int >( command.data.raw.bytes[ i ] )
                << "]";
        }
        os << "}";

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Data block bytes: "
                << os.str()
            );
    };

    const auto ensureAllZeros = [ & ]( SAA_in const std::size_t startPos ) -> void
    {
        for( std::size_t i = startPos; i < sizeof( command.data.raw.bytes ); ++i )
        {
            UTF_REQUIRE( command.data.raw.bytes[ i ] == 0U );
        }
    };

    /*
     * These tests below assume little endian architecture which is currently the case
     * for all architectures we are actually supporting
     *
     * In the future when/if we add big endian architectures these tests must be
     * adjusted
     *
     * Note that the blob server code does support both big and little endian as it
     * always converts number on the wire to network byte order format (which is same
     * as big endian actually)
     */

    {
        /*
         * Test the command.data.version fields
         */

        command = CommandBlock();
        command.data.version.value = CommandBlock::BLOB_TRANSFER_PROTOCOL_SERVER_VERSION;

        printDataBytes();

        UTF_REQUIRE( command.data.raw.bytes[ 0 ] == CommandBlock::BLOB_TRANSFER_PROTOCOL_SERVER_VERSION );

        ensureAllZeros( 1U );
    }

    {
        /*
         * Test the command.data.blockInfo fields
         */

        command = CommandBlock();
        command.data.blockInfo.flags = CommandBlock::IgnoreIfNotFound;
        command.data.blockInfo.protocolDataOffset = 0xFFFFFFFF;
        command.data.blockInfo.blockType = BlockTransferDefs::BlockType::TransferOnly;

        printDataBytes();

        const auto blockTypeAsUshort =
            static_cast< std::uint16_t >( BlockTransferDefs::BlockType::TransferOnly );

        UTF_REQUIRE( command.data.raw.bytes[ 0 ] == CommandBlock::IgnoreIfNotFound );
        UTF_REQUIRE( command.data.raw.bytes[ 1 ] == 0U );
        UTF_REQUIRE( command.data.raw.bytes[ 2 ] == 0U );
        UTF_REQUIRE( command.data.raw.bytes[ 3 ] == 0U );

        UTF_REQUIRE( command.data.raw.bytes[ 4 ] == 0xFF );
        UTF_REQUIRE( command.data.raw.bytes[ 5 ] == 0xFF );
        UTF_REQUIRE( command.data.raw.bytes[ 6 ] == 0xFF );
        UTF_REQUIRE( command.data.raw.bytes[ 7 ] == 0xFF );

        UTF_REQUIRE( command.data.raw.bytes[ 8 ] == blockTypeAsUshort );

        ensureAllZeros( 9U );
    }
}

UTF_AUTO_TEST_CASE( IO_MessagingClientBlockDispatchLocalTests )
{
    using namespace bl;
    using namespace bl::messaging;

    const auto targetPeerIdExpected = uuids::create();

    const std::size_t sizeExpected = 42;
    const std::size_t offset1Expected = sizeExpected / 3;

    const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

    std::atomic< std::size_t > callsCount( 0U );

    const auto receiver = om::lockDisposable(
        MessagingClientBlockDispatchFromCallback::createInstance(
            [ & ](
                SAA_in              const bl::uuid_t&                               targetPeerId,
                SAA_in              const om::ObjPtr< data::DataBlock >&            dataBlock
                ) -> void
            {
                UTF_REQUIRE_EQUAL( targetPeerIdExpected, targetPeerId );

                UTF_REQUIRE( dataBlock );
                UTF_REQUIRE_EQUAL( sizeExpected, dataBlock -> size() );
                UTF_REQUIRE_EQUAL( offset1Expected, dataBlock -> offset1() );

                const char* psz = dataBlock -> begin();
                UTF_REQUIRE_EQUAL( *psz + *( psz + 1 ), *( psz + 2 ) );

                ++callsCount;
            }
            )
        );

    {
        const auto dispatcher = om::lockDisposable(
            MessagingClientBlockDispatchLocal::createInstance(
                om::qi< MessagingClientBlockDispatch >( receiver ),
                om::copy( dataBlocksPool )
                )
            );

        const std::size_t noOfBlocks = 1024U;

        for( std::size_t i = 0U; i < noOfBlocks; ++i )
        {
            const auto dataBlock = data::DataBlock::createInstance( 512U /* capacity */ );

            dataBlock -> setSize( sizeExpected );
            dataBlock -> setOffset1( offset1Expected );

            char* psz = dataBlock -> begin();

            *psz = static_cast< char >( random::getUniformRandomUnsignedValue< int >( 32 ) );
            *( psz + 1 ) = static_cast< char >( random::getUniformRandomUnsignedValue< int >( 32 ) );
            *( psz + 2 ) = *psz + *( psz + 1 );

            dispatcher -> pushBlock( targetPeerIdExpected, dataBlock );
        }

        dispatcher -> flush();

        UTF_REQUIRE_EQUAL( callsCount.load(), noOfBlocks );
    }
}

UTF_AUTO_TEST_CASE( IO_MessagingClientTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto callbackTests = []() -> void
    {
        const auto target = om::lockDisposable(
            MessagingClientBlockDispatchFromCallback::createInstance< MessagingClientBlockDispatch >(
                [](
                    SAA_in              const bl::uuid_t&                               targetPeerId,
                    SAA_in              const om::ObjPtr< data::DataBlock >&            dataBlock
                    ) -> void
                {
                    BL_UNUSED( targetPeerId );
                    BL_UNUSED( dataBlock );

                    UTF_FAIL( "This should not be called from this test" );
                }
                )
            );

        const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

        const auto eq = om::lockDisposable(
            ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepAll )
            );

        const auto backend = om::lockDisposable(
            MessagingClientFactorySsl::createClientBackendProcessingFromBlockDispatch( om::copy( target ) )
            );

        const auto asyncWrapper = om::lockDisposable(
            MessagingClientFactorySsl::createAsyncWrapperFromBackend(
                om::copy( backend ),
                0U              /* threadsCount */,
                0U              /* maxConcurrentTasks */,
                om::copy( dataBlocksPool )
                )
            );

        const auto peerId = uuids::create();

        {
            const auto client = om::lockDisposable(
                MessagingClientFactorySsl::createWithSmartDefaults(
                    om::copy( eq ),
                    peerId,
                    om::copy( backend ),
                    om::copy( asyncWrapper ),
                    test::UtfArgsParser::host(),
                    test::UtfArgsParser::port()             /* inboundPort */
                    )
                );

            os::sleep( time::seconds( 2L ) );
        }

        {
            /*
             * Create some number of clients backed by the same async wrapper
             *
             * Note that these don't own the backend and the queue, so when they
             * get disposed they will not actually dispose the backend
             */

            std::vector< om::ObjPtrDisposable< MessagingClientBlockDispatch > > clients;

            for( std::size_t i = 0; i < 120; ++i )
            {
                clients.emplace_back(
                    om::lockDisposable(
                        MessagingClientFactorySsl::createWithSmartDefaults(
                            om::copy( eq ),
                            peerId,
                            om::copy( backend ),
                            om::copy( asyncWrapper ),
                            test::UtfArgsParser::host(),
                            test::UtfArgsParser::port()             /* inboundPort */
                            )
                        )
                    );
            }

            os::sleep( time::seconds( 2L ) );
        }

        {
            auto connections1 = MessagingClientFactorySsl::createEstablishedConnections(
                "localhost"                                         /* host */,
                test::UtfArgsParser::port()                         /* inboundPort */,
                test::UtfArgsParser::port() + 1                     /* outboundPort */
                );

            auto connections2 = MessagingClientFactorySsl::createEstablishedConnections(
                "localhost"                                         /* host */,
                test::UtfArgsParser::port()                         /* inboundPort */,
                test::UtfArgsParser::port() + 1                     /* outboundPort */
                );

            const auto client1 = om::lockDisposable(
                MessagingClientFactorySsl::createWithSmartDefaults(
                    nullptr                                 /* eq */,
                    peerId,
                    om::copy( backend ),
                    om::copy( asyncWrapper ),
                    test::UtfArgsParser::host(),
                    test::UtfArgsParser::port()             /* inboundPort */,
                    test::UtfArgsParser::port() + 1         /* outboundPort */,
                    std::move( connections1.first )         /* inboundConnection */,
                    std::move( connections1.second )        /* outboundConnection */
                    )
                );

            const auto client2 = om::lockDisposable(
                MessagingClientFactorySsl::createWithSmartDefaults(
                    peerId,
                    om::copy( target ),
                    test::UtfArgsParser::host(),
                    test::UtfArgsParser::port()             /* inboundPort */,
                    test::UtfArgsParser::port() + 1         /* outboundPort */,
                    std::move( connections2.first )         /* inboundConnection */,
                    std::move( connections2.second )        /* outboundConnection */
                    )
                );

            os::sleep( time::seconds( 2L ) );
        }
    };

    test::MachineGlobalTestLock lock;

    auto blockDispatch = om::lockDisposable(
        MessagingClientBlockDispatchFromCallback::createInstance< MessagingClientBlockDispatch >(
            [ & ](
                SAA_in              const bl::uuid_t&                               targetPeerId,
                SAA_in              const om::ObjPtr< data::DataBlock >&            dataBlock
                ) -> void
            {
                BL_UNUSED( targetPeerId );
                BL_UNUSED( dataBlock );
            }
            )
        );

    const auto processingBackend = bl::om::lockDisposable(
        MessagingClientBackendProcessing::createInstance< bl::messaging::BackendProcessing >(
            std::move( blockDispatch )
            )
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingBackendProcessingHelpers )
{
    using namespace bl;
    using namespace bl::messaging;
    using namespace bl::tasks;

    const std::string messagePrefix = "My server operation";

    const eh::errc::errc_t errorCondition = eh::errc::address_in_use;

    const eh::error_code errorCode = eh::errc::make_error_code( errorCondition );

    const auto throwNonDecoratedServerErrorException = []() -> void
    {
        BL_THROW(
            ServerErrorException(),
            BL_MSG()
                << "Test server raw exception"
            );
    };

    const auto throwDecoratedArgumentException = [ &errorCode ]() -> void
    {
        BL_THROW(
            ArgumentException()
                << eh::errinfo_errno( errorCode.value() )
                << eh::errinfo_error_code( errorCode )
                << eh::errinfo_error_code_message( errorCode.message() )
                << eh::errinfo_category_name( errorCode.category().name() )
                << eh::errinfo_is_expected( true ),
            BL_MSG()
                << "Test server wrapped exception"
            );
    };

    const auto throwPartiallyDecoratedArgumentException = [ &errorCode ]() -> void
    {
        BL_THROW(
            ArgumentException()
                << eh::errinfo_errno( errorCode.value() )
                << eh::errinfo_is_expected( true ),
            BL_MSG()
                << "Test server wrapped exception"
            );
    };

    const auto testNoWrappingCase = [ & ]( SAA_in const cpp::void_callback_t& callback ) -> void
    {
        /*
         * Verify that ServerErrorException exceptions are propagated as is and no
         * wrapping occurs
         */

        try
        {
            callback();

            UTF_FAIL( "BackendProcessingBase::chkToWrapInServerErrorAndThrow must throw" );
        }
        catch( ServerErrorException& exception )
        {
            const std::string msg = exception.what();
            UTF_REQUIRE_EQUAL( msg, std::string( "Test server raw exception" ) );

            UTF_REQUIRE( ! eh::get_error_info< eh::errinfo_errno >( exception ) );
            UTF_REQUIRE( ! eh::get_error_info< eh::errinfo_error_code >( exception ) );
            UTF_REQUIRE( ! eh::get_error_info< eh::errinfo_error_code_message >( exception ) );
            UTF_REQUIRE( ! eh::get_error_info< eh::errinfo_category_name >( exception ) );

            UTF_REQUIRE( ! eh::get_error_info< eh::errinfo_is_expected >( exception ) );
            UTF_REQUIRE( ! eh::get_error_info< eh::errinfo_nested_exception_ptr >( exception ) );
        }
    };

    const auto testWrappingCase = [ & ]( SAA_in const cpp::void_callback_t& callback ) -> void
    {
        /*
         * Verify that non-ServerErrorException exceptions are wrapped and the relevant
         * properties are copied accordingly
         */

        try
        {
            callback();

            UTF_FAIL( "BackendProcessingBase::chkToWrapInServerErrorAndThrow must throw" );
        }
        catch( ServerErrorException& exception )
        {
            const std::string msg = exception.what();
            UTF_REQUIRE_EQUAL( msg, messagePrefix + " has failed" );

            UTF_REQUIRE( eh::get_error_info< eh::errinfo_errno >( exception ) );
            UTF_REQUIRE( eh::get_error_info< eh::errinfo_error_code >( exception ) );
            UTF_REQUIRE( eh::get_error_info< eh::errinfo_error_code_message >( exception ) );
            UTF_REQUIRE( eh::get_error_info< eh::errinfo_category_name >( exception ) );

            UTF_REQUIRE_EQUAL(
                errorCode.value(),
                *eh::get_error_info< eh::errinfo_errno >( exception )
                );

            UTF_REQUIRE_EQUAL(
                errorCode,
                *eh::get_error_info< eh::errinfo_error_code >( exception )
                );

            UTF_REQUIRE_EQUAL(
                errorCode.message(),
                *eh::get_error_info< eh::errinfo_error_code_message >( exception )
                );

            UTF_REQUIRE_EQUAL(
                errorCode.category().name(),
                *eh::get_error_info< eh::errinfo_category_name >( exception )
                );

            const bool* isExpected = eh::get_error_info< eh::errinfo_is_expected >( exception );
            UTF_REQUIRE( isExpected && *isExpected );

            const auto* eeptr = eh::get_error_info< eh::errinfo_nested_exception_ptr >( exception );
            UTF_REQUIRE( eeptr );

            UTF_REQUIRE_THROW_MESSAGE(
                cpp::safeRethrowException( *eeptr ),
                ArgumentException,
                "Test server wrapped exception"
                );
        }
    };

    /*
     * Tests for BackendProcessingBase::chkToWrapInServerErrorAndThrow
     */

    testNoWrappingCase(
        cpp::bind(
            &BackendProcessingBase::chkToWrapInServerErrorAndThrow,
            throwNonDecoratedServerErrorException,
            messagePrefix,
            eh::errc::success
            )
        );

    testWrappingCase(
        cpp::bind(
            &BackendProcessingBase::chkToWrapInServerErrorAndThrow,
            throwDecoratedArgumentException,
            messagePrefix,
            eh::errc::success
            )
        );

    testWrappingCase(
        cpp::bind(
            &BackendProcessingBase::chkToWrapInServerErrorAndThrow,
            throwPartiallyDecoratedArgumentException,
            messagePrefix,
            errorCondition
            )
        );

    /*
     * Tests for BackendProcessingBase::chkToRemapToServerError
     */

    const auto convertToEptr = []( SAA_in const cpp::void_callback_t& callback ) -> std::exception_ptr
    {
        try
        {
            callback();

            UTF_FAIL( "callback must throw" );
        }
        catch( std::exception& )
        {
            return std::current_exception();
        }

        UTF_FAIL( "callback must throw" );

        return std::exception_ptr();
    };

    testNoWrappingCase(
        cpp::bind< void >(
            cpp::safeRethrowException,
            BackendProcessingBase::chkToRemapToServerError(
                convertToEptr( throwNonDecoratedServerErrorException ),
                messagePrefix,
                eh::errc::success
                )
            )
        );

    testWrappingCase(
        cpp::bind< void >(
            cpp::safeRethrowException,
            BackendProcessingBase::chkToRemapToServerError(
                convertToEptr( throwDecoratedArgumentException ),
                messagePrefix,
                eh::errc::success
                )
            )
        );

    testWrappingCase(
        cpp::bind< void >(
            cpp::safeRethrowException,
            BackendProcessingBase::chkToRemapToServerError(
                convertToEptr( throwPartiallyDecoratedArgumentException ),
                messagePrefix,
                errorCondition
                )
            )
        );

    /*
     * Test TcpSocketCommonBase::isExpectedSocketException logic
     */

    const auto ecOperationAborted = asio::error::make_error_code( asio::error::operation_aborted );

    UTF_REQUIRE(
        TcpSocketCommonBase::isExpectedSocketException(
            true /* isCancelExpected */,
            &ecOperationAborted
            )
        );

    UTF_REQUIRE(
        ! TcpSocketCommonBase::isExpectedSocketException(
            false /* isCancelExpected */,
            &ecOperationAborted
            )
        );

    const auto testExpectedSocketException = [ &errorCode ]( SAA_in const bool isCancelExpected ) -> void
    {
        UTF_REQUIRE(
            ! TcpSocketCommonBase::isExpectedSocketException(
                isCancelExpected,
                nullptr /* ec */
                )
            );

        UTF_REQUIRE(
            ! TcpSocketCommonBase::isExpectedSocketException(
                isCancelExpected,
                &errorCode
                )
            );

        const auto ecEof = asio::error::make_error_code( asio::error::eof );

        UTF_REQUIRE(
            TcpSocketCommonBase::isExpectedSocketException(
                isCancelExpected,
                &ecEof
                )
            );

        /*
         * On Windows the error codes for connection reset and aborted are different
         * and we need to handle these separately
         *
         * The values for WSAECONNRESET (10054), WSAECONNABORTED (10053) and
         * WSAETIMEDOUT (10060), etc are from here:
         * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%28v=vs.85%29.aspx
         */

        const auto ecNotConnected =
            os::onUNIX() ?  eh::errc::not_connected : 10057 /* WSAENOTCONN */;
        const auto ecConnectionAborted =
            os::onUNIX() ?  eh::errc::connection_aborted : 10053 /* WSAECONNABORTED */;
        const auto ecConnectionReset =
            os::onUNIX() ?  eh::errc::connection_reset : 10054 /* WSAECONNRESET */;
        const auto ecConnectionInProgress =
            os::onUNIX() ?  eh::errc::connection_already_in_progress : 10037 /* WSAEALREADY */;
        const auto ecConnectionRefused =
            os::onUNIX() ?  eh::errc::connection_refused : 10061 /* WSAECONNREFUSED */;
        const auto ecBrokenPipe =
            os::onUNIX() ?  eh::errc::broken_pipe : 10053 /* WSAECONNABORTED */;
        const auto ecTimedOut =
            os::onUNIX() ?  eh::errc::timed_out : 10060 /* WSAETIMEDOUT */;
        const auto ecHostUnreachable =
            os::onUNIX() ?  eh::errc::host_unreachable : 10065 /* WSAEHOSTUNREACH */;

        {
            const auto ec = eh::error_code( ecNotConnected, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:107" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:57" ) : std::string( "system:10057" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecConnectionAborted, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:103" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:53" ) : std::string( "system:10053" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecConnectionReset, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:104" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:54" ) : std::string( "system:10054" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecConnectionInProgress, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:114" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:37" ) : std::string( "system:10037" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecConnectionRefused, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:111" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:61" ) : std::string( "system:10061" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecBrokenPipe, eh::system_category() );

            UTF_REQUIRE_EQUAL(
                eh::errorCodeToString( ec ),
                os::onUNIX() ? std::string( "system:32" ) : std::string( "system:10053" )
                );

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecTimedOut, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:110" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:60" ) : std::string( "system:10060" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }

        {
            const auto ec = eh::error_code( ecHostUnreachable, eh::system_category() );

            if( os::onLinux() )
            {
                UTF_REQUIRE_EQUAL( eh::errorCodeToString( ec ), std::string( "system:113" ) );                                    }
            else
            {
                UTF_REQUIRE_EQUAL(
                    eh::errorCodeToString( ec ),
                    os::onUNIX() ? std::string( "system:65" ) : std::string( "system:10065" )
                    );
            }

            UTF_REQUIRE( TcpSocketCommonBase::isExpectedSocketException( isCancelExpected, &ec ) );
        }
    };

    testExpectedSocketException( true /* isCancelExpected */ );
    testExpectedSocketException( false /* isCancelExpected */ );
}

