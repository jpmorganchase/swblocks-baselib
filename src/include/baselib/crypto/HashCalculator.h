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

#ifndef __BL_CRYPTO_HASHCALCULATOR_H_
#define __BL_CRYPTO_HASHCALCULATOR_H_

#include <baselib/crypto/ErrorHandling.h>
#include <baselib/crypto/CryptoBase.h>

#include <baselib/core/BaseIncludes.h>

#include <openssl/sha.h>

namespace bl
{
    namespace hash
    {
        namespace detail
        {
            /*
             * @brief class Sha512CalculatorImpl
             */

            template
            <
                typename E = void
            >
            class Sha512CalculatorImplT
            {
                BL_NO_COPY_OR_MOVE( Sha512CalculatorImplT )

            private:

                std::uint8_t     m_digest[ SHA512_DIGEST_LENGTH ];
                SHA512_CTX       m_context;

            public:

                Sha512CalculatorImplT()
                {
                    BL_CHK_CRYPTO_API_NM( SHA512_Init( &m_context ) );
                }

                void update(
                    SAA_in      const void*              buffer,
                    SAA_in      const std::size_t        size
                    )
                {
                    BL_CHK_CRYPTO_API_NM( SHA512_Update( &m_context, buffer, size ) );
                }

                void finalize()
                {
                    BL_CHK_CRYPTO_API_NM( SHA512_Final( m_digest, &m_context ) );
                }

                const std::uint8_t* digest() const NOEXCEPT
                {
                    return m_digest;
                }

                CONSTEXPR static std::size_t digestSize() NOEXCEPT
                {
                    return SHA512_DIGEST_LENGTH;
                }

                CONSTEXPR static std::size_t id() NOEXCEPT
                {
                    return NID_sha512;
                }
            };

            typedef Sha512CalculatorImplT<> Sha512CalculatorImpl;

            /*
             * @brief class Sha384CalculatorImpl
             */

            template
            <
                typename E = void
            >
            class Sha384CalculatorImplT
            {
                BL_NO_COPY_OR_MOVE( Sha384CalculatorImplT )

            private:

                std::uint8_t     m_digest[ SHA384_DIGEST_LENGTH ];
                SHA512_CTX       m_context;

            public:

                Sha384CalculatorImplT()
                {
                    BL_CHK_CRYPTO_API_NM( SHA384_Init( &m_context ) );
                }

                void update(
                    SAA_in      const void*              buffer,
                    SAA_in      const std::size_t        size
                    )
                {
                    BL_CHK_CRYPTO_API_NM( SHA384_Update( &m_context, buffer, size ) );
                }

                void finalize()
                {
                    BL_CHK_CRYPTO_API_NM( SHA384_Final( m_digest, &m_context ) );
                }

                const std::uint8_t* digest() const NOEXCEPT
                {
                    return m_digest;
                }

                CONSTEXPR static std::size_t digestSize() NOEXCEPT
                {
                    return SHA384_DIGEST_LENGTH;
                }

                CONSTEXPR static std::size_t id() NOEXCEPT
                {
                    return NID_sha384;
                }
            };

            typedef Sha384CalculatorImplT<> Sha384CalculatorImpl;

        } // detail

        /*
         * @brief class HashCalculator
         */

        template
        <
            typename T
        >
        class HashCalculator :
            public crypto::CryptoBase
        {
            BL_CTR_DEFAULT( HashCalculator, public )
            BL_NO_COPY_OR_MOVE( HashCalculator )

        private:

            T           m_hashCalculator;

        public:

            void update(
                SAA_in      const void*                 buffer,
                SAA_in      const std::size_t           size
                )
            {
                m_hashCalculator.update( buffer, size );
            }

            void finalize()
            {
                m_hashCalculator.finalize();
            }

            const std::uint8_t* digest() const NOEXCEPT
            {
                return m_hashCalculator.digest();
            }

            std::string digestStr() const
            {
                const auto digest = m_hashCalculator.digest();

                return uint8ToStr( digest, digestSize() );
            }

            static std::string uint8ToStr(
                SAA_in      const std::uint8_t*         data,
                SAA_in      const std::size_t           size
                )
            {
                cpp::SafeOutputStringStream oss;
                oss << std::hex << std::setfill( '0' );

                for( std::size_t count = 0; count < size; ++count )
                {
                    oss << std::setw( 2 ) << static_cast< unsigned >( data[ count ] );
                }

                return oss.str();
            }

            CONSTEXPR static std::size_t digestSize() NOEXCEPT
            {
                return T::digestSize();
            }

            CONSTEXPR static std::size_t id() NOEXCEPT
            {
                return T::id();
            }

            static std::string hash( SAA_in const std::string& text )
            {
                T hashCalculator;
                hashCalculator.update( text.c_str(), text.size() );
                hashCalculator.finalize();

                return uint8ToStr( hashCalculator.digest(), hashCalculator.digestSize() );
            }

            static std::string hash( SAA_in const std::set< std::string >& values )
            {
                T hashCalculator;

                for( const auto& value : values )
                {
                    hashCalculator.update( value.c_str(), value.size() );
                }

                hashCalculator.finalize();

                return uint8ToStr( hashCalculator.digest(), hashCalculator.digestSize() );
            }
        };

        typedef HashCalculator< detail::Sha512CalculatorImpl > HashCalculatorDefault;

    } // hash

} // bl

#endif /* __BL_CRYPTO_HASHCALCULATOR_H_ */
