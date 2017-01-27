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

#ifndef __BL_CRYPTO_TRUSTEDROOTS_H_
#define __BL_CRYPTO_TRUSTEDROOTS_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace crypto
    {
        namespace detail
        {
            /**
             * @brief class TrustedRoots - The storage and configuration for trusted root certificates
             */

            template
            <
                typename E = void
            >
            class TrustedRootsT
            {
                BL_DECLARE_STATIC( TrustedRootsT )

            private:

                static std::set< std::string > g_trustedRoots;

            public:

                static void initGlobalTrustedRoots()
                {
                    /*
                     * The CA roots are also checked in source control in the following location:
                     * certs\CAs
                     */

                    {
                        /*
                         * VeriSign-Class-3-Public-Primary-Certification-Authority.crt
                         */

                        const auto certificatePemText =
                            "-----BEGIN CERTIFICATE-----\n"
                            "MIICPDCCAaUCEDyRMcsf9tAbDpq40ES/Er4wDQYJKoZIhvcNAQEFBQAwXzELMAkG\n"
                            "A1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMTcwNQYDVQQLEy5DbGFz\n"
                            "cyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0aG9yaXR5MB4XDTk2\n"
                            "MDEyOTAwMDAwMFoXDTI4MDgwMjIzNTk1OVowXzELMAkGA1UEBhMCVVMxFzAVBgNV\n"
                            "BAoTDlZlcmlTaWduLCBJbmMuMTcwNQYDVQQLEy5DbGFzcyAzIFB1YmxpYyBQcmlt\n"
                            "YXJ5IENlcnRpZmljYXRpb24gQXV0aG9yaXR5MIGfMA0GCSqGSIb3DQEBAQUAA4GN\n"
                            "ADCBiQKBgQDJXFme8huKARS0EN8EQNvjV69qRUCPhAwL0TPZ2RHP7gJYHyX3KqhE\n"
                            "BarsAx94f56TuZoAqiN91qyFomNFx3InzPRMxnVx0jnvT0Lwdd8KkMaOIG+YD/is\n"
                            "I19wKTakyYbnsZogy1Olhec9vn2a/iRFM9x2Fe0PonFkTGUugWhFpwIDAQABMA0G\n"
                            "CSqGSIb3DQEBBQUAA4GBABByUqkFFBkyCEHwxWsKzH4PIRnN5GfcX6kb5sroc50i\n"
                            "2JhucwNhkcV8sEVAbkSdjbCxlnRhLQ2pRdKkkirWmnWXbj9T/UWZYB2oK0z5XqcJ\n"
                            "2HUw19JlYD1n1khVdWk/kfVIC0dpImmClr7JyDiGSnoscxlIaU5rfGW/D/xwzoiQ\n"
                            "-----END CERTIFICATE-----\n";

                        registerTrustedRoot( certificatePemText );
                    }
                }

                static auto trustedRoots() NOEXCEPT -> const std::set< std::string >&
                {
                    return g_trustedRoots;
                }

                static void registerTrustedRoot( SAA_in std::string&& certificatePemText )
                {
                    BL_CHK(
                        false,
                        g_trustedRoots.emplace( BL_PARAM_FWD( certificatePemText ) ).second,
                        BL_MSG()
                            << "Trusted root key is already registered"
                        );
                }
            };

            typedef TrustedRootsT<> TrustedRoots;

            BL_DEFINE_STATIC_MEMBER( TrustedRootsT, std::set< std::string >, g_trustedRoots );

        } // detail

        inline auto trustedRoots() NOEXCEPT -> const std::set< std::string >&
        {
            const auto& roots = detail::TrustedRoots::trustedRoots();

            if( roots.empty() )
            {
                BL_RIP_MSG( "Trusted root certificates are not initialized" );
            }

            return roots;
        }

        inline void registerTrustedRoot( SAA_in std::string&& certificatePemText )
        {
            detail::TrustedRoots::registerTrustedRoot( BL_PARAM_FWD( certificatePemText ) );
        }

    } // crypto

} // bl

#endif /* __BL_CRYPTO_TRUSTEDROOTS_H_ */
