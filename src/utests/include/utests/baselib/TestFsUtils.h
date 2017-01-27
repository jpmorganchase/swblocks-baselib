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

#ifndef __UTESTS_TESTFSUTILS_H_
#define __UTESTS_TESTFSUTILS_H_

#include <baselib/core/OS.h>
#include <baselib/core/FsUtils.h>
#include <baselib/core/BaseIncludes.h>
#include <baselib/core/PathUtils.h>
#include <baselib/core/Logging.h>
#include <baselib/core/TlsState.h>
#include <baselib/core/CPP.h>

#include <utests/baselib/TestUtils.h>

#include <algorithm>
#include <random>
#include <cstddef>

namespace utest
{
    /**
     * class TestFsUtilsT
     */

    template
    <
        typename E = void
    >
    class TestFsUtilsT
    {
    private:

        typedef std::vector< bl::fs::path >                                     list_t;
        typedef std::vector< std::pair< bl::fs::path, std::uint64_t > >         fileList_t;
        typedef std::vector< std::pair< bl::fs::path, bl::fs::path > >          symlinkList_t;

        bool                m_supportLargeFiles;
        bool                m_supportLongFileNames;

        list_t              m_fixedDirTree;
        fileList_t          m_fixedFilePaths;
        symlinkList_t       m_fixedDirSymlinkPaths;
        symlinkList_t       m_fixedFileSymlinkPaths;

    public:

        TestFsUtilsT()
            :
            m_supportLargeFiles( false ),
            m_supportLongFileNames( true )
        {
        }

        void createDummyTestDir( SAA_in const bl::fs::path& root )
        {
            initFixedDirStructure();
            createFixedDir( root );
        }

        void enableLargeFileSupport( SAA_in const bool enable)
        {
            m_supportLargeFiles = enable;
        }

        void enableLongFileNames( SAA_in const bool enable )
        {
            m_supportLongFileNames = enable;
        }

        static bool compareFolders(
             SAA_in              const bl::fs::path&     left,
             SAA_in              const bl::fs::path&     right
             )
        {
            const auto printFolders = [](
                SAA_in              const bl::fs::path&     left,
                SAA_in              const bl::fs::path&     right,
                SAA_in              const bool              isSimilar
                )
            {
                const auto getFileList = []( SAA_in const bl::fs::path& path )
                    -> std::string
                {
                    bl::cpp::SafeOutputStringStream oss;
                    for( const auto& elem: path )
                    {
                        oss
                            << "["
                            << elem.string()
                            << "]\n";
                    }
                    return oss.str();
                };

                if ( ! isSimilar )
                {
                    BL_LOG(
                        bl::Logging::debug(),
                        BL_MSG()
                            << "\nFolders are different\n"
                            << "Left folder : ["
                            << left
                            << "]\nRight folder: ["
                            << right
                            << "]\nLeft folder files:\n"
                            << getFileList( left )
                            << "Right folder files:\n"
                            << getFileList( right )
                        );
                }
            };

            bool isSimilar = true;

            if( exists( left ) && exists( right ) && is_directory( left ) && is_directory( right ) )
            {
                list_t leftVec, rightVec;

                copy( bl::fs::directory_iterator( left ), bl::fs::directory_iterator(), back_inserter( leftVec ) );
                copy( bl::fs::directory_iterator( right ), bl::fs::directory_iterator(), back_inserter( rightVec ) );

                isSimilar = isSimilar && ( leftVec.size() == rightVec.size() );

                if( ! isSimilar)
                {
                    printFolders( left, right, isSimilar );
                    return isSimilar;
                }

                sort( leftVec.begin(), leftVec.end() );
                sort( rightVec.begin(), rightVec.end() );

                auto leftIter = leftVec.begin();
                auto rightIter = rightVec.begin();

                for( ; leftIter != leftVec.end() && rightIter != rightVec.end(); ++leftIter, ++rightIter )
                {
                    if( is_directory( *leftIter ) )
                    {
                        isSimilar = isSimilar &&
                            ( leftIter->filename() == rightIter->filename() ) &&
                            compareFolders( *leftIter, *rightIter );
                    }
                    else if( is_regular_file( *leftIter ) )
                    {
                        isSimilar = isSimilar && compareFileContents( *leftIter, *rightIter );
                    }
                    else if( is_symlink( *leftIter ) )
                    {
                        isSimilar = isSimilar &&
                            ( read_symlink( *leftIter ).filename() == read_symlink( *rightIter ).filename() );
                    }

                    if( ! isSimilar )
                    {
                        BL_LOG(
                            bl::Logging::debug(),
                            BL_MSG()
                                << "TestFsUtils: Comparison failed "
                                << *leftIter
                                << " "
                                << *rightIter
                            );

                        break;
                    }
                }
            }

            printFolders( left, right, isSimilar );
            return isSimilar;
        }

        static bool compareFileContents(
             SAA_in              const bl::fs::path&     leftPath,
             SAA_in              const bl::fs::path&     rightPath,
             SAA_in_opt          const bool              ignoreTimestamp = false,
             SAA_in_opt          const bool              ignoreName = false
             )
        {
            bool isSimilar = false;

            if( exists( leftPath ) && exists( rightPath ) && is_regular_file( leftPath ) && is_regular_file( rightPath ) )
            {
                isSimilar =
                    ( ignoreName || ( leftPath.filename() == rightPath.filename() ) ) &&
                    ( file_size( leftPath ) == file_size( rightPath ) ) &&
                    ( ignoreTimestamp || ( last_write_time( leftPath ) == last_write_time( rightPath ) ) );

                if( ! isSimilar )
                {
                    BL_LOG(
                        bl::Logging::debug(),
                        BL_MSG()
                            << "TestFsUtils: File Metadata Comparison failed "
                            << leftPath
                            << " "
                            << rightPath
                        );
                }

                bl::fs::SafeInputFileStreamWrapper leftFileWrapper( leftPath );
                auto& leftFile = leftFileWrapper.stream();
                leftFile.seekg( 0, std::ios::end );

                bl::fs::SafeInputFileStreamWrapper rightFileWrapper( rightPath );
                auto& rightFile = rightFileWrapper.stream();
                rightFile.seekg( 0, std::ios::end );

                if( leftFile.good() && rightFile.good() )
                {
                    const auto leftSize = leftFile.tellg();
                    const auto rightSize = rightFile.tellg();
                    isSimilar = isSimilar && ( leftSize == rightSize );

                    if( isSimilar && leftSize > 0 )
                    {
                        const auto readSize = ( std::size_t ) std::min< decltype( leftSize ) >( 1024U, leftSize );
                        const auto leftContents = bl::cpp::SafeUniquePtr< char[] >::attach( new char[ readSize ] );
                        const auto rightContents = bl::cpp::SafeUniquePtr< char[] >::attach( new char[ readSize ] );

                        leftFile.seekg( 0, std::ios::beg );
                        rightFile.seekg( 0, std::ios::beg );
                        leftFile.read( leftContents.get(), readSize );
                        rightFile.read( rightContents.get(), readSize );

                        std::size_t counter = readSize - 1;

                        while( counter && isSimilar )
                        {
                            isSimilar = isSimilar && ( leftContents.get()[ counter ] == rightContents.get()[ counter ] );
                            --counter;
                        }
                    }
                }
            }

            return isSimilar;
        }

        static void createDummyFile(
             SAA_in              const bl::fs::path&        path,
             SAA_in              const std::uint64_t        fileSize
             )
        {
            const unsigned char pattern[] = { 1, 2, 3, 4, 5 };
            const auto outfile = bl::os::fopen( path, "wb" );

            if( fileSize > 0 )
            {
                std::uint64_t pos = fileSize > BL_ARRAY_SIZE( pattern ) ? fileSize - BL_ARRAY_SIZE( pattern ) : 0U;
                const auto writeSize = ( std::size_t ) std::min< decltype( fileSize ) >( fileSize, BL_ARRAY_SIZE( pattern ) );
                bl::os::fseek( outfile, pos, SEEK_SET );
                bl::os::fwrite( outfile, pattern , writeSize );
            }
        }

    private:

        void initFixedDirStructure()
        {
            m_fixedDirTree.clear();
            m_fixedFilePaths.clear();
            m_fixedDirSymlinkPaths.clear();
            m_fixedFileSymlinkPaths.clear();
            bl::fs::path basePath( "foo" );
            bl::fs::path barPath( basePath / "bar" );
            bl::fs::path normalFilePath( barPath / "normalFile.bin" );

            m_fixedDirTree.push_back( basePath );
            m_fixedDirTree.push_back( barPath );
            m_fixedDirTree.push_back( basePath / "emptyDirectory" );
            m_fixedDirTree.push_back( barPath / " DirName with Spaces" );
            m_fixedDirTree.push_back( barPath / "DirNameWithOne Space" );
            m_fixedDirTree.push_back( barPath / "DirNameWith'!£$%^&_-@;,.(WinRestrictedSpecialCharacters)" );
            m_fixedDirTree.push_back( barPath / L"DirNameWithUnicode\u263A" );

            if( bl::os::onUNIX() )
            {
                m_fixedDirTree.push_back( barPath / "DirNameWith\"quotes\" " );
                m_fixedDirTree.push_back( barPath / "DirNameWith'!£$%^&*_-@;:<>,.?|(SpecialCharacters)" );
            }

            /*
             * TODO: UTF8 dir names
             */

            m_fixedFilePaths.push_back( std::make_pair( basePath / "zeroSizeFile.bin", 0U ) );
            m_fixedFilePaths.push_back( std::make_pair( normalFilePath, 20U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / "oneChunkFile.bin", 510U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / "chunkSizedFile.bin", 512U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / "multipleChunkFile.bin", 2U * 512U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / " FileName with Spaces.bin", 20U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / "FileNameWithOne Space.bin", 20U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / "FileNameWith'!£$%^&_-@;,.(WinRestrictedSpecialCharacters).bin", 20U * 1024U ) );
            m_fixedFilePaths.push_back( std::make_pair( barPath / L"FileNameWithUnicode\u263A.bin", 20U * 1024U ) );

            if( bl::os::onUNIX() )
            {
                m_fixedFilePaths.push_back( std::make_pair( barPath / "FileNameWith\"quotes\".bin", 20U * 1024U ) );
                m_fixedFilePaths.push_back( std::make_pair( barPath / "FileNameWith'!£$%^&*_-@;:<>,.?|(SpecialCharacters).bin", 20U * 1024U ) );
            }

            if( m_supportLargeFiles )
            {
                std::uint64_t fileSize = 0;
                fileSize += 1024U * 1024U;
                fileSize *= 1024U;
                fileSize *= 5U;
                m_fixedFilePaths.push_back( std::make_pair( barPath / "LargeFile.bin", fileSize ) );
            }

            if( bl::os::onUNIX() )
            {
                m_fixedDirSymlinkPaths.push_back( std::make_pair( barPath, basePath / "linkToBar" ) );
                m_fixedDirSymlinkPaths.push_back( std::make_pair( basePath / "emptyDirectory", basePath / "linkToEmpty" ) );
                m_fixedFileSymlinkPaths.push_back( std::make_pair( normalFilePath, basePath / "linkToNormal.bin" ) );
            }
        }

        void createFixedDir( SAA_in const bl::fs::path& path ) const
        {
            for( auto dirIter = m_fixedDirTree.begin(); dirIter != m_fixedDirTree.end(); ++dirIter )
            {
                bl::fs::safeCreateDirectory( path / * dirIter);
            }

            for( auto fileIter = m_fixedFilePaths.begin(); fileIter != m_fixedFilePaths.end(); ++fileIter )
            {
                createDummyFile( path / fileIter -> first, fileIter -> second );
            }

            if( bl::os::onWindows() && m_supportLongFileNames )
            {
                bl::fs::path longPath( path );

                for( std::size_t i = 0; i < 400; ++i )
                {
                    longPath /= "LongName";
                    bl::fs::safeCreateDirectory( longPath );
                }

                longPath /= "LongFileName.bin";
                createDummyFile( longPath, 20U * 1024U );
            }

            if( bl::os::onUNIX() )
            {
                for( auto dirSymlinkIter = m_fixedDirSymlinkPaths.begin(); dirSymlinkIter != m_fixedDirSymlinkPaths.end(); ++dirSymlinkIter )
                {
                    bl::fs::create_directory_symlink( path / dirSymlinkIter -> first, path / dirSymlinkIter -> second );
                }

                for( auto fileSymlinkIter = m_fixedFileSymlinkPaths.begin(); fileSymlinkIter != m_fixedFileSymlinkPaths.end(); ++fileSymlinkIter )
                {
                    bl::fs::create_symlink( path / fileSymlinkIter -> first, path / fileSymlinkIter -> second );
                }
            }
        }
    };

    typedef TestFsUtilsT<> TestFsUtils;

} // utest

#endif /* __UTESTS_TESTFSUTILS_H_ */
