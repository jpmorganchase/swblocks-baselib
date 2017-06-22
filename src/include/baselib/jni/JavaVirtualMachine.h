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

#ifndef __BL_JNI_JAVAVIRTUALMACHINE_H_
#define __BL_JNI_JAVAVIRTUALMACHINE_H_

#include <jni.h>
#include <baselib/jni/JniEnvironment.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        std::string jniErrorMessage( SAA_in const jint jniErrorCode )
        {
            /*
             * Possible return values for JNI functions from jni.h
             */

            switch( jniErrorCode )
            {
                case JNI_OK:        return "success";
                case JNI_ERR:       return "unknown error";
                case JNI_EDETACHED: return "thread detached from the VM";
                case JNI_EVERSION:  return "JNI version error";
                case JNI_ENOMEM:    return "not enough memory";
                case JNI_EEXIST:    return "VM already created";
                case JNI_EINVAL:    return "invalid arguments";
                default:            return "invalid JNI error code";
            }
        }

        /**
         * @brief class JavaVirtualMachine - representation of Java Virtual Machine (JavaVM).
         * Only one JavaVM can be created in a process, and after JavaVM has been destroyed, another JavaVM can't be created.
         * Also the jvm library itself (jvm.dll, libjvm.so) can't be unloaded after JavaVM has been created and destroyed.
         */

        template
        <
            typename E = void
        >
        class JavaVirtualMachineT FINAL
        {
            BL_NO_COPY_OR_MOVE( JavaVirtualMachineT )

        private:

            typedef cpp::SafeUniquePtr< JavaVirtualMachineT >           JavaVirtualMachinePtr;

            static JavaVirtualMachinePtr                                g_instance;

            static cpp::ScalarTypeIniter< bool >                        g_javaVMCreated;
            static os::mutex                                            g_lock;

            const os::library_ref                                       m_jvmLibrary;
            JavaVM*                                                     m_javaVM;

            JavaVirtualMachineT()
                :
                JavaVirtualMachineT( getJvmPathFromJavaHome() )
            {
            }

            JavaVirtualMachineT( SAA_in const std::string& jvmLibraryPath )
                :
                m_jvmLibrary( os::loadLibrary( jvmLibraryPath ) ),
                m_javaVM( nullptr )
            {
                const auto procAddress = os::getProcAddress( m_jvmLibrary, "JNI_CreateJavaVM" );

                const auto jniCreateJavaVM = reinterpret_cast< jint ( JNICALL* )( JavaVM**, void**, void *) >( procAddress );

                JavaVMInitArgs vmArgs = {};
                vmArgs.version = JNI_VERSION_1_8;
                vmArgs.nOptions = 0;
                vmArgs.options = nullptr;
                vmArgs.ignoreUnrecognized = JNI_FALSE;

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Creating JVM"
                    );

                JNIEnv* jniEnv;
                JavaVM* javaVM;

                const jint jniErrorCode = jniCreateJavaVM(
                    &javaVM,
                    ( void** )&jniEnv,
                    &vmArgs
                    );

                BL_CHK_T(
                    false,
                    jniErrorCode == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to create JVM. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );

                m_javaVM = javaVM;

                g_javaVMCreated = true;

                /*
                 * Detach the thread that created JavaVM so that JavaVM can be destroyed from any thread.
                 */

                detachCurrentThread();
            }

            JniEnvPtr attachCurrentThread() const
            {
                JNIEnv* jniEnv;

                const jint jniErrorCode = m_javaVM -> AttachCurrentThread(
                    reinterpret_cast< void** >( &jniEnv ),
                    nullptr /* thread_args */
                    );

                BL_CHK_T(
                    false,
                    jniErrorCode == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to attach current thread to JVM. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );

                return JniEnvPtr::attach( jniEnv );
            }

            void detachCurrentThread() const
            {
                const jint jniErrorCode = m_javaVM -> DetachCurrentThread();

                BL_CHK_T(
                    false,
                    jniErrorCode == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to detach current thread from JVM. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );
            }

            static std::string getJvmPathFromJavaHome()
            {
                const auto javaHome = os::tryGetEnvironmentVariable( "JAVA_HOME" );

                BL_CHK_T_USER_FRIENDLY(
                    true,
                    ! javaHome.get(),
                    JavaException(),
                    "Environment variable JAVA_HOME is not defined"
                    );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "JAVA_HOME = '"
                        << *javaHome
                        << "'"
                    );

                fs::path jvmPath( *javaHome );

                if( os::onWindows() )
                {
                    jvmPath += "/jre/bin/server/jvm.dll";
                }
                else
                {
                    jvmPath += "/jre/lib/amd64/server/libjvm.so";
                }

                jvmPath = fs::normalize( jvmPath );

                BL_CHK_T_USER_FRIENDLY(
                    false,
                    fs::exists( jvmPath ),
                    JavaException(),
                    BL_MSG()
                        << "Path doesn't exist "
                        << fs::normalizePathParameterForPrint( jvmPath )
                    );

                if( os::onWindows() )
                {
                    /*
                     * Remove "\\?\" prefix from JavaVM path on Windows.
                     * Otherwise after the JVM library is loaded the call to JNI_CreateJavaVM will fail with:
                     *     Error occurred during initialization of VM
                     *     java/lang/NoClassDefFoundError: java/lang/Object
                     */

                    return fs::detail::WinLfnUtils::chk2RemovePrefix( std::move( jvmPath ) ).string();
                }
                else
                {
                    return jvmPath.string();
                }
            }

        public:

            ~JavaVirtualMachineT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Destroying JVM"
                    );

                const jint jniErrorCode = m_javaVM -> DestroyJavaVM();

                BL_CHK_T(
                    false,
                    jniErrorCode == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to destroy JVM. ErrorCode "
                        << jniErrorCode
                        << " ["
                        << jniErrorMessage( jniErrorCode )
                        << "]"
                    );

                BL_NOEXCEPT_END()
            }

            static void create()
            {
                BL_MUTEX_GUARD( g_lock );

                /*
                 * Creation of multiple VMs in a single process is not supported.
                 * http://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/invocation.html#JNI_CreateJavaVM
                 */

                BL_CHK_T(
                    true,
                    g_javaVMCreated,
                    JavaException(),
                    "JavaVM has already been created"
                    );

                g_instance = JavaVirtualMachinePtr::attach( new JavaVirtualMachineT() );
            }

            static void destroy()
            {
                BL_MUTEX_GUARD( g_lock );

                BL_ASSERT( g_instance );

                JniEnvironment::destroy();

                g_instance.reset();
            }

            static JniEnvironmentT< E >& createJniEnv()
            {
                if( ! JniEnvironment::exists() )
                {
                    BL_ASSERT( g_instance );
                    return JniEnvironment::create( g_instance -> attachCurrentThread() );
                }

                return getJniEnv();
            }

            static JniEnvironmentT< E >& getJniEnv() NOEXCEPT
            {
                return JniEnvironment::get();
            }

            static void deleteJniEnv()
            {
                JniEnvironment::destroy();
            }

            static void detachThread()
            {
                BL_ASSERT( g_instance );
                g_instance -> detachCurrentThread();
            }
        };

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, cpp::ScalarTypeIniter< bool >,    g_javaVMCreated );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, os::mutex,                        g_lock );

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT,
            typename JavaVirtualMachineT< TCLASS >::JavaVirtualMachinePtr,              g_instance );

        typedef JavaVirtualMachineT<> JavaVirtualMachine;

    } // jni

} // bl

#endif /* __BL_JNI_JAVAVIRTUALMACHINE_H_ */
