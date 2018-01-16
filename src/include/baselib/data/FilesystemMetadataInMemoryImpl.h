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

#ifndef __BL_FILESYSTEMMETADATAINMEMORYIMPL_H_
#define __BL_FILESYSTEMMETADATAINMEMORYIMPL_H_

#include <baselib/data/FilesystemMetadata.h>

#include <baselib/core/UuidIteratorImpl.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <unordered_map>
#include <unordered_set>

namespace bl
{
    namespace data
    {
        /**
         * @brief class FilesystemMetadataInMemoryImpl
         */

        template
        <
            typename E = void
        >
        class FilesystemMetadataInMemoryImplT :
            public FilesystemMetadata,
            public FilesystemMetadataRO,
            public FilesystemMetadataWO
        {
            BL_CTR_DEFAULT( FilesystemMetadataInMemoryImplT, protected )
            BL_DECLARE_OBJECT_IMPL( FilesystemMetadataInMemoryImplT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( FilesystemMetadataRO )
                BL_QITBL_ENTRY( FilesystemMetadataWO )
            BL_QITBL_END( FilesystemMetadataRO )

        public:

            typedef FilesystemMetadata::EntryInfo               EntryInfo;
            typedef FilesystemMetadata::ChunkInfo               ChunkInfo;

        protected:

            struct ChunkObj
            {
                uuid_t                                          chunkId;
                ChunkInfo                                       info;
            };

            struct EntryObj
            {
                uuid_t                                          entryId;
                EntryInfo                                       info;
                std::vector< uuid_t >                           chunkIds;
                std::unordered_map< uuid_t, ChunkObj >          chunksInfo;
            };

            std::vector< uuid_t >                               m_fileIds;
            std::vector< uuid_t >                               m_chunkIds;

            std::unordered_map< uuid_t, EntryObj >              m_files;
            std::unordered_map< uuid_t, uuid_t >                m_chunk2files;
            cpp::ScalarTypeIniter< bool >                       m_locked;
            os::mutex                                           m_lock;
            std::unordered_set< std::string >                   m_relPaths;

            EntryObj& getEntry( SAA_in const uuid_t& entryId )
            {
                const auto pos = m_files.find( entryId );

                BL_CHK(
                    false,
                    pos != m_files.end(),
                    BL_MSG()
                        << "Invalid entry id '"
                        << uuids::uuid2string( entryId )
                        << "'"
                    );

                return pos -> second;
            }

            ChunkObj& getChunk( SAA_in const uuid_t& chunkId )
            {
                const auto posEntryId = m_chunk2files.find( chunkId );

                BL_CHK(
                    false,
                    posEntryId != m_chunk2files.end(),
                    BL_MSG()
                        << "Invalid chunk id '"
                        << uuids::uuid2string( chunkId )
                        << "'"
                    );

                auto& entryObj = getEntry( posEntryId -> second /* entryId */ );

                const auto pos = entryObj.chunksInfo.find( chunkId );

                BL_CHK(
                    false,
                    pos != entryObj.chunksInfo.end(),
                    BL_MSG()
                        << "Invalid chunk id '"
                        << uuids::uuid2string( chunkId )
                        << "'"
                    );

                return pos -> second;
            }

            void chkLocked()
            {
                BL_CHK(
                    false,
                    m_locked,
                    BL_MSG()
                        << "Using the FilesystemMetadataRO interface is only allowed on immutable object"
                    );
            }

            void chkUnlocked()
            {
                BL_CHK(
                    true,
                    m_locked,
                    BL_MSG()
                        << "Using the FilesystemMetadataWO interface is only allowed on mutable object"
                    );
            }

        public:

            MetadataStatistics computeStatistics()
            {
                MetadataStatistics result;

                result.entriesCount = ( std::uint32_t ) m_files.size();

                /*
                 * This check is to guard for the cast above not truncating data
                 */

                BL_CHK(
                    false,
                    result.entriesCount == m_files.size(),
                    BL_MSG()
                        << "Number of entries being uploaded "
                        << m_files.size()
                        << " is too big"
                    );

                for( const auto& pair : m_files )
                {
                    const auto& info = pair.second.info;

                    result.totalSize += info.size;

                    switch( ( int ) info.type )
                    {
                        case EntryType::Symlink:
                            ++result.symlinksCount;
                            break;

                        case EntryType::Directory:
                            ++result.directoriesCount;
                            break;

                        case EntryType::File:
                            ++result.filesCount;
                            break;

                        default:
                            BL_THROW(
                                UnexpectedException(),
                                BL_MSG()
                                    << "Unexpected file system entry type: "
                                    << info.type
                                );
                            break;
                    }
                }

                return result;
            }

            /*
             * Implementation of FilesystemMetadataRO
             */

            virtual om::ObjPtr< UuidIterator >      queryAllEntries() OVERRIDE
            {
                chkLocked();

                return UuidIteratorImpl::createInstance< UuidIterator >( m_fileIds );
            }

            virtual om::ObjPtr< UuidIterator >      queryAllChunks() OVERRIDE
            {
                chkLocked();

                return UuidIteratorImpl::createInstance< UuidIterator >( m_chunkIds );
            }

            virtual std::size_t                     queryEntriesCount() OVERRIDE
            {
                chkLocked();

                return m_fileIds.size();
            }

            virtual om::ObjPtr< UuidIterator >      queryChunks( SAA_in const uuid_t& entryId ) OVERRIDE
            {
                chkLocked();

                return UuidIteratorImpl::createInstance< UuidIterator >( getEntry( entryId ).chunkIds );
            }

            virtual std::size_t                     queryChunksCount( SAA_in const uuid_t& entryId ) OVERRIDE
            {
                chkLocked();

                return getEntry( entryId ).chunkIds.size();
            }

            virtual uuid_t                          queryEntryId( SAA_in const uuid_t& chunkId ) OVERRIDE
            {
                const auto pos = m_chunk2files.find( chunkId );

                BL_CHK(
                    false,
                    pos != m_chunk2files.end(),
                    BL_MSG()
                        << "Invalid chunk id '"
                        << uuids::uuid2string( chunkId )
                        << "'"
                    );

                return pos -> second;
            }

            virtual EntryInfo                       loadEntryInfo( SAA_in const uuid_t& entryId ) OVERRIDE
            {
                chkLocked();

                const auto& entry = getEntry( entryId );

                return entry.info;
            }

            virtual ChunkInfo                       loadChunkInfo( SAA_in const uuid_t& chunkId ) OVERRIDE
            {
                chkLocked();

                const auto& chunk = getChunk( chunkId );

                return chunk.info;
            }

            /*
             * Implementation of FilesystemMetadataWO
             */

            virtual uuid_t  createEntry( SAA_in EntryInfo&& entryInfo ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkUnlocked();

                const auto entryId = uuids::create();

                m_fileIds.push_back( entryId );

                auto g1 = BL_SCOPE_GUARD( { m_fileIds.pop_back(); } );

                EntryObj& newObj = m_files[ entryId ];
                auto g2 = BL_SCOPE_GUARD( { m_files.erase( entryId ); } );

                newObj.entryId = entryId;
                newObj.info = std::forward< EntryInfo >( entryInfo );

                /*
                 * Ensure the relative path provided is always unique
                 */

                BL_CHK(
                    false,
                    m_relPaths.insert( newObj.info.relPath -> value().string() ).second,
                    BL_MSG()
                        << "relPath must be unique for each entry"
                    );

                g1.dismiss();
                g2.dismiss();

                return entryId;
            }

            virtual void    associateChecksum( SAA_in const uuid_t& entryId, SAA_in const std::uint32_t checksum ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkUnlocked();

                auto& entry = getEntry( entryId );
                entry.info.checksum = checksum;
                entry.info.isChecksumSet = true;
            }

            virtual void associateHash(
                SAA_in      const uuid_t&           entryId,
                SAA_in      const std::string&      hash
                ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkUnlocked();

                auto& entry = getEntry( entryId );
                entry.info.hash = bo::string::createInstance( hash.c_str() );
            }

            virtual uuid_t  createChunk( SAA_in const uuid_t& entryId, SAA_in ChunkInfo&& chunkInfo ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkUnlocked();

                const auto chunkId = uuids::create();

                m_chunkIds.push_back( chunkId );
                auto g1 = BL_SCOPE_GUARD( { m_chunkIds.pop_back(); } );

                auto& entry = getEntry( entryId );

                entry.chunkIds.push_back( chunkId );
                auto g2 = BL_SCOPE_GUARD( { entry.chunkIds.pop_back(); } );

                ChunkObj& newChunk = entry.chunksInfo[ chunkId ];
                auto g3 = BL_SCOPE_GUARD( { entry.chunksInfo.erase( chunkId ); } );

                newChunk.chunkId = chunkId;
                newChunk.info = std::forward< ChunkInfo >( chunkInfo );

                m_chunk2files[ chunkId ] = entryId;

                g1.dismiss();
                g2.dismiss();
                g3.dismiss();

                return chunkId;
            }

            virtual void    finalize() OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkUnlocked();

                m_locked = true;
            }

            virtual bool    isFinalized() const NOEXCEPT OVERRIDE
            {
                return m_locked;
            }
        };

        typedef FilesystemMetadataInMemoryImplT<> FilesystemMetadataInMemory;
        typedef om::ObjectImpl< FilesystemMetadataInMemoryImplT<> > FilesystemMetadataInMemoryImpl;

        /**
         * @brief class FilesystemMetadataUtils
         */

        template
        <
            typename E = void
        >
        class FilesystemMetadataUtilsT :
            public FilesystemMetadata
        {
            BL_DECLARE_STATIC( FilesystemMetadataUtilsT )

        protected:

            template
            <
                typename T
            >
            static bool compare( SAA_in const T& lhs, SAA_in const T& rhs )
            {
                if( lhs != rhs )
                {
                    BL_LOG(
                        Logging::warning(),
                        BL_MSG()
                            << "lhs '"
                            << lhs
                            << "' is different from rhs '"
                            << rhs
                            << "'"
                        );

                    return false;
                }

                return true;
            }

        public:

            static std::string entryTypeToString( SAA_in const EntryType type )
            {
                switch( type )
                {
                    default:
                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Incorrect entry type '"
                                << type
                                << "' encountered"
                            );
                    break;

                    case File:
                        return "File";
                    break;

                    case Directory:
                        return "Directory";
                    break;

                    case Symlink:
                        return "Symlink";
                    break;
                }

                return "";
            }

            static void dumpMissingEntries(
                SAA_in              const std::unordered_set< uuid_t >&             entriesIn,
                SAA_in              const om::ObjPtr< FilesystemMetadataRO >&       fsmd
                )
            {
                const auto entriesCount = fsmd -> queryEntriesCount();

                if( entriesCount != entriesIn.size() )
                {
                    const auto allEntries = fsmd -> queryAllEntries();

                    while( allEntries -> hasCurrent() )
                    {
                        const auto entryId = allEntries -> current();

                        auto info = fsmd -> loadEntryInfo( entryId );

                        BL_CHK(
                            false,
                            File == info.type || Symlink == info.type || Directory == info.type,
                            BL_MSG()
                                << "Incorrect entry type '"
                                << info.type
                                << "' encountered"
                            );

                        if( entriesIn.find( entryId ) == entriesIn.end() )
                        {
                            BL_LOG(
                                Logging::warning(),
                                BL_MSG()
                                    << "Entry of type '"
                                    << entryTypeToString( info.type )
                                    << "' was not processed; relPath="
                                    << info.relPath -> value()
                                );
                        }

                        allEntries -> loadNext();
                    }
                }
            }

            static bool areEqual(
                SAA_in              const om::ObjPtr< FilesystemMetadataRO >&       lhs,
                SAA_in              const om::ObjPtr< FilesystemMetadataRO >&       rhs
                )
            {
                bool equal = true;

                const auto lhsCount = lhs -> queryEntriesCount();
                const auto rhsCount = rhs -> queryEntriesCount();

                if( lhsCount != rhsCount )
                {
                    BL_LOG(
                        Logging::warning(),
                        BL_MSG()
                            << "lhsCount '"
                            << lhsCount
                            << "' is different from rhsCount '"
                            << rhsCount
                            << "'"
                        );

                    equal = false;
                }

                const auto fnCompareEntries = [](
                    SAA_in          const uuid_t&            /* entryId */,
                    SAA_in          const EntryInfo&         lhs,
                    SAA_in          const EntryInfo&         rhs
                    )
                {
                    bool equal = true;

                    equal = equal && compare( lhs.type, rhs.type );
                    equal = equal && compare( lhs.size, rhs.size );
                    equal = equal && compare( lhs.timeCreated, rhs.timeCreated );
                    equal = equal && compare( lhs.lastModified, rhs.lastModified );
                    equal = equal && compare( lhs.flags, rhs.flags );
                    equal = equal && compare( lhs.relPath -> value(), rhs.relPath -> value() );

                    /*
                     * Note: the rhs.isChecksumSet is only set when loading from the store
                     * so we should skip checking this
                     *
                     * equal = equal && compare( lhs.isChecksumSet, rhs.isChecksumSet );
                     */

                    equal = equal && compare( lhs.checksum, rhs.checksum );

                    if( lhs.targetPath && rhs.targetPath )
                    {
                        equal = equal && compare( lhs.targetPath -> value(), rhs.targetPath -> value() );
                    }
                    else
                    {
                        equal = equal && compare< decltype( lhs.targetPath.get() ) >( lhs.targetPath.get(), nullptr );
                        equal = equal && compare< decltype( lhs.targetPath.get() ) >( rhs.targetPath.get(), nullptr );
                    }

                    return equal;
                };

                const auto lhsEntries = lhs -> queryAllEntries();
                const auto rhsEntries = rhs -> queryAllEntries();

                std::set< uuid_t > lhsIds;

                while( lhsEntries -> hasCurrent() )
                {
                    const auto& entryId = lhsEntries -> current();

                    BL_CHK(
                        false,
                        lhsIds.insert( lhsEntries -> current() ).second,
                        BL_MSG()
                            << "Duplicate entry id '"
                            << lhsEntries -> current()
                            << "'"
                        );

                    const auto lhsInfo = lhs -> loadEntryInfo( entryId );
                    const auto rhsInfo = rhs -> loadEntryInfo( entryId );

                    if( ! fnCompareEntries( entryId, lhsInfo, rhsInfo ) )
                    {
                        equal = false;
                    }

                    const auto lhsChunksCount = lhs -> queryChunksCount( entryId );
                    const auto rhsChunksCount = rhs -> queryChunksCount( entryId );

                    if( lhsChunksCount != rhsChunksCount )
                    {
                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "lhsChunksCount '"
                                << lhsChunksCount
                                << "' is different from rhsChunksCount '"
                                << rhsChunksCount
                                << "' for entry with id '"
                                << entryId
                                << "'"
                            );

                        equal = false;
                    }

                    lhsEntries -> loadNext();
                }

                while( rhsEntries -> hasCurrent() )
                {
                    const auto& entryId = rhsEntries -> current();

                    const auto pos = lhsIds.find( entryId );

                    if( pos == lhsIds.end() )
                    {
                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Entry with id '"
                                << rhsEntries -> current()
                                << "' found in rhs, but not present in lhs"
                            );
                    }

                    rhsEntries -> loadNext();
                }

                return equal;
            }
        };

        typedef FilesystemMetadataUtilsT<> FilesystemMetadataUtils;

    } // data

} // bl

#endif /* __BL_FILESYSTEMMETADATAINMEMORYIMPL_H_ */
