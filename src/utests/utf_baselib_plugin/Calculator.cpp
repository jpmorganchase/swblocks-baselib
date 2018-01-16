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

#define BL_PLUGINS_CLASS_IMPLEMENTATION

#include "utf/plugins/calculator/Calculator.h"

#include <baselib/core/AppInitDone.h>
#include <baselib/core/PluginDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <stdio.h>

namespace utest
{
    namespace plugins
    {
        namespace detail
        {
            class PluginImpl : public bl::om::Plugin
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE( PluginImpl, bl::om::Plugin )
                BL_CTR_DEFAULT( PluginImpl, public )

            public:

                virtual int init() NOEXCEPT OVERRIDE
                {
                    return 0;
                }

                virtual int start() NOEXCEPT OVERRIDE
                {
                    bl::AppInitDoneDefault init( bl::Logging::LL_DEBUG );

                    BL_LOG( bl::Logging::debug(), BL_MSG() << "PluginImpl::start()" );
                    return 0;
                }

                virtual void dispose() OVERRIDE
                {
                    BL_LOG( bl::Logging::debug(), BL_MSG() << "PluginImpl::dispose()" );
                }
            };

            class CalculatorImpl : public Calculator
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE( CalculatorImpl, Calculator )
                BL_CTR_DEFAULT( CalculatorImpl, public )

            public:

                virtual int add(
                    SAA_in      const int           x,
                    SAA_in      const int           y
                    ) OVERRIDE
                {
                    return x + y;
                }

                virtual int subtract(
                    SAA_in      const int           x,
                    SAA_in      const int           y
                    ) OVERRIDE
                {
                    return x - y;
                }
            };

        } // detail

        typedef bl::om::ObjectImpl< detail::PluginImpl > PluginObj;
        typedef bl::om::ObjectImpl< detail::CalculatorImpl > CalculatorObj;

    } // plugins

} // utest

BL_PLUGINS_DECLARE_PLUGIN_BEGIN( CalculatorPlugin, "calculator", "Simple calculator plug-in", "8e213524-8c75-4622-8273-6a5eeaa26250", 1, 2, clsids::PluginObj(), false, true )
    BL_PLUGINS_DECLARE_PLUGIN_CLASS( utest::plugins::PluginObj, clsids::PluginObj() )
    BL_PLUGINS_DECLARE_PLUGIN_CLASS( utest::plugins::CalculatorObj, clsids::CalculatorObj() )
BL_PLUGINS_DECLARE_PLUGIN_END( CalculatorPlugin )

BL_PLUGINS_REGISTER_PLUGIN( bl::plugins::CalculatorPlugin )
