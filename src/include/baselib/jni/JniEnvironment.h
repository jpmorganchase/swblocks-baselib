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

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        template
        <
            typename E
        >
        class JavaVirtualMachineT;

        template
        <
            typename E = void
        >
        class JniEnvDeleterT FINAL
        {
        public:

            void operator ()( SAA_in JNIEnv* jniEnv ) const NOEXCEPT
            {
                BL_UNUSED( jniEnv );

                BL_NOEXCEPT_BEGIN();

                JavaVirtualMachineT< E >::detachThread();

                BL_NOEXCEPT_END();
            }
        };

        typedef JniEnvDeleterT<>                                JniEnvDeleter;
        typedef cpp::SafeUniquePtr< JNIEnv, JniEnvDeleter >     JniEnvPtr;

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

            static os::thread_specific_ptr< this_type >         g_tlsData;

            JniEnvironmentT( SAA_inout JniEnvPtr&& jniEnv )
                :
                m_jniEnv( BL_PARAM_FWD( jniEnv ) )
            {
            }

        public:

            JniEnvPtr                                           m_jniEnv;

            static auto create( SAA_inout JniEnvPtr&& jniEnv ) -> this_type&
            {
                if( nullptr == g_tlsData.get() )
                {
                    g_tlsData.reset( new this_type( BL_PARAM_FWD( jniEnv ) ) );
                }

                return *g_tlsData.get();
            }

            static auto get() NOEXCEPT -> this_type&
            {
                if( nullptr == g_tlsData.get() )
                {
                    BL_RIP_MSG( "JniEnvironment TLS data cannot be null" );
                }

                return *g_tlsData.get();
            }

            static bool exists() NOEXCEPT
            {
                return g_tlsData.get() != nullptr;
            }

            static void destroy()
            {
                g_tlsData.reset();
            }

            jint getVersion() const NOEXCEPT
            {
                return m_jniEnv -> GetVersion();
            }

        };

        typedef JniEnvironmentT<> JniEnvironment;

        template
        <
            typename E
        >
        os::thread_specific_ptr< typename JniEnvironmentT< E >::this_type >
        JniEnvironmentT< E >::g_tlsData;

    } // jni

} // bl

#endif /* __BL_JNI_JNIENVIRONMENT_H_ */
