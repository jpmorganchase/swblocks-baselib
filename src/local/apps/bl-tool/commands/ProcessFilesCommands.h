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

#ifndef __BL_APPS_BL_TOOL_COMMANDS_PROCESSFILESCOMMANDS_H_
#define __BL_APPS_BL_TOOL_COMMANDS_PROCESSFILESCOMMANDS_H_

#include <apps/bl-tool/GlobalOptions.h>

#include <apps/bl-tool/commands/ProcessFilesUtils.h>

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
         * @brief class ProcessFilesCommandBase - the base class for the process files commands
         */

        template
        <
            typename E = void
        >
        class ProcessFilesCommandBaseT : public bl::cmdline::CommandBase
        {
        protected:

            const GlobalOptions& m_globalOptions;

            BL_CMDLINE_OPTION(
                m_path,
                StringOption,
                "path",
                "A file path or a root directory path for the file(s) to convert",
                bl::cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_extensions,
                MultiStringOption,
                "extension",
                "An extension filter to use when processing a directory recursively",
                bl::cmdline::MultiValue
                )

            BL_CMDLINE_OPTION(
                m_ignorePathFragments,
                MultiStringOption,
                "ignorepathfragment",
                "A a path fragment filter to use when processing a directory recursively",
                bl::cmdline::MultiValue
                )

            ProcessFilesCommandBaseT(
                SAA_inout           bl::cmdline::CommandBase*          parent,
                SAA_in              const GlobalOptions&               globalOptions,
                SAA_inout           std::string&&                      commandName,
                SAA_inout           std::string&&                      helpCaption
                )
                :
                bl::cmdline::CommandBase( parent, BL_PARAM_FWD( commandName ), BL_PARAM_FWD( helpCaption ) ),
                m_globalOptions( globalOptions )
            {
                addOption( m_path, m_extensions, m_ignorePathFragments );
            }
        };

        typedef ProcessFilesCommandBaseT<> ProcessFilesCommandBase;

        /**
         * @brief class ProcessFilesPrintFilesBySize - prints files ordered by their size
         */

        template
        <
            typename E = void
        >
        class ProcessFilesPrintFilesBySizeT : public ProcessFilesCommandBase
        {
        protected:

            using ProcessFilesCommandBase::m_path;
            using ProcessFilesCommandBase::m_extensions;
            using ProcessFilesCommandBase::m_ignorePathFragments;

        public:

            ProcessFilesPrintFilesBySizeT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                ProcessFilesCommandBase( parent, globalOptions, "printfilesbysize", "bl-tool @FULLNAME@ [options]" )
            {
                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Prints files list and other statistics (files ordered by their sizes).\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;

                const auto& extensions = m_extensions.getValue();
                const std::set< std::string > extensionsFilter( extensions.begin(), extensions.end() );

                const auto& ignorePathFragments = m_ignorePathFragments.getValue();

                std::uint64_t filesSize = 0U;
                std::vector< ProcessFilesUtils::BasicFileInfo > filesInfo;

                ProcessFilesUtils::processAllFiles(
                    m_path.getValue(),
                    cpp::bind(
                        &ProcessFilesUtils::getFilesListAndStats,
                        _1,
                        cpp::ref( filesInfo ),
                        cpp::ref( filesSize )
                        ),
                    ignorePathFragments,
                    extensionsFilter
                    );

                std::sort( filesInfo.begin(), filesInfo.end(), ProcessFilesUtils::FileSizeGreater() );

                BL_LOG_MULTILINE(
                    Logging::notify(),
                    BL_MSG()
                        << "\nThe # of files matching the filters is "
                        << filesInfo.size()
                        << " and their total size is "
                        << filesSize
                        << "\n\n"
                    );

                for( const auto& info : filesInfo )
                {
                    BL_LOG(
                        Logging::notify(),
                        BL_MSG()
                            << info.fileSize
                            << ", "
                            << fs::normalizePathParameterForPrint( info.filePath )
                        );
                }

                return 0;
            }
        };

        typedef ProcessFilesPrintFilesBySizeT<> ProcessFilesPrintFilesBySize;

        /**
         * @brief class ProcessFilesTabsToSpaces - converts tabs to spaces in the processed files
         */

        template
        <
            typename E = void
        >
        class ProcessFilesTabsToSpacesT : public ProcessFilesCommandBase
        {
        protected:

            using ProcessFilesCommandBase::m_path;
            using ProcessFilesCommandBase::m_extensions;
            using ProcessFilesCommandBase::m_ignorePathFragments;

        public:

            ProcessFilesTabsToSpacesT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                ProcessFilesCommandBase( parent, globalOptions, "tabstospaces", "bl-tool @FULLNAME@ [options]" )
            {
                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Converts tabs to spaces in the specified list of files.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;

                const auto& extensions = m_extensions.getValue();
                const std::set< std::string > extensionsFilter( extensions.begin(), extensions.end() );

                const auto& ignorePathFragments = m_ignorePathFragments.getValue();

                ProcessFilesUtils::processAllFiles(
                    m_path.getValue(),
                    &ProcessFilesUtils::fileConvertTabs2Spaces,
                    ignorePathFragments,
                    extensionsFilter
                    );

                return 0;
            }
        };

        typedef ProcessFilesTabsToSpacesT<> ProcessFilesTabsToSpaces;

        /**
         * @brief class ProcessFilesSpacingTrimRight - trims whitespace from the right of each line
         */

        template
        <
            typename E = void
        >
        class ProcessFilesSpacingTrimRightT : public ProcessFilesCommandBase
        {
        protected:

            using ProcessFilesCommandBase::m_path;
            using ProcessFilesCommandBase::m_extensions;
            using ProcessFilesCommandBase::m_ignorePathFragments;

        public:

            ProcessFilesSpacingTrimRightT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                ProcessFilesCommandBase( parent, globalOptions, "spacingtrimright", "bl-tool @FULLNAME@ [options]" )
            {
                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Trims while space on the right of each line in the specified list of files.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;

                const auto& extensions = m_extensions.getValue();
                const std::set< std::string > extensionsFilter( extensions.begin(), extensions.end() );

                const auto& ignorePathFragments = m_ignorePathFragments.getValue();

                ProcessFilesUtils::processAllFiles(
                    m_path.getValue(),
                    &ProcessFilesUtils::fileTrimSpacesFromTheRight,
                    ignorePathFragments,
                    extensionsFilter
                    );

                return 0;
            }
        };

        typedef ProcessFilesSpacingTrimRightT<> ProcessFilesSpacingTrimRight;

        /**
         * @brief class ProcessFilesUpdateHeaderComment - inserts or updates the header comment for a file
         */

        template
        <
            typename E = void
        >
        class ProcessFilesUpdateHeaderCommentT : public ProcessFilesCommandBase
        {
        protected:

            using ProcessFilesCommandBase::m_path;
            using ProcessFilesCommandBase::m_extensions;
            using ProcessFilesCommandBase::m_ignorePathFragments;

            BL_CMDLINE_OPTION(
                m_headerCommentPath,
                StringOption,
                "headercommentpath",
                "The file path for the header comment to be used"
                )

        public:

            ProcessFilesUpdateHeaderCommentT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                ProcessFilesCommandBase( parent, globalOptions, "updateheadercomment", "bl-tool @FULLNAME@ [options]" )
            {
                addOption( m_headerCommentPath );

                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Updates or inserts the standard header comment in the specified list of files.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;

                const auto headerText =
                    m_headerCommentPath.hasValue()
                    ?
                    encoding::readTextFile( m_headerCommentPath.getValue() )
                    :
                    std::string();

                const auto& extensions = m_extensions.getValue();
                const std::set< std::string > extensionsFilter( extensions.begin(), extensions.end() );

                const auto& ignorePathFragments = m_ignorePathFragments.getValue();

                ProcessFilesUtils::processAllFiles(
                    m_path.getValue(),
                    cpp::bind(
                        &ProcessFilesUtils::fileUpdateFileHeaderComment,
                        _1,
                        cpp::cref( headerText )
                        ),
                    ignorePathFragments,
                    extensionsFilter
                    );

                return 0;
            }
        };

        typedef ProcessFilesUpdateHeaderCommentT<> ProcessFilesUpdateHeaderComment;

        /**
         * @brief class ProcessFilesRemoveEmptyComments - deletes empty comments in a file
         */

        template
        <
            typename E = void
        >
        class ProcessFilesRemoveEmptyCommentsT : public ProcessFilesCommandBase
        {
        protected:

            using ProcessFilesCommandBase::m_path;
            using ProcessFilesCommandBase::m_extensions;
            using ProcessFilesCommandBase::m_ignorePathFragments;

        public:

            ProcessFilesRemoveEmptyCommentsT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                ProcessFilesCommandBase( parent, globalOptions, "removeemptycomments", "bl-tool @FULLNAME@ [options]" )
            {
                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Remove empty comments in the specified list of files.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;

                const auto& extensions = m_extensions.getValue();
                const std::set< std::string > extensionsFilter( extensions.begin(), extensions.end() );

                const auto& ignorePathFragments = m_ignorePathFragments.getValue();

                std::uint64_t filesCount = 0U;

                ProcessFilesUtils::processAllFiles(
                    m_path.getValue(),
                    cpp::bind(
                        &ProcessFilesUtils::fileRemoveEmptyComments,
                        _1,
                        cpp::ref( filesCount )
                        ),
                    ignorePathFragments,
                    extensionsFilter
                    );

                BL_LOG(
                    Logging::notify(),
                    BL_MSG()
                        << "The # of files with empty comments is "
                        << filesCount
                    );

                return 0;
            }
        };

        typedef ProcessFilesRemoveEmptyCommentsT<> ProcessFilesRemoveEmptyComments;

        /**
         * @brief class ProcessFilesRemoveCommentsWithMarkers - deletes comments with ore or more markers in a file
         */

        template
        <
            typename E = void
        >
        class ProcessFilesRemoveCommentsWithMarkersT : public ProcessFilesCommandBase
        {
        protected:

            using ProcessFilesCommandBase::m_path;
            using ProcessFilesCommandBase::m_extensions;
            using ProcessFilesCommandBase::m_ignorePathFragments;

            BL_CMDLINE_OPTION(
                m_markers,
                MultiStringOption,
                "marker",
                "A comment marker for matching to use when processing a directory recursively",
                bl::cmdline::RequiredMultiValue
                )

        public:

            ProcessFilesRemoveCommentsWithMarkersT(
                SAA_inout    bl::cmdline::CommandBase*          parent,
                SAA_in       const GlobalOptions&               globalOptions
                )
                :
                ProcessFilesCommandBase( parent, globalOptions, "removecommentswithmarkers", "bl-tool @FULLNAME@ [options]" )
            {
                addOption( m_markers );

                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Remove comments which contain specific markers in the specified list of files.\n"
                    "\nUsage: @CAPTION@\n"
                    "\nOptions:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;

                const auto& extensions = m_extensions.getValue();
                const std::set< std::string > extensionsFilter( extensions.begin(), extensions.end() );

                const auto& ignorePathFragments = m_ignorePathFragments.getValue();

                const auto& markers = m_markers.getValue();

                std::uint64_t filesCount = 0U;

                ProcessFilesUtils::processAllFiles(
                    m_path.getValue(),
                    cpp::bind(
                        &ProcessFilesUtils::removeCommentsWithMarkers,
                        _1,
                        cpp::cref( markers ),
                        cpp::ref( filesCount )
                        ),
                    ignorePathFragments,
                    extensionsFilter
                    );

                BL_LOG(
                    Logging::notify(),
                    BL_MSG()
                        << "The # of files with comments with markers is "
                        << filesCount
                    );

                return 0;
            }
        };

        typedef ProcessFilesRemoveCommentsWithMarkersT<> ProcessFilesRemoveCommentsWithMarkers;

    } // commands

} // bltool

#endif /* __BL_APPS_BL_TOOL_COMMANDS_PROCESSFILESCOMMANDS_H_ */
