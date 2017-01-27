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

#ifndef __BL_ERRORDISPATCHER_H_
#define __BL_ERRORDISPATCHER_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( ErrorDispatcher, "9000e252-7d39-46fe-9ec5-051164346791" )

namespace bl
{
    /**
     * @brief interface ErrorDispatcher
     */

    class ErrorDispatcher : public om::Object
    {
        BL_DECLARE_INTERFACE( ErrorDispatcher )

    public:

        virtual void dispatchException( SAA_in const std::exception_ptr eptr ) NOEXCEPT = 0;
    };

} // bl

#endif /* __BL_ERRORDISPATCHER_H_ */
