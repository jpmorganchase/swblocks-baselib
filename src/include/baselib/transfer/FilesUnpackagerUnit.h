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

#ifndef __BL_TRANSFER_FILESUNPACKAGERUNIT_H_
#define __BL_TRANSFER_FILESUNPACKAGERUNIT_H_

#include <baselib/transfer/FilesPkgUnpkgBase.h>
#include <baselib/transfer/FilesPkgUnpkgIncludes.h>

#include <baselib/core/FileEncoding.h>
#include <baselib/core/Checksum.h>
#include <baselib/core/FsUtils.h>

#include <map>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class FilesUnpackagerUnit - the unpackager processing unit
         */

        template
        <
            typename STREAM
        >
        class FilesUnpackagerUnitT :
            public data::FilesystemMetadata,
            public FilesPkgUnpkgBase< STREAM >
        {
            BL_DECLARE_OBJECT_IMPL( FilesUnpackagerUnitT )

        public:

            typedef FilesPkgUnpkgBase< STREAM >                                             base_type;

            enum SymlinkUnsupportedAction
            {
                SuaError,
                SuaWarnAndIgnore,
                SuaWarnAndCreateFile,
                SuaSilentIgnore,
                SuaSilentCreateFile,
            };

        protected:

            typedef FilesUnpackagerUnitT< STREAM >                                          unpackager_t;

            class EntryObj : public om::ObjectDefaultBase
            {
                BL_DECLARE_OBJECT_IMPL( EntryObj )

            protected:

                EntryObj(
                    SAA_in          const uuid_t&                                           entryIdArg,
                    SAA_in          const std::size_t                                       chunksExpectedArg,
                    SAA_in          EntryInfo&&                                             infoArg
                    )
                    :
                    entryId( entryIdArg ),
                    chunksExpected( chunksExpectedArg ),
                    info( infoArg )
                {
                }

            public:

                const uuid_t                                                                entryId;
                const std::size_t                                                           chunksExpected;
                const EntryInfo                                                             info;

                os::mutex                                                                   lock;
                os::stdio_file_ptr                                                          filePtr;
                cpp::ScalarTypeIniter< bool >                                               canceled;
                std::map< std::uint64_t, ChunkInfo >                                        chunksWritten;
            };

            typedef om::ObjectImpl< EntryObj >                                              entry_obj_t;

            static void updateTimestamps(
                SAA_in              const fs::path&                                         path,
                SAA_in              const EntryInfo&                                        info
                )
            {
                if( os::onWindows() )
                {
                    /*
                     * If we're on Windows let's set the created time
                     *
                     * If the time is not available in the metadata (e.g. the data was uploaded
                     * from a UNIX machine) then we set the created time to be the same as
                     * lastModified time
                     */

                    fs::safeSetFileCreateTime(
                        path,
                        info.timeCreated ? info.timeCreated.value() : info.lastModified.value()
                        );
                }

                fs::last_write_time( path, info.lastModified );
            }

            /**
             * @brief Files unpackager scheduler task
             */

            template
            <
                typename E2 = void
            >
            class FilesUnpackagerSchedulerT :
                public tasks::SimpleTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( FilesUnpackagerSchedulerT )

            protected:

                typedef FilesUnpackagerSchedulerT< E2 >                                     this_type;

                typedef cpp::function
                    <
                        bool ( SAA_in const uuid_t& entryId, SAA_in const EntryInfo& info )
                    >
                    callback_t;

                enum
                {
                    MAX_ENTRIES_QUEUE_SIZE = 10 * 1024,
                };

                const om::ObjPtr< data::FilesystemMetadataRO >                              m_fsmd;
                fs::path                                                                    m_targetTmpDir;
                cpp::circular_buffer< om::ObjPtrCopyable< entry_obj_t > >                   m_entriesQueue;
                om::ObjPtr< UuidIterator >                                                  m_entriesIterator;

                cpp::ScalarTypeIniter< bool >                                               m_allDirectoriesScheduled;
                cpp::ScalarTypeIniter< bool >                                               m_allFilesAndDirsDone;
                cpp::ScalarTypeIniter< bool >                                               m_isFinished;
                cpp::ScalarTypeIniter< bool >                                               m_isInputDisconnectedAndAllWorkersDone;

                FilesUnpackagerSchedulerT(
                    SAA_in          const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                    SAA_in          fs::path&&                                              targetTmpDir
                    )
                    :
                    m_fsmd( om::copy( fsmd ) ),
                    m_entriesQueue( MAX_ENTRIES_QUEUE_SIZE )
                {
                    m_targetTmpDir.swap( targetTmpDir );
                }

                fs::path getFullPath( SAA_in const EntryInfo& info ) const
                {
                    return m_targetTmpDir / info.relPath -> value();
                }

                /**
                 * @brief returns false if more chunks need to be scheduled and true when
                 * we're done and all chunks are scheduled
                 */

                bool isDirectoryOrZeroLengthFile( SAA_in const uuid_t& entryId, SAA_in const EntryInfo& info )
                {
                    if( info.type == Directory )
                    {
                        return true;
                    }

                    if( info.type == File && 0U == m_fsmd -> queryChunksCount( entryId ) )
                    {
                        return true;
                    }

                    return false;
                }

                /**
                 * @brief Returns true when all entries are scheduled
                 */

                bool scheduleEntries( SAA_in const callback_t& cbPred )
                {
                    while( m_entriesIterator -> hasCurrent() && false == m_entriesQueue.full() )
                    {
                        const auto entryId = m_entriesIterator -> current();

                        auto info = m_fsmd -> loadEntryInfo( entryId );

                        BL_CHK(
                            false,
                            File == info.type || Symlink == info.type || Directory == info.type,
                            BL_MSG()
                                << "Incorrect entry type '"
                                << info.type
                                << "' encountered"
                            );

                        if( cbPred( entryId, info ) )
                        {
                            auto entry = om::ObjPtrCopyable< entry_obj_t >(
                                entry_obj_t::template createInstance< entry_obj_t >(
                                    entryId,
                                    0U /* chunksExpected */,
                                    std::move( info )
                                    )
                                );

                            m_entriesQueue.push_back( entry );
                        }

                        m_entriesIterator -> loadNext();
                    }

                    /*
                     * Return true only when all entries are scheduled *and* processed
                     */

                    return ( false == m_entriesIterator -> hasCurrent() && m_entriesQueue.empty() );
                }

                void updateDirsTimestamps()
                {
                    /*
                     * The dirs have to be sorted and then processed in reverse order -
                     * see comment below before the traversal code
                     */

                    std::map< std::string, EntryInfo > dirsSorted;

                    m_entriesIterator -> reset();

                    while( m_entriesIterator -> hasCurrent() )
                    {
                        const auto entryId = m_entriesIterator -> current();

                        const auto info = m_fsmd -> loadEntryInfo( entryId );

                        BL_CHK(
                            false,
                            File == info.type || Symlink == info.type || Directory == info.type,
                            BL_MSG()
                                << "Incorrect entry type '"
                                << info.type
                                << "' encountered"
                            );

                        const auto dir = Directory == info.type ?
                            info.relPath -> value() : fs::path( info.relPath -> value().parent_path() );

                        const auto pathAsString = dir.string();

                        if( dirsSorted.find( pathAsString ) == dirsSorted.end() )
                        {
                            dirsSorted[ pathAsString ] = info;
                        }

                        m_entriesIterator -> loadNext();
                    }

                    /*
                     * When setting the timestamps the directories have to be processed
                     * in descending order to make sure the timestamps of all children
                     * are set before the timestamps of a parent
                     */

                    for( auto i = dirsSorted.rbegin(); i != dirsSorted.rend(); ++i )
                    {
                        const EntryInfo& info = i -> second;

                        const auto fullPath = getFullPath( info );

                        updateTimestamps(
                            Directory == info.type ? fullPath : fs::path( fullPath.parent_path() ),
                            info
                            );
                    }
                }

                bool scheduleTasks()
                {
                    if( ! m_entriesIterator )
                    {
                        /*
                         * Called for the first time; just query all entries which we
                         * must iterate over to schedule the internal tasks
                         */

                        m_entriesIterator = m_fsmd -> queryAllEntries();
                    }

                    if( ! m_allDirectoriesScheduled )
                    {
                        /*
                         * Try to schedule as much as possible for this slice
                         */

                        if( scheduleEntries( cpp::bind( &this_type::isDirectoryOrZeroLengthFile, this, _1, _2 ) ) )
                        {
                            m_allDirectoriesScheduled = true;

                            m_entriesIterator -> reset();
                        }
                    }

                    /*
                     * If we're here that means all the directory objects were scheduled
                     *
                     * Now we must wait until all file objects and all directory objects
                     * are done and then we schedule all symlinks and then finally exit
                     * when all symlinks are scheduled on the worker pool
                     */

                    if( ! m_allFilesAndDirsDone )
                    {
                        /*
                         * All files and directories are not done we must wait
                         */

                        if( ! m_isInputDisconnectedAndAllWorkersDone )
                        {
                            return false;
                        }

                        /*
                         * All files and directories are done; cache this in m_allFilesAndDirsDone
                         * so we don't have to check m_isInputDisconnectedAndAllWorkersDone
                         * from now on
                         */

                        m_allFilesAndDirsDone = true;
                    }

                    /*
                     * All file objects and directory objects were created; time to now schedule
                     * all symlinks
                     */

                    const bool allDone = scheduleEntries(
                        [] ( SAA_in const uuid_t& entryId, SAA_in const EntryInfo& info ) -> bool
                        {
                            BL_UNUSED( entryId );

                            return ( info.type == Symlink );
                        }
                        );

                    if( ! allDone )
                    {
                        return false;
                    }

                    if( ! m_isInputDisconnectedAndAllWorkersDone )
                    {
                        return false;
                    }

                    /*
                     * At this point we're all done with the creation of files,
                     * directories and symlinks and all worker tasks are done
                     *
                     * Now we only need to update the timestamps of all
                     * directories in the right order
                     */

                    updateDirsTimestamps();

                    /*
                     * Timestamps of the directories are set so we're all done now
                     */

                    return true;
                }

                virtual void onExecute() NOEXCEPT OVERRIDE
                {
                    BL_TASKS_HANDLER_BEGIN()

                    if( ! m_isFinished )
                    {
                        if( ! isCanceled() )
                        {
                            m_isFinished = scheduleTasks();
                        }
                        else
                        {
                            m_isFinished = true;
                        }
                    }

                    BL_TASKS_HANDLER_END()
                }

            public:

                bool isFinished() const NOEXCEPT
                {
                    return m_isFinished;
                }

                auto entriesQueue() NOEXCEPT -> cpp::circular_buffer< om::ObjPtrCopyable< entry_obj_t > >&
                {
                    return m_entriesQueue;
                }

                void setIsInputDisconnectedAndAllWorkersDone( SAA_in const bool isInputDisconnectedAndAllWorkersDone ) NOEXCEPT
                {
                    m_isInputDisconnectedAndAllWorkersDone = isInputDisconnectedAndAllWorkersDone;
                }
            };

            typedef om::ObjectImpl< FilesUnpackagerSchedulerT<> >                           scheduler_t;

            /**
             * @brief class IoOperationTask
             */

            template
            <
                typename E2 = void
            >
            class IoOperationTaskT :
                public tasks::SimpleTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( IoOperationTaskT )

            protected:

                typedef tasks::SimpleTaskBase                                               base_type;

                const om::ObjPtr< data::FilesystemMetadataRO >                              m_fsmd;
                unpackager_t&                                                               m_unpackager;

                om::ObjPtr< entry_obj_t >                                                   m_entry;
                ChunkInfo                                                                   m_chunkInfo;
                data::DataChunkBlock                                                        m_chunkData;
                cpp::ScalarTypeIniter< bool >                                               m_entryFinalized;

                IoOperationTaskT(
                    SAA_in          const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                    SAA_inout       unpackager_t&                                           unpackager
                    )
                    :
                    m_fsmd( om::copy( fsmd ) ),
                    m_unpackager( unpackager )
                {
                    m_name = "FilesUnpackagerUnit_IoOperationTask";
                }

                fs::path getFullPath() const
                {
                    return m_unpackager.m_targetTmpDir / m_entry -> info.relPath -> value();
                }

                void chkChunk( SAA_in const bool cond )
                {
                    BL_CHK(
                        false,
                        cond,
                        BL_MSG()
                            << "Some chunks have not arrived properly or duplicated chunks were received"
                        );
                }

                void finalizeFile( SAA_in const fs::path& fullPath )
                {
                    /*
                     * Set the time and data correctly after the file is closed
                     */

                    BL_ASSERT( m_entry -> filePtr );

                    m_entry -> filePtr.reset();

                    if( os::onUNIX() && ( data::FilesystemMetadata::Executable & m_entry -> info.flags ) )
                    {
                        fs::permissions( fullPath, fs::perms::add_perms | fs::ExecutableFileMask );
                    }

                    updateTimestamps( fullPath, m_entry -> info );

                    m_entryFinalized = true;
                }

                void verifyChecksumAndAllChunksPresent()
                {
                    /*
                     * Verify all chunks have arrived and the offsets and sizes are correct
                     */

                    const ChunkInfo* prev = nullptr;

                    cs::crc_32_type crcc;
                    for( const auto& pair : m_entry -> chunksWritten )
                    {
                        const auto& chunkInfo = pair.second;

                        if( prev )
                        {
                            chkChunk( ( prev -> pos + prev -> size ) == chunkInfo.pos );
                        }

                        prev = &chunkInfo;

                        /*
                         * Updating the computation of chunk-level checksum, if
                         * the information is available in the metadata
                         */

                        if( m_entry -> info.isChecksumSet )
                        {
                            crcc.process_bytes( &chunkInfo.checksum, sizeof( chunkInfo.checksum ) );
                        }
                    }

                    if( m_entry -> info.isChecksumSet &&
                        m_entry -> info.checksum != crcc.checksum() )
                    {
                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Integrity check failed. Invalid file checksum, expected: "
                                << m_entry -> info.checksum
                                << " computed: "
                                << crcc.checksum()
                                << " for file "
                                << m_entry -> info.relPath -> value()
                            );
                    }

                    /*
                     * prev now points to the last chunk; verify the pos + size matches
                     * the expected file size
                     */

                    BL_ASSERT( prev );
                    chkChunk( ( prev -> pos + prev -> size ) == m_entry -> info.size );
                }

                void handleFileInternal(
                    SAA_in          const fs::path&                                         fullPath,
                    SAA_in          const char*                                             openMode
                    )
                {
                    if( ! m_entry -> filePtr )
                    {
                        m_entry -> filePtr = os::fopen( fullPath, openMode );
                    }

                    if( ! m_entry -> chunksExpected )
                    {
                        /*
                         * This is a zero length file; finalize immediately
                         */

                        finalizeFile( fullPath );

                        return;
                    }

                    if( m_entry -> info.isChecksumSet )
                    {
                        cs::crc_32_type crcc;

                        crcc.process_bytes( m_chunkData.data -> pv(), m_chunkInfo.size );

                        if( m_chunkInfo.checksum != crcc.checksum() )
                        {
                            BL_THROW(
                                UnexpectedException(),
                                BL_MSG()
                                    << "Integrity check failed. Invalid chunk checksum, expected: "
                                    << m_chunkInfo.checksum
                                    << " computed: "
                                    << crcc.checksum()
                                    << " for file "
                                    << m_entry -> info.relPath -> value()
                                );
                        }
                    }

                    BL_ASSERT( m_entry -> filePtr );

                    os::fseek( m_entry -> filePtr, m_chunkInfo.pos, SEEK_SET );

                    os::fwrite( m_entry -> filePtr, m_chunkData.data -> pv(), m_chunkInfo.size );

                    if( 1 == m_entry -> chunksExpected )
                    {
                        /*
                         * This is the one and only chunk for this file
                         *
                         * Finalize the entry and return
                         */

                        chkChunk( ( m_chunkInfo.pos + m_chunkInfo.size ) == m_entry -> info.size );

                        finalizeFile( fullPath );

                        return;
                    }

                    if( m_entryFinalized )
                    {
                        return;
                    }

                    /*
                     * If we're here we need to update the chunks bookkeeping info
                     * and check if this is the last chunk to mark the entry as
                     * finalized
                     *
                     * Note that since we're holding the entry lock only the last
                     * task will win in the check below and close the file and
                     * only this task will have m_entryFinalized=true
                     */

                    m_entry -> chunksWritten[ m_chunkInfo.pos ] = m_chunkInfo;

                    if( m_entry -> chunksWritten.size() == m_entry -> chunksExpected )
                    {
                        /*
                         * This is expected to be the last chunk
                         *
                         * Verify all chunks have arrived and that file checksum
                         * is correct before we mark the entry for finalization
                         */

                        verifyChecksumAndAllChunksPresent();

                        finalizeFile( fullPath );
                    }
                }

                void verifyEntryOnlyTask()
                {
                    BL_ASSERT( m_entry );
                    BL_ASSERT( ! m_entry -> chunksExpected );

                    BL_ASSERT( 0U == m_chunkInfo.size && 0U == m_chunkInfo.pos );
                    BL_ASSERT( uuids::nil() == m_chunkData.chunkId );
                    BL_ASSERT( ! m_chunkData.data );
                }

                void handleFileNoLock( SAA_in const fs::path& fullPath )
                {
                    if( m_entry -> canceled )
                    {
                        /*
                         * This entry has been canceled already and we should
                         * not access its internal state because the filePtr
                         * maybe nullptr now
                         *
                         * This could happen when we have a task which is
                         * terminating due to cancellation and in
                         * onTaskStoppedNothrow we close the file handle
                         * by calling filePtr.reset() to ensure the files
                         * are flushed and there are no outstanding handles
                         * open after the task has been stopped
                         */

                        BL_CHK_EC(
                            asio::error::operation_aborted,
                            BL_MSG()
                                << "This entry has been canceled already"
                            );
                    }

                    /*
                     * Check if the file is to be created and ensure it doesn't exist already
                     * and also ensure the parent directory is created before we attempt
                     * to open the file for writing (in handleFileInternal call below)
                     */

                    if( ! m_entry -> filePtr )
                    {
                        BL_CHK_USER_FRIENDLY(
                            true,
                            fs::exists( fullPath ),
                            BL_MSG()
                                << "File "
                                << fullPath
                                << " is not expected to exist"
                            );

                        fs::safeMkdirs( fullPath.parent_path() );
                    }

                    /*
                     * Catch all exceptions and rethrow after enhancing the exception
                     * with the file name and open mode info
                     */

                    const auto openMode = "wb";

                    try
                    {
                        handleFileInternal( fullPath, openMode );
                    }
                    catch( BaseException& e )
                    {
                        e   << eh::errinfo_file_name( fullPath.string() )
                            << eh::errinfo_file_open_mode( openMode );

                        throw;
                    }
                }

                void handleFileWithLock( SAA_in const fs::path& fullPath )
                {
                    BL_MUTEX_GUARD( m_entry -> lock );

                    handleFileNoLock( fullPath );
                }

                bool verifySymlinkIsSupported( SAA_in const fs::path& fullPath )
                {
                    if( os::onUNIX() )
                    {
                        /*
                         * Symlinks are well supported on all UNIX platforms, so
                         * there is nothing to validate further
                         */

                        return true;
                    }

                    if( m_unpackager.m_symlinksUnsupportedAction == unpackager_t::SuaError )
                    {
                        /*
                         * The action value requires us to error out; this will probably
                         * be the default case
                         */

                        BL_THROW_USER_FRIENDLY(
                            NotSupportedException(),
                            BL_MSG()
                                << "Path "
                                << fullPath
                                << " is a symbolic link or junction and these are not supported on Windows platform"
                            );
                    }

                    if(
                        m_unpackager.m_symlinksUnsupportedAction == unpackager_t::SuaWarnAndIgnore ||
                        m_unpackager.m_symlinksUnsupportedAction == unpackager_t::SuaWarnAndCreateFile
                        )
                    {
                        /*
                         * The action value requires us to issue a warning
                         */

                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "Path '"
                                << fullPath.string()
                                << "' is a symbolic link and these are not supported on Windows platform; "
                                << ( m_unpackager.m_symlinksUnsupportedAction == unpackager_t::SuaWarnAndIgnore ?
                                        "it will be ignored" : "a symlink file will be created" )
                            );
                    }

                    if(
                        m_unpackager.m_symlinksUnsupportedAction == unpackager_t::SuaSilentCreateFile ||
                        m_unpackager.m_symlinksUnsupportedAction == unpackager_t::SuaWarnAndCreateFile
                        )
                    {
                        /*
                         * The action value requires us to create a .symlink text file
                         */

                        auto symlinkFilePath = fullPath;
                        symlinkFilePath += ".symlink";

                        encoding::writeTextFile(
                            symlinkFilePath,
                            m_entry -> info.targetPath -> value().string(),
                            encoding::TextFileEncoding::Utf8
                            );

                        updateTimestamps( symlinkFilePath, m_entry -> info );
                    }

                    return false;
                }

                void doExecute()
                {
                    BL_ASSERT( m_entry );

                    const auto fullPath = getFullPath();
                    BL_ASSERT( ! fullPath.empty() );

                    /*
                     * For now we only handle directories and symlinks
                     */

                    switch( m_entry -> info.type.value() )
                    {
                        default:
                            {
                                BL_THROW(
                                    UnexpectedException(),
                                    BL_MSG()
                                        << "Incorrect entry type '"
                                        << m_entry -> info.type.value()
                                        << "' encountered"
                                    );
                            }
                            break;

                        case File:
                            {
                                if( m_entry -> chunksExpected > 1 )
                                {
                                    handleFileWithLock( fullPath );
                                }
                                else
                                {
                                    handleFileNoLock( fullPath );
                                }
                            }
                            break;

                        case Directory:
                            {
                                verifyEntryOnlyTask();

                                fs::safeMkdirs( fullPath );

                                m_entryFinalized = true;
                            }
                            break;

                        case Symlink:
                            {
                                verifyEntryOnlyTask();

                                if( verifySymlinkIsSupported( fullPath ) )
                                {
                                    eh::error_code ec;

                                    fs::create_symlink(
                                        m_entry -> info.targetPath -> value(),      /* target */
                                        fullPath                                    /* symlink path */,
                                        ec
                                        );

                                    if( ec )
                                    {
                                        /*
                                         * We failed to create a symlink; it maybe because it is
                                         * not relative and / or the target path it points to doesn't
                                         * exist and we should not fail in these cases, but just
                                         * issue a warning
                                         */

                                         BL_LOG(
                                            Logging::warning(),
                                            BL_MSG()
                                                << "Could not create a symlink '"
                                                << fullPath
                                                << "' pointing to '"
                                                << m_entry -> info.targetPath -> value()
                                                << "' because of the following error: '"
                                                << ec.message()
                                                << "'; it will be ignored"
                                            );
                                    }

                                    /*
                                     * Note: looks like changing the timestamp on a symlink is not
                                     * possible (see link below), so there is not much we can do
                                     * further
                                     *
                                     * http://www.mail-archive.com/bug-coreutils@gnu.org/msg10849.html
                                     */
                                }

                                m_entryFinalized = true;
                            }
                            break;
                    }
                }

                virtual auto onTaskStoppedNothrow( SAA_in_opt const std::exception_ptr& eptrIn ) NOEXCEPT
                    -> std::exception_ptr OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    /*
                     * We need to ensure that upon task termination the file handle will be closed no
                     * matter what the reason is (success vs. failure)
                     *
                     * Note that we should *only* attempt to close the file handle on failure or
                     * when the task has been canceled because the task is executed and re-used
                     * multiple times for the same file on success and upon successful download
                     * of all chunks it will close the file, so the only case which isn't covered
                     * is when the task fails or has been canceled
                     */

                    BL_MUTEX_GUARD( m_entry -> lock );

                    if( eptrIn || isFailedOrFailing() || isCanceled() )
                    {
                        m_entry -> filePtr.reset();
                        m_entry -> canceled = true;
                    }

                    BL_NOEXCEPT_END()

                    return base_type::onTaskStoppedNothrow( eptrIn );
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
                        doExecute();
                    }

                    BL_TASKS_HANDLER_END()
                }

            public:

                bool isEntryFinalized() const NOEXCEPT
                {
                    return m_entryFinalized;
                }

                const om::ObjPtr< entry_obj_t >& entry() const NOEXCEPT
                {
                    return m_entry;
                }

                const ChunkInfo& chunkInfo() const NOEXCEPT
                {
                    return m_chunkInfo;
                }

                const data::DataChunkBlock& chunkData() const NOEXCEPT
                {
                    return m_chunkData;
                }

                data::DataChunkBlock& chunkDataLValue() NOEXCEPT
                {
                    return m_chunkData;
                }

                void reset() NOEXCEPT
                {
                    attachData( nullptr /* entry */ );
                }

                void attachData(
                    SAA_in_opt      om::ObjPtr< entry_obj_t >&&                             entry,
                    SAA_in          ChunkInfo&&                                             chunkInfo = ChunkInfo(),
                    SAA_in          data::DataChunkBlock&&                                  chunkData = data::DataChunkBlock()
                    ) NOEXCEPT
                {
                    m_entry = std::forward< om::ObjPtr< entry_obj_t > >( entry );
                    m_chunkInfo = std::forward< ChunkInfo >( chunkInfo );
                    m_chunkData = std::forward< data::DataChunkBlock >( chunkData );
                    m_entryFinalized = false;
                }
            };

            typedef om::ObjectImpl< IoOperationTaskT<> >                                    io_operation_t;

            const SymlinkUnsupportedAction                                                  m_symlinksUnsupportedAction;
            const om::ObjPtr< data::FilesystemMetadataRO >                                  m_fsmd;
            fs::path                                                                        m_targetTmpDir;
            om::ObjPtr< scheduler_t >                                                       m_scheduler;
            cpp::ScalarTypeIniter< std::uint64_t >                                          m_dirsAndSymlinksTotal;

            std::unordered_map< uuid_t, om::ObjPtr< entry_obj_t > >                         m_entriesInProgress;
            std::unordered_set< uuid_t >                                                    m_entriesCompleted;

            FilesUnpackagerUnitT(
                SAA_in              const SymlinkUnsupportedAction                          symlinksUnsupportedAction,
                SAA_in              const om::ObjPtr< SendRecvContextImpl< STREAM > >&      context,
                SAA_in              const om::ObjPtr< data::FilesystemMetadataRO >&         fsmd,
                SAA_in              fs::path&&                                              targetDir
                )
                :
                base_type( context, "success:Files_Unpackager" /* taskName */ ),
                m_symlinksUnsupportedAction( symlinksUnsupportedAction ),
                m_fsmd( om::copy( fsmd ) )
            {
                BL_CHK_USER_FRIENDLY(
                    false,
                    false == fs::exists( targetDir ) && targetDir.is_absolute() && targetDir.has_parent_path(),
                    BL_MSG()
                        << "The target directory "
                        << targetDir
                        << " should be absolute, it should not exist and must have a valid parent path"
                    );

                auto targetTmpDir = targetDir.parent_path() / uuids::uuid2string( uuids::create() );

                fs::safeMkdirs( targetTmpDir );
                auto g  = BL_SCOPE_GUARD( { fs::safeDeletePathNothrow( targetTmpDir ); } );

                targetTmpDir = fs::makeHidden( targetTmpDir );

                g.dismiss();

                m_targetTmpDir.swap( targetTmpDir );
            }

            void dumpInProgressEntries()
            {
                for( const auto& pair : m_entriesInProgress )
                {
                    const auto& entry = pair.second;

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Entry of type '"
                            << data::FilesystemMetadataUtils::entryTypeToString( entry -> info.type )
                            << "' was not completed; relPath="
                            << entry -> info.relPath -> value()
                        );
                }
            }

            bool isInputDisconnectedAndAllWorkersDone()
            {
                return (
                    base_type::m_inputDisconnected &&
                    base_type::m_eqWorkerTasks -> size() == base_type::m_eqWorkerTasks -> getQueueSize( tasks::ExecutionQueue::Ready )
                    );
            }

            om::ObjPtr< tasks::Task > getTopReady( SAA_inout om::ObjPtr< io_operation_t >& unpackager )
            {
                if( ! base_type::waitForReadyWorkerTask( false /* force */, false /* unwinding */ ) )
                {
                    return nullptr;
                }

                auto topReady = base_type::m_eqWorkerTasks -> top( false /* wait */ );
                BL_ASSERT( topReady );

                unpackager = om::qi< io_operation_t >( topReady );

                return topReady;
            }

            bool chk2ScheduleEntryOnly( SAA_inout om::ObjPtrCopyable< entry_obj_t >& entry )
            {
                om::ObjPtr< io_operation_t > unpackager;

                const auto topReady = getTopReady( unpackager );

                if( ! topReady )
                {
                    return false;
                }

                BL_ASSERT( unpackager );

                /*
                 * The entry must be a directory, symlink or a zero length file
                 */

                const auto& info = entry -> info;

                BL_UNUSED( info );

                BL_ASSERT( info.type == File || info.type == Directory || info.type == Symlink );
                BL_ASSERT( info.type != File || 0U == m_fsmd -> queryChunksCount( entry -> entryId ) );

                /*
                 * If the entry is a symlink then the target path must be non-empty string
                 *
                 * We can assert on this because the data validation will be done at the
                 * data access layer and the FilesystemMetadataRO / FilesystemMetadataWO
                 * interfaces
                 */

                BL_ASSERT( info.type != Symlink || false == info.targetPath -> value().empty() );

                unpackager -> attachData( entry.detachAsUnique() );
                base_type::m_eqWorkerTasks -> push_back( topReady );

                return true;
            }

            /**
             * @brief Handle incoming chunk data block
             */

            bool chk2ScheduleChunk( SAA_in data::DataChunkBlock&& info )
            {
                BL_MUTEX_GUARD( base_type::m_lock );

                om::ObjPtr< io_operation_t > unpackager;

                const auto topReady = getTopReady( unpackager );

                if( ! topReady )
                {
                    return false;
                }

                BL_ASSERT( unpackager );

                const auto entryId = m_fsmd -> queryEntryId( info.chunkId );

                /*
                 * Ensure the entry is not in the completed list
                 */

                BL_CHK(
                    false,
                    m_entriesCompleted.find( entryId ) == m_entriesCompleted.end(),
                    BL_MSG()
                        << "Chunk has arrived for an entry which has been finalized"
                    );

                om::ObjPtr< entry_obj_t > entry;

                const auto pos = m_entriesInProgress.find( entryId );

                if( pos == m_entriesInProgress.end() )
                {
                    const auto chunksCount = m_fsmd -> queryChunksCount( entryId );

                    BL_CHK(
                        false,
                        0U != chunksCount,
                        BL_MSG()
                            << "Chunk has arrived for an entry which is expected to have zero chunks"
                        );

                    auto entryObjNew = entry_obj_t::template createInstance< entry_obj_t >(
                        entryId,
                        chunksCount,
                        m_fsmd -> loadEntryInfo( entryId )
                        );

                    BL_ASSERT( entryObjNew -> info.type == File && 0U != m_fsmd -> queryChunksCount( entryId ) );

                    if( chunksCount > 1 )
                    {
                        /*
                         * More than one chunk is expected for this entry thus we must
                         * remember it in the 'in-progress' list
                         */

                        m_entriesInProgress[ entryId ] = om::copy( entryObjNew );
                    }

                    entry = std::move( entryObjNew );
                }
                else
                {
                    /*
                     * This is an entry which is 'in-progress', just obtain a reference to it
                     */

                    entry = om::copy( pos -> second );
                }

                unpackager -> attachData(
                    std::move( entry ),
                    m_fsmd -> loadChunkInfo( info.chunkId ),
                    std::forward< data::DataChunkBlock >( info )
                    );

                base_type::m_eqWorkerTasks -> push_back( topReady );

                return true;
            }

            virtual auto processTopReadyTask( SAA_in const om::ObjPtr< tasks::Task >& topReady ) -> typename base_type::ProcessTaskResult OVERRIDE
            {
                const auto unpackager = om::qi< io_operation_t >( topReady );

                /*
                 * Check to update the 'in-progress' and completed lists
                 */

                const auto& entry = unpackager -> entry();

                if( entry && unpackager -> isEntryFinalized() )
                {
                    if( entry -> info.type == File )
                    {
                        ++base_type::m_filesTotal;
                        base_type::m_fileSizeTotal += entry -> info.size;
                    }
                    else
                    {
                        ++m_dirsAndSymlinksTotal;
                    }

                    const auto& entryId = entry -> entryId;

                    const auto pos = m_entriesInProgress.find( entryId );

                    if( pos != m_entriesInProgress.end() )
                    {
                        m_entriesInProgress.erase( pos );
                    }

                    BL_VERIFY( m_entriesCompleted.insert( entryId ).second );
                }

                /*
                 * Check to return the block in the pool and then just reset
                 * the task object, so it is ready for next unit of work
                 */

                auto& chunkData = unpackager -> chunkDataLValue();

                if( chunkData.data )
                {
                    base_type::m_dataSizeTotal.lvalue() += chunkData.data -> size();

                    base_type::m_context -> dataBlocksPool() -> put( chunkData.data.detachAsUnique() );
                }

                unpackager -> reset();

                /*
                 * We don't re-schedule tasks and we don't push out data, so
                 * the top ready task is always ready to be consumed once
                 * it has completed
                 */

                return base_type::ReturnTrue;
            }

            virtual std::string getStats() OVERRIDE
            {
                return resolveMessage(
                    BL_MSG()
                        << "\n    dirsAndSymlinksTotal: "
                        << m_dirsAndSymlinksTotal
                        << "\n    totalEntriesCompleted: "
                        << m_entriesCompleted.size()
                    );
            }

            virtual om::ObjPtr< tasks::Task > createSeedingTask() OVERRIDE
            {
                /*
                 * First let's populate the worker pool
                 */

                BL_ASSERT( base_type::m_eqWorkerTasks );

                for( std::size_t i = 0; i < base_type::m_tasksPoolSize; ++i )
                {
                    const auto blockWriter = io_operation_t::template createInstance< io_operation_t >( m_fsmd, *this );
                    base_type::m_eqWorkerTasks -> push_back( om::qi< tasks::Task >( blockWriter ), true /* dontSchedule */ );
                }

                /*
                 * The only seeding task is the scheduler which will handle the
                 * self generated events such as creating the directories and
                 * also the symlinks at the end
                 *
                 * Note that we also will remember the scheduler task, so we can
                 * ensure that it is the only task pushed in the pushReadyTask()
                 * call below
                 */

                m_scheduler = scheduler_t::template createInstance< scheduler_t >(
                    m_fsmd,
                    cpp::copy( m_targetTmpDir )
                    );

                return om::qi< tasks::Task >( m_scheduler );
            }

            virtual bool pushReadyTask( SAA_in const om::ObjPtrCopyable< tasks::Task >& task ) OVERRIDE
            {
                /*
                 * No other task except the scheduler should be pushed
                 */

                BL_ASSERT( om::areEqual( m_scheduler, task.get() ) );
                BL_UNUSED( task );

                if( m_scheduler -> isCanceled() )
                {
                    /*
                     * The task was canceled
                     */

                    return true;
                }

                /*
                 * First we try to process the current queue as much as
                 * possible and if the queue isn't fully processed we
                 * will continue re-scheduling
                 */

                auto& queue = m_scheduler -> entriesQueue();

                while( ! queue.empty() )
                {
                    auto& entry = queue.front();

                    if( chk2ScheduleEntryOnly( entry ) )
                    {
                        queue.pop_front();

                        continue;
                    }

                    /*
                     * Cannot schedule more entries
                     */

                    break;
                }

                if( queue.empty() && m_scheduler -> isFinished() )
                {
                    /*
                     * The queue is empty and the scheduler is done
                     *
                     * Nothing more to do at this point
                     */

                    return true;
                }

                /*
                 * Update the status of m_isInputDisconnectedAndAllWorkersDone and request
                 * the task to be re-scheduled since more work needs to be done
                 */

                m_scheduler -> setIsInputDisconnectedAndAllWorkersDone( isInputDisconnectedAndAllWorkersDone() );

                return false;
            }

            virtual bool flushAllPendingTasks() OVERRIDE
            {
                const bool flushed = base_type::flushAllPendingTasks();

                if( flushed )
                {
                    /*
                     * All file writing is done
                     *
                     * Check to see if we should commit or rollback the transaction
                     */

                    if( base_type::isFailedOrFailing() )
                    {
                        fs::safeDeletePathNothrow( m_targetTmpDir );
                        m_targetTmpDir.clear();
                    }
                }

                if( flushed && false == base_type::isFailedOrFailing() )
                {
                    /*
                     * If the task is successful we want to ensure that all data have been
                     * received and created properly
                     */

                    const auto entriesInProgress = m_entriesInProgress.size();

                    if( entriesInProgress )
                    {
                        /*
                         * Some missing chunks have been encountered
                         *
                         * dumpInProgressEntries();
                         */

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Missing chunks have been encountered; number of entries that are not completed is "
                                << entriesInProgress
                            );
                    }

                    const auto entriesCompleted = m_entriesCompleted.size();
                    const auto entriesCount = m_fsmd -> queryEntriesCount();

                    if( entriesCount != entriesCompleted )
                    {
                        /*
                         * Some entries were not created for some reason
                         *
                         * FilesystemMetadataUtils::dumpMissingEntries( m_entriesCompleted, m_fsmd );
                         */

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Missing or unexpected entries have been encountered; expected count is "
                                << entriesCount
                                << " while the processed count is "
                                << entriesCompleted
                            );
                    }
                }

                return flushed;
            }

        public:

            const fs::path& targetTmpDir() const NOEXCEPT
            {
                return m_targetTmpDir;
            }

            bool onChunkArrived( SAA_in const cpp::any& value )
            {
                auto chunkInfo = cpp::any_cast< data::DataChunkBlock >( value );

                return chk2ScheduleChunk( std::move( chunkInfo ) );
            }
        };

        typedef FilesUnpackagerUnitT< tasks::TcpSocketAsyncBase >       FilesUnpackagerUnit;
        typedef FilesUnpackagerUnitT< tasks::TcpSslSocketAsyncBase >    SslFilesUnpackagerUnit;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_FILESUNPACKAGERUNIT_H_ */
