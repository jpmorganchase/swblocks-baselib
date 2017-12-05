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

#ifndef __BL_APPS_BL_TOOL_COMMANDS_PROCESSFILESUTILS_H_
#define __BL_APPS_BL_TOOL_COMMANDS_PROCESSFILESUTILS_H_

#include <apps/bl-tool/GlobalOptions.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/Algorithms.h>

#include <baselib/core/FsUtils.h>
#include <baselib/core/FileEncoding.h>
#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class ProcessFilesUtils - utility code for the files process files commands
         */

        template
        <
            typename E = void
        >
        class ProcessFilesUtilsT
        {
            BL_DECLARE_STATIC( ProcessFilesUtilsT )

        public:

            typedef bl::cpp::function< void ( SAA_in const bl::fs::path& path ) > file_processor_callback_t;

            struct BasicFileInfo
            {
                std::uint64_t           fileSize;
                bl::fs::path            filePath;
            };

            struct FileSizeGreater
            {
                bool operator()( SAA_in const BasicFileInfo& lhs, SAA_in const BasicFileInfo& rhs ) const
                {
                    return lhs.fileSize > rhs.fileSize;
                }
            };

            static void processAllFiles(
                SAA_in          const bl::fs::path&                             rootPath,
                SAA_in          const file_processor_callback_t&                processorCallback,
                SAA_in_opt      const std::vector< std::string >&               ignoreFragments = std::vector< std::string >(),
                SAA_in_opt      const std::set< std::string >&                  extensionsFilter = std::set< std::string >()
                )
            {
                bl::fs::ensurePathExists( rootPath );

                if( ! bl::fs::is_directory( rootPath ) )
                {
                    processorCallback( rootPath );

                    return;
                }

                for( bl::fs::recursive_directory_iterator end, it( rootPath ) ; it != end; ++it  )
                {
                    if( ! bl::fs::is_regular_file( it -> status() ) )
                    {
                        /*
                         * Skip non-file entries
                         */

                        continue;
                    }

                    const auto pathw = it -> path().native();
                    std::string path( pathw.begin(), pathw.end() );

                    const auto pos = path.rfind( '.' );
                    if( std::string::npos == pos )
                    {
                        /*
                         * Skip files without extensions
                         */

                        continue;
                    }

                    const auto ext = bl::str::to_lower_copy( path.substr( pos ) );
                    if( ! extensionsFilter.empty() && extensionsFilter.find( ext ) == extensionsFilter.end() )
                    {
                        /*
                         * Skip all extensions we don't care about
                         */

                        continue;
                    }

                    if( ! ignoreFragments.empty() )
                    {
                        bool skipFile = false;

                        for( const auto& ignoreFragment : ignoreFragments )
                        {
                            if( bl::str::contains( path, ignoreFragment ) )
                            {
                                /*
                                 * Skip path fragments which should be ignored
                                 */

                                skipFile = true;
                                break;
                            }
                        }

                        if( skipFile )
                        {
                            continue;
                        }
                    }

                    processorCallback( path );
                }
            }

            static auto getFileLines( SAA_in const bl::fs::path& path ) -> std::vector< std::string >
            {
                std::vector< std::string > lines;

                bl::fs::SafeInputFileStreamWrapper file( path );
                auto& is = file.stream();

                std::string line;
                while( ! is.eof() )
                {
                    std::getline( is, line );

                    if( ! is.eof() || ! line.empty() )
                    {
                        lines.push_back( line );
                    }
                }

                return lines;
            }

            static void fileConvertTabs2Spaces( SAA_in const bl::fs::path& path )
            {
                BL_LOG(
                    bl::Logging::notify(),
                    BL_MSG()
                        << "Converts tabs to spaces for file: "
                        << bl::fs::normalizePathParameterForPrint( path )
                    );

                /*
                 * Just read all lines and, replace tabs with spaces and
                 * write the lines back into the same file
                 */

                auto lines = getFileLines( path );

                {
                    bl::fs::SafeOutputFileStreamWrapper outputFile( path );
                    auto& os = outputFile.stream();

                    for( auto& line : lines )
                    {
                        bl::str::replace_all( line, "\t", "    " );
                        os << line << "\n";
                    }
                }
            }

            static void fileTrimSpacesFromTheRight( SAA_in const bl::fs::path& path )
            {
                BL_LOG(
                    bl::Logging::notify(),
                    BL_MSG()
                        << "Trims spaces on the right for file: "
                        << bl::fs::normalizePathParameterForPrint( path )
                    );

                /*
                 * Just read all lines, trim spaces on the right and
                 * write the lines back into the same file
                 */

                auto lines = getFileLines( path );

                {
                    bl::fs::SafeOutputFileStreamWrapper outputFile( path );
                    auto& os = outputFile.stream();

                    for( auto& line : lines )
                    {
                        bl::str::trim_right( line );
                        os << line << "\n";
                    }
                }
            }

            static void getFilesListAndStats(
                SAA_in          const bl::fs::path&                             path,
                SAA_in          std::vector< BasicFileInfo >&                   filesInfo,
                SAA_inout       std::uint64_t&                                  filesSize
                )
            {
                BasicFileInfo info;

                info.filePath = path;
                info.fileSize = bl::fs::file_size( path );

                filesSize += info.fileSize;

                filesInfo.emplace_back( std::move( info ) );
            }

            static void fileUpdateFileHeaderComment(
                SAA_in          const bl::fs::path&                             path,
                SAA_in          const std::string&                              headerCommentText
                )
            {
                BL_LOG(
                    bl::Logging::notify(),
                    BL_MSG()
                        << "Updating header comment of file: "
                        << bl::fs::normalizePathParameterForPrint( path )
                    );

                /*
                 * Just read all lines, replace or insert the header comment and
                 * write the lines back into the same file
                 */

                auto lines = getFileLines( path );

                {
                    bl::fs::SafeOutputFileStreamWrapper outputFile( path );
                    auto& os = outputFile.stream();

                    bool inFileHader = false;
                    bool inFileBody = false;
                    bool licenseWritten = false;
                    bool headerParsed = false;

                    for( const auto& line : lines )
                    {
                        if( ! inFileHader && ! inFileBody )
                        {
                            const auto trimmed = bl::str::trim_copy( line );

                            if( trimmed.empty() )
                            {
                                /*
                                 * Skip over all empty lines in the beginning of the file
                                 */

                                continue;
                            }

                            if( ! headerParsed && bl::str::starts_with( trimmed, "/*" ) )
                            {
                                inFileHader = true;
                            }
                            else
                            {
                                inFileBody = true;
                            }
                        }

                        if( inFileBody )
                        {
                            if( ! licenseWritten )
                            {
                                if( ! headerCommentText.empty() )
                                {
                                    os << headerCommentText;
                                }

                                licenseWritten = true;
                            }

                            /*
                             * If we are in the file body we simply copy the line and continue
                             */

                            os << line << "\n";

                            continue;
                        }

                        /*
                         * If we are here that means we are in he current header, but not in
                         * the file body yet - check if the current header is about to close
                         */

                        if( ! headerParsed && inFileHader && bl::cpp::contains( line, "*/" ) )
                        {
                            inFileHader = false;
                            headerParsed = true;
                        }
                    }
                }
            }

        };

        typedef ProcessFilesUtilsT<> ProcessFilesUtils;

    } // commands

} // bltool

#endif /* __BL_APPS_BL_TOOL_COMMANDS_PROCESSFILESUTILS_H_ */
