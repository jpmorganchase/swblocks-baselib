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

#include <baselib/jni/JavaVirtualMachineConfig.h>
#include <baselib/jni/JniEnvironment.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        /**
         * @brief class JavaVirtualMachine - representation of Java Virtual Machine (JavaVM).
         * Only one JavaVM can be created in a process, and after JavaVM has been destroyed, another JavaVM can't be created.
         * Also the JVM library itself (jvm.dll, libjvm.so) can't be unloaded after JavaVM has been created and destroyed.
         */

        template
        <
            typename E = void
        >
        class JavaVirtualMachineT FINAL
        {
            BL_NO_COPY_OR_MOVE( JavaVirtualMachineT )

        private:

            typedef JavaVirtualMachineT< E >                            this_type;

            static this_type*                                           g_instance;

            static cpp::ScalarTypeIniter< bool >                        g_javaVMDestroyed;
            static os::mutex                                            g_lock;
            static JavaVirtualMachineConfig                             g_jvmConfig;

            os::library_ref                                             m_jvmLibrary;
            JavaVM*                                                     m_javaVM;

            JavaVirtualMachineT()
                :
                m_javaVM( nullptr )
            {
                const auto& jvmLibraryPath = ! g_jvmConfig.getLibraryPath().empty()
                    ? g_jvmConfig.getLibraryPath()
                    : getJvmPathFromJavaHome();

                m_jvmLibrary = os::loadLibrary( jvmLibraryPath );

                const auto procAddress = os::getProcAddress( m_jvmLibrary, "JNI_CreateJavaVM" );

                const auto jniCreateJavaVM =
                    reinterpret_cast< jint ( JNICALL* )( JavaVM**, void**, void *) >( procAddress );

                JavaVMInitArgs vmArgs;
                std::memset( &vmArgs, 0, sizeof( vmArgs ) );
                vmArgs.version = JNI_VERSION_1_8;
                vmArgs.ignoreUnrecognized = JNI_FALSE;

                const auto options = g_jvmConfig.getJavaVMOptions();

                const auto optionsSize = options.size();

                const auto javaVMOptions = cpp::SafeUniquePtr< JavaVMOption[] >::attach(
                    new ::JavaVMOption[ optionsSize ]
                    );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Creating JVM with "
                        << optionsSize
                        << " options: "
                    );

                for( std::size_t i = 0; i < optionsSize; ++i )
                {
                    javaVMOptions[i].optionString = const_cast< char* >( options[ i ].c_str() );

                    BL_LOG(
                        Logging::debug(),
                        javaVMOptions[i].optionString
                        );
                }

                vmArgs.nOptions = static_cast< jint >( optionsSize );
                vmArgs.options = javaVMOptions.get();

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

                /*
                 * Detach the thread that created JavaVM so that JavaVM can be destroyed from any thread.
                 */

                BL_CHK_T(
                    false,
                    m_javaVM -> DetachCurrentThread() == JNI_OK,
                    JavaException(),
                    BL_MSG()
                        << "Failed to detach jni thread from JVM. ErrorCode "
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
                    if( os::on32BitPlatform() )
                    {
                        jvmPath += "/jre/lib/i386/server/libjvm.so";
                    }
                    else
                    {
                        jvmPath += "/jre/lib/amd64/server/libjvm.so";
                    }
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

        public:

            static void setConfig( SAA_in JavaVirtualMachineConfig&& jvmConfig )
            {
                g_jvmConfig = BL_PARAM_FWD( jvmConfig );
            }

            static auto getConfig() NOEXCEPT -> const JavaVirtualMachineConfig&
            {
                return g_jvmConfig;
            }

            static auto instance() -> this_type&
            {
                BL_MUTEX_GUARD( g_lock );

                if( ! g_instance )
                {
                    BL_CHK_T(
                        true,
                        g_javaVMDestroyed,
                        JavaException(),
                        "JavaVM has already been destroyed"
                        );

                    g_instance = new JavaVirtualMachineT();

                    JniEnvironment::setJavaVM( g_instance -> getJavaVM() );
                }

                BL_ASSERT( g_instance );

                return *g_instance;
            }

            static void destroy() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                JniEnvironment::detach();

                const auto jniThreadCount = JniEnvironment::getJniThreadCount();

                BL_CHK_T(
                    false,
                    jniThreadCount == 0,
                    JavaException(),
                    BL_MSG()
                        << "Can't destroy JavaVM because not all jni threads were detached from it. Number of attached threads: "
                        << jniThreadCount
                    );

                BL_MUTEX_GUARD( g_lock );

                BL_ASSERT( g_instance );

                delete g_instance;
                g_instance = nullptr;

                g_javaVMDestroyed = true;

                BL_NOEXCEPT_END()
            }

            auto getJavaVM() const NOEXCEPT -> JavaVM*
            {
                BL_ASSERT( g_instance );

                return m_javaVM;
            }
        };

        typedef JavaVirtualMachineT<> JavaVirtualMachine;

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, cpp::ScalarTypeIniter< bool >,    g_javaVMDestroyed );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, os::mutex,                        g_lock );
        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT, JavaVirtualMachineConfig,         g_jvmConfig );

        BL_DEFINE_STATIC_MEMBER( JavaVirtualMachineT,
            typename JavaVirtualMachineT< TCLASS >::this_type*,                         g_instance );

    } // jni

} // bl

#endif /* __BL_JNI_JAVAVIRTUALMACHINE_H_ */
