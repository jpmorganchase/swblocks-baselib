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

#ifndef __BL_ASIOSSL_H_
#define __BL_ASIOSSL_H_

/*
 * This header must be included before we include Boost ASIO
 * headers, so we can override the error handling if we need to
 */

#include <baselib/core/detail/BoostAsioErrorCallback.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/asio/ssl.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

#ifdef _WIN32
#pragma comment( lib, "gdi32.lib" )
#pragma comment( lib, "user32.lib" )
#endif // _WIN32

#endif /* __BL_ASIOSSL_H_ */
