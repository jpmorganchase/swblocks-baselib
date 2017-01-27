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

#ifndef __UTEST_UTFPLUGINFIXTURE_H_
#define __UTEST_UTFPLUGINFIXTURE_H_

#include <baselib/core/FsUtils.h>
#include <baselib/core/EcUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/UtfDirectoryFixture.h>
#include <utests/baselib/TestUtils.h>
#include <utests/baselib/Utf.h>

namespace utest
{
    /**
     * @brief class TestPluginT
     */
    template
    <
        typename E = void
    >
    class TestPluginT
    {
    private:

        static const std::string g_libExt;
        static const std::string g_objName;

        bl::fs::path m_libFile;

    public:

        TestPluginT()
        {
            const auto libName = "utf-baselib-plugin" + g_libExt;

            const auto currentExecutablePath =
                bl::fs::path( bl::fs::path( bl::os::getCurrentExecutablePath() ).parent_path() );

            auto libFile = currentExecutablePath / libName;

            if( ! bl::fs::path_exists( libFile ) )
            {
                libFile = currentExecutablePath.parent_path() / "utf_baselib_plugin" / libName;

                UTF_REQUIRE( bl::fs::path_exists( libFile ) );
            }

            m_libFile = std::move( libFile );
        }

        std::string path() const
        {
            return m_libFile.string();
        }

        ~TestPluginT() NOEXCEPT
        {
            /*
             * Clear the global loader state at the end of the fixture to allow
             * the unit tests to reuse the same plug-in library without an
             * exception being triggered due to duplicate class ids being
             * registered.
             */

            BL_SCOPE_EXIT_WARN_ON_FAILURE(
                {
                    bl::om::detail::GlobalInit::getLoader() -> reset();
                },
                "TestPluginT::~TestPluginT" /* location */
                );
        }

    };

#if defined( _WIN32 )
    template
    <
        typename E
    >
    const std::string TestPluginT< E >::g_libExt = ".dll";

#else
    template
    <
        typename E
    >
    const std::string TestPluginT< E >::g_libExt = ".so";

#endif

    typedef TestPluginT<> TestPlugin;

} // utest

#endif /* __UTEST_UTFPLUGINFIXTURE_H_ */
