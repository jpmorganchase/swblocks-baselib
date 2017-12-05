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

#ifndef __BL_DATA_FILESYSTEMMETADATA_H_
#define __BL_DATA_FILESYSTEMMETADATA_H_

#include <baselib/core/BoxedObjects.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/UuidIterator.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( FilesystemMetadataRO, "685f3129-35db-4ef4-bb3f-2670d0e47efa" )
BL_IID_DECLARE( FilesystemMetadataWO, "e172b357-422a-4d0a-aac2-13b7fb73fdb5" )

namespace bl
{
    namespace data
    {
        /**********************************************************************************************
         * FilesystemMetadata
         */

        /**
         * @brief FilesystemMetadata common definitions
         */

        class FilesystemMetadata
        {
            BL_DECLARE_ABSTRACT( FilesystemMetadata )

        public:

            enum EntryType
            {
                File,
                Directory,
                Symlink,
            };

            enum EntryFlags : std::uint32_t
            {
                EntryFlagsNone      = 0,
                Executable          = 0x1,
            };

            struct EntryInfo
            {
                cpp::ScalarTypeIniter< EntryType >                                  type;
                cpp::ScalarTypeIniter< std::uint64_t >                              size;
                cpp::ScalarTypeIniter< std::time_t >                                timeCreated;
                cpp::ScalarTypeIniter< std::time_t >                                lastModified;
                cpp::ScalarTypeIniter< EntryFlags >                                 flags;
                om::ObjPtrCopyable< bo::path >                                      relPath;
                om::ObjPtrCopyable< bo::path >                                      targetPath;
                om::ObjPtrCopyable< bo::path >                                      sourcePath;
                cpp::ScalarTypeIniter< bool >                                       isChecksumSet;
                cpp::ScalarTypeIniter< std::uint32_t >                              checksum;
                om::ObjPtrCopyable< bo::string >                                    hash;
            };

            struct ChunkInfo
            {
                cpp::ScalarTypeIniter< std::uint64_t >                              pos;
                cpp::ScalarTypeIniter< std::uint32_t >                              size;
                cpp::ScalarTypeIniter< std::uint32_t >                              checksum;
                om::ObjPtrCopyable< bo::string >                                    hash;
                cpp::ScalarTypeIniter< bool >                                       isChunkDuplicate;
            };

            struct ChunkPositions
            {
                om::ObjPtrCopyable< bo::uint64_vector >                             chunkLocations;
            };

            struct MetadataStatistics
            {
                cpp::ScalarTypeIniter< std::uint32_t >                              entriesCount;
                cpp::ScalarTypeIniter< std::uint32_t >                              symlinksCount;
                cpp::ScalarTypeIniter< std::uint32_t >                              directoriesCount;
                cpp::ScalarTypeIniter< std::uint32_t >                              filesCount;
                cpp::ScalarTypeIniter< std::uint64_t >                              totalSize;
            };
        };

        BL_DECLARE_BITMASK( FilesystemMetadata::EntryFlags )

        /**********************************************************************************************
         * FilesystemMetadataRO
         */

        /**
         * @brief interface FilesystemMetadataRO (read-only)
         */

        class FilesystemMetadataRO :
            public om::Object
        {
            BL_DECLARE_INTERFACE( FilesystemMetadataRO )

        public:

            typedef FilesystemMetadata::EntryInfo EntryInfo;
            typedef FilesystemMetadata::ChunkInfo ChunkInfo;

            virtual om::ObjPtr< UuidIterator >      queryAllEntries() = 0;
            virtual om::ObjPtr< UuidIterator >      queryAllChunks() = 0;
            virtual std::size_t                     queryEntriesCount() = 0;
            virtual om::ObjPtr< UuidIterator >      queryChunks( SAA_in const uuid_t& entryId ) = 0;
            virtual std::size_t                     queryChunksCount( SAA_in const uuid_t& entryId ) = 0;
            virtual uuid_t                          queryEntryId( SAA_in const uuid_t& chunkId ) = 0;
            virtual EntryInfo                       loadEntryInfo( SAA_in const uuid_t& entryId ) = 0;
            virtual ChunkInfo                       loadChunkInfo( SAA_in const uuid_t& chunkId ) = 0;
        };

        /**********************************************************************************************
         * FilesystemMetadataWO
         */

        /**
         * @brief interface FilesystemMetadataWO (write-only)
         */

        class FilesystemMetadataWO :
            public om::Object
        {
            BL_DECLARE_INTERFACE( FilesystemMetadataWO )

        public:

            typedef FilesystemMetadata::EntryInfo EntryInfo;
            typedef FilesystemMetadata::ChunkInfo ChunkInfo;

            virtual uuid_t  createEntry( SAA_in EntryInfo&& entryInfo ) = 0;
            virtual void    associateChecksum( SAA_in const uuid_t& entryId, SAA_in const std::uint32_t checksum ) = 0;
            virtual void    associateHash( SAA_in const uuid_t& entryId, SAA_in const std::string& hash ) = 0;
            virtual uuid_t  createChunk( SAA_in const uuid_t& entryId, SAA_in ChunkInfo&& chunkInfo ) = 0;
            virtual void    finalize() = 0;
            virtual bool    isFinalized() const NOEXCEPT = 0;
        };

    } // data

} // bl

#endif /* __BL_DATA_FILESYSTEMMETADATA_H_ */
