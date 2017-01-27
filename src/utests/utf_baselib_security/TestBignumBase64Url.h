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

#include <baselib/crypto/BignumBase64Url.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>

UTF_AUTO_TEST_CASE( BignumBase64UrlTest )
{
    using namespace bl::crypto;

    {
        const auto bignum = BignumBase64Url::base64UrlToBignum( "AQAB" );

        const auto actualDecimal = openssl_string_ptr_t::attach(
            BN_bn2dec( bignum.get() )
            );

        BL_CHK_CRYPTO_API_NM( actualDecimal );

        UTF_REQUIRE_EQUAL( actualDecimal.get(), std::string( "65537" ) );
    }

    {
        BIGNUM* bignum = nullptr;
        const auto length = BN_dec2bn( &bignum, "65537" );

        const auto guard = bignum_ptr_t::attach( bignum );

        BL_CHK_CRYPTO_API_NM( length );

        UTF_REQUIRE_EQUAL(
            BignumBase64Url::bignumToBase64Url( bignum ),
            "AQAB"
            );
    }
}
