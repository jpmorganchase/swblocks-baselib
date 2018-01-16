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

#ifndef __BL_DATA_MODELS_JOSE_H_
#define __BL_DATA_MODELS_JOSE_H_

#include <baselib/data/DataModelObjectDefs.h>

namespace bl
{
    namespace dm
    {
        namespace jose
        {
            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( SigningAlgorithm, "SigningAlgorithm",
                ( ( HS256, "HS256" ) )
                ( ( HS384, "HS384" ) )
                ( ( HS512, "HS512" ) )
                ( ( RS256, "RS256" ) )
                ( ( RS384, "RS384" ) )
                ( ( RS512, "RS512" ) )
                ( ( ES256, "ES256" ) )
                ( ( ES384, "ES384" ) )
                ( ( ES512, "ES512" ) )
                ( ( PS256, "PS256" ) )
                ( ( PS384, "PS384" ) )
                ( ( PS512, "PS512" ) )
                ( ( none, "none" ) )
                )

            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( EncryptionAlgorithm, "EncryptionAlgorithm",
                ( ( RSA1_5, "RSA1_5" ) )
                ( ( RSA_OAEP, "RSA-OAEP" ) )
                ( ( RSA_OAEP_256, "RSA-OAEP-256" ) )
                ( ( A128KW, "A128KW" ) )
                ( ( A192KW, "A192KW" ) )
                ( ( A256KW, "A256KW" ) )
                ( ( DIR, "dir" ) )
                ( ( ECDH_ES, "ECDH-ES" ) )
                ( ( ECDH_ES_A128KW, "ECDH-ES+A128KW" ) )
                ( ( ECDH_ES_A192KW, "ECDH-ES+A192KW" ) )
                ( ( ECDH_ES_A256KW, "ECDH-ES+A256KW" ) )
                ( ( A128GCMKW, "A128GCMKW" ) )
                ( ( A192GCMKW, "A192GCMKW" ) )
                ( ( A256GCMKW, "A256GCMKW" ) )
                ( ( PBES2_HS256_A128KW, "PBES2-HS256+A128KW" ) )
                ( ( PBES2_HS384_A192KW, "PBES2-HS384+A192KW" ) )
                ( ( PBES2_HS512_A256KW, "PBES2-HS512+A256KW" ) )
                )

            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( KeyType, "KeyType",
                ( ( EC, "EC" ) )
                ( ( RSA, "RSA" ) )
                ( ( OCT, "oct" ) )
                )

            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( PublicKeyUse, "PublicKeyUse",
                ( ( sig, "sig" ) )
                ( ( enc, "enc" ) )
                )

            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( KeyOperations, "KeyOperations",
                ( ( sign, "sign" ) )
                ( ( verify, "verify" ) )
                ( ( encrypt, "encrypt" ) )
                ( ( decrypt, "decrypt" ) )
                ( ( wrapKey, "wrapKey" ) )
                ( ( unwrapKey, "unwrapKey" ) )
                ( ( deriveKey, "deriveKey" ) )
                ( ( deriveBits, "deriveBits" ) )
                )

            /*
             * @brief Class X509ParametersBase
             */

            BL_DM_DEFINE_CLASS_BEGIN( X509ParametersBase )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( X509ParametersBase )

            BL_DM_DEFINE_PROPERTY( X509ParametersBase, x509Url )
            BL_DM_DEFINE_PROPERTY( X509ParametersBase, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( X509ParametersBase, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( X509ParametersBase, x509CertificateSha256Thumbprint )

            /*
             * @brief Class CryptoCommonBase
             */

            BL_DM_DEFINE_CLASS_BEGIN( CryptoCommonBase )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( CryptoCommonBase )

            BL_DM_DEFINE_PROPERTY( CryptoCommonBase, x509Url )
            BL_DM_DEFINE_PROPERTY( CryptoCommonBase, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( CryptoCommonBase, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( CryptoCommonBase, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( CryptoCommonBase, algorithm )
            BL_DM_DEFINE_PROPERTY( CryptoCommonBase, keyId )

            /*
             * @brief Class KeyBase
             */

            BL_DM_DEFINE_CLASS_BEGIN( KeyBase )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( keyType, "kty" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( publicKeyUse, "use" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyOperations, "key_ops" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                    BL_DM_IMPL_PROPERTY( keyType )
                    BL_DM_IMPL_PROPERTY( publicKeyUse )
                    BL_DM_IMPL_PROPERTY( keyOperations )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( KeyBase )

            BL_DM_DEFINE_PROPERTY( KeyBase, x509Url )
            BL_DM_DEFINE_PROPERTY( KeyBase, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( KeyBase, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( KeyBase, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( KeyBase, algorithm )
            BL_DM_DEFINE_PROPERTY( KeyBase, keyId )
            BL_DM_DEFINE_PROPERTY( KeyBase, keyType )
            BL_DM_DEFINE_PROPERTY( KeyBase, publicKeyUse )
            BL_DM_DEFINE_PROPERTY( KeyBase, keyOperations )

            /*
             * @brief Class RsaPublicKey
             */

            BL_DM_DEFINE_CLASS_BEGIN( RsaPublicKey )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( keyType, "kty" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( publicKeyUse, "use" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyOperations, "key_ops" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( modulus, "n" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( exponent, "e" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                    BL_DM_IMPL_PROPERTY( keyType )
                    BL_DM_IMPL_PROPERTY( publicKeyUse )
                    BL_DM_IMPL_PROPERTY( keyOperations )
                    BL_DM_IMPL_PROPERTY( modulus )
                    BL_DM_IMPL_PROPERTY( exponent )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( RsaPublicKey )

            BL_DM_DEFINE_PROPERTY( RsaPublicKey, x509Url )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, algorithm )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, keyId )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, keyType )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, publicKeyUse )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, keyOperations )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, modulus )
            BL_DM_DEFINE_PROPERTY( RsaPublicKey, exponent )

            /*
             * @brief Class OtherPrimesInfo
             */

            BL_DM_DEFINE_CLASS_BEGIN( OtherPrimesInfo )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( primeFactor, "r" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( factorCrtExponent, "d" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( factorCrtCoefficient, "t" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( primeFactor )
                    BL_DM_IMPL_PROPERTY( factorCrtExponent )
                    BL_DM_IMPL_PROPERTY( factorCrtCoefficient )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( OtherPrimesInfo )

            BL_DM_DEFINE_PROPERTY( OtherPrimesInfo, primeFactor )
            BL_DM_DEFINE_PROPERTY( OtherPrimesInfo, factorCrtExponent )
            BL_DM_DEFINE_PROPERTY( OtherPrimesInfo, factorCrtCoefficient )

            /*
             * @brief Class RsaPrivateKey
             */

            BL_DM_DEFINE_CLASS_BEGIN( RsaPrivateKey )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( keyType, "kty" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( publicKeyUse, "use" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyOperations, "key_ops" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( modulus, "n" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( exponent, "e" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( privateExponent, "d" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( firstPrimeFactor, "p" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( secondPrimeFactor, "q" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( firstFactorCrtExponent, "dp" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( secondFactorCrtExponent, "dq" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( firstCrtCoefficient, "qi" )
                BL_DM_DECLARE_COMPLEX_PROPERTY( otherPrimesInfo, bl::dm::jose::OtherPrimesInfo )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                    BL_DM_IMPL_PROPERTY( keyType )
                    BL_DM_IMPL_PROPERTY( publicKeyUse )
                    BL_DM_IMPL_PROPERTY( keyOperations )
                    BL_DM_IMPL_PROPERTY( modulus )
                    BL_DM_IMPL_PROPERTY( exponent )
                    BL_DM_IMPL_PROPERTY( privateExponent )
                    BL_DM_IMPL_PROPERTY( firstPrimeFactor )
                    BL_DM_IMPL_PROPERTY( secondPrimeFactor )
                    BL_DM_IMPL_PROPERTY( firstFactorCrtExponent )
                    BL_DM_IMPL_PROPERTY( secondFactorCrtExponent )
                    BL_DM_IMPL_PROPERTY( firstCrtCoefficient )
                    BL_DM_IMPL_PROPERTY( otherPrimesInfo )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( RsaPrivateKey )

            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, x509Url )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, algorithm )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, keyId )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, keyType )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, publicKeyUse )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, keyOperations )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, modulus )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, exponent )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, privateExponent )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, firstPrimeFactor )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, secondPrimeFactor )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, firstFactorCrtExponent )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, secondFactorCrtExponent )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, firstCrtCoefficient )
            BL_DM_DEFINE_PROPERTY( RsaPrivateKey, otherPrimesInfo )

            /*
             * @brief Class SymmetricKey
             */

            BL_DM_DEFINE_CLASS_BEGIN( SymmetricKey )

                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( keyValue, "k" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( keyValue )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( SymmetricKey )

            BL_DM_DEFINE_PROPERTY( SymmetricKey, keyValue )

            /*
             * @brief Class KeySet
             */

            BL_DM_DEFINE_CLASS_BEGIN( KeySet )

                BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( keys, bl::dm::jose::RsaPublicKey )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( keys )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( KeySet )

            BL_DM_DEFINE_PROPERTY( KeySet, keys )

            /*
             * @brief Class HeaderBase
             */

            BL_DM_DEFINE_CLASS_BEGIN( HeaderBase )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keySetUrl, "jku" )
                BL_DM_DECLARE_COMPLEX_PROPERTY( key, bl::dm::jose::RsaPublicKey )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( headerType, "typ" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( contentType, "cty" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( critical, "crit" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                    BL_DM_IMPL_PROPERTY( keySetUrl )
                    BL_DM_IMPL_PROPERTY( key )
                    BL_DM_IMPL_PROPERTY( headerType )
                    BL_DM_IMPL_PROPERTY( contentType )
                    BL_DM_IMPL_PROPERTY( critical )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( HeaderBase )

            BL_DM_DEFINE_PROPERTY( HeaderBase, x509Url )
            BL_DM_DEFINE_PROPERTY( HeaderBase, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( HeaderBase, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( HeaderBase, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( HeaderBase, algorithm )
            BL_DM_DEFINE_PROPERTY( HeaderBase, keyId )
            BL_DM_DEFINE_PROPERTY( HeaderBase, keySetUrl )
            BL_DM_DEFINE_PROPERTY( HeaderBase, key )
            BL_DM_DEFINE_PROPERTY( HeaderBase, headerType )
            BL_DM_DEFINE_PROPERTY( HeaderBase, contentType )
            BL_DM_DEFINE_PROPERTY( HeaderBase, critical )

            /*
             * @brief Class SigningHeader
             */

            BL_DM_DEFINE_CLASS_BEGIN( SigningHeader )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keySetUrl, "jku" )
                BL_DM_DECLARE_COMPLEX_PROPERTY( key, bl::dm::jose::RsaPublicKey )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( headerType, "typ" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( contentType, "cty" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( critical, "crit" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                    BL_DM_IMPL_PROPERTY( keySetUrl )
                    BL_DM_IMPL_PROPERTY( key )
                    BL_DM_IMPL_PROPERTY( headerType )
                    BL_DM_IMPL_PROPERTY( contentType )
                    BL_DM_IMPL_PROPERTY( critical )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( SigningHeader )

            BL_DM_DEFINE_PROPERTY( SigningHeader, x509Url )
            BL_DM_DEFINE_PROPERTY( SigningHeader, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( SigningHeader, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( SigningHeader, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( SigningHeader, algorithm )
            BL_DM_DEFINE_PROPERTY( SigningHeader, keyId )
            BL_DM_DEFINE_PROPERTY( SigningHeader, keySetUrl )
            BL_DM_DEFINE_PROPERTY( SigningHeader, key )
            BL_DM_DEFINE_PROPERTY( SigningHeader, headerType )
            BL_DM_DEFINE_PROPERTY( SigningHeader, contentType )
            BL_DM_DEFINE_PROPERTY( SigningHeader, critical )

            /*
             * @brief Class EncryptionHeader
             */

            BL_DM_DEFINE_CLASS_BEGIN( EncryptionHeader )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509Url, "x5u" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateChain, "x5c" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha1Thumbprint, "x5t" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( x509CertificateSha256Thumbprint, "x5t#S256" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( algorithm, "alg" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keyId, "kid" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( keySetUrl, "jku" )
                BL_DM_DECLARE_COMPLEX_PROPERTY( key, bl::dm::jose::RsaPublicKey )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( headerType, "typ" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( contentType, "cty" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( critical, "crit" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( encryptionAlgorithm, "enc" )
                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( compressionAlgorithm, "zip" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( x509Url )
                    BL_DM_IMPL_PROPERTY( x509CertificateChain )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha1Thumbprint )
                    BL_DM_IMPL_PROPERTY( x509CertificateSha256Thumbprint )
                    BL_DM_IMPL_PROPERTY( algorithm )
                    BL_DM_IMPL_PROPERTY( keyId )
                    BL_DM_IMPL_PROPERTY( keySetUrl )
                    BL_DM_IMPL_PROPERTY( key )
                    BL_DM_IMPL_PROPERTY( headerType )
                    BL_DM_IMPL_PROPERTY( contentType )
                    BL_DM_IMPL_PROPERTY( critical )
                    BL_DM_IMPL_PROPERTY( encryptionAlgorithm )
                    BL_DM_IMPL_PROPERTY( compressionAlgorithm )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( EncryptionHeader )

            BL_DM_DEFINE_PROPERTY( EncryptionHeader, x509Url )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, x509CertificateChain )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, x509CertificateSha1Thumbprint )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, x509CertificateSha256Thumbprint )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, algorithm )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, keyId )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, keySetUrl )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, key )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, headerType )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, contentType )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, critical )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, encryptionAlgorithm )
            BL_DM_DEFINE_PROPERTY( EncryptionHeader, compressionAlgorithm )

            /*
             * @brief Class SignedTokenSignature
             */

            BL_DM_DEFINE_CLASS_BEGIN( SignedTokenSignature )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( protectedHeader, "protected" )
                BL_DM_DECLARE_COMPLEX_PROPERTY( unprotectedHeader, bl::dm::jose::SigningHeader )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( signature, "signature" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( protectedHeader )
                    BL_DM_IMPL_PROPERTY( unprotectedHeader )
                    BL_DM_IMPL_PROPERTY( signature )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( SignedTokenSignature )

            BL_DM_DEFINE_PROPERTY( SignedTokenSignature, protectedHeader )
            BL_DM_DEFINE_PROPERTY( SignedTokenSignature, unprotectedHeader )
            BL_DM_DEFINE_PROPERTY( SignedTokenSignature, signature )

            /*
             * @brief Class SignedToken
             */

            BL_DM_DEFINE_CLASS_BEGIN( SignedToken )

                BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( payload, "payload" )
                BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( signatures, bl::dm::jose::SignedTokenSignature )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( payload )
                    BL_DM_IMPL_PROPERTY( signatures )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( SignedToken )

            BL_DM_DEFINE_PROPERTY( SignedToken, payload )
            BL_DM_DEFINE_PROPERTY( SignedToken, signatures )

            /*
             * @brief Class EncryptedTokenRecepient
             */

            BL_DM_DEFINE_CLASS_BEGIN( EncryptedTokenRecepient )

                BL_DM_DECLARE_COMPLEX_PROPERTY( unprotectedHeader, bl::dm::jose::EncryptionHeader )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( encryptedKey, "encrypted_key" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( unprotectedHeader )
                    BL_DM_IMPL_PROPERTY( encryptedKey )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( EncryptedTokenRecepient )

            BL_DM_DEFINE_PROPERTY( EncryptedTokenRecepient, unprotectedHeader )
            BL_DM_DEFINE_PROPERTY( EncryptedTokenRecepient, encryptedKey )

            /*
             * @brief Class EncryptedToken
             */

            BL_DM_DEFINE_CLASS_BEGIN( EncryptedToken )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( protectedHeader, "protected" )
                BL_DM_DECLARE_COMPLEX_PROPERTY( unprotectedHeader, bl::dm::jose::EncryptionHeader )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( initializationVector, "iv" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( additionalAuthenticatedData, "aad" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( cipherText, "ciphertext" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( tag, "tag" )
                BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( recipients, bl::dm::jose::EncryptedTokenRecepient )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( protectedHeader )
                    BL_DM_IMPL_PROPERTY( unprotectedHeader )
                    BL_DM_IMPL_PROPERTY( initializationVector )
                    BL_DM_IMPL_PROPERTY( additionalAuthenticatedData )
                    BL_DM_IMPL_PROPERTY( cipherText )
                    BL_DM_IMPL_PROPERTY( tag )
                    BL_DM_IMPL_PROPERTY( recipients )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( EncryptedToken )

            BL_DM_DEFINE_PROPERTY( EncryptedToken, protectedHeader )
            BL_DM_DEFINE_PROPERTY( EncryptedToken, unprotectedHeader )
            BL_DM_DEFINE_PROPERTY( EncryptedToken, initializationVector )
            BL_DM_DEFINE_PROPERTY( EncryptedToken, additionalAuthenticatedData )
            BL_DM_DEFINE_PROPERTY( EncryptedToken, cipherText )
            BL_DM_DEFINE_PROPERTY( EncryptedToken, tag )
            BL_DM_DEFINE_PROPERTY( EncryptedToken, recipients )

        } // jose

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_JOSE_H_ */
