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

#ifndef __BL_BOXEDOBJECTS_H_
#define __BL_BOXEDOBJECTS_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/OS.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

#include <vector>

namespace bl
{
    /**
     * @brief Boxed objects definitions
     */

    namespace bo
    {
        typedef om::ObjectImpl< om::BoxedValueObject< fs::path > >                              path;
        typedef om::ObjectImpl< om::BoxedValueObject< std::string > >                           string;
        typedef om::ObjectImpl< om::BoxedValueObject< std::vector< std::string > > >            string_vector;
        typedef om::ObjectImpl< om::BoxedValueObject< std::vector< uuid_t > > >                 uuid_vector;
        typedef om::ObjectImpl< om::BoxedValueObject< std::vector< std::uint64_t > > >          uint64_vector;

    } // bo

} // bl

#endif /* __BL_BOXEDOBJECTS_H_ */
