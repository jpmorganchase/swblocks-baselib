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

#ifndef __UTEST_TESTUTILS_H_
#define __UTEST_TESTUTILS_H_

#ifndef BL_PRECOMPILED_ENABLED
#include <baselib/core/FileEncoding.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/OS.h>
#include <baselib/core/ErrorHandling.h>
#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>
#endif

namespace utest
{
    /**
     * class TestUtilsT
     */
    template
    <
        typename E = void
    >
    class TestUtilsT
    {
    private:

        static std::string tempDir( SAA_in_opt const bool ignoreEnvironment = false )
        {
            if( ! ignoreEnvironment )
            {
                const auto temp = bl::os::tryGetEnvironmentVariable( "TEMP" );

                if( temp )
                {
                    return *temp;
                }
            }

            return bl::os::onWindows() ? "C:\\Temp" : "/var/tmp";
        }

    public:

        static std::string uniqueName()
        {
            const auto uuid = bl::uuids::create();
            return bl::uuids::uuid2string( uuid );
        }

        static std::string testDir( SAA_in const std::string& dirName, SAA_in_opt const bool ignoreEnvironment = false )
        {
            auto tmp = bl::fs::nolfn::path( tempDir( ignoreEnvironment ) );

            tmp.make_preferred();

            return ( tmp / dirName ).string();
        }

        static std::string uniqueTestDir( SAA_in_opt const bool ignoreEnvironment = false )
        {
            return testDir( uniqueName(), ignoreEnvironment );
        }

        static void measureRuntime(
            SAA_in          const std::string&                  message,
            SAA_in          const bl::cpp::void_callback_t&     action
            )
        {
            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "START : "
                    << message
                );

            const auto startTime = bl::time::microsec_clock::universal_time();

            action();

            const auto duration = bl::time::microsec_clock::universal_time() - startTime;
            const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "DONE in "
                    << durationInSeconds
                    << " sec : "
                    << message
                );
        }

        static bl::fs::path resolveDataFilePath( SAA_in const bl::fs::path& fileName )
        {
            const auto path = bl::fs::path( bl::os::getCurrentExecutablePath() );

            /*
             * The assumption is that during build data files are copied
             * in sub-directory in the same path where executable is with name:
             * <exe name w/o extension>-data
             */

            return ( path.parent_path() / ( path.stem().string() + "-data" ) / fileName );
        }

        static std::string loadDataFile(
            SAA_in          const bl::fs::path&                 fileName,
            SAA_in_opt      const bool                          normalizeLineBreaks = false
            )
        {
            auto text = bl::encoding::readTextFile( resolveDataFilePath( fileName ) );

            if( normalizeLineBreaks )
            {
                bl::str::replace_all( text, "\r\n", "\n" );
            }

            return text;
        }

        static bool logExceptionDetails( SAA_in const std::exception& ex )
        {
            BL_LOG_MULTILINE(
                bl::Logging::debug(),
                BL_MSG()
                    << "Exception details:\n"
                    << bl::eh::diagnostic_information( ex )
                );

            return true;
        }
    };

    typedef TestUtilsT<> TestUtils;

} // utest

#endif /* __UTEST_TESTUTILS_H_ */

