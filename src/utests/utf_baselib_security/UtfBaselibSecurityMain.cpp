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

#include <baselib/crypto/CryptoBase.h>
#define UTF_TEST_APP_INIT_PHASE1_INIT bl::crypto::CryptoBase::isEnableTlsV10( true );

#define UTF_TEST_MODULE utf_baselib_security
#include <utests/baselib/UtfMain.h>

#include "TestHashUtils.h"
#include "TestBignumBase64Url.h"
#include "TestRsaSignVerify.h"
#include "TestCryptoUtils.h"
#include "TestAuthorizationCacheImpl.h"
#include "TestAuthorizationCacheRestImpl.h"
