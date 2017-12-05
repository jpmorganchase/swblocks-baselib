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

#ifndef __BL_CRYPTO_ERRORHANDLING_H_
#define __BL_CRYPTO_ERRORHANDLING_H_

#include <baselib/core/ErrorHandling.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

/*
 * Basic error handling support for OpenSSL and crypto code in general
 */

#define BL_CRYPTO_ERROR_DEFAULT_MSG "Cryptographic system error has occurred"

#define BL_CHK_CRYPTO_API_RESET_ERROR() \
    ( void ) ::ERR_clear_error(); \

#define BL_CHK_CRYPTO_API( call, msg ) \
    do \
    { \
        if( ! ( call ) ) \
        { \
            BL_THROW( bl::crypto::getException( msg ), msg ); \
        } \
    } \
    while( false ) \

#define BL_CHK_CRYPTO_API_NM( call ) \
    BL_CHK_CRYPTO_API( call, BL_CRYPTO_ERROR_DEFAULT_MSG )

namespace bl
{
    namespace crypto
    {
        namespace detail
        {
            /**
             * @brief class CryptoErrorCategory - an error category for crypto APIs
             */

            template
            <
                typename E = void
            >
            class CryptoErrorCategoryT : public eh::error_category
            {
                BL_NO_COPY_OR_MOVE( CryptoErrorCategoryT )

            public:

                CryptoErrorCategoryT() NOEXCEPT
                {
                }

                virtual const char* name() const NOEXCEPT OVERRIDE
                {
                    return "OpenSSL";
                }

                virtual std::string message( SAA_in int value ) const OVERRIDE
                {
                    /*
                     * Note that we are going to use ::ERR_reason_error_string API here
                     * instead of using ::ERR_error_string which formats the message as:
                     *
                     * error:[error code]:[library name]:[function name]:[reason string]
                     *
                     * The meaningful message is only the reason string
                     *
                     * The error code, library name and function name are implementation
                     * details and don't need to be formatted into the message string
                     */

                    const char* reason = ::ERR_reason_error_string( value );

                    if( ! reason )
                    {
                        /*
                         * This check is to guard for weird cases where the error string
                         * can't be loaded due to some part of the code unloading the
                         * strings by calling ::ERR_free_strings
                         */

                        loadErrorStrings();

                        reason = ::ERR_reason_error_string( value );
                    }

                    return reason ? reason : "OpenSSL error";
                }

                static void loadErrorStrings()
                {
                    ( void ) ::SSL_load_error_strings();
                }
            };

            typedef CryptoErrorCategoryT<> CryptoErrorCategory;

            /**
             * @brief class ErrorHandling
             */

            template
            <
                typename E = void
            >
            class ErrorHandlingT
            {
                BL_DECLARE_STATIC( ErrorHandlingT )

            private:

                static const eh::error_category& g_errorCategory;

                static const eh::error_category& initCryptoErrorHandling()
                {
                    static const CryptoErrorCategory g_category;

                    CryptoErrorCategory::loadErrorStrings();

                    return g_category;
                }

            public:

                static auto getErrorCategory() -> const eh::error_category&
                {
                    return g_errorCategory;
                }

                static auto getFirstError( SAA_in_opt const bool clear = false ) -> eh::error_code
                {
                    return eh::error_code(
                        static_cast< int >( clear ? ::ERR_get_error() : ::ERR_peek_error() ),
                        getErrorCategory()
                        );
                }

                static auto getAndClearErrorQueue() -> std::vector< eh::error_code >
                {
                    std::vector< eh::error_code > queue;

                    for( ;; )
                    {
                        const auto value = static_cast< int >( ::ERR_get_error() );

                        if( ! value )
                        {
                            break;
                        }

                        queue.push_back( eh::error_code( value, getErrorCategory() ) );
                    }

                    return queue;
                }

                /**
                 * @brief Obtains the full exception chain from the OpenSSL "error queue"
                 */

                static auto getException(
                    SAA_in          const std::string&              message,
                    SAA_in_opt      const eh::error_code&           firstEC = getFirstError( true /* clear */ )
                    )
                    -> SystemException
                {
                    auto exception = SystemException::create( firstEC, message );

                    if( ::ERR_peek_error() )
                    {
                        /*
                         * There are more errors in the "error queue" - capture all of them
                         * and convert them into an exception chain
                         */

                        std::exception_ptr eptr = nullptr;
                        const auto queue = getAndClearErrorQueue();

                        BL_ASSERT( queue.size() );
                        BL_ASSERT( ! ::ERR_peek_error() );

                        for( auto i = queue.crbegin(); i != queue.crend(); ++i )
                        {
                            auto chainedException = SystemException::create( *i, message );

                            if( eptr )
                            {
                                chainedException << eh::errinfo_nested_exception_ptr( eptr );
                            }

                            eptr = std::make_exception_ptr( std::move( chainedException ) );
                        }

                        BL_ASSERT( eptr );

                        exception << eh::errinfo_nested_exception_ptr( eptr );
                    }

                    return exception;
                }
            };

            template
            <
                typename E
            >
            const eh::error_category&
            ErrorHandlingT< E >::g_errorCategory = ErrorHandlingT< E >::initCryptoErrorHandling();

            typedef ErrorHandlingT<> ErrorHandling;

            inline auto getErrorCategory() -> const eh::error_category&
            {
                return detail::ErrorHandling::getErrorCategory();
            }

            inline auto getFirstError( SAA_in const bool clear = false ) -> eh::error_code
            {
                return detail::ErrorHandling::getFirstError( clear );
            }

            inline auto getAndClearErrorQueue() -> std::vector< eh::error_code >
            {
                return detail::ErrorHandling::getAndClearErrorQueue();
            }

        } // detail

        inline auto getException(
            SAA_in          const std::string&              message,
            SAA_in_opt      const eh::error_code&           firstEC = detail::getFirstError( true /* clear */ )
            )
            -> SystemException
        {
            return detail::ErrorHandling::getException( message, firstEC );
        }

    } // crypto

} // bl

#endif /* __BL_CRYPTO_ERRORHANDLING_H_ */
