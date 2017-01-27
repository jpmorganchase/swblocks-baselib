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

#define UTF_TEST_MODULE utf_baselib
#include <utests/baselib/UtfMain.h>

/*
 * TODO: bringing all definitions of bl into the global namespace like this is of course typically not
 * a good idea, but unfortunately many tests here are written under the assumption that the bl namespace
 * is visible and thus we have to do it
 *
 * In the future if/when we cleanup the tests this using 'using namespace bl;' line can be removed
 */

using namespace bl;

#include "TestBaselibDefault.h"
#include "TestObjModel.h"
#include "TestTimeZoneData.h"
#include "TestTransaction.h"
#include "TestNetUtils.h"
#include "TestWatchdog.h"

/*
 * Global fixtures are included last
 */

#include "UtfLoaderInit.h"
