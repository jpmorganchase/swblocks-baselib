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

#ifndef __BL_JNI_JAVAVIRTUALMACHINECONFIG_H_
#define __BL_JNI_JAVAVIRTUALMACHINECONFIG_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        /**
         * @brief class JavaVirtualMachineConfig
         */

        template
        <
            typename E = void
        >
        class JavaVirtualMachineConfigT
        {
        private:

            std::string                                         m_libraryPath;
            std::string                                         m_classPath;

        public:

            const std::string& getLibraryPath() const NOEXCEPT
            {
                return m_libraryPath;
            }

            void setLibraryPath( SAA_in std::string&& libraryPath )
            {
                m_libraryPath = BL_PARAM_FWD( libraryPath );
            }

            const std::string& getClassPath() const NOEXCEPT
            {
                return m_classPath;
            }

            void setClassPath( SAA_in std::string&& classPath )
            {
                m_classPath = BL_PARAM_FWD( classPath );
            }

            std::vector< std::string > getJavaVMOptions()
            {
                std::vector< std::string > options;

                if( ! m_classPath.empty() )
                {
                    options.push_back( "-Djava.class.path=" + m_classPath );
                }

                return options;
            }
        };

        typedef JavaVirtualMachineConfigT<> JavaVirtualMachineConfig;

    } // jni

} // bl

#endif /* __BL_JNI_JAVAVIRTUALMACHINECONFIG_H_ */
