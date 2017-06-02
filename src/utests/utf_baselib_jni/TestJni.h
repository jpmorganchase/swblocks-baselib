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

bl::om::ObjPtr< bl::jni::JavaVirtualMachine > g_javaVM;

class JniTestGlobalFixture
{
public:

    JniTestGlobalFixture()
    {
        g_javaVM = bl::jni::JavaVirtualMachine::createInstance();
    }

    ~JniTestGlobalFixture()
    {
        g_javaVM.reset();
    }
};

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
