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

#ifndef __BL_UUIDITERATOR_H_
#define __BL_UUIDITERATOR_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( UuidIterator, "8b1aade6-2360-4fff-9a36-5d41930ff728" )

namespace bl
{
    /**
     * @brief interface UuidIterator
     */

    class UuidIterator : public om::Object
    {
        BL_DECLARE_INTERFACE( UuidIterator )

    public:

        virtual bool hasCurrent() const NOEXCEPT = 0;
        virtual void loadNext() = 0;
        virtual const uuid_t& current() const NOEXCEPT = 0;
        virtual void reset() = 0;
    };

} // bl

#endif /* __BL_UUIDITERATOR_H_ */
