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

#ifndef __UTEST_DATAMODELTESTUTILS_H_
#define __UTEST_DATAMODELTESTUTILS_H_

#include <baselib/data/DataModelObjectDefs.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

namespace utest
{
    /**
     * @brief Data model test utility code
     */

    template
    <
        typename E = void
    >
    class DataModelTestUtilsT
    {
        BL_DECLARE_STATIC( DataModelTestUtilsT )

    public:

        template
        <
            typename T
        >
        static void requireObjectsEqual(
            SAA_in          const bl::om::ObjPtr< T >&          object1,
            SAA_in          const bl::om::ObjPtr< T >&          object2
            )
        {
            const auto dataString1 = bl::dm::DataModelUtils::getDocAsPackedJsonString( object1 );

            const auto dataString2 = bl::dm::DataModelUtils::getDocAsPackedJsonString( object2 );

            UTF_REQUIRE_EQUAL( dataString1, dataString2 );
        }
    };

    typedef DataModelTestUtilsT<> DataModelTestUtils;

} // utest

#endif /* __UTEST_DATAMODELTESTUTILS_H_ */
