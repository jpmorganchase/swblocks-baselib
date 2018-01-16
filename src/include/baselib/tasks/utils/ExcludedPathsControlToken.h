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

#ifndef __BL_TASKS_UTILS_EXCLUDEDPATHSCONTROLTOKEN_H_
#define __BL_TASKS_UTILS_EXCLUDEDPATHSCONTROLTOKEN_H_

#include <baselib/tasks/utils/DirectoryScannerControlToken.h>
#include <baselib/core/FsUtils.h>

#include <unordered_set>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class ExcludedPathsControlToken
         */

        class ExcludedPathsControlTokenT : public DirectoryScannerControlToken
        {
            BL_DECLARE_OBJECT_IMPL( ExcludedPathsControlTokenT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( TaskControlToken )
                BL_QITBL_ENTRY( DirectoryScannerControlToken )
            BL_QITBL_END( DirectoryScannerControlToken )

            std::unordered_set< std::string >                  m_excludedPaths;

        public:

            ExcludedPathsControlTokenT( SAA_in const std::vector< std::string >& excludedPaths )
                :
                m_excludedPaths( excludedPaths.begin(), excludedPaths.end() )
            {
            }

            virtual bool isErrorAllowed( SAA_in const eh::error_code& /* code */ ) const NOEXCEPT OVERRIDE
            {
                return false;
            }

            virtual bool isEntryAllowed( SAA_in const fs::directory_entry& entry ) OVERRIDE
            {
                auto normEntryPath = fs::normalize( entry.path() ).string();

                if( os::onWindows() )
                {
                    str::to_lower( normEntryPath );
                }

                return m_excludedPaths.find( normEntryPath ) == m_excludedPaths.end();
            }

            virtual bool isCanceled() const NOEXCEPT OVERRIDE
            {
                return false;
            }
        };

        typedef om::ObjectImpl< ExcludedPathsControlTokenT > ExcludedPathsControlToken;

    } // tasks

} // bl

#endif /* __BL_TASKS_UTILS_EXCLUDEDPATHSCONTROLTOKEN_H_ */

