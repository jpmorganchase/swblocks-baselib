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

#include <baselib/security/JsonSecuritySerialization.h>

#include <baselib/crypto/RsaKey.h>
#include <baselib/crypto/RsaSignVerify.h>

#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>

namespace utest
{
    template
    <
        typename E = void
    >
    class LocalTestRsaSignVerifyHelpersT
    {
        BL_DECLARE_STATIC( LocalTestRsaSignVerifyHelpersT )

    public:

        static std::string createEncryptedPemKeyAsText(
            SAA_in              const std::string&                                          password,
            SAA_in_opt          const bool                                                  print = false
            )
        {
            const auto rsaKey = bl::crypto::RsaKey::createInstance();
            rsaKey -> generate();

            auto privateKeyPemString = bl::security::JsonSecuritySerialization::getPrivateKeyAsPemString(
                rsaKey,
                password
                );

            if( print )
            {
                BL_LOG_MULTILINE(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "PEM key as text:\n"
                        << privateKeyPemString
                    );
            }

            return privateKeyPemString;
        }
    };

    typedef LocalTestRsaSignVerifyHelpersT<> LocalTestRsaSignVerifyHelpers;

} // utest

UTF_AUTO_TEST_CASE( TestRsaSignVerifyPositive )
{
    const auto privateKeyPemString = utest::LocalTestRsaSignVerifyHelpers::createEncryptedPemKeyAsText(
        "1234" /* password */,
        true /* print */
        );

    /*
     * Verify that the key is actually encrypted by trying to decrypt it with the incorrect password
     */

    UTF_REQUIRE_THROW(
        bl::security::JsonSecuritySerialization::loadPrivateKeyFromPemString( privateKeyPemString, "12345678" ),
        bl::SystemException
        );

    const std::string message( "C++ is a wonderful language!" );

    const auto rsaKey = bl::security::JsonSecuritySerialization::loadPrivateKeyFromPemString(
        privateKeyPemString,
        "1234" /* password */
        );

    const auto signatureBase64Url = bl::crypto::RsaSignVerify::signAsBase64Url(
        rsaKey,
        message
        );

    const auto verifyResult = bl::crypto::RsaSignVerify::tryVerify(
        rsaKey,
        message,
        signatureBase64Url
        );

    UTF_REQUIRE( verifyResult == 1 );

    UTF_CHECK_NO_THROW(
        bl::crypto::RsaSignVerify::verify(
            rsaKey,
            message,
            signatureBase64Url
            )
        );
}

UTF_AUTO_TEST_CASE( TestRsaSignVerifyNegative )
{
    const auto privateKeyPemString =
        utest::LocalTestRsaSignVerifyHelpers::createEncryptedPemKeyAsText( "1234" /* password */ );;

    const std::string message( "C++ is a wonderful language!" );

    const auto rsaKey = bl::security::JsonSecuritySerialization::loadPrivateKeyFromPemString(
        privateKeyPemString,
        "1234" /* password */
        );

    auto signatureBase64Url = bl::crypto::RsaSignVerify::signAsBase64Url(
        rsaKey,
        message
        );

    /*
     * Break signature with random modification
     */

    for( std::size_t i = 0U, count = signatureBase64Url.size() - 1U; i < count; ++i )
    {
        /*
         * Note that i + 1U is always valid index as we iterate only to the one
         * before the last
         */

        if( signatureBase64Url[ i ] != signatureBase64Url[ i + 1U ] )
        {
            std::swap( signatureBase64Url[ i ], signatureBase64Url[ i + 1U ] );
            break;
        }
    }

    const auto verifyResult = bl::crypto::RsaSignVerify::tryVerify(
        rsaKey,
        message,
        signatureBase64Url
        );

    UTF_REQUIRE( verifyResult == 0 );

    UTF_CHECK_THROW(
        bl::crypto::RsaSignVerify::verify(
            rsaKey,
            message,
            signatureBase64Url
            ),
        std::exception
        );
}
