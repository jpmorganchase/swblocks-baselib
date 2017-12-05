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

#include <utests/baselib/TestTaskUtils.h>
#include <utests/baselib/MachineGlobalTestLock.h>

#include <baselib/core/BaseIncludes.h>

namespace
{
    typedef bl::tasks::TcpBlockTransferClientConnectionImpl< bl::tasks::TcpSocketAsyncBase >    connection_t;
    typedef bl::tasks::TcpConnectionEstablisherConnectorImpl< bl::tasks::TcpSocketAsyncBase >   connector_t;

    /*
     * Functions below return values of the type int to make it simple
     * ( without passing references ) to ensure that the function was
     * actually called (asyncDataChunkStorage will return the return
     * value of callback or errorCallback depending on which was called).
     * Or it will return 0 if none of the callbacks was called
     */

    typedef bl::cpp::function<
            int (
                SAA_in const bl::om::ObjPtr< connection_t >& connection
                )
        >
        callback_t;

    typedef bl::cpp::function<
            int (
                std::exception& e
                )
        >
        error_callback_t;

    using bl::data::DataChunkStorage;
    using bl::AsyncDataChunkStorage;

    int asyncDataChunkStorageTest(
        SAA_in      const bl::om::ObjPtr< AsyncDataChunkStorage >&  asyncStorage,
        SAA_in      const callback_t&                               callback,
        SAA_in      const error_callback_t&                         errorCallback
        )
    {
        using bl::tasks::SimpleTaskControlTokenImpl;
        using bl::tasks::TaskControlTokenRW;

        const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

        test::MachineGlobalTestLock lock;

        using bl::data::datablocks_pool_type;

        std::atomic< int > returnValue( 0 );

        utest::TestTaskUtils::createAcceptorAndExecute< bl::tasks::TcpBlockServerDataChunkStorage >(
            controlToken,
            [ &callback, &errorCallback, &returnValue ]() -> void
            {
                const auto eq = bl::om::lockDisposable(
                    bl::tasks::ExecutionQueueImpl::createInstance(
                        bl::tasks::ExecutionQueue::OptionKeepAll
                        )
                    );

                const auto connector = connector_t::createInstance(
                    "localhost",
                    28100
                    );

                const auto taskConnector = bl::om::qi< bl::tasks::Task >( connector.get() );
                eq -> push_back( taskConnector );
                eq -> waitForSuccess( taskConnector );


                const auto stats = connection_t::createInstance(
                    connection_t::CommandId::ReceiveChunk,
                    bl::uuids::nil() /* chunkId */,
                    datablocks_pool_type::createInstance( "[client pool]" ),
                    bl::tasks::BlockTransferDefs::BlockType::ServerState
                    );

                stats -> setCommandInfo(
                    connection_t::CommandId::ReceiveChunk,
                    bl::uuids::nil() /* chunkId */,
                    nullptr /* chunkData */,
                    bl::tasks::BlockTransferDefs::BlockType::ServerState
                    );

                stats -> attachStream( connector -> detachStream() );
                const auto taskStats = bl::om::qi< bl::tasks::Task >( stats );
                eq -> push_back( taskStats );

                try
                {
                    eq -> waitForSuccess( taskStats );
                    returnValue = callback( stats );
                }
                catch( std::exception& e)
                {
                    returnValue = errorCallback( e );
                }

            },
            datablocks_pool_type::createInstance( "[server pool]" ),
            asyncStorage,
            std::string( test::UtfArgsParser::host() ),
            test::UtfArgsParser::port()
            );

        return returnValue;
    }
}

UTF_AUTO_TEST_CASE( Test_AsyncDataChunkStorageStats )
{
    const auto storage = bl::om::lockDisposable(
        utest::BackendImplTestImpl::createInstance< bl::data::DataChunkStorage >()
        );

    const auto asyncStorage =
        bl::AsyncDataChunkStorage::createInstance(
            storage /* writeStorage */,
            storage /* readStorage */,
            test::UtfArgsParser::threadsCount(),
            nullptr /* controlToken */,
            0U /* maxConcurrentTasks */,
            nullptr /* dataBlocksPool */,
            bl::AsyncDataChunkStorage::datablock_callback_t(),
            []( SAA_in const bl::om::ObjPtr< bl::data::DataBlock >& data ) -> void
            {
                const std::string output = "Test output";

                const auto dataLength = output.size() + 1;

                BL_CHK(
                    false,
                    dataLength <= data -> capacity(),
                    BL_MSG()
                        << "Block size not sufficient"
                    );

                /*
                 * Chunk data will include the zero terminator character
                 */

                ::memcpy( data -> pv(), output.c_str(), dataLength );
                data -> setSize( dataLength );
            }
            );

    const int returnValue = asyncDataChunkStorageTest(
        asyncStorage,
        [] ( SAA_in const bl::om::ObjPtr< connection_t >& connection ) -> int
        {
            UTF_CHECK_EQUAL(
                "Test output",
                std::string(
                    connection -> getChunkData() -> begin(),
                    connection -> getChunkData() -> end() - 1
                    )
                );

            return 1;
        },
        [] ( std::exception& e )
        {
            BL_UNUSED( e );

            return 2;
        }
    );

    UTF_CHECK_EQUAL( 1, returnValue ); /* Make sure that callback was called */
}

UTF_AUTO_TEST_CASE( Test_AsyncDataChunkStorageStatsException )
{
    const auto storage = bl::om::lockDisposable(
        utest::BackendImplTestImpl::createInstance()
        );

    const auto asyncStorage =
        bl::AsyncDataChunkStorage::createInstance(
            bl::om::copy< bl::data::DataChunkStorage >( storage ) /* writeStorage */,
            bl::om::copy< bl::data::DataChunkStorage >( storage ) /* readStorage */,
            test::UtfArgsParser::threadsCount(),
            nullptr /* controlToken */,
            0U /* maxConcurrentTasks */,
            nullptr /* dataBlocksPool */,
            bl::AsyncDataChunkStorage::datablock_callback_t(),
            []( SAA_in const bl::om::ObjPtr< bl::data::DataBlock >& data )
            {
                BL_UNUSED( data );

                BL_THROW_EC(
                    bl::eh::errc::make_error_code( bl::eh::errc::no_message ),
                    "Simulated exception"
                    );
            }
            );

    const int returnValue = asyncDataChunkStorageTest(
        asyncStorage,
        [] ( SAA_in const bl::om::ObjPtr< connection_t >& connection ) -> int
        {
            BL_UNUSED( connection );

            return 1;
        },
        [] ( std::exception& e ) -> int
        {
            UTF_CHECK_EQUAL( bl::eh::errc::no_message, *bl::eh::get_error_info< bl::eh::errinfo_errno >( e ) );

            return 2;
        }
    );

    UTF_CHECK_EQUAL( 2, returnValue ); /* Make sure that errorCallback was called */
}
