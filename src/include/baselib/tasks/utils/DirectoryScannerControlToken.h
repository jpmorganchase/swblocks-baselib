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

#ifndef __BL_TASKS_UTILS_DIRECTORYSCANNERCONTROLTOKEN_H_
#define __BL_TASKS_UTILS_DIRECTORYSCANNERCONTROLTOKEN_H_

#include <baselib/tasks/TaskControlToken.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( DirectoryScannerControlToken, "521c3c1d-42c6-453d-9d3f-feaaa798e987" )

namespace bl
{
    namespace tasks
    {
        /**
         * @brief interface DirectoryScannerControlToken
         */

        class DirectoryScannerControlToken : public TaskControlToken
        {
            BL_DECLARE_INTERFACE( DirectoryScannerControlToken )

        public:

            virtual bool isErrorAllowed( SAA_in const eh::error_code& code ) const NOEXCEPT = 0;
            virtual bool isEntryAllowed( SAA_in const fs::directory_entry& entry ) = 0;
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_UTILS_DIRECTORYSCANNERCONTROLTOKEN_H_ */
