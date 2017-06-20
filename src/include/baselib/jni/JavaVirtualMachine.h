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
        class JavaVirtualMachineT : public std::enable_shared_from_this< JavaVirtualMachineT< E > >
        {
            typedef std::enable_shared_from_this< JavaVirtualMachineT< E > >    base_type;

        public:

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
                createJvm();
            }

            ~JavaVirtualMachineT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                BL_ASSERT( g_jniEnv == nullptr );
                BL_ASSERT( g_jniEnvCount == 0 );

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

                g_javaVMDestroyed = true;

                BL_NOEXCEPT_END()
            }

            static std::shared_ptr< JavaVirtualMachineT > createInstance()
            {
                return std::make_shared< JavaVirtualMachineT >();
            }

            static bool javaVMCreated() NOEXCEPT
            {
                return g_javaVMCreated;
            }

            static bool javaVMDestroyed() NOEXCEPT
            {
                return g_javaVMDestroyed;
            }

            om::ObjPtr< JniEnvironment > getJniEnv()
            {
                const auto javaVM = base_type::shared_from_this();

                if( g_jniEnvCount == 0 )
                {
                    BL_ASSERT( g_jniEnv == nullptr );
                    g_jniEnv = attachCurrentThread();
                }

                ++g_jniEnvCount;

                return JniEnvironment::createInstance( JniEnvPtr::attach( g_jniEnv, std::move( javaVM ) ) );
            }

            void detachCurrentThread() const
            {
                if( g_jniEnvCount == 1 )
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

                        g_jniEnv = nullptr;
                        jniThreadDetached();
                }

                --g_jniEnvCount;
            }

            static std::int64_t getCurrentAttachedThreadCount() NOEXCEPT
            {
                return g_currentAttachedThreadCount;
            }

            static std::int64_t getTotalAttachedThreadCount() NOEXCEPT
            {
                return g_totalAttachedThreadCount;
            }

        private:

            static std::atomic< bool >                                  g_javaVMCreated;
            static std::atomic< bool >                                  g_javaVMDestroyed;
            static os::mutex                                            g_lock;

            static std::atomic< uint64_t >                              g_currentAttachedThreadCount;
            static std::atomic< uint64_t >                              g_totalAttachedThreadCount;

            thread_local static uint64_t                                g_jniEnvCount;
            thread_local static JNIEnv*                                 g_jniEnv;

            const os::library_ref                                       m_jvmLibrary;
            JavaVM*                                                     m_javaVM;

            void jniThreadAttached() const NOEXCEPT
            {
                ++g_currentAttachedThreadCount;
                ++g_totalAttachedThreadCount;
            }

            void jniThreadDetached() const NOEXCEPT
            {
                --g_currentAttachedThreadCount;
            }

            JNIEnv* attachCurrentThread() const
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

                jniThreadAttached();

                return jniEnv;
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

            void createJvm()
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

                g_jniEnv = jniEnv;
                g_jniEnvCount = 1;
                g_javaVMCreated = true;

                jniThreadAttached();

                /*
                 * Detach the thread that created JavaVM so that JavaVM can be destroyed from any thread.
                 */

                detachCurrentThread();
            }

        };

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, std::atomic< bool >,              g_javaVMCreated );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, std::atomic< bool >,              g_javaVMDestroyed );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, os::mutex,                        g_lock );

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, std::atomic< uint64_t >,          g_currentAttachedThreadCount );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, std::atomic< uint64_t >,          g_totalAttachedThreadCount );

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, thread_local uint64_t,            g_jniEnvCount )         = 0;
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, thread_local JNIEnv*,             g_jniEnv )              = nullptr;


        typedef JavaVirtualMachineT<>                   JavaVirtualMachine;
        typedef std::shared_ptr< JavaVirtualMachine >   JavaVirtualMachinePtr;

    } // jni

} // bl

#endif /* __BL_JNI_JAVAVIRTUALMACHINE_H_ */
