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

#ifndef __BL_CRYPTO_BIGNUMBASE64URL_H_
#define __BL_CRYPTO_BIGNUMBASE64URL_H_

#include <baselib/core/SerializationUtils.h>
#include <baselib/crypto/ErrorHandling.h>
#include <baselib/crypto/OpenSSLTypes.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace crypto
    {
        /**
         * @brief BignumBase64Url - class for BIGNUM <-> base64Url conversions
         */

        template
        <
            typename E = void
        >
        class BignumBase64UrlT
        {
        public:

            static std::string bignumToBase64Url( SAA_in const BIGNUM* bigNumber )
            {
                const auto length = BN_num_bytes( bigNumber );
                std::unique_ptr< unsigned char[] > buffer( new unsigned char[ length ] );

                BL_CHK_CRYPTO_API_NM( ::BN_bn2bin( bigNumber, buffer.get() ) );

                return SerializationUtils::base64UrlEncode(
                    static_cast< const void* >( buffer.get() ),
                    length
                    );
            }

            static std::string bignumToBase64Url( SAA_in const bignum_ptr_t& bigNumber )
            {
                return bignumToBase64Url( bigNumber.get() );
            }

            static bignum_ptr_t base64UrlToBignum( SAA_in const std::string& base64Url )
            {
                typedef std::basic_string< unsigned char > unsigned_string_t;

                const auto buffer = SerializationUtils::base64UrlDecodeT< unsigned_string_t >( base64Url );

                auto result = bignum_ptr_t::attach(
                    ::BN_bin2bn(
                        buffer.c_str(),
                        static_cast< int >( buffer.size() ),
                        nullptr
                        )
                    );

                BL_CHK_CRYPTO_API_NM( result );

                return result;
            }
        };

        typedef BignumBase64UrlT<> BignumBase64Url;

    } // crypto

} // bl

#endif /* __BL_CRYPTO_BIGNUMBASE64URL_H_ */
