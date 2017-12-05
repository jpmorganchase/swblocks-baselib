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

#include "examples/objmodel/MyObjectImpl.h"

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/MessageBuffer.h>
#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

#include <string>
#include <sstream>

#include <utests/baselib/Utf.h>

class LoaderInit
{
public:

    LoaderInit()
    {
        bl::om::registerClass( clsids::MyObjectImpl(), &bl::om::SimpleFactoryImpl< utest::MyObjectImpl >::createInstance );
    }

    ~LoaderInit()
    {
        bl::om::unregisterClass( clsids::MyObjectImpl() );
    }
};

UTF_GLOBAL_FIXTURE( LoaderInit )

