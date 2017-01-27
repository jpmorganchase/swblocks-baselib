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

#ifndef __APPS_BLTOOL_COMMANDS_PATHSPACEUSED_H_
#define __APPS_BLTOOL_COMMANDS_PATHSPACEUSED_H_

#include <apps/bl-tool/GlobalOptions.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/utils/DirectoryScannerControlToken.h>
#include <baselib/tasks/utils/ScanDirectoryTask.h>

#include <baselib/core/StringUtils.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/FsUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class PathSpaceUsed
         */

        template
        <
            typename E = void
        >
        class PathSpaceUsedT : public bl::cmdline::CommandBase
        {
        private:

            const GlobalOptions& m_globalOptions;

            /**
             * @brief class ScanningControlIgnoreSomeErrors
             */

            template
            <
                typename E2 = void
            >
            class ScanningControlIgnoreSomeErrorsT :
                public bl::tasks::DirectoryScannerControlToken
            {
                BL_CTR_DEFAULT( ScanningControlIgnoreSomeErrorsT, protected )
                BL_DECLARE_OBJECT_IMPL_ONEIFACE( ScanningControlIgnoreSomeErrorsT, bl::tasks::DirectoryScannerControlToken )

            public:

                virtual bool isCanceled() const NOEXCEPT OVERRIDE
                {
                    return false;
                }

                virtual bool isErrorAllowed( SAA_in const bl::eh::error_code& code ) const NOEXCEPT OVERRIDE
                {
                    /*
                     * Check to ignore certain expected errors such as
                     * file not found and access denied
                     *
                     * For files which we have no access for we can just
                     * ignore
                     *
                     * File not found can happen if a file found during
                     * the scanning stage was deleted for some reason
                     */

                    const auto condition = code.default_error_condition();

                    return
                    (
                        bl::eh::errc::permission_denied == condition ||
                        bl::eh::errc::no_such_file_or_directory == condition
                    );
                }

                virtual bool isEntryAllowed( SAA_in const bl::fs::directory_entry& entry ) OVERRIDE
                {
                    /*
                     * Filter out symlinks and 'other' file entries
                     */

                    const auto status = entry.symlink_status();

                    if( bl::fs::is_other( status ) )
                    {
                        return false;
                    }

                    if( bl::fs::is_symlink( status ) )
                    {
                        return false;
                    }

                    return true;
                }
            };

            typedef bl::om::ObjectImpl< ScanningControlIgnoreSomeErrorsT<> > ScanningControlIgnoreSomeErrors;

        public:

            BL_CMDLINE_OPTION( m_path,  StringOption,   PathName,   PathDesc,   bl::cmdline::Required )

            PathSpaceUsedT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "spaceused", "bl-tool @FULLNAME@ [options]" ),
                m_globalOptions( globalOptions )
            {
                addOption( m_path );

                setHelpMessage(
                    "Calculates disk usage by all sub-directories in a path.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;
                using namespace bl::tasks;

                auto rootDir = fs::absolute( fs::path( m_path.getValue() ) );

                rootDir.make_preferred();

                const auto controlToken =
                    ScanningControlIgnoreSomeErrors::template createInstance< ScanningControlIgnoreSomeErrors >();

                /*
                 * First we simply collect the directories in the top level
                 * path and put them in a map that will hold the disk space
                 */

                std::unordered_map< fs::path, std::uint64_t > map;

                std::size_t noOfEntriesInRootPath = 0U;

                fs::directory_iterator end, it( rootDir );

                for( ; it != end; ++it )
                {
                    const auto& entry = *it;

                    if( controlToken -> isEntryAllowed( entry ) )
                    {
                        const auto& dir = entry.path();

                        std::size_t noOfEntries = 0U;

                        for( auto i = dir.begin(); i != dir.end(); ++i )
                        {
                            ++noOfEntries;
                        }

                        if( noOfEntriesInRootPath )
                        {
                            BL_CHK(
                                false,
                                noOfEntries == noOfEntriesInRootPath,
                                BL_MSG()
                                    << "The number of entries for path "
                                    << dir
                                    << " was "
                                    << noOfEntries
                                    << " where the expected count was "
                                    << noOfEntriesInRootPath
                                );
                        }
                        else
                        {
                            noOfEntriesInRootPath = noOfEntries;
                        }

                        map[ dir ] = 0U;
                    }
                }

                const auto t1 = time::microsec_clock::universal_time();

                scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                    {
                        eq -> setOptions( ExecutionQueue::OptionKeepAll );

                        {
                            const auto boxedRootPath = bo::path::createInstance();
                            boxedRootPath -> swapValue( cpp::copy( rootDir ) );

                            const auto scanner = ScanDirectoryTaskImpl::createInstance(
                                rootDir,
                                boxedRootPath,
                                om::qi< DirectoryScannerControlToken >( controlToken )
                                );

                            const auto scannerTask = om::qi< Task >( scanner );
                            eq -> push_back( scannerTask );
                        }

                        eh::error_code ec;
                        long entriesCount = 0L;

                        for( ;; )
                        {
                            const auto scannerTask = eq -> pop( true /* wait */ );

                            if( ! scannerTask )
                            {
                                break;
                            }

                            if( scannerTask -> isFailed() )
                            {
                                cpp::safeRethrowException( scannerTask -> exception() );
                            }

                            const auto scanner = om::qi< ScanDirectoryTaskImpl >( scannerTask );

                            for( const auto& entry : scanner -> entries() )
                            {
                                ++entriesCount;

                                const auto status = entry.symlink_status();

                                if( fs::is_regular_file( status ) )
                                {
                                    const auto& entryPath = entry.path();

                                    /*
                                     * Construct the prefix path based on the # of entries we expect
                                     */

                                    fs::path pathPrefix;
                                    std::size_t noOfEntries = 0U;

                                    for( auto i = entryPath.begin(); i != entryPath.end(); ++i )
                                    {
                                        if( noOfEntries == noOfEntriesInRootPath )
                                        {
                                            break;
                                        }

                                        ++noOfEntries;

                                        if( pathPrefix.empty() )
                                        {
                                            pathPrefix = *i;
                                        }
                                        else
                                        {
                                            pathPrefix /= *i;
                                        }
                                    }

                                    const auto pos = map.find( pathPrefix );

                                    BL_CHK(
                                        false,
                                        pos != map.end(),
                                        BL_MSG()
                                            << "Cannot find the following path prefix "
                                            << pathPrefix
                                        );

                                    const auto size = fs::file_size( entryPath, ec );

                                    /*
                                     * TODO: remove the double negation in the if statement below once we upgrade the compiler
                                     *
                                     * The double negation is to avoid the following MSVC compiler bug:
                                     * https://svn.boost.org/trac/boost/ticket/7964
                                     *
                                     * Which emits the following incorrect error:
                                     * C4800: 'boost::system::error_code::unspecified_bool_type' : forcing value to bool 'true' or 'false' (performance warning)
                                     */

                                    if( !! ec )
                                    {
                                        if( ! controlToken -> isErrorAllowed( ec ) )
                                        {
                                            BL_CHK_EC_USER_FRIENDLY(
                                                ec,
                                                BL_MSG()
                                                    << "Error obtaining the file size for "
                                                    << entryPath
                                                );
                                        }
                                    }
                                    else
                                    {
                                        pos -> second += size;
                                    }
                                }
                            }
                        }

                        BL_LOG(
                            Logging::notify(),
                            BL_MSG()
                                << "Total number of entries in the directory is "
                                << entriesCount
                            );
                    });

                const auto duration = time::microsec_clock::universal_time() - t1;

                const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

                BL_LOG(
                    Logging::notify(),
                    BL_MSG()
                        << "Scanning "
                        << rootDir
                        << " took "
                        << durationInSeconds
                        << " seconds"
                    );

                /*
                 * We need to use multimap to ensure that duplicate sizes are handled correctly
                 */

                std::multimap< std::uint64_t, fs::path, std::greater< std::uint64_t > > orderedMap;

                for( const auto& pair : map )
                {
                    orderedMap.emplace( pair.second, pair.first );
                }

                BL_ASSERT( map.size() == orderedMap.size() );

                BL_LOG_MULTILINE(
                    Logging::notify(),
                    BL_MSG()
                        << "\nDirectory sizes are as follows:\n"
                    );

                std::uint64_t totalSpaceUsed = 0U;

                for( const auto& pair : orderedMap )
                {
                    BL_LOG(
                        Logging::notify(),
                        BL_MSG()
                            << "Size of "
                            << pair.second
                            << " is "
                            << str::dataRateFormatter( pair.first )
                        );

                    totalSpaceUsed += pair.first;
                }

                BL_LOG_MULTILINE(
                    Logging::notify(),
                    BL_MSG()
                        << "\nTotal space used by the directory is "
                        << str::dataRateFormatter( totalSpaceUsed )
                    );

                return 0;
            }
        };

        typedef PathSpaceUsedT<> PathSpaceUsed;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_PATHSPACEUSED_H_ */
