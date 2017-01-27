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

#ifndef __BL_MYINTERFACES_H_
#define __BL_MYINTERFACES_H_

#ifndef BL_PRECOMPILED_ENABLED
#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>
#endif

// iids
BL_IID_DECLARE( MyInterface1, "16d70a8a-ac12-46fc-aa8a-c0e22d6175b7" )
BL_IID_DECLARE( MyInterface2, "066b50bc-f6d6-481a-8e0d-9e7b0d3d77e1" )
BL_IID_DECLARE( MyInterface3, "57f56684-2fca-4ff7-baff-d0d3cc20475e" )

// clsid
BL_CLSID_DECLARE( MyObjectImpl, "6931a2d6-96d2-4a14-a833-d212d2af1e18" )

namespace utest
{
    /**
     * @brief interface MyInterface1
     */
    class MyInterface1 : public bl::om::Object
    {
        BL_DECLARE_INTERFACE( MyInterface1 )

    public:

        virtual long getValue() = 0;
    };

    /**
     * @brief interface MyInterface2
     */
    class MyInterface2 : public bl::om::Object
    {
        BL_DECLARE_INTERFACE( MyInterface2 )

    public:

        virtual void incValue( SAA_in const long step = 5) = 0;
    };

    /**
     * @brief interface MyInterface2
     */
    class MyInterface3 : public bl::om::Object
    {
        BL_DECLARE_INTERFACE( MyInterface3 )

    public:

        virtual void setValue( SAA_in const long value ) = 0;
    };

} // utest

#endif /* __BL_MYINTERFACES_H_ */

