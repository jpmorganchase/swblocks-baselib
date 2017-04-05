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

#include <baselib/core/PreCompiled.h>
#include <baselib/data/PreCompiled.h>

#include <utests/baselib/PreCompiled.h>

/************************************************************************
 * FilesystemMetadataInMemoryImpl tests
 */

UTF_AUTO_TEST_CASE( TestFilesystemMetadataInMemoryImpl )
{
    using namespace bl;

    const auto uuidIteratorVerifyCount = [](
        SAA_in      const bl::om::ObjPtr< bl::UuidIterator >&       iter,
        SAA_in      const size_t                                    countExpected
        )
        -> void
    {
        size_t countEntries = 0;

        for( ; iter -> hasCurrent() ; iter -> loadNext() )
        {
            ++countEntries;
        }

        UTF_REQUIRE_EQUAL( countEntries, countExpected );

        iter -> reset();
        countEntries = 0;

        for( ; iter -> hasCurrent() ; iter -> loadNext() )
        {
            ++countEntries;
        }

        UTF_REQUIRE_EQUAL( countEntries, countExpected );
    };

    typedef bl::data::FilesystemMetadataInMemoryImpl fsmd_t;

    const auto randomId = bl::uuids::create();

    {
        const auto fsmd = fsmd_t::createInstance< bl::data::FilesystemMetadataRO >();

        /*
         * The object is not finalized, so any method calls should throw
         */

        UTF_REQUIRE_THROW( fsmd -> queryAllEntries(), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> queryAllChunks(), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> queryChunks( randomId ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> loadEntryInfo( randomId ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> loadChunkInfo( randomId ), bl::UnexpectedException );

        const auto wo = bl::om::qi< bl::data::FilesystemMetadataWO >( fsmd );

        wo -> finalize();

        /*
         * Now the object is finalized RO interface should work
         */

        {
            const auto iter = fsmd -> queryAllEntries();
            UTF_REQUIRE( iter );
            UTF_REQUIRE( ! iter -> hasCurrent() );
        }

        {
            const auto iter = fsmd -> queryAllChunks();
            UTF_REQUIRE( iter );
            UTF_REQUIRE( ! iter -> hasCurrent() );
        }

        /*
         * These methods should throw since there is still no data in it
         */

        UTF_REQUIRE_THROW( fsmd -> queryChunks( randomId ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> loadEntryInfo( randomId ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> loadChunkInfo( randomId ), bl::UnexpectedException );

        /*
         * Since the object is now immutable any calls to the WO methods should throw
         */

        UTF_REQUIRE_THROW( wo -> createEntry( fsmd_t::EntryInfo() ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( wo -> createChunk( randomId, fsmd_t::ChunkInfo() ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( wo -> finalize(), bl::UnexpectedException );
    }

    {
        std::set< bl::uuid_t > s;

        const auto fsmd = fsmd_t::createInstance< bl::data::FilesystemMetadataWO >();

        const auto now = std::time( nullptr );
        BL_CHK_ERRNO_NM( ( std::time_t )( -1 ), now );

        const std::uint64_t pos64 = 5ULL * std::numeric_limits< std::uint32_t >::max();

        fsmd_t::EntryInfo entry;
        entry.size = 4321U;
        entry.type = fsmd_t::File;
        entry.lastModified = now;
        entry.timeCreated = now;
        entry.relPath = bl::bo::path::createInstance();

        fsmd_t::ChunkInfo chunk;
        chunk.pos = pos64;
        chunk.size = 1234;

        bl::fs::path path1( "foo/bar1/baz" );
        entry.relPath -> lvalue().swap( path1 );
        const auto entryId1 = fsmd -> createEntry( bl::cpp::copy( entry ) );

        bl::fs::path path2( "foo/bar/baz2" );
        entry.relPath -> lvalue().swap( path2 );
        const auto entryId2 = fsmd -> createEntry( bl::cpp::copy( entry ) );

        UTF_REQUIRE( s.insert( entryId1 ).second );
        UTF_REQUIRE( s.insert( entryId2 ).second );

        const auto chunkId1 = fsmd -> createChunk( entryId1, bl::cpp::copy( chunk ) );
        const auto chunkId2 = fsmd -> createChunk( entryId1, bl::cpp::copy( chunk ) );
        const auto chunkId3 = fsmd -> createChunk( entryId2, bl::cpp::copy( chunk ) );
        const auto chunkId4 = fsmd -> createChunk( entryId2, bl::cpp::copy( chunk ) );

        UTF_REQUIRE( s.insert( chunkId1 ).second );
        UTF_REQUIRE( s.insert( chunkId2 ).second );
        UTF_REQUIRE( s.insert( chunkId3 ).second );
        UTF_REQUIRE( s.insert( chunkId4 ).second );

        fsmd -> finalize();

        {
            const auto stats = om::qi< fsmd_t >( fsmd ) -> computeStatistics();

            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "Filesystem metadata statistics (1):\n"
                    << "\nentriesCount: "
                    << stats.entriesCount
                    << "\nsymlinksCount: "
                    << stats.symlinksCount
                    << "\ndirectoriesCount: "
                    << stats.directoriesCount
                    << "\nfilesCount: "
                    << stats.filesCount
                    << "\ntotalSize: "
                    << stats.totalSize
                    << "\n\n"
                );

            UTF_REQUIRE_EQUAL( stats.entriesCount, 2U );
            UTF_REQUIRE_EQUAL( stats.symlinksCount, 0U );
            UTF_REQUIRE_EQUAL( stats.directoriesCount, 0U );
            UTF_REQUIRE_EQUAL( stats.filesCount, 2U );
            UTF_REQUIRE_EQUAL( stats.totalSize, 2U * 4321U );
        }

        /*
         * Since the object is now immutable any calls to the WO methods should throw
         */

        UTF_REQUIRE_THROW( fsmd -> createEntry( fsmd_t::EntryInfo() ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> createChunk( randomId, fsmd_t::ChunkInfo() ), bl::UnexpectedException );
        UTF_REQUIRE_THROW( fsmd -> finalize(), bl::UnexpectedException );

        /*
         * Now the object is finalized RO interface should work
         */

        const auto ro = bl::om::qi< bl::data::FilesystemMetadataRO >( fsmd );

        s.clear();

        uuidIteratorVerifyCount( ro -> queryAllEntries(), 2 );
        uuidIteratorVerifyCount( ro -> queryAllChunks(), 4 );

        const auto iter = ro -> queryAllEntries();

        size_t countEntries = 0;

        for( ; iter -> hasCurrent() ; iter -> loadNext() )
        {
            const auto entryId = iter -> current();
            UTF_REQUIRE( s.insert( entryId ).second );
            ++countEntries;

            const auto entryInfo = ro -> loadEntryInfo( entryId );

            UTF_REQUIRE( fsmd_t::File == entryInfo.type );
            UTF_REQUIRE( now == entryInfo.lastModified );
            UTF_REQUIRE( now == entryInfo.timeCreated );

            UTF_REQUIRE(
                "foo/bar1/baz" == entryInfo.relPath -> value() ||
                "foo/bar/baz2" == entryInfo.relPath -> value()
                );

            const auto iterChunks = ro -> queryChunks( entryId );

            size_t countChunks = 0;

            for( ; iterChunks -> hasCurrent() ; iterChunks -> loadNext() )
            {
                const auto chunkId = iterChunks -> current();
                UTF_REQUIRE( s.insert( chunkId ).second );
                ++countChunks;

                const auto chunkInfo = ro -> loadChunkInfo( chunkId );

                UTF_REQUIRE( pos64 == chunkInfo.pos );
                UTF_REQUIRE( 1234 == chunkInfo.size );
            }

            UTF_REQUIRE_EQUAL( 2U, countChunks );
        }

        UTF_REQUIRE_EQUAL( 2U, countEntries );

        {
            const auto stats = om::qi< fsmd_t >( fsmd ) -> computeStatistics();

            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "Filesystem metadata statistics (1):\n"
                    << "\nentriesCount: "
                    << stats.entriesCount
                    << "\nsymlinksCount: "
                    << stats.symlinksCount
                    << "\ndirectoriesCount: "
                    << stats.directoriesCount
                    << "\nfilesCount: "
                    << stats.filesCount
                    << "\ntotalSize: "
                    << stats.totalSize
                    << "\n\n"
                );

            UTF_REQUIRE_EQUAL( stats.entriesCount, 2U );
            UTF_REQUIRE_EQUAL( stats.symlinksCount, 0U );
            UTF_REQUIRE_EQUAL( stats.directoriesCount, 0U );
            UTF_REQUIRE_EQUAL( stats.filesCount, 2U );
            UTF_REQUIRE_EQUAL( stats.totalSize, 2U * 4321U );
        }
    }
}
