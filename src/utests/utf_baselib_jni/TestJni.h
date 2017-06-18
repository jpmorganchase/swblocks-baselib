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

bl::jni::JavaVirtualMachinePtr g_javaVM;

bool g_globalFixtureDestroysJavaVM = true;

namespace
{
    using namespace bl;
    using namespace bl::jni;

    class JniTestGlobalFixture
    {
    public:

        JniTestGlobalFixture()
        {
            g_javaVM = JavaVirtualMachine::createInstance();
        }

        ~JniTestGlobalFixture() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            BL_CHK_T(
                false,
                JavaVirtualMachine::getCurrentAttachedThreadCount() == 0,
                JavaException(),
                BL_MSG()
                    << "All threads should be detached from JavaVM by the end of the tests"
                );

            if( g_globalFixtureDestroysJavaVM )
            {
                BL_CHK_T(
                    true,
                    JavaVirtualMachine::javaVMDestroyed(),
                    JavaException(),
                    BL_MSG()
                        << "The main thread has to destroy JavaVM, but it's already destroyed"
                    );

                g_javaVM.reset();
            }

            BL_CHK_T(
                false,
                JavaVirtualMachine::javaVMDestroyed(),
                JavaException(),
                BL_MSG()
                    << "JavaVM should have been destroyed by the end of the tests"
                );

            BL_NOEXCEPT_END()
        }
    };

} // __unnamed

UTF_GLOBAL_FIXTURE( JniTestGlobalFixture )

UTF_AUTO_TEST_CASE( Jni_CreateJavaVM )
{
    /*
     * Test that JVM was created by JniTestGlobalFixture.
     */

    UTF_REQUIRE( g_javaVM.get() != nullptr );
}

UTF_AUTO_TEST_CASE( Jni_CreateSecondJavaVM )
{
    /*
     * Test that an exception is thrown when attempting to create a second JVM.
     */

    UTF_CHECK_THROW_MESSAGE(
        bl::jni::JavaVirtualMachine::createInstance(),
        bl::JavaException,
        "JavaVM has already been created"
        );
}

UTF_AUTO_TEST_CASE( Jni_CreateMultipleJniEnvironmentsInOneThread )
{
    using namespace bl::jni;

    const auto totalAttachedThreadsBeforeTest = JavaVirtualMachine::getTotalAttachedThreadCount();

    UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 0 );

    {
        /*
         * Creating multiple JniEnvironment objects in the same scope (including dependent scopes)
         * should attach the current thread to JavaVM only once.
         */

        const auto jniEnv = g_javaVM -> getJniEnv();
        UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 1 );

        const auto jniEnv2 = g_javaVM -> getJniEnv();
        UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 1 );

        {
            const auto jniEnv = g_javaVM -> getJniEnv();
            UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 1 );

            const auto jniEnv2 = g_javaVM -> getJniEnv();
            UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 1 );
        }

        UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 1 );
    }

    UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 0 );

    UTF_REQUIRE_EQUAL(
        totalAttachedThreadsBeforeTest + 1,
        JavaVirtualMachine::getTotalAttachedThreadCount()
        );
}

UTF_AUTO_TEST_CASE( Jni_CreateMultipleJniEnvironmentsInMultipleThreads )
{
    using namespace bl;
    using namespace bl::jni;

    const auto totalAttachedThreadsBeforeTest = JavaVirtualMachine::getTotalAttachedThreadCount();

    UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 0 );

    const auto createJniEnvironments = []()
    {
        const auto jniEnv = g_javaVM -> getJniEnv();
        const auto jniEnv2 = g_javaVM -> getJniEnv();

        {
            const auto jniEnv = g_javaVM -> getJniEnv();
            const auto jniEnv2 = g_javaVM -> getJniEnv();
        };
    };

    createJniEnvironments();

    const int numThreads = 11;

    os::thread threads[ numThreads ];

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i] = os::thread( createJniEnvironments );
    }

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i].join();
    }

    UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 0 );

    UTF_REQUIRE_EQUAL(
        totalAttachedThreadsBeforeTest + 1 + numThreads,
        JavaVirtualMachine::getTotalAttachedThreadCount()
        );
}

UTF_AUTO_TEST_CASE( Jni_DestroyJavaVmFromNonMainThread )
{
    /*
     * Start multiple threads, create Jni Environment in each thread, wait until the main thread
     * releases JavaVM shared pointer, and let the last exiting Jni thread to destroy the JavaVM.
     */

    using namespace bl;
    using namespace bl::jni;

    const auto totalAttachedThreadsBeforeTest = JavaVirtualMachine::getTotalAttachedThreadCount();

    UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 0 );

    int threadCount = 0;
    os::mutex lockThreadCount;
    os::condition_variable cvThreadCount;

    bool javaVmReleased = false;
    os::mutex lockJavaVmReleased;
    os::condition_variable cvJavaVmReleased;

    const int numberOfThreads = 11;

    const auto createJniEnvironment = [ & ]()
    {
        const auto jniEnv = g_javaVM -> getJniEnv();

        os::unique_lock< os::mutex > guardThreadCount( lockThreadCount );
        int readyThreadCount = ++threadCount;
        guardThreadCount.unlock();

        UTF_REQUIRE( g_javaVM.get() != nullptr )

        if( readyThreadCount == numberOfThreads )
        {
            cvThreadCount.notify_one();
        }

        os::unique_lock< os::mutex > guardJavaVmReleased( lockJavaVmReleased );
        cvJavaVmReleased.wait(
            guardJavaVmReleased,
            [ & ]
            {
                return javaVmReleased;
            });
        guardJavaVmReleased.unlock();

        UTF_REQUIRE( g_javaVM.get() == nullptr )
        UTF_REQUIRE( ! JavaVirtualMachine::javaVMDestroyed() );
    };

    os::thread threads[ numberOfThreads ];

    for( int i = 0; i < numberOfThreads; ++i )
    {
        threads[i] = os::thread( createJniEnvironment );
    }

    std::unique_lock< os::mutex > guardThreadCount( lockThreadCount );
    cvThreadCount.wait(
        guardThreadCount,
        [ & ]
        {
            return threadCount == numberOfThreads;
        });
    guardThreadCount.unlock();

    g_javaVM.reset();
    g_globalFixtureDestroysJavaVM = false;

    os::unique_lock< os::mutex > guardJavaVmReleased( lockJavaVmReleased );
    javaVmReleased = true;
    guardJavaVmReleased.unlock();
    cvJavaVmReleased.notify_all();

    for( int i = 0; i < numberOfThreads; ++i )
    {
        threads[i].join();
    }

    UTF_REQUIRE_EQUAL( JavaVirtualMachine::getCurrentAttachedThreadCount(), 0 );

    UTF_REQUIRE_EQUAL(
        totalAttachedThreadsBeforeTest + numberOfThreads,
        JavaVirtualMachine::getTotalAttachedThreadCount()
        );

    UTF_REQUIRE( JavaVirtualMachine::javaVMDestroyed() );
}

/*
 * Note: the Jni_DestroyJavaVmFromNonMainThread test case above should be
 * the last test in the file, because after JavaVM is destroyed it can't created again.
 */
