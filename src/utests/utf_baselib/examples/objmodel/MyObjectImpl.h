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

#ifndef __BL_MYOBJECTIMPL_H_
#define __BL_MYOBJECTIMPL_H_

#include "MyInterfaces.h"

#ifndef BL_PRECOMPILED_ENABLED
#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>
#endif

namespace utest
{
    /**
     * @brief class MyObjectImpl
     */
    template
    <
        typename E = void
    >
    class MyObjectImplT :
        public MyInterface1,
        public MyInterface2,
        public MyInterface3
    {
        BL_DECLARE_OBJECT_IMPL( MyObjectImplT )

        BL_QITBL_BEGIN()
            BL_QITBL_ENTRY( MyInterface1 )
            BL_QITBL_ENTRY( MyInterface2 )
            BL_QITBL_ENTRY( MyInterface3 )
        BL_QITBL_END( MyInterface1 )

    private:

        int m_value;

    protected:

        MyObjectImplT()
            :
            m_value( 13L )
        {
        }

    public:

        virtual long getValue() OVERRIDE
        {
            return m_value;
        }

        virtual void incValue( SAA_in const long step ) OVERRIDE
        {
            m_value += step;
        }

        virtual void setValue( SAA_in const long value ) OVERRIDE
        {
            m_value = value;
        }
    };

    typedef bl::om::ObjectImpl< MyObjectImplT<> > MyObjectImpl;

} // bl

#endif /* __BL_MYOBJECTIMPL_H_ */

