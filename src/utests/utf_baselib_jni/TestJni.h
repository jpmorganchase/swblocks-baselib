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

#include <baselib/jni/JavaVirtualMachine.h>
#include <baselib/jni/JniEnvironment.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

namespace
{
    using namespace bl::jni;

    class JniTestGlobalFixture
    {
    public:

        JniTestGlobalFixture()
        {
            ( void ) JavaVirtualMachine::instance();
        }

        ~JniTestGlobalFixture() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            JavaVirtualMachine::destroy();

            BL_NOEXCEPT_END()
        }
    };

} // __unnamed

UTF_GLOBAL_FIXTURE( JniTestGlobalFixture )

UTF_AUTO_TEST_CASE( Jni_CreateJniEnvironments )
{
    using namespace bl;
    using namespace bl::jni;

    const auto createJniEnvironment = []( SAA_in const bool detachJniEnvAfterTest )
    {
        const auto& jniEnv = JniEnvironment::instance();
        UTF_REQUIRE_EQUAL( jniEnv.getVersion(), JNI_VERSION_1_8 );

        if( detachJniEnvAfterTest )
        {
            JniEnvironment::detach();
        }
    };

    createJniEnvironment( false /* detachJniEnvAfterTest */ );

    const int numThreads = 10;

    os::thread threads[ numThreads ];

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i] = os::thread( createJniEnvironment, i % 2 == 0 /* detachJniEnvAfterTest */ );
    }

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i].join();
    }
}
