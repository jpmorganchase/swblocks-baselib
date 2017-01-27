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

#ifndef __UTEST_UTFDIRECTORYFIXTURE_H_
#define __UTEST_UTFDIRECTORYFIXTURE_H_

#include <utests/baselib/TestUtils.h>
#include <baselib/core/OS.h>
#include <baselib/core/Logging.h>
#include <baselib/core/FsUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace utest
{
    template
    <
        typename E = void
    >
    class TestDirectoryT
    {
    private:

        const std::string                           m_dirPath;
        const bl::fs::path                          m_dir;
        const bl::fs::nolfn::path                   m_nolfnDir;

        const bool                                  m_ignoreErrors;

    public:

        TestDirectoryT(
            SAA_in_opt const bool ignoreErrors = false,
            SAA_in_opt const bool ignoreEnvironment = false
            )
            :
            m_dirPath( utest::TestUtils::uniqueTestDir( ignoreEnvironment ) ),
            m_dir( m_dirPath ),
            m_nolfnDir( m_dirPath ),
            m_ignoreErrors( ignoreErrors )
        {
            bl::fs::safeMkdirs( m_dir );
        }

        const bl::fs::path& path() const NOEXCEPT
        {
            return m_dir;
        }

        const bl::fs::nolfn::path& nolfnPath() const NOEXCEPT
        {
            return m_nolfnDir;
        }

        bl::fs::path testFile( SAA_in const std::string& filename ) const
        {
            return m_dir / filename;
        }

        ~TestDirectoryT() NOEXCEPT
        {
            const auto ec = bl::fs::trySafeRemoveAll( m_dir );

            if( ec && ! m_ignoreErrors )
            {
                /*
                 * Note: plug-in libraries that are loaded into the current process
                 * during the tests cannot be safely unloaded, so they cannot be deleted
                 * until after the process has terminated.
                 *
                 * We still attempt to delete the test directory in order to clean-up any
                 * files that remain from previous test runs, but we (optionally) tolerate
                 * failure to delete libraries that were loaded during this test run.
                 */

                BL_LOG(
                    bl::Logging::warning(),
                    BL_MSG()
                        << "Failed to remove test directory '"
                        << m_dir.string()
                        << "'; Error code: "
                        << ec.value()
                        << "; Message: "
                        << ec.message()
                    );
            }
        }
    };

    typedef TestDirectoryT<> TestDirectory;

} // utest

#endif /* __UTEST_UTFDIRECTORYFIXTURE_H_ */
