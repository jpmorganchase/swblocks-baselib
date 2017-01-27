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

#ifndef __BL_TRANSFER_FILESPACKAGERUNIT_H_
#define __BL_TRANSFER_FILESPACKAGERUNIT_H_

#include <baselib/core/Checksum.h>
#include <baselib/transfer/FilesPkgUnpkgBase.h>
#include <baselib/transfer/FilesPkgUnpkgIncludes.h>

#include <baselib/crypto/HashCalculator.h>

#include <baselib/core/FsUtils.h>

#include <map>

namespace bl
{
    namespace transfer
    {
        namespace detail
        {
            /**
             * @brief class FileTask
             */

            template
            <
                typename E = void
            >
            class FileTaskT :
                public tasks::SimpleTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( FileTaskT )

            protected:

                typedef data::FilesystemMetadata::EntryInfo                                     EntryInfo;

                const om::ObjPtr< data::FilesystemMetadataWO >                                  m_fsmd;
                const om::ObjPtrCopyable< tasks::ScanDirectoryTaskImpl >                        m_entryTask;
                const fs::directory_entry&                                                      m_entry;

                EntryInfo                                                                       m_info;
                uuid_t                                                                          m_entryId;
                std::map< std::uint64_t, std::uint32_t >                                        m_chunksChecksums;
                std::map< std::uint64_t, std::string >                                          m_chunksHashes;

                FileTaskT(
                    SAA_in          const om::ObjPtr< data::FilesystemMetadataWO >&             fsmd,
                    SAA_in          const om::ObjPtrCopyable< tasks::ScanDirectoryTaskImpl >    entryTask,
                    SAA_in          const fs::directory_entry&                                  entry
                    )
                    :
                    m_fsmd( om::copy( fsmd ) ),
                    m_entryTask( entryTask ),
                    m_entry( entry )
                {
                }

                virtual void onExecute() NOEXCEPT OVERRIDE
                {
                    BL_TASKS_HANDLER_BEGIN()

                    /*
                     * Simple tasks must complete in one shot
                     * and then we notify the EQ we're done
                     */

                    if( false == isCanceled() )
                    {
                        /*
                         * Just obtain the file size and open the file
                         * so it is ready for reading
                         */

                        const auto status = m_entry.symlink_status();
                        const auto& path = m_entry.path();

                        m_info.lastModified = fs::last_write_time( path );
                        m_info.timeCreated = os::onWindows() ? fs::safeGetFileCreateTime( path ) : 0;

                        m_info.relPath = bo::path::createInstance();
                        BL_VERIFY( fs::getRelativePath( path, m_entryTask -> rootPath(), m_info.relPath -> lvalue() ) );
                        BL_ASSERT( m_info.relPath -> value() != "" );

                        if( fs::is_regular_file( status ) )
                        {
                            /*
                             * This is a regular file; just open the file in
                             * preparation for reading
                             */

                            if( status.permissions() & fs::ExecutableFileMask )
                            {
                                m_info.flags |= data::FilesystemMetadata::Executable;
                            }

                            m_info.type = data::FilesystemMetadata::File;
                            m_info.size = fs::file_size( path );
                        }
                        else if( fs::is_symlink( status ) )
                        {
                            /*
                             * Symlinks on Windows are supported only after Vista and also they
                             * require admin privilege, so we can't really support them
                             *
                             * On Windows we also have junctions, which are available since XP,
                             * but they can only point to directories and have limited scope
                             * and usage and they can point to file shares which isn't supported
                             * on Linux, so for now we won't support them either.
                             */

                            BL_CHK_T_USER_FRIENDLY(
                                true,
                                os::onWindows(),
                                NotSupportedException(),
                                BL_MSG()
                                    << "Path "
                                    << path
                                    << " is a symbolic link or junction and these are not supported on Windows platform"
                                );

                            /*
                             * This is a symlink and we're on a UNIX platform;
                             * read the symlink target path
                             */

                            m_info.type = data::FilesystemMetadata::Symlink;
                            m_info.targetPath = bo::path::createInstance();
                            m_info.targetPath -> swapValue( fs::read_symlink( path ) );
                        }
                        else if( fs::is_directory( status ) )
                        {
                            m_info.type = data::FilesystemMetadata::Directory;
                        }
                        else
                        {
                            BL_THROW_USER_FRIENDLY(
                                UnexpectedException(),
                                BL_MSG()
                                    << "Path "
                                    << m_entry.path()
                                    << " cannot be processed because it is not a file, directory or symlink"
                                );
                        }

                        /*
                         * Create and commit the entry immediately since we're not going to
                         * change it anymore
                         */

                        m_entryId = m_fsmd -> createEntry( cpp::copy( m_info ) );
                    }

                    BL_TASKS_HANDLER_END()
                }

            public:

                const fs::directory_entry& entry() const NOEXCEPT
                {
                    return m_entry;
                }

                std::uint64_t fileSize() const NOEXCEPT
                {
                    return m_info.size;
                }

                const om::ObjPtrCopyable< bo::path > relPath() const NOEXCEPT
                {
                    return m_info.relPath;
                }

                const fs::path& rootPath() const NOEXCEPT
                {
                    return m_entryTask -> rootPath();
                }

                const uuid_t& entryId() const NOEXCEPT
                {
                    return m_entryId;
                }

                auto chunksChecksums() NOEXCEPT -> std::map< std::uint64_t, std::uint32_t >&
                {
                    return m_chunksChecksums;
                }

                auto chunksHashes() NOEXCEPT -> std::map< std::uint64_t, std::string >&
                {
                    return m_chunksHashes;
                }
            };

            typedef om::ObjectImpl< FileTaskT<> > FileTaskImpl;

            /**
             * @brief class BlockReaderTask
             */

            template
            <
                typename E = void
            >
            class BlockReaderTaskT : public tasks::SimpleTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( BlockReaderTaskT )

            protected:

                typedef data::FilesystemMetadata::ChunkInfo                                     ChunkInfo;

                const om::ObjPtr< data::FilesystemMetadataWO >                                  m_fsmd;

                om::ObjPtr< FileTaskImpl >                                                      m_fileTask;
                os::stdio_file_ptr                                                              m_filePtr;
                om::ObjPtr< data::DataBlock >                                                   m_dataBlock;
                cpp::ScalarTypeIniter< std::uint64_t >                                          m_filePos;
                uuid_t                                                                          m_chunkId;

                BlockReaderTaskT(
                    SAA_in          const om::ObjPtr< data::FilesystemMetadataWO >&             fsmd
                    )
                    :
                    m_fsmd( om::copy( fsmd ) )
                {
                }

                void doExecute()
                {
                    /*
                     * Just obtain the file size and open the file
                     * so it is ready for reading
                     */

                    const std::uint64_t fileSize = m_fileTask -> fileSize();
                    const std::uint64_t bytesLeft = ( fileSize - m_filePos );
                    const std::uint64_t capacity = m_dataBlock -> capacity64();

                    /*
                     * Calculate if we're reading a full block or a partial block
                     */

                    const std::size_t bytesToRead = ( std::size_t )( ( bytesLeft <= capacity ) ? bytesLeft : capacity );

                    if( ! m_filePtr )
                    {
                        m_filePtr = os::fopen( m_fileTask -> entry().path(), "rb" );
                    }

                    os::fread( m_filePtr, m_dataBlock -> pv(), bytesToRead );

                    m_dataBlock -> setSize( bytesToRead );

                    /*
                     * Create the chunk in the filesystem metadata, but we don't
                     * commit it yet; it will be committed when the chunk is later
                     * transmitted; note also that we have to do this before we
                     * move the file pointer
                     */

                    ChunkInfo chunk;

                    chunk.pos = m_filePos;
                    BL_ASSERT( bytesToRead < std::numeric_limits< std::uint32_t >::max() );
                    chunk.size = ( std::uint32_t ) bytesToRead;

                    cs::crc_32_type crcc;
                    crcc.process_bytes( m_dataBlock -> pv(), bytesToRead );
                    chunk.checksum = crcc.checksum();
                    m_fileTask -> chunksChecksums()[ m_filePos ] = chunk.checksum;

                    hash::HashCalculatorDefault hashCalculator;
                    hashCalculator.update( m_dataBlock -> pv(), m_dataBlock -> size() );
                    hashCalculator.finalize();
                    m_fileTask -> chunksHashes()[ m_filePos ] = hashCalculator.digestStr();

                    m_chunkId = m_fsmd -> createChunk( m_fileTask -> entryId(), std::move( chunk ) );

                    /*
                     * Move the file pointer and ensure it is correct
                     */

                    m_filePos += bytesToRead;
                    BL_ASSERT( m_filePos <= fileSize );

                    if( m_filePos == m_fileTask -> fileSize() )
                    {
                        /*
                         * Promptly close file handle when no longer needed to avoid
                         * hitting open file handles limit imposed by stdio library
                         */

                        m_filePtr.reset();
                    }
                }

                virtual void onExecute() NOEXCEPT OVERRIDE
                {
                    BL_TASKS_HANDLER_BEGIN()

                    /*
                     * Simple tasks must complete in one shot
                     * and then we notify the EQ we're done
                     */

                    BL_ASSERT( hasMoreBlocks() );

                    if( false == isCanceled() )
                    {
                        doExecute();
                    }

                    BL_TASKS_HANDLER_END()
                }

            public:

                bool hasMoreBlocks() const NOEXCEPT
                {
                    return ( m_fileTask && m_filePos < m_fileTask -> fileSize() );
                }

                const om::ObjPtr< FileTaskImpl >& fileTask() const NOEXCEPT
                {
                    return m_fileTask;
                }

                const om::ObjPtr< data::DataBlock >& dataBlock() const NOEXCEPT
                {
                    return m_dataBlock;
                }

                om::ObjPtr< data::DataBlock > detachDataBlock() NOEXCEPT
                {
                    return std::move( m_dataBlock );
                }

                void attachDataBlockOnly(
                    SAA_in          const om::ObjPtr< data::DataBlock >&                        dataBlock
                    ) NOEXCEPT
                {
                    m_dataBlock = om::copy( dataBlock );
                }

                void attachAllData(
                    SAA_in          const om::ObjPtr< FileTaskImpl >&                           fileTask,
                    SAA_in          const om::ObjPtr< data::DataBlock >&                        dataBlock
                    ) NOEXCEPT
                {
                    m_fileTask = om::copy( fileTask );
                    m_dataBlock = om::copy( dataBlock );
                    m_filePos = 0U;
                }

                const uuid_t& chunkId() const NOEXCEPT
                {
                    return m_chunkId;
                }
            };

            typedef om::ObjectImpl< BlockReaderTaskT<> > BlockReaderTaskImpl;

        } // detail

        /**
         * @brief class FilesPackagerUnit
         *
         * Important notes:
         *
         * The fastest and most efficient way to serve file data over sockets is by using
         * the ::sendfile() / ::TransmitFile() APIs (for POSIX vs. Windows respectively)
         *
         * These APIs do the file transfer async on the kernel side and avoid copying
         * unnecessarily the file data in user space. However these APIs have some
         * limitations (e.g. the file has to be map-able and others) and also the main
         * advantage of using them would be if we can do this on the server side to
         * avoid allocating memory and copying the file data in user space. However
         * on the server side the file data will be stored in a data store, so this main
         * advantage is negated. Another reason that will negate this advantage on the
         * client side as well is the fact that may we want to read the data and compress
         * it before sending it to the data store cluster
         *
         * Because of the above reason our initial implementation will read the file
         * data and chunk it in memory before sending it over the socket.
         */

        template
        <
            typename STREAM
        >
        class FilesPackagerUnitT :
            public FilesPkgUnpkgBase< STREAM >
        {
        public:

            typedef FilesPkgUnpkgBase< STREAM >                                             base_type;
            typedef typename base_type::context_t                                           context_t;

        private:

            BL_DECLARE_OBJECT_IMPL( FilesPackagerUnitT )

        protected:

            cpp::ScalarTypeIniter< std::uint64_t >                                          m_entriesTotalPushed;
            cpp::ScalarTypeIniter< std::uint64_t >                                          m_batchTotalPushed;
            const om::ObjPtr< data::FilesystemMetadataWO >                                  m_fsmd;

            FilesPackagerUnitT(
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              const om::ObjPtr< data::FilesystemMetadataWO >&         fsmd
                )
                :
                base_type( context, "success:Files_Packager" /* taskName */ ),
                m_fsmd( om::copy( fsmd ) )
            {
            }

            virtual auto processTopReadyTask( SAA_in const om::ObjPtr< tasks::Task >& topReady ) -> typename base_type::ProcessTaskResult OVERRIDE
            {
                /*
                 * This is the regular case; let's see if we can push out the data
                 * and re-schedule the task to read the next chunk
                 */

                const auto packager = om::qi< detail::BlockReaderTaskImpl >( topReady );

                /*
                 * Try to push out the data block now...
                 *
                 * Note: The block can be empty because it was pushed out and we
                 * couldn't allocate new one for some reason (e.g. out of memory),
                 * but the packager task has yet more blocks to produce
                 */

                const auto& block = packager -> dataBlock();

                if( block )
                {
                    data::DataChunkBlock chunkInfo;

                    chunkInfo.chunkId = packager -> chunkId();
                    BL_ASSERT( uuids::nil() != chunkInfo.chunkId );
                    chunkInfo.data = block;

                    if( ! base_type::notifyOnNext( cpp::any( chunkInfo ) ) )
                    {
                        /*
                         * We could not push the data block into the next processing unit
                         * events queue
                         */

                        return base_type::ReturnFalse;
                    }

                    /*
                     * The data was pushed successfully into the next processing unit queue
                     * and thus we must detach it (since we no longer own it in the sense
                     * that the data will be used / read from a different unit). The data
                     * block will be returned in the pool for reuse once it has been
                     * processed and discarded by the last processing unit in the pipeline.
                     */

                    base_type::m_dataSizeTotal += block -> size();

                    if( ! packager -> hasMoreBlocks() )
                    {
                        base_type::m_fileSizeTotal += packager -> fileTask() -> fileSize();
                        ++base_type::m_filesTotal;
                    }

                    packager -> detachDataBlock();
                }

                if( packager -> hasMoreBlocks() )
                {
                    /*
                     * The top packager task has more blocks to process; push it back
                     * and continue searching for a ready packager task
                     */

                    packager -> attachDataBlockOnly( data::DataBlock::get( base_type::m_context -> dataBlocksPool() ) );

                    base_type::m_eqWorkerTasks -> push_back( topReady );

                    /*
                     * The topReady task was re-scheduled and it is not available
                     */

                    return base_type::TaskRescheduled;
                }

                /*
                 * computing and storing file-based checksum, based on the
                 * chunks' checksums
                 */

                const auto& chunksChecksums = packager -> fileTask() -> chunksChecksums();
                if( chunksChecksums.size() )
                {
                    cs::crc_32_type crcc;

                    for( const auto& pair : chunksChecksums )
                    {
                        crcc.process_bytes( &pair.second, sizeof( pair.second ) );
                    }

                    m_fsmd -> associateChecksum( packager-> fileTask() -> entryId(), crcc.checksum() );
                }

                /*
                 *  Computing and storing file-based hash based on the chunk's hash
                 */

                const auto& chunksHashes = packager-> fileTask() -> chunksHashes();

                if( chunksHashes.size() )
                {
                    hash::HashCalculatorDefault hashCalculator;

                    for( const auto& pair : chunksHashes )
                    {
                        const auto& chunkHash = pair.second;
                        hashCalculator.update( chunkHash.c_str(), chunkHash.size() );
                    }

                    hashCalculator.finalize();

                    m_fsmd -> associateHash( packager -> fileTask() -> entryId(), hashCalculator.digestStr() );
                }

                /*
                 * The topReady task is available now
                 */

                return base_type::ReturnTrue;
            }

            virtual std::string getStats() OVERRIDE
            {
                return resolveMessage(
                    BL_MSG()
                        << "\n    entriesTotalPushed: "
                        << m_entriesTotalPushed
                        << "\n    batchTotalPushed: "
                        << m_batchTotalPushed
                    );
            }

            virtual bool pushReadyTask( SAA_in const om::ObjPtrCopyable< tasks::Task >& task ) OVERRIDE
            {
                const auto fileTask = om::qi< detail::FileTaskImpl >( task );

                /*
                 * We need to process only regular files with non-zero size
                 */

                if( fs::is_regular_file( fileTask -> entry().symlink_status() ) )
                {
                    if( 0U == fileTask -> fileSize() )
                    {
                        ++base_type::m_filesTotal;

                        return true;
                    }
                }
                else
                {
                    return true;
                }

                om::ObjPtr< detail::BlockReaderTaskImpl > packager;

                if( base_type::m_eqWorkerTasks -> size() < base_type::m_tasksPoolSize )
                {
                    packager = detail::BlockReaderTaskImpl::createInstance( m_fsmd );
                }
                else
                {
                    packager = om::qi< detail::BlockReaderTaskImpl >( base_type::m_eqWorkerTasks -> top( false /* wait */ ) );
                    BL_ASSERT( packager );
                }

                BL_ASSERT( ! packager -> dataBlock() );
                BL_ASSERT( ! packager -> hasMoreBlocks() );

                packager -> attachAllData( fileTask, data::DataBlock::get( base_type::m_context -> dataBlocksPool() ) );
                BL_ASSERT( packager -> dataBlock() );
                BL_ASSERT( packager -> hasMoreBlocks() );

                base_type::m_eqWorkerTasks -> push_back( om::qi< tasks::Task >( packager ) );

                return true;
            }

        public:

            bool onFilesBatchArrived( SAA_in const cpp::any& value )
            {
                BL_MUTEX_GUARD( base_type::m_lock );

                if( ! base_type::verifyInputInternal( value ) )
                {
                    return false;
                }

                const auto task = cpp::any_cast< om::ObjPtrCopyable< tasks::Task > >( value );
                auto scanner = om::qi< tasks::ScanDirectoryTaskImpl >( task );

                ++m_batchTotalPushed;

                for( const auto& entry : scanner -> entries() )
                {
                    base_type::m_eqChildTasks -> push_back(
                        detail::FileTaskImpl::createInstance< tasks::Task >(
                            m_fsmd,
                            scanner,
                            entry
                            )
                        );

                    ++m_entriesTotalPushed;
                }

                return true;
            }
        };

        typedef FilesPackagerUnitT< tasks::TcpSocketAsyncBase >     FilesPackagerUnit;
        typedef FilesPackagerUnitT< tasks::TcpSslSocketAsyncBase >  SslFilesPackagerUnit;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_FILESPACKAGERUNIT_H_ */
