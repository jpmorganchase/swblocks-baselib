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

#include <baselib/messaging/DataChunkStorageFilesystem.h>

#include <utests/baselib/UtfBaseLibCommon.h>
#include <utests/baselib/TestTaskUtils.h>

/************************************************************************
 * DataChunkStorageFilesystem tests
 */

namespace utest
{
    template
    <
        typename E = void
    >
    class TestDataChunkStorageFilesystemLocalHelperT
    {
        BL_DECLARE_STATIC( TestDataChunkStorageFilesystemLocalHelperT )

    public:

        template
        <
            typename STORAGEIMPL
        >
        static void executeTests( SAA_in std::string&& testsName )
        {
            using namespace bl;
            using namespace bl::data;
            using namespace utest;

            const std::string testData( "This is test data" );

            const auto dataBlock = DataBlock::createInstance( 128 );

            std::memcpy( dataBlock -> begin(), testData.c_str(), testData.size() );

            dataBlock -> setSize( testData.size() );
            dataBlock -> setOffset1( 0U );

            const auto sessionId = uuids::create();

            const auto testNotFoundException = [](
                SAA_in          const cpp::void_callback_t&                 callback,
                SAA_in          const bl::uuid_t&                           chunkId,
                SAA_in          const bool                                  dumpException
                ) -> void
            {
                const auto exceptionPredicate = [ & ]( SAA_in const ServerErrorException& exception ) -> bool
                {
                    if( dumpException )
                    {
                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << eh::diagnostic_information( exception )
                            );
                    }

                    const std::string message = exception.what();

                    const auto expectedMessage = resolveMessage(
                        BL_MSG()
                            << "Chunk with id "
                            << chunkId
                            << " does not exist"
                        );

                    if( message != expectedMessage )
                    {
                        return false;
                    }

                    const auto* exceptionChunkId =
                        eh::get_error_info< eh::errinfo_error_uuid >( exception );

                    if( ! exceptionChunkId || chunkId != *exceptionChunkId )
                    {
                        return false;
                    }

                    const auto* errorCode =
                        eh::get_error_info< eh::errinfo_error_code >( exception );

                    if(
                        ! errorCode ||
                        eh::errc::make_error_code( eh::errc::no_such_file_or_directory ) != *errorCode
                        )
                    {
                        return false;
                    }

                    return true;
                };

                UTF_REQUIRE_EXCEPTION(
                    callback(),
                    ServerErrorException,
                    exceptionPredicate
                    );
            };

            const auto runBasicStorageTests = [ & ]( SAA_in const om::ObjPtr< DataChunkStorage >& storage ) -> void
            {
                storage -> flushPeerSessions( uuids::create() /* peerId */ );

                /*
                 * Test exceptions with non-existing chunks
                 */

                const auto testChunkNotFound = [ & ](
                    SAA_in          const bl::uuid_t&                           chunkId,
                    SAA_in          const bool                                  dumpException
                    ) -> void
                {
                    testNotFoundException(
                        [ & ]() -> void
                        {
                            const auto newBlock = DataBlock::createInstance( 128 );

                            storage -> load( sessionId, chunkId, newBlock );
                        },
                        chunkId,
                        dumpException
                        );

                    testNotFoundException(
                        [ & ]() -> void
                        {
                            storage -> remove( sessionId, chunkId );
                        },
                        chunkId,
                        dumpException
                        );
                };

                testChunkNotFound( uuids::create() /* chunkId */, true /* dumpException */ );

                /*
                 * Test save, load and removal of a single chunk... and verify that it
                 * doesn't exist after removal
                 */

                {
                    const auto chunkId = uuids::create();

                    storage -> save( sessionId, chunkId, dataBlock );

                    const auto newBlock = DataBlock::createInstance( 128 );

                    storage -> load( sessionId, chunkId, newBlock );

                    UTF_REQUIRE( BackendImplTestImpl::areBlocksEqual( dataBlock, newBlock ) );

                    storage -> remove( sessionId, chunkId );

                    testChunkNotFound( uuids::create() /* chunkId */, false /* dumpException */ );

                    /*
                     * Now save and load again the same chunk and verify that it works
                     */

                    storage -> save( sessionId, chunkId, dataBlock );

                    newBlock -> setSize( 0U );

                    storage -> load( sessionId, chunkId, newBlock );

                    UTF_REQUIRE( BackendImplTestImpl::areBlocksEqual( dataBlock, newBlock ) );

                    storage -> remove( sessionId, chunkId );

                    testChunkNotFound( uuids::create() /* chunkId */, false /* dumpException */ );
                }

                /*
                 * Test save, load and removal of an empty chunk... and verify that it
                 * doesn't exist after removal
                 */

                {
                    const auto chunkId = uuids::create();

                    const auto emptyBlock = DataBlock::createInstance( 128 );
                    emptyBlock -> setSize( 0U );

                    storage -> save( sessionId, chunkId, emptyBlock );

                    const auto newBlock = DataBlock::createInstance( 128 );
                    newBlock -> setSize( newBlock -> capacity() );

                    storage -> load( sessionId, chunkId, newBlock );

                    UTF_REQUIRE( 0U == newBlock -> size() );

                    storage -> remove( sessionId, chunkId );

                    testChunkNotFound( uuids::create() /* chunkId */, false /* dumpException */ );
                }
            };

            const std::size_t noOfChunks = 100;

            const auto populateStorage = [ & ](
                SAA_in              const om::ObjPtr< DataChunkStorage >&                       storage,
                SAA_inout           std::vector< bl::uuid_t >&                                  chunks,
                SAA_inout           std::unordered_map< bl::uuid_t, bool /* deleted */ >&       chunksDeletedState
                ) -> void
            {
                for( std::size_t i = 0U; i < noOfChunks; ++i )
                {
                    const auto chunkId = uuids::create();

                    storage -> save( sessionId, chunkId, dataBlock );

                    chunks.push_back( chunkId );
                    chunksDeletedState.emplace( chunkId, false /* deleted */ );
                }
            };

            const auto runSimpleLoadAndSaveTests = [ & ](
                SAA_in              const om::ObjPtr< DataChunkStorage >&                       storage,
                SAA_inout           std::vector< bl::uuid_t >&                                  chunks
                ) -> void
            {
                /*
                 * Load all chunks sequentially and then load them at random
                 */

                for( const auto& chunkId : chunks )
                {
                    const auto newBlock = DataBlock::createInstance( 128 );

                    storage -> load( sessionId, chunkId, newBlock );

                    UTF_REQUIRE( BackendImplTestImpl::areBlocksEqual( dataBlock, newBlock ) );
                }

                for( std::size_t i = 0U; i < noOfChunks; ++i )
                {
                    const auto pos = random::getUniformRandomUnsignedValue< std::size_t >( chunks.size() - 1 );

                    const auto& chunkId = chunks[ pos ];

                    const auto newBlock = DataBlock::createInstance( 128 );

                    storage -> load( sessionId, chunkId, newBlock );

                    UTF_REQUIRE( BackendImplTestImpl::areBlocksEqual( dataBlock, newBlock ) );
                }
            };

            const auto runRandomDeleteAndRecreateTests = [ & ](
                SAA_in              const om::ObjPtr< DataChunkStorage >&                       storage,
                SAA_inout           std::vector< bl::uuid_t >&                                  chunks,
                SAA_inout           std::unordered_map< bl::uuid_t, bool /* deleted */ >&       chunksDeletedState
                ) -> void
            {
                /*
                 * Delete some chunks and then verify that the deleted chunks are indeed deleted
                 */

                for( std::size_t i = 0U; i < noOfChunks; ++i )
                {
                    const auto pos = random::getUniformRandomUnsignedValue< std::size_t >( chunks.size() - 1 );

                    const auto& chunkId = chunks[ pos ];

                    if( ! chunksDeletedState[ chunkId ] )
                    {
                        storage -> remove( sessionId, chunkId );
                    }

                    testNotFoundException(
                        [ & ]() -> void
                        {
                            storage -> remove( sessionId, chunkId );
                        },
                        chunkId,
                        false /* dumpException */
                        );

                    /*
                     * Based on a coin toss either leave the chunk deleted or restore it
                     */

                    const bool coinToss =
                        0U == ( random::getUniformRandomUnsignedValue( 1024U /* maxValue */ ) % 2U );

                    if( coinToss )
                    {
                        storage -> save( sessionId, chunkId, dataBlock );
                    }

                    chunksDeletedState[ chunkId ] = ! coinToss;
                }

                /*
                 * Now simply verify all chunks that are not deleted and delete them
                 */

                std::size_t noOfDeleted = 0U;

                for( const auto& chunkId : chunks )
                {
                    if( ! chunksDeletedState[ chunkId ] )
                    {
                        const auto newBlock = DataBlock::createInstance( 128 );

                        storage -> load( sessionId, chunkId, newBlock );

                        UTF_REQUIRE( BackendImplTestImpl::areBlocksEqual( dataBlock, newBlock ) );

                        storage -> remove( sessionId, chunkId );

                        chunksDeletedState[ chunkId ] = true;
                    }
                    else
                    {
                        ++noOfDeleted;
                    }

                    testNotFoundException(
                        [ & ]() -> void
                        {
                            storage -> remove( sessionId, chunkId );
                        },
                        chunkId,
                        false /* dumpException */
                        );
                }

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "Number of deleted chunks is "
                        << noOfDeleted
                        << " and the number of not deleted is "
                        << chunks.size() - noOfDeleted
                    );
            };

            const auto runAllStorageTests = [ & ]( SAA_in const om::ObjPtr< DataChunkStorage >& storage ) -> void
            {
                runBasicStorageTests( storage );

                /*
                 * Test save, load and removal of many chunks...
                 */

                std::vector< bl::uuid_t > chunks;
                std::unordered_map< bl::uuid_t, bool /* deleted */ > chunksDeletedState;

                populateStorage( storage, chunks, chunksDeletedState );
                runSimpleLoadAndSaveTests( storage, chunks );
                runRandomDeleteAndRecreateTests( storage, chunks, chunksDeletedState );
            };

            {
                utils::ExecutionTimer timer( BL_PARAM_FWD( testsName ) );

                {
                    const auto storage = om::lockDisposable(
                        STORAGEIMPL::template createInstance< DataChunkStorage >()
                        );

                    runAllStorageTests( storage );
                }

                {
                    /*
                     * Now run the separate phases of all tests on different storage objects,
                     * but the same real file system storage
                     */

                    fs::TmpDir tempDir;
                    std::vector< bl::uuid_t > chunks;
                    std::unordered_map< bl::uuid_t, bool /* deleted */ > chunksDeletedState;

                    {
                        const auto storage = om::lockDisposable(
                            STORAGEIMPL::template createInstance< DataChunkStorage >(
                                cpp::copy( tempDir.path() ) /* rootPath */
                                )
                            );

                        populateStorage( storage, chunks, chunksDeletedState );
                    }

                    {
                        const auto storage = om::lockDisposable(
                            STORAGEIMPL::template createInstance< DataChunkStorage >(
                                cpp::copy( tempDir.path() ) /* rootPath */
                                )
                            );

                        runSimpleLoadAndSaveTests( storage, chunks );
                    }

                    {
                        const auto storage = om::lockDisposable(
                            STORAGEIMPL::template createInstance< DataChunkStorage >(
                                cpp::copy( tempDir.path() ) /* rootPath */
                                )
                            );

                        runRandomDeleteAndRecreateTests( storage, chunks, chunksDeletedState );
                    }
                }
            }
        }
    };

    typedef TestDataChunkStorageFilesystemLocalHelperT<> TestDataChunkStorageFilesystemLocalHelper;

} // utest

UTF_AUTO_TEST_CASE( TestDataChunkStorageFilesystemMultiFiles )
{
    using namespace bl::data;
    using namespace utest;

    TestDataChunkStorageFilesystemLocalHelper::executeTests< DataChunkStorageFilesystemMultiFiles >(
        "DataChunkStorageFilesystemMultiFiles tests"
        );
}

UTF_AUTO_TEST_CASE( TestDataChunkStorageFilesystemSingleFile )
{
    using namespace bl::data;
    using namespace utest;

    TestDataChunkStorageFilesystemLocalHelper::executeTests< DataChunkStorageFilesystemSingleFile >(
        "DataChunkStorageFilesystemSingleFile tests"
        );
}
