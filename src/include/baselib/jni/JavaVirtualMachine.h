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

 #include <jni.h>

namespace bl
{
    namespace jni
    {
        template
        <
            typename E = void
        >
        class JavaVirtualMachineT : public bl::om::ObjectDefaultBase
        {
        protected:

            JavaVirtualMachineT()
            :
            JavaVirtualMachineT( getJvmPathFromJavaHome() )
            {
            }

            JavaVirtualMachineT( SAA_in const std::string& jvmLibraryPath )
            :
            m_jvmLibrary( bl::os::loadLibrary( jvmLibraryPath ) ),
            m_javaVM( nullptr )
            {
                createJvm();
            }

            ~JavaVirtualMachineT()
            {
                BL_LOG(
                    bl::Logging::debug(),
                    "Destroying JVM"
                    );

                jint jniReturn = m_javaVM->DestroyJavaVM();

                if( jniReturn != JNI_OK )
                {
                    BL_LOG(
                        bl::Logging::warning(),
                        BL_MSG()
                            << "Failed to destroy JVM. Error code "
                            << jniReturn
                        );
                }
            }

        private:

            static cpp::ScalarTypeIniter< bool >                        g_javaVMCreated;
            static os::mutex                                            g_lock;

            const bl::os::library_ref                                   m_jvmLibrary;
            JavaVM*                                                     m_javaVM;

            std::string getJvmPathFromJavaHome()
            {
                const auto javaHome = bl::os::tryGetEnvironmentVariable( "JAVA_HOME" );

                BL_CHK_USER_FRIENDLY(
                    true,
                    ! javaHome.get(),
                    "Environment variable JAVA_HOME is not defined"
                    );

                bl::fs::path jvmPath( *javaHome );

                if( bl::os::onWindows() )
                {
                    jvmPath += "/jre/bin/server/jvm.dll";
                }
                else
                {
                    jvmPath += "/jre/lib/amd64/server/libjvm.so";
                }

                jvmPath = bl::fs::normalize( jvmPath );

                BL_CHK_USER_FRIENDLY(
                    false,
                    bl::fs::exists( jvmPath ),
                    BL_MSG()
                        << "Path doesn't exist "
                        << bl::fs::normalizePathParameterForPrint( jvmPath )
                    );

                if( bl::os::onWindows() )
                {
                    /*
                     * Remove "\\?\" prefix from JavaVM path on Windows.
                     * Otherwise after the JVM library is loaded the call to JNI_CreateJavaVM will fail with:
                     *     Error occurred during initialization of VM
                     *     java/lang/NoClassDefFoundError: java/lang/Object
                     */

                    return bl::fs::detail::WinLfnUtils::chk2RemovePrefix( std::move( jvmPath ) ).string();
                }
                else
                {
                    return jvmPath.string();
                }
            }

            void createJvm()
            {
                BL_MUTEX_GUARD( g_lock );

                /*
                 * Creation of multiple VMs in a single process is not supported.
                 * http://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/invocation.html#JNI_CreateJavaVM
                 */

                BL_CHK_USER_FRIENDLY(
                    true,
                    g_javaVMCreated,
                    "JavaVM has already been created"
                    );

                const auto procAddress = bl::os::getProcAddress( m_jvmLibrary, "JNI_CreateJavaVM" );

                const auto jniCreateJavaVM = reinterpret_cast< jint ( JNICALL* )( JavaVM**, void**, void *) >( procAddress );

                JavaVMInitArgs vmArgs;
                vmArgs.version = JNI_VERSION_1_8;
                vmArgs.nOptions = 0;
                vmArgs.options = nullptr;
                vmArgs.ignoreUnrecognized = JNI_FALSE;

                BL_LOG(
                    bl::Logging::debug(),
                    "Creating JVM"
                    );

                JNIEnv *jniEnv;

                jint jniReturn = jniCreateJavaVM(
                    &m_javaVM,
                    ( void** )&jniEnv,
                    &vmArgs
                    );

                BL_CHK_USER_FRIENDLY(
                    true,
                    jniReturn != JNI_OK,
                    BL_MSG()
                        << "Failed to create JVM. Error code "
                        << jniReturn
                    );

                g_javaVMCreated = true;
            }
        };

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, cpp::ScalarTypeIniter< bool >,    g_javaVMCreated );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, os::mutex,                        g_lock );

        typedef bl::om::ObjectImpl< JavaVirtualMachineT<> > JavaVirtualMachine;

    } // jni

} // bl
