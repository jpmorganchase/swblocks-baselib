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

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

namespace
{
    class JniTestGlobalFixture
    {
    public:

        JniTestGlobalFixture()
        {
            bl::jni::JavaVirtualMachine::create();
        }

        ~JniTestGlobalFixture() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            bl::jni::JavaVirtualMachine::destroy();

            BL_NOEXCEPT_END()
        }
    };

} // __unnamed

UTF_GLOBAL_FIXTURE( JniTestGlobalFixture )

UTF_AUTO_TEST_CASE( Jni_CreateSecondJavaVM )
{
    /*
     * Test that an exception is thrown when attempting to create a JVM the second time.
     */

    UTF_CHECK_THROW_MESSAGE(
        bl::jni::JavaVirtualMachine::create(),
        bl::JavaException,
        "JavaVM has already been created"
        );
}

UTF_AUTO_TEST_CASE( Jni_CreateJniEnvironments )
{
    using namespace bl;
    using namespace bl::jni;

    const auto createJniEnvironment = []( bool deleteJniEnvAfterTest )
    {
        UTF_REQUIRE( ! JniEnvironment::exists() );

        const auto& jniEnv = JavaVirtualMachine::createJniEnv();
        UTF_REQUIRE_EQUAL( jniEnv.getVersion(), JNI_VERSION_1_8 );

        const auto& jniEnv2 = JavaVirtualMachine::getJniEnv();
        UTF_REQUIRE( &jniEnv== &jniEnv2 );

        if( deleteJniEnvAfterTest )
        {
            JavaVirtualMachine::deleteJniEnv();
            UTF_REQUIRE( ! JniEnvironment::exists() );
        }
        else
        {
            UTF_REQUIRE( JniEnvironment::exists() );
        }
    };

    createJniEnvironment( true /* deleteJniEnvAfterTest */ );

    const int numThreads = 11;

    os::thread threads[ numThreads ];

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i] = os::thread( createJniEnvironment, i % 2 == 0 /* deleteJniEnvAfterTest */ );
    }

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i].join();
    }
}
