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

#ifndef __UTEST_PLUGINS_CALCULATOR_H_
#define __UTEST_PLUGINS_CALCULATOR_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <stdio.h>

BL_IID_DECLARE( Calculator, "77ff26f2-e2bd-4dde-ab66-e8aa3ecc1734" )

BL_CLSID_DECLARE( PluginObj, "3fd48332-db97-42bd-9cf6-f6a332895d92" )
BL_CLSID_DECLARE( CalculatorObj, "c76d0949-92a2-4925-8c9f-890f05484474" )

namespace utest
{
    namespace plugins
    {
        class Calculator : public bl::om::Object
        {
            BL_DECLARE_INTERFACE( Calculator )

        public:

            virtual int add(
                SAA_in      const int       x,
                SAA_in      const int       y
                ) = 0;

            virtual int subtract(
                SAA_in      const int       x,
                SAA_in      const int       y
                ) = 0;
        };

    } // plugins

} // utest

#endif /* __UTEST_PLUGINS_CALCULATOR_H_ */
