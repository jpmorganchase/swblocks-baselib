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

#ifndef __BL_CRYPTO_RSAKEY_H_
#define __BL_CRYPTO_RSAKEY_H_

#include <baselib/core/ObjModel.h>

#include <baselib/crypto/BignumBase64Url.h>
#include <baselib/crypto/CryptoBase.h>
#include <baselib/crypto/ErrorHandling.h>
#include <baselib/crypto/OpenSSLTypes.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace crypto
    {
        template
        <
            typename E = void
        >
        class RsaKeyT :
            public CryptoBase,
            public om::ObjectDefaultBase
        {
        public:

            enum
            {
                /*
                 * 2K for RSA key size should be good enough
                 */

                RSA_KEY_SIZE_DEFAULT                            = 2048,

                /*
                 * RSA_F4 is defined as 0x10001L (65537) in rsa.h
                 *
                 * The exponent must not be too small (e.g. 3) otherwise
                 * the key may be vulnerable to certain attacks:
                 *
                 * http://en.wikipedia.org/wiki/Coppersmith%27s_Attack
                 */

                RSA_KEY_EXPONENT_DEFAULT                        = RSA_F4,
            };

        private:

            rsakey_ptr_t m_rsaKey;

        protected:

            RsaKeyT()
            {
                BL_CHK_CRYPTO_API_NM(
                    m_rsaKey = rsakey_ptr_t::attach( ::RSA_new() )
                    );
            }

            RsaKeyT( SAA_in rsakey_ptr_t&& rsaKey )
                : m_rsaKey( BL_PARAM_FWD( rsaKey ) )
            {
            }

        public:

            void generate()
            {
                bignum_ptr_t exponent = nullptr;

                BL_CHK_CRYPTO_API_NM(
                    exponent = bignum_ptr_t::attach( ::BN_new() )
                    );

                BL_CHK_CRYPTO_API_NM(
                    ::BN_set_word(
                        exponent.get(),
                        RSA_KEY_EXPONENT_DEFAULT
                        ) == 1
                    );

                BL_CHK_CRYPTO_API_NM(
                    ::RSA_generate_key_ex(
                        m_rsaKey.get(),
                        RSA_KEY_SIZE_DEFAULT,
                        exponent.get(),
                        nullptr /* cb_arg */
                        ) == 1
                    );
            }

            ::RSA& get() NOEXCEPT
            {
                return *m_rsaKey;
            }

            const ::RSA& get() const NOEXCEPT
            {
                return *m_rsaKey;
            }

            ::RSA* releaseRsa()
            {
                return m_rsaKey.release();
            }
        };

        typedef om::ObjectImpl< RsaKeyT<> > RsaKey;

    } // crypto

} // bl

#endif /* __BL_CRYPTO_RSAKEY_H_ */
