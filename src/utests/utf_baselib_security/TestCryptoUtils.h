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

#include <baselib/security/JsonSecuritySerializationImpl.h>
#include <baselib/data/models/Jose.h>
#include <baselib/crypto/RsaEncryption.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

class JoseTypesPolicy
{
    BL_DECLARE_STATIC( JoseTypesPolicy )

public:

    typedef bl::dm::jose::RsaPublicKey           RsaPublicKey;
    typedef bl::dm::jose::RsaPrivateKey          RsaPrivateKey;
    typedef bl::dm::jose::KeyType                KeyType;
    typedef bl::dm::jose::SigningAlgorithm       SigningAlgorithm;
    typedef bl::dm::jose::PublicKeyUse           PublicKeyUse;

};

bl::om::ObjPtr< bl::crypto::RsaKey > getRsaKeyFromFile( SAA_in const std::string& fileName )
{
    using namespace  bl::security;

    const auto rsaKeyStr = utest::TestUtils::loadDataFile( fileName );

    if( fileName.find( "private" ) != std::string::npos )
    {
    	return JsonSecuritySerializationImpl< JoseTypesPolicy >::loadPrivateKeyFromPemString( rsaKeyStr );
    }

    return JsonSecuritySerializationImpl< JoseTypesPolicy >::loadPublicKeyFromPemString( rsaKeyStr );
}

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
    const std::string password = "mypass123";

    const auto publicKey = getRsaKeyFromFile( "test-public-key.pem" );

    const auto encryptedPass = bl::crypto::RsaEncryption::encryptAsBase64Url( publicKey, password );

    const auto privateKey =  getRsaKeyFromFile( "test-private-key.pem" );

    const auto decrypted = bl::crypto::RsaEncryption::decryptBase64Message( privateKey, encryptedPass );

    UTF_CHECK( decrypted == password );

    UTF_CHECK( decrypted != encryptedPass );
}

UTF_AUTO_TEST_CASE( RsaEncryption_encryptTests )
{
    const std::string password = "mypass123";

    const auto publicKey = getRsaKeyFromFile( "test-public-key.pem" );

    unsigned outputSize = 0u;

    const auto out = bl::crypto::RsaEncryption::encrypt( publicKey, password, outputSize );

    const auto charBuffer = ( unsigned char * ) out.get();

    const std::string encryptedPassword( reinterpret_cast< char* >( charBuffer ) );

    UTF_CHECK( ! encryptedPassword.empty() );

    UTF_CHECK( encryptedPassword != password );
}

UTF_AUTO_TEST_CASE( RsaEncryption_decryptTests )
{
    const std::string password = "mypass123";

    const auto publicKey = getRsaKeyFromFile( "test-public-key.pem" );

    unsigned outputSize = 0u;
    const auto out = bl::crypto::RsaEncryption::encrypt( publicKey, password, outputSize );

    const auto encryptedBuffer = ( unsigned char * ) out.get();

    const std::string encryptedPassword( reinterpret_cast< char* >( encryptedBuffer ) );

    UTF_CHECK( ! encryptedPassword.empty() );

    UTF_CHECK( encryptedPassword != password );


    const auto privateKey = getRsaKeyFromFile( "test-private-key.pem" );

    const auto out2 = bl::crypto::RsaEncryption::decrypt( privateKey, encryptedPassword, outputSize );

    const auto decryptedBuffer = ( unsigned char * ) out2.get();

    const std::string decryptedPassword( reinterpret_cast< char* >( decryptedBuffer ) );

    UTF_CHECK( ! decryptedPassword.empty() );

    UTF_CHECK_EQUAL( decryptedPassword, password );
}
