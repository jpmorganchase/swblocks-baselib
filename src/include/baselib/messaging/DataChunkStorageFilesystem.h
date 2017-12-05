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

#ifndef __BL_MESSAGING_DATACHUNKSTORAGEFILESYSTEM_H_
#define __BL_MESSAGING_DATACHUNKSTORAGEFILESYSTEM_H_

#include <baselib/messaging/DataChunkStorage.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace data
    {
        /**
         * @brief class DataChunkStorageFilesystemBase - a file system base implementation of
         * the DataChunkStorage interface
         */

        template
        <
            typename E = void
        >
        class DataChunkStorageFilesystemBaseT : public DataChunkStorage
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE_NO_DESTRUCTOR( DataChunkStorageFilesystemBaseT, DataChunkStorage )

        protected:

            cpp::SafeUniquePtr< fs::TmpDir >                                            m_tempDirectory;
            fs::path                                                                    m_rootPath;
            fs::path                                                                    m_rootPathChunks;
            cpp::ScalarTypeIniter< bool >                                               m_disposed;

            DataChunkStorageFilesystemBaseT(
                SAA_in_opt                  fs::path&&                                  rootPath = fs::path(),
                SAA_in_opt                  const bool                                  isRootTemp = false
                )
            {
                if( rootPath.empty() )
                {
                    m_tempDirectory = cpp::SafeUniquePtr< fs::TmpDir >::attach(
                        new fs::TmpDir( isRootTemp ? rootPath : fs::path() )
                        );

                    m_rootPath = m_tempDirectory -> path();
                }
                else
                {
                    m_rootPath = BL_PARAM_FWD( rootPath );
                }

                m_rootPathChunks = m_rootPath / "chunks";

                fs::safeMkdirs( m_rootPathChunks );
            }

            ~DataChunkStorageFilesystemBaseT() NOEXCEPT
            {
                disposeInternal();
            }

            void disposeInternal() NOEXCEPT
            {
                BL_WARN_NOEXCEPT_BEGIN()

                if( m_disposed )
                {
                    return;
                }

                m_tempDirectory.reset();

                m_disposed = true;

                BL_WARN_NOEXCEPT_END( "DataChunkStorageFilesystemBaseT::dispose()" )
            }

            void chkNotDisposed()
            {
                BL_CHK(
                    true,
                    m_disposed.value(),
                    BL_MSG()
                        << "Using data storage object after it has been disposed"
                    );
            }

            void throwChunkDoesNotExist( SAA_in const uuid_t& chunkId )
            {
                BL_THROW(
                    ServerErrorException()
                        << eh::errinfo_error_code(
                            eh::errc::make_error_code( eh::errc::no_such_file_or_directory )
                            )
                        << eh::errinfo_error_uuid( chunkId ),
                    BL_MSG()
                        << "Chunk with id "
                        << chunkId
                        << " does not exist"
                    );
            }

            void chkBlockSize(
                SAA_in                  const std::uint64_t                             size,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                )
            {
                BL_CHK(
                    false,
                    size <= data -> capacity64(),
                    BL_MSG()
                        << "Data block capacity is too small: "
                        << data -> capacity64()
                        << "; required capacity is "
                        << size
                    );
            }

        public:

            /*
             * om::Disposable implementation
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                disposeInternal();
            }

            /*
             * partial data::DataChunkStorage implementation
             */

            virtual void flushPeerSessions( SAA_in const uuid_t& peerId ) OVERRIDE
            {
                BL_UNUSED( peerId );

                chkNotDisposed();
            }
        };

        /**
         * @brief class DataChunkStorageFilesystemMultiFiles - a file system implementation of
         * the DataChunkStorage interface with multiple files (one file per chunk)
         */

        template
        <
            typename E = void
        >
        class DataChunkStorageFilesystemMultiFilesT : public DataChunkStorageFilesystemBaseT<>
        {
            BL_DECLARE_OBJECT_IMPL( DataChunkStorageFilesystemMultiFilesT )

        protected:

            typedef DataChunkStorageFilesystemBaseT<>                                   base_type;

            DataChunkStorageFilesystemMultiFilesT(
                SAA_in_opt                  fs::path&&                                  rootPath = fs::path(),
                SAA_in_opt                  const bool                                  isRootTemp = false
                )
                :
                base_type( BL_PARAM_FWD( rootPath ), isRootTemp )
            {
            }

            auto getChunkPath( SAA_in const uuid_t& chunkId ) -> fs::path
            {
                return m_rootPathChunks / str::to_lower_copy( uuids::uuid2string( chunkId ) );
            }

            auto getExistingChunkPath( SAA_in const uuid_t& chunkId ) -> fs::path
            {
                auto chunkPath = getChunkPath( chunkId );

                if( ! fs::path_exists( chunkPath ) )
                {
                    base_type::throwChunkDoesNotExist( chunkId );
                }

                return chunkPath;
            }

        public:

            /*
             * partial data::DataChunkStorage implementation
             */

            virtual void load(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );

                chkNotDisposed();

                const auto chunkPath = getExistingChunkPath( chunkId );

                const auto size = fs::file_size( chunkPath );

                base_type::chkBlockSize( size, data );

                /*
                 * Note that because of the check above this static cast here is safe
                 */

                data -> setSize( static_cast< std::size_t >( size ) );
                data -> setOffset1( 0U );

                if( size )
                {
                    const auto filePtr = os::fopen( getChunkPath( chunkId ), "rb" );

                    os::fread( filePtr, data -> pv(), data -> size() );
                }
            }

            virtual void save(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );

                chkNotDisposed();

                const auto filePtr = os::fopen( getChunkPath( chunkId ), "wb" );

                if( data -> size() )
                {
                    os::fwrite( filePtr, data -> pv(), data -> size() );
                }
            }

            virtual void remove(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );

                chkNotDisposed();

                const auto chunkPath = getExistingChunkPath( chunkId );

                fs::safeRemove( chunkPath );
            }
        };

        typedef om::ObjectImpl< DataChunkStorageFilesystemMultiFilesT<> > DataChunkStorageFilesystemMultiFiles;

        /**
         * @brief class DataChunkStorageFilesystemSingleFile - a file system implementation of
         * the DataChunkStorage interface with multiple files (one file per chunk)
         */

        template
        <
            typename E = void
        >
        class DataChunkStorageFilesystemSingleFileT : public DataChunkStorageFilesystemBaseT<>
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( DataChunkStorageFilesystemSingleFileT )

        protected:

            typedef DataChunkStorageFilesystemSingleFileT< E >                          this_type;
            typedef DataChunkStorageFilesystemBaseT<>                                   base_type;

            enum : std::uint32_t
            {
                CHUNK_FLAG_DELETED = 1U,
            };

            struct ChunkHeader
            {
                bl::uuid_t          chunkId;
                std::uint64_t       pos;
                std::uint64_t       size;
                std::uint32_t       flags;

                ChunkHeader() NOEXCEPT
                    :
                    chunkId( uuids::nil() ),
                    pos( 0U ),
                    size( 0U ),
                    flags( 0U )
                {
                }
            };

            os::stdio_file_ptr                                                          m_file;
            fs::path                                                                    m_filePath;
            std::unordered_map< uuid_t, ChunkHeader >                                   m_activeChunks;
            os::mutex                                                                   m_lock;

            DataChunkStorageFilesystemSingleFileT(
                SAA_in_opt                  fs::path&&                                  rootPath = fs::path(),
                SAA_in_opt                  const bool                                  isRootTemp = false
                )
                :
                base_type( BL_PARAM_FWD( rootPath ), isRootTemp )
            {
                m_filePath = m_rootPathChunks / "data.bin";
                m_file = os::fopen( m_filePath, "ab+" );

                loadChunksData();
            }

            ~DataChunkStorageFilesystemSingleFileT() NOEXCEPT
            {
                disposeInternal();
            }

            void disposeInternal() NOEXCEPT
            {
                BL_WARN_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                if( m_disposed )
                {
                    return;
                }

                m_file.reset();

                base_type::disposeInternal();

                m_disposed = true;

                BL_WARN_NOEXCEPT_END( "DataChunkStorageFilesystemSingleFileT::dispose()" )
            }

            void chkFileFormatInvariant( SAA_in const bool cond )
            {
                BL_CHK(
                    false,
                    cond,
                    BL_MSG()
                        << "The file format of "
                        << m_filePath
                        << " in invalid"
                    );
            }

            void loadChunksData()
            {
                BL_MUTEX_GUARD( m_lock );

                const auto size = fs::file_size( m_filePath );

                m_activeChunks.clear();

                if( ! size )
                {
                    return;
                }

                std::uint64_t pos = 0U;

                os::fseek( m_file, 0U, SEEK_SET );

                while( pos < size )
                {
                    ChunkHeader header;

                    chkFileFormatInvariant( ( pos + sizeof( header ) ) <= size );

                    os::fread( m_file, &header, sizeof( header ) );

                    chkFileFormatInvariant( header.pos == pos );

                    pos += sizeof( header );

                    if( header.size )
                    {
                        chkFileFormatInvariant( ( pos + header.size ) <= size );

                        pos += header.size;

                        os::fseek( m_file, pos, SEEK_SET );
                    }

                    if( ! ( header.flags & CHUNK_FLAG_DELETED ) )
                    {
                        m_activeChunks.emplace( header.chunkId, header );
                    }
                }
            }

            void removeChunk(
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const bool                                      throwIfDoesNotExist
                )
            {
                auto pos = m_activeChunks.find( chunkId );

                if( pos == m_activeChunks.end() )
                {
                    if( throwIfDoesNotExist )
                    {
                        throwChunkDoesNotExist( chunkId );
                    }

                    return;
                }

                auto& header = pos -> second;

                header.flags |= CHUNK_FLAG_DELETED;

                os::fseek( m_file, header.pos, SEEK_SET );
                os::fwrite( m_file, &header, sizeof( header ) );

                m_activeChunks.erase( pos );
            }

        public:

            /*
             * om::Disposable implementation
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                disposeInternal();
            }

            /*
             * partial data::DataChunkStorage implementation
             */

            virtual void load(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );

                BL_MUTEX_GUARD( m_lock );

                chkNotDisposed();

                const auto pos = m_activeChunks.find( chunkId );

                if( pos == m_activeChunks.end() )
                {
                    base_type::throwChunkDoesNotExist( chunkId );
                }

                const auto& header = pos -> second;

                base_type::chkBlockSize( header.size, data );

                data -> setSize( static_cast< std::size_t >( header.size ) );
                data -> setOffset1( 0U );

                if( header.size )
                {
                    os::fseek( m_file, header.pos + sizeof( header ), SEEK_SET );
                    os::fread( m_file, data -> pv(), data -> size() );
                }
            }

            virtual void save(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );

                BL_MUTEX_GUARD( m_lock );

                chkNotDisposed();

                removeChunk( chunkId, false /* throwIfDoesNotExist */ );

                os::fseek( m_file, 0U, SEEK_END );

                ChunkHeader header;

                header.chunkId = chunkId;
                header.pos = os::ftell( m_file );
                header.size = data -> size();

                os::fwrite( m_file, &header, sizeof( header ) );

                if( header.size )
                {
                    os::fwrite( m_file, data -> pv(), data -> size() );
                }

                BL_VERIFY( m_activeChunks.emplace( header.chunkId, header ).second );
            }

            virtual void remove(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId
                ) OVERRIDE
            {
                BL_UNUSED( sessionId );

                BL_MUTEX_GUARD( m_lock );

                chkNotDisposed();

                removeChunk( chunkId, true /* throwIfDoesNotExist */ );
            }
        };

        typedef om::ObjectImpl< DataChunkStorageFilesystemSingleFileT<> > DataChunkStorageFilesystemSingleFile;

    } // data

} // bl

#endif /* __BL_MESSAGING_DATACHUNKSTORAGEFILESYSTEM_H_ */
