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

#ifndef __BL_CRYPTO_HMACSHA256_H_
#define __BL_CRYPTO_HMACSHA256_H_

#include <baselib/core/CPP.h>
#include <baselib/core/ErrorHandling.h>
#include <baselib/core/SerializationUtils.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

namespace bl
{
    namespace crypto
    {
        namespace detail
        {
            /**
             * @brief class HmacSha256
             */

            template
            <
                typename E = void
            >
            class HmacSha256T FINAL
            {
                BL_DECLARE_STATIC( HmacSha256T )

            public:

                static std::string calculateMessageDigest(
                    SAA_in  const std::string&                              message,
                    SAA_in  const std::string&                              key
                    )
                {
                    unsigned char messageDigest[ SHA256_DIGEST_LENGTH ];
                    unsigned int digestLength = sizeof( messageDigest );

                    ::HMAC_CTX ctx;

                    ( void ) ::HMAC_CTX_init( &ctx );

                    BL_SCOPE_EXIT(
                        {
                            ( void ) ::HMAC_CTX_cleanup( &ctx );
                        }
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::HMAC_Init_ex(
                            &ctx,
                            key.c_str(),
                            static_cast< int >( key.length() ),
                            EVP_sha256(),
                            nullptr
                            )
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::HMAC_Update(
                            &ctx,
                            reinterpret_cast< const unsigned char* >( message.c_str() ),
                            message.length()
                            )
                        );

                    BL_CHK_CRYPTO_API_NM(
                        ::HMAC_Final(
                            &ctx,
                            messageDigest,
                            &digestLength
                            )
                        );

                    cpp::SafeOutputStringStream stream;

                    stream
                        << std::hex
                        << std::setfill( '0' )
                        << std::uppercase;

                    for( std::size_t count = 0; count < digestLength; ++count )
                    {
                        stream
                            << std::setw( 2 )
                            << static_cast< unsigned >( messageDigest[ count ] );
                    }

                    return stream.str();
                }
            };

            typedef HmacSha256T< > HmacSha256;

        } // detail

    } // crypto

} // bl

#endif /* __BL_CRYPTO_HMACSHA256_H_ */
