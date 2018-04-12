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

#include <baselib/crypto/RsaKey.h>
#include <baselib/crypto/CryptoBase.h>
#include <baselib/crypto/X509Cert.h>
#include <baselib/crypto/RsaEncryption.h>

#include <baselib/data/models/Jose.h>

#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

UTF_AUTO_TEST_CASE( CryptoUtils_InitSsl )
{
    bl::crypto::CryptoBase::init();
}

UTF_AUTO_TEST_CASE( CryptoUtils_RsaTests )
{
    const auto rsaKey = bl::crypto::RsaKey::createInstance();
}

UTF_AUTO_TEST_CASE( CryptoUtils_X509tests )
{
    bl::crypto::evppkey_ptr_t evpPkey = bl::crypto::createPrivateKey();

    UTF_REQUIRE( evpPkey );

    std::string evpPkeyPem = bl::crypto::getEvpPkeyAsPemString( evpPkey );

    UTF_REQUIRE( evpPkeyPem.find( "-----BEGIN PRIVATE KEY-----" ) != std::string::npos );

    const std::string country = "US";
    const std::string organization = "MyOrg";
    const std::string commonName = "localhost";
    int serial = 1;
    int daysValid = 365;

    const auto x509cert =
        bl::crypto::createSelfSignedX509Cert( evpPkey, country, organization, commonName, serial, daysValid );

    UTF_REQUIRE( x509cert );

    const auto x509certPem = bl::crypto::geX509CertAsPemString( x509cert );

    UTF_REQUIRE( x509certPem.find( "-----BEGIN CERTIFICATE-----" ) != std::string::npos );
}

UTF_AUTO_TEST_CASE( RsaEncryption_encryptAsBase64Tests )
{
    const std::string secret = "secret123";

    const auto publicKey = utest::TestUtils::getRsaKeyFromFile( "test-public-key.pem" );

    const auto encrypted = bl::crypto::RsaEncryption::encryptAsBase64Url( publicKey, secret );

    const auto privateKey = utest::TestUtils::getRsaKeyFromFile( "test-private-key.pem" );

    const auto decrypted = bl::crypto::RsaEncryption::decryptBase64Message( privateKey, encrypted );

    UTF_CHECK( decrypted == secret );

    UTF_CHECK( decrypted != encrypted );
}

UTF_AUTO_TEST_CASE( RsaEncryption_encryptTests )
{
    const std::string secret = "secret123";

    const auto publicKey = utest::TestUtils::getRsaKeyFromFile( "test-public-key.pem" );

    unsigned outputSize = 0u;

    const auto out = bl::crypto::RsaEncryption::encrypt( publicKey, secret, outputSize );

    const auto charBuffer = ( const unsigned char * ) out.get();

    const auto encrypted = reinterpret_cast< const char* >( charBuffer );

    UTF_CHECK( ! encrypted.empty() );

    UTF_CHECK( encrypted != secret );
}

UTF_AUTO_TEST_CASE( RsaEncryption_decryptTests )
{
    const std::string secret = "secret123";

    const auto publicKey = utest::TestUtils::getRsaKeyFromFile( "test-public-key.pem" );

    unsigned outputSize = 0u;
    const auto out = bl::crypto::RsaEncryption::encrypt( publicKey, secret, outputSize );

    const auto encryptedBuffer = ( const unsigned char * ) out.get();

    const auto encrypted = reinterpret_cast< const char* >( encryptedBuffer );

    UTF_CHECK( ! encrypted.empty() );

    UTF_CHECK( encrypted != secret );

    const auto privateKey = utest::TestUtils::getRsaKeyFromFile( "test-private-key.pem" );

    const auto out2 = bl::crypto::RsaEncryption::decrypt( privateKey, encrypted, outputSize );

    const auto decryptedBuffer = ( const unsigned char * ) out2.get();

    const auto decrypted = reinterpret_cast< const char* >( decryptedBuffer );

    UTF_CHECK( ! decrypted.empty() );

    UTF_CHECK_EQUAL( decrypted, secret );
}
