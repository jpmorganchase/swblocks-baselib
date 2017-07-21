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

#ifndef __BL_JNI_JNIRESOURCEWRAPPERS_H_
#define __BL_JNI_JNIRESOURCEWRAPPERS_H_

#include <jni.h>

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
        class JniEnvironmentT;

        template
        <
            typename E = void
        >
        class LocalReferenceDeleterT FINAL
        {
        public:

            void operator ()( SAA_in jobject object ) const
            {
                const auto& jniEnv = JniEnvironmentT< E >::instance();
                jniEnv.deleteLocalRef( object );
            }
        };

        template
        <
            typename T
        >
        using LocalReference = cpp::SafeUniquePtr< typename std::remove_pointer< T >::type, LocalReferenceDeleterT<> >;

        template
        <
            typename E = void
        >
        class GlobalReferenceDeleterT FINAL
        {
        public:

            void operator ()( SAA_in jobject object ) const NOEXCEPT
            {
                const auto& jniEnv = JniEnvironmentT< E >::instance();
                jniEnv.deleteGlobalRef( object );
            }
        };

        template
        <
            typename T
        >
        using GlobalReference = cpp::SafeUniquePtr< typename std::remove_pointer< T >::type, GlobalReferenceDeleterT<> >;

        template
        <
            typename T,
            typename E = void
        >
        GlobalReference< T > createGlobalReference( SAA_in const LocalReference < T >& localRef )
        {
            const auto& jniEnv = JniEnvironmentT< E >::instance();

            return GlobalReference< T >::attach(
                reinterpret_cast< T >( jniEnv.newGlobalRef( localRef.get() ) )
                );
        }

    } // jni

} // bl

#endif /* __BL_JNI_JNIRESOURCEWRAPPERS_H_ */
