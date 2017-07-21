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

#ifndef __BL_JNI_JNIENVIRONMENT_H_
#define __BL_JNI_JNIENVIRONMENT_H_

#include <baselib/jni/JavaVirtualMachine.h>
#include <baselib/jni/JniResourceWrappers.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        /**
         * @brief class JniEnvironment - a wrapper class for JNIEnv* raw pointer
         */

        template
        <
            typename E = void
        >
        class JniEnvironmentT
        {
            BL_NO_COPY_OR_MOVE( JniEnvironmentT )

        private:

            typedef JniEnvironmentT< E >                        this_type;

            static os::thread_specific_ptr< this_type >         g_tlsDataJni;

            static std::atomic< int64_t >                       g_jniThreadCount;

            JNIEnv*                                             m_jniEnv;

            JniEnvironmentT()
            {
                JNIEnv* jniEnv;

                const jint jniErrorCode = JavaVirtualMachine::instance().getJavaVM() -> AttachCurrentThread(
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

                m_jniEnv = jniEnv;

                ++g_jniThreadCount;
            }

        public:

            ~JniEnvironmentT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                const jint jniErrorCode = JavaVirtualMachine::instance().getJavaVM() -> DetachCurrentThread();

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

                --g_jniThreadCount;

                BL_NOEXCEPT_END()
            }

            static void detach() NOEXCEPT
            {
                if( g_tlsDataJni.get() )
                {
                    g_tlsDataJni.reset();
                }
            }

            static auto instance() -> this_type&
            {
                if( ! g_tlsDataJni.get() )
                {
                    g_tlsDataJni.reset( new this_type() );
                }

                BL_ASSERT( g_tlsDataJni.get() );

                return *g_tlsDataJni.get();
            }

            static auto instanceNoThrow() NOEXCEPT -> this_type&
            {
                BL_ASSERT( g_tlsDataJni.get() );

                return *g_tlsDataJni.get();
            }

            static int64_t getJniThreadCount() NOEXCEPT
            {
                return g_jniThreadCount;
            }

            jint getVersion() const NOEXCEPT
            {
                return m_jniEnv -> GetVersion();
            }

            jobjectRefType getObjectRefType( SAA_in const jobject object ) const NOEXCEPT
            {
                return m_jniEnv -> GetObjectRefType( object );
            }

            void deleteLocalRef( SAA_in const jobject object) const NOEXCEPT
            {
                m_jniEnv -> DeleteLocalRef( object );
            }

            jobject newGlobalRef( SAA_in const jobject object ) const
            {
                const auto globalRef = m_jniEnv -> NewGlobalRef( object );

                BL_CHK_T(
                    nullptr,
                    globalRef,
                    JavaException(),
                    BL_MSG()
                        << "Failed to create new global reference."
                    );

                return globalRef;
            }

            void deleteGlobalRef( SAA_in const jobject object) const NOEXCEPT
            {
                m_jniEnv -> DeleteGlobalRef( object );
            }

            LocalReference< jclass > findJavaClass( SAA_in const std::string& javaClassName ) const
            {
                const auto javaClass = m_jniEnv -> FindClass( javaClassName.c_str() );

                const auto javaVmException = m_jniEnv -> ExceptionOccurred();

                if( javaVmException )
                {
                    m_jniEnv -> ExceptionClear();

                    /* TODO: extract more info from java exception
                     * and add it to c++ exception.
                     */

                    BL_THROW(
                        JavaException(),
                        BL_MSG()
                            << "Java class '"
                            << javaClassName
                            << "' not found.\n"
                        );
                }

                return LocalReference< jclass >::attach( javaClass );
            }
        };

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT,
            os::thread_specific_ptr< typename JniEnvironmentT< TCLASS >::this_type >,           g_tlsDataJni );

        BL_DEFINE_STATIC_MEMBER( JniEnvironmentT, std::atomic< int64_t >,                       g_jniThreadCount );

        typedef JniEnvironmentT<> JniEnvironment;

    } // jni

} // bl

#endif /* __BL_JNI_JNIENVIRONMENT_H_ */
