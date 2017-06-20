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
        private:

            typedef std::shared_ptr< JavaVirtualMachineT< E > > JavaVirtualMachinePtr;

            JavaVirtualMachinePtr                   m_javaVM;

        public:

            JniEnvDeleterT( SAA_in JavaVirtualMachinePtr javaVM ) NOEXCEPT
                :
                m_javaVM( javaVM )
            {
                BL_ASSERT( m_javaVM );
            }

            void operator ()( SAA_in JNIEnv* jniEnv ) const NOEXCEPT
            {
                BL_UNUSED( jniEnv );

                BL_NOEXCEPT_BEGIN()

                m_javaVM -> detachCurrentThread();

                BL_NOEXCEPT_END()
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
        class JniEnvironmentT : public om::ObjectDefaultBase
        {
        protected:

            JniEnvironmentT( SAA_inout JniEnvPtr&& jniEnv )
                :
                m_jniEnv( BL_PARAM_FWD( jniEnv ) )
            {
                BL_ASSERT( m_jniEnv );
            }

        private:

            JniEnvPtr                               m_jniEnv;
        };

        typedef om::ObjectImpl< JniEnvironmentT<> > JniEnvironment;

    } // jni

} // bl

#endif /* __BL_JNI_JNIENVIRONMENT_H_ */
