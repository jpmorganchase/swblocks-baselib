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

#ifndef __BL_CRYPTO_X509CERT_H_
#define __BL_CRYPTO_X509CERT_H_

#include <baselib/crypto/RsaKey.h>
#include <baselib/crypto/ErrorHandling.h>
#include <baselib/crypto/OpenSSLTypes.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace crypto
    {
        namespace detail
        {
            /**
             * @brief class X509Cert
             */

            template
            <
                typename E = void
            >
            class X509CertT
            {
                BL_DECLARE_STATIC( X509CertT )

            public:

                static evppkey_ptr_t createPrivateKey()
                {
                    evppkey_ptr_t evpPkey;

                    BL_CHK_CRYPTO_API_NM(
                        evpPkey = evppkey_ptr_t::attach( ::EVP_PKEY_new() )
                        );

                    const auto rsaKey = RsaKey::createInstance();
                    rsaKey -> generate();

                    BL_CHK_CRYPTO_API_NM(
                        ::EVP_PKEY_assign_RSA( evpPkey.get(), &rsaKey -> get() )
                        );

                    /*
                     * ::EVP_PKEY_assign_RSA() above succeeded, so evpPkey will be cleaning up
                     * the memory of the ::RSA* pointer, and we are releasing it from the rsaKey
                     */

                    ( void ) rsaKey -> releaseRsa();

                    return evpPkey;
                }

                static x509cert_ptr_t createSelfSignedX509Cert(
                    SAA_in          const evppkey_ptr_t&                    evpPkey,
                    SAA_in          const std::string&                      country,
                    SAA_in          const std::string&                      organization,
                    SAA_in          const std::string&                      commonName,
                    SAA_in          const int                               serial,
                    SAA_in          const int                               daysValid
                    )
                {
                    x509cert_ptr_t x509cert;

                    BL_CHK_CRYPTO_API_NM(
                        x509cert = x509cert_ptr_t::attach( ::X509_new() )
                        );

                    /*
                     * to set X509 version 3 we need to pass 2 to ::X509_set_version
                     */

                    const long version3 = 2;

                    BL_CHK_CRYPTO_API_NM(
                        ::X509_set_version( x509cert.get(), version3 )
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::ASN1_INTEGER_set(
                            ::X509_get_serialNumber( x509cert.get() ),
                            serial )
                            );

                    BL_CHK_CRYPTO_API_NM(
                        ::X509_gmtime_adj( X509_get_notBefore( x509cert.get() ), 0 )
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::X509_gmtime_adj( X509_get_notAfter( x509cert.get() ), 60 * 60 * 24 * daysValid )
                        );

                    ::X509_NAME* x509name = nullptr;

                    BL_CHK_CRYPTO_API_NM(
                        x509name = ::X509_get_subject_name( x509cert.get() )
                        );

                    addEntryByText( x509name, "C", country );

                    addEntryByText( x509name, "O", organization );

                    addEntryByText( x509name, "CN", commonName );

                    BL_CHK_CRYPTO_API_NM(
                        X509_set_issuer_name( x509cert.get(), x509name )
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::X509_set_pubkey( x509cert.get(), evpPkey.get() )
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::X509_sign( x509cert.get(), evpPkey.get(), ::EVP_sha512() )
                        );

                    return x509cert;
                }

                static std::string getEvpPkeyAsPemString( SAA_in const evppkey_ptr_t& evpPkey )
                {
                    const auto bioBuffer = bio_ptr_t::attach( ::BIO_new( ::BIO_s_mem() ) );
                    BL_CHK_CRYPTO_API_NM( bioBuffer );

                    BL_CHK_CRYPTO_API_NM(
                        ::PEM_write_bio_PrivateKey(
                            bioBuffer.get(),
                            evpPkey.get(),
                            nullptr,    /* No encryption */
                            nullptr,    /* Key data */
                            0,          /* Key length */
                            nullptr,    /* Password callback */
                            nullptr     /* Password */
                            )
                        );

                    return bioBufferToString( bioBuffer );
                }

                static std::string geX509CertAsPemString( SAA_in const x509cert_ptr_t& x509cert )
                {
                    const auto bioBuffer = bio_ptr_t::attach( ::BIO_new( ::BIO_s_mem() ) );
                    BL_CHK_CRYPTO_API_NM( bioBuffer );

                    BL_CHK_CRYPTO_API_NM(
                        ::PEM_write_bio_X509(
                            bioBuffer.get(),
                            x509cert.get()
                            )
                        );

                    return bioBufferToString( bioBuffer );
                }

            private:

                static void addEntryByText(
                    SAA_in          ::X509_NAME*                            x509name,
                    SAA_in          const std::string&                      field,
                    SAA_in          const std::string&                      fieldValue
                    )
                {
                    BL_CHK_CRYPTO_API_NM(
                        ::X509_NAME_add_entry_by_txt(
                            x509name,
                            field.c_str(),
                            MBSTRING_ASC,
                            reinterpret_cast< const unsigned char* >( fieldValue.c_str() ),
                            -1,     /* length,         -1: calculate using strlen */
                            -1,     /* location index, -1: append */
                            0       /* set,             0: create new RDN */
                            )
                        );
                }

                static std::string bioBufferToString( const bio_ptr_t& bioBuffer )
                {
                    const int length = BIO_pending( bioBuffer.get() );

                    const auto pemBytes = cpp::SafeUniquePtr< char[] >::attach( new char[ length ] );

                    BL_CHK_CRYPTO_API_NM(
                        ::BIO_read( bioBuffer.get(), pemBytes.get(), length )
                        );

                    return std::string( pemBytes.get(), length );
                }
            };

            typedef X509CertT<> X509Cert;

        } // detail

        inline evppkey_ptr_t createPrivateKey()
        {
            return detail::X509Cert::createPrivateKey();
        }

        inline x509cert_ptr_t createSelfSignedX509Cert(
            SAA_in          const evppkey_ptr_t&                    evpPkey,
            SAA_in          const std::string&                      country,
            SAA_in          const std::string&                      organization,
            SAA_in          const std::string&                      commonName,
            SAA_in          const int                               serial,
            SAA_in          const int                               daysValid
            )
        {
            return detail::X509Cert::createSelfSignedX509Cert( evpPkey, country, organization, commonName, serial, daysValid );
        }

        inline std::string getEvpPkeyAsPemString( SAA_in const evppkey_ptr_t& evpPkey )
        {
            return detail::X509Cert::getEvpPkeyAsPemString( evpPkey );
        }

        inline std::string geX509CertAsPemString( SAA_in const x509cert_ptr_t& x509cert )
        {
            return detail::X509Cert::geX509CertAsPemString( x509cert );
        }

    } // crypto

} // bl

#endif /* __BL_CRYPTO_X509CERT_H_ */
