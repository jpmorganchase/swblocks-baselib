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

#ifndef __BL_CRYPTO_RSASIGNVERIFY_H_
#define __BL_CRYPTO_RSASIGNVERIFY_H_

#include <baselib/crypto/HashCalculator.h>
#include <baselib/crypto/RsaKey.h>

#include <baselib/core/BaseIncludes.h>

#include <openssl/rsa.h>
#include <openssl/sha.h>

namespace bl
{
    namespace crypto
    {
        /**
         * @brief class RsaSignVerify
         */

        template
        <
            typename E = void
        >
        class RsaSignVerifyT
        {
        public:

            static std::unique_ptr< unsigned char[] > sign(
                SAA_in      const om::ObjPtr< RsaKey >&                   rsaKey,
                SAA_in      const std::string&                            message,
                SAA_inout   unsigned&                                     signatureSize
                )
            {
                hash::HashCalculatorDefault hashCalculator;

                hashCalculator.update(
                    message.c_str(),
                    message.size()
                    );

                hashCalculator.finalize();

                const auto* digest = hashCalculator.digest();

                std::unique_ptr< unsigned char[] > signatureBuffer(
                    new unsigned char[ ::RSA_size( &rsaKey -> get() ) ]
                    );

                BL_CHK_CRYPTO_API_NM(
                    ::RSA_sign(
                        static_cast< int >( hashCalculator.id() ),
                        digest,
                        static_cast< int >( hashCalculator.digestSize() ),
                        signatureBuffer.get(),
                        &signatureSize,
                        &rsaKey -> get()
                        )
                    );

                return signatureBuffer;
            }

            static std::string signAsBase64Url(
                SAA_in  const om::ObjPtr< RsaKey >&                   rsaKey,
                SAA_in  const std::string&                            message
                )
            {
                unsigned signatureSize = 0u;

                const auto signatureBuffer = sign(
                    rsaKey,
                    message,
                    signatureSize
                    );

                return SerializationUtils::base64UrlEncode(
                    signatureBuffer.get(),
                    signatureSize
                    );
            }

            /*
             * @brief Returns `true` is verification is successful, `false` otherwise.
             *        For detailed information about failure reason call `bl::crypto::getException`
             */

            static bool tryVerify(
                SAA_in  const om::ObjPtr< RsaKey >&                     rsaKey,
                SAA_in  const std::string&                              message,
                SAA_in  const std::string&                              signatureBase64Url
                )
            {
                hash::HashCalculatorDefault hashCalculator;

                hashCalculator.update(
                    message.c_str(),
                    message.size()
                    );

                hashCalculator.finalize();

                const auto* digest = hashCalculator.digest();

                typedef std::basic_string< unsigned char > ustring_t;
                const auto signature = SerializationUtils::base64UrlDecodeT< ustring_t >( signatureBase64Url );

                return ::RSA_verify(
                    static_cast< int >( hashCalculator.id() ),
                    digest,
                    static_cast< int >( hashCalculator.digestSize() ),
                    signature.c_str(),
                    static_cast< int >( signature.size() ),
                    &rsaKey -> get()
                    ) == 1;
            }

            /*
             * @brief Throws an exception if verification fails
             */

            static void verify(
                SAA_in  const om::ObjPtr< RsaKey >&                     rsaKey,
                SAA_in  std::string                                     message,
                SAA_in  const std::string&                              signatureBase64Url
                )
            {
                BL_CHK_CRYPTO_API_NM(
                    tryVerify(
                        rsaKey,
                        message,
                        signatureBase64Url
                        )
                    );
            }
        };

        typedef RsaSignVerifyT<> RsaSignVerify;

    } // crypto

} // bl

#endif /* __BL_CRYPTO_RSASIGNVERIFY_H_ */
