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

#ifndef __BL_CRYPTO_OPENSSLTYPES_H_
#define __BL_CRYPTO_OPENSSLTYPES_H_

#include <baselib/core/BaseIncludes.h>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

namespace bl
{
    namespace crypto
    {
        namespace detail
        {

            class BigNumDeleter
            {
            public:

                void operator ()( SAA_in ::BIGNUM* bignum ) const NOEXCEPT
                {
                    ( void ) ::BN_free( bignum );
                }
            };

            class BioDeleter
            {
            public:

                void operator ()( SAA_in ::BIO* bio ) const NOEXCEPT
                {
                    ( void ) ::BIO_free_all( bio );
                }
            };

            class OpenSslFree
            {
            public:

                void operator ()( SAA_in void* ptr ) const NOEXCEPT
                {
                    ( void ) ::CRYPTO_free( ptr );
                }
            };

            class RsaDeleter
            {
            public:

                void operator ()( SAA_in ::RSA* rsa ) const NOEXCEPT
                {
                    ( void ) ::RSA_free( rsa );
                }
            };

            class X509CertDeleter
            {
            public:

                void operator ()( SAA_in ::X509* cert ) const NOEXCEPT
                {
                    ( void ) ::X509_free( cert );
                }
            };

            class EvpPkeyDeleter
            {
            public:

                void operator ()( SAA_in ::EVP_PKEY* key ) const NOEXCEPT
                {
                    ( void ) ::EVP_PKEY_free( key );
                }
            };

        } // detail

        typedef cpp::SafeUniquePtr< ::BIGNUM, detail::BigNumDeleter >                   bignum_ptr_t;
        typedef cpp::SafeUniquePtr< ::BIO, detail::BioDeleter >                         bio_ptr_t;
        typedef cpp::SafeUniquePtr< ::RSA, detail::RsaDeleter >                         rsakey_ptr_t;
        typedef cpp::SafeUniquePtr< ::X509, detail::X509CertDeleter >                   x509cert_ptr_t;
        typedef cpp::SafeUniquePtr< ::EVP_PKEY, detail::EvpPkeyDeleter >                evppkey_ptr_t;
        typedef cpp::SafeUniquePtr< char[], detail::OpenSslFree >                       openssl_string_ptr_t;

    } // crypto

} // bl

#endif /* __BL_CRYPTO_OPENSSLTYPES_H_ */
