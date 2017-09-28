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

#ifndef __BL_CRYPTO_CRYPTOBASE_H_
#define __BL_CRYPTO_CRYPTOBASE_H_

#include <baselib/crypto/OpenSSLTypes.h>
#include <baselib/crypto/ErrorHandling.h>
#include <baselib/crypto/TrustedRoots.h>

#include <baselib/core/AsioSSL.h>
#include <baselib/core/Random.h>
#include <baselib/core/BaseIncludes.h>

#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>

namespace bl
{
    namespace crypto
    {
        namespace detail
        {
            /**
             * @brief class CryptoInit
             *
             * OpenSSL is initialized from many places already (e.g. boost asio, thrift),
             * but we need to also provide an initializer for the case where it is used
             * without any of these external libraries
             */

            template
            <
                typename E = void
            >
            class CryptoInitT
            {
                BL_DECLARE_STATIC( CryptoInitT )

            private:

                static asio::ssl::context*                      g_sslContext;
                static os::mutex*                               g_locks;
                static int                                      g_lockCount;

                static std::map< std::string, std::string >     g_untrustedEndpointsInfo;
                static os::mutex                                g_untrustedEndpointsInfoLock;
                static bool                                     g_isEnableTlsV10;

                static void initRandomEngine()
                {
                    /*
                     * SSL would use /dev/urandom to seed its RNG. We want to avoid this due
                     * to reasons mentioned in RandomBoostImports.h hence do our own seeding here
                     */

                    unsigned char buffer[ 2048 ];

                    random::getRandomBytes( buffer, sizeof( buffer ) );

                    ( void ) ::RAND_seed( buffer, sizeof( buffer ) );

                    BL_CHK_CRYPTO_API_NM( ::RAND_status() );
                }

                static void callbackLocking(
                    SAA_in              int                                     mode,
                    SAA_in              int                                     lockId,
                    SAA_in              const char*                             file,
                    SAA_in              int                                     line
                    )
                {
                    BL_UNUSED( file );
                    BL_UNUSED( line );

                    BL_NOEXCEPT_BEGIN()

                    if( ! g_locks )
                    {
                        BL_RIP_MSG( "OpenSSL library was not initialized properly" );
                    }

                    if( lockId < 0 || lockId >= g_lockCount )
                    {
                        const auto msg = resolveMessage(
                            BL_MSG()
                                << "Invalid lockId "
                                << lockId
                                << " passed to callbackLocking(); [g_lockCount="
                                << g_lockCount
                                << "]"
                            );

                        BL_RIP_MSG( msg.c_str() );
                    }

                    if( mode & CRYPTO_LOCK )
                    {
                        g_locks[ lockId ].lock();
                    }
                    else
                    {
                        g_locks[ lockId ].unlock();
                    }

                    BL_NOEXCEPT_END()
                }

            public:

                static void loadTrustedRootFromPem(
                    SAA_inout           ::SSL_CTX*                              nativeSslContext,
                    SAA_in              const std::string&                      pemKeyText
                    )
                {
                    BL_ASSERT( nativeSslContext );

                    X509_STORE* store = ::SSL_CTX_get_cert_store( nativeSslContext );

                    BL_CHK(
                        nullptr,
                        store,
                        BL_MSG()
                           << "Unable to obtain certificate store from SSL context"
                        );

                    const auto buffer = bio_ptr_t::attach(
                        ::BIO_new_mem_buf(
                            const_cast< char* >( pemKeyText.c_str() ),
                            static_cast< int >( pemKeyText.size() )
                            )
                        );

                    BL_CHK_CRYPTO_API_NM( buffer );

                    const auto x509cert = x509cert_ptr_t::attach(
                        ::PEM_read_bio_X509_AUX(
                            buffer.get(),
                            nullptr                 /* X509 certificate out pointer (**) */,
                            nullptr                 /* Password callback */,
                            nullptr                 /* Password bytes */
                            )
                        );

                    BL_CHK_CRYPTO_API_NM( x509cert );

                    BL_CHK_CRYPTO_API_NM( ::X509_STORE_add_cert( store, x509cert.get() ) );
                }

                static void loadAllKnownCertificateAuthorities( SAA_inout ::SSL_CTX* nativeSslContext )
                {
                    BL_ASSERT( nativeSslContext );

                    /*
                     * Load certificate authorities used for verification
                     *
                     * In the new versions of boost we can simply call
                     * asio::ssl::context::add_certificate_authority() method, but since in the one
                     * we use (1.52) it is not provided, so we will have to call the relevant
                     * OpenSSL functions directly (the code is wrapped in loadTrustedRootFromPem)
                     */

                    for( const auto& certificatePemText : trustedRoots() )
                    {
                        loadTrustedRootFromPem( nativeSslContext, certificatePemText );
                    }
                }

                static void initNativeSslContext( SAA_inout ::SSL_CTX* nativeSslContext )
                {
                    auto options = ::SSL_CTX_get_options( nativeSslContext );

                    /*
                     * Disable most of the non-secure protocols (according to Wikipedia):
                     * http://en.wikipedia.org/wiki/Transport_Layer_Security#SSL_1.0.2C_2.0_and_3.0
                     *
                     * By default the configuration is secure - i.e. we basically want
                     * TLS 1.1 or higher
                     *
                     * Note that even TLS 1.0 is considered weak, but in some cases it might have
                     * to be allowed as some servers are not supporting TLS 1.1; we will allow the
                     * users of baselib to control this via g_isEnableTlsV10 global flag
                     *
                     * Note that we also allow for all bug workarounds via SSL_OP_ALL
                     */

                    options |= ( SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_ALL );

                    if( ! g_isEnableTlsV10 )
                    {
                        options |= SSL_OP_NO_TLSv1;
                    }

                    /*
                     * Ignore the return value because it is the new bitmask
                     */

                    ( void ) ::SSL_CTX_set_options( nativeSslContext, options );

                    ( void ) loadAllKnownCertificateAuthorities( nativeSslContext );
                }

                static void initSsl()
                {
                    /*
                     * According to the OpenSSL docs (https://www.openssl.org/docs/ssl/SSL_library_init.html)
                     * ::SSL_library_init() always returns 1, so the return value should not be checked
                     */

                    ( void ) ::SSL_library_init();

                    /*
                     * First register the lock callbacks and then initialize the global state
                     * (e.g. the random engine, context, etc)
                     */

                    const int lockCount = CRYPTO_num_locks();
                    BL_CHK_CRYPTO_API_NM( lockCount > 0 );

                    g_locks = new os::mutex[ lockCount ];
                    g_lockCount = lockCount;

                    CRYPTO_set_locking_callback( &callbackLocking );

                    initRandomEngine();

                    /*
                     * TODO: we need to load the root certificates here
                     * calling m_sslContext -> set_default_verify_paths() does not
                     * work because OpenSSL doesn't work natively with the MSFT CERT
                     * store on Windows
                     */

                    g_sslContext = new asio::ssl::context( asio::ssl::context::sslv23 );

                    initNativeSslContext( g_sslContext -> native_handle() );
                }

                static auto getAsioSslContext() NOEXCEPT -> asio::ssl::context&
                {
                    if( g_sslContext )
                    {
                        return *g_sslContext;
                    }

                    BL_RIP_MSG( "OpenSSL was not initialized properly" );
                }

                static auto createAsioSslServerContext(
                    SAA_in              const std::string&                  privateKeyPem,
                    SAA_in              const std::string&                  certificatePem
                    ) -> cpp::SafeUniquePtr< asio::ssl::context >
                {
                    auto context = cpp::SafeUniquePtr< asio::ssl::context >::attach(
                        new asio::ssl::context( asio::ssl::context::sslv23 )
                        );

                    initNativeSslContext( context -> native_handle() );

                    context -> use_private_key(
                        asio::const_buffer( privateKeyPem.data(), privateKeyPem.size() ),
                        boost::asio::ssl::context::pem
                        );

                    context -> use_certificate_chain(
                        asio::const_buffer( certificatePem.data(), certificatePem.size() )
                        );

                    return context;
                }

                static bool hasUntrustedEndpoints() NOEXCEPT
                {
                    BL_MUTEX_GUARD( g_untrustedEndpointsInfoLock );

                    return ! g_untrustedEndpointsInfo.empty();
                }

                static auto getUntrustedEndpointsInfo() -> std::map< std::string, std::string >
                {
                    std::map< std::string, std::string > result;

                    {
                        BL_MUTEX_GUARD( g_untrustedEndpointsInfoLock );

                        result = g_untrustedEndpointsInfo;
                    }

                    return result;
                }

                static void setUntrustedEndpointInfo(
                    SAA_in              std::string&&                       endpointId,
                    SAA_in              std::string&&                       info
                    )
                {
                    BL_MUTEX_GUARD( g_untrustedEndpointsInfoLock );

                    const auto pair =
                        g_untrustedEndpointsInfo.emplace( BL_PARAM_FWD( endpointId ), BL_PARAM_FWD( info ) );

                    if( pair.second /* true if added */ )
                    {
                        /*
                         * Note we can't use endpointId and info in the log message below
                         * because they have already been moved into the container
                         *
                         * We can use 'pair.first -> first/second' instead is an iterator
                         * to the inserted element (which is a pair since the container
                         * is a map)
                         */

                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "An SSL certificate sent from '"
                                << pair.first -> first /* endpointId */
                                << "' cannot be verified: "
                                << pair.first -> second /* error info */
                            );
                    }
                }

                static void clearUntrustedEndpointInfo( SAA_in const std::string& endpointId )
                {
                    BL_MUTEX_GUARD( g_untrustedEndpointsInfoLock );

                    if( g_untrustedEndpointsInfo.empty() )
                    {
                        return;
                    }

                    /*
                     * Erase returns the # of elements which were deleted
                     *
                     * For a map container this can only be zero or one of course
                     */

                    if( g_untrustedEndpointsInfo.erase( endpointId ) )
                    {
                        BL_LOG(
                            Logging::info(),
                            BL_MSG()
                                << "A valid SSL certificate was obtained successfully from '"
                                << endpointId
                                << "'"
                            );
                    }
                }

                static void isEnableTlsV10( SAA_in const bool isEnableTlsV10 )
                {
                    g_isEnableTlsV10 = isEnableTlsV10;
                }
            };

            BL_DEFINE_STATIC_MEMBER( CryptoInitT, asio::ssl::context*, g_sslContext ) = nullptr;
            BL_DEFINE_STATIC_MEMBER( CryptoInitT, os::mutex*, g_locks ) = nullptr;
            BL_DEFINE_STATIC_MEMBER( CryptoInitT, int, g_lockCount ) = 0;
            BL_DEFINE_STATIC_MEMBER( CryptoInitT, bool, g_isEnableTlsV10 ) = false;

            template
            <
                typename E
            >
            std::map< std::string, std::string >
            CryptoInitT< E >::g_untrustedEndpointsInfo;

            BL_DEFINE_STATIC_MEMBER( CryptoInitT, os::mutex, g_untrustedEndpointsInfoLock );

            typedef CryptoInitT<> CryptoInit;

        } // detail

        template
        <
            typename E = void
        >
        class CryptoBaseT
        {
        protected:

            static os::mutex                                            g_lock;
            static bool                                                 g_initialized;

            static bool                                                 g_dllsPinned;

            static void chk2InitCrypto()
            {
                BL_MUTEX_GUARD( g_lock );

                if( g_initialized )
                {
                    return;
                }

                detail::TrustedRoots::initGlobalTrustedRoots();

                detail::CryptoInit::initSsl();

                g_initialized = true;
            }

            CryptoBaseT()
            {
                init();
            }

            ~CryptoBaseT() NOEXCEPT
            {
                /*
                 * The destructor is declared protected just to make sure this base
                 * cannot be used as virtual base
                 */
            }

        public:

            static void init()
            {
                #if defined( _WIN32 )

                /*
                 * OpenSSL triggers app verifier leak issue where it tries to
                 * unload some DLLs which have made memory allocations, but
                 * have not freed them
                 *
                 * These DLLs are likely not designed to be loaded and unloaded
                 * dynamically as OpenSSL tries to do
                 *
                 * The fix is to pin these DLLs before OpenSSL attempts to load
                 * and unload, so they're never unloaded
                 */

                if( ! g_dllsPinned )
                {
                    const auto cb = []( SAA_in const std::string& dllName ) -> void
                    {
                        std::wstring wname( dllName.begin(), dllName.end() );

                        if( ! ::LoadLibraryW( wname.c_str() ) )
                        {
                            eh::error_code ec( ::GetLastError(), eh::system_category() );

                            BL_CHK_EC(
                                ec,
                                BL_MSG()
                                    << "Could not load the following DLL: '"
                                    << dllName
                                    << "'"
                                );
                        }
                    };

                    cb( "wkscli.dll" );
                    cb( "netapi32.dll" );

                    g_dllsPinned = true;
                }

                #endif // defined( _WIN32 )

                chk2InitCrypto();
            }

            static auto getAsioSslContext() NOEXCEPT -> asio::ssl::context&
            {
                return detail::CryptoInit::getAsioSslContext();
            }

            static auto createAsioSslServerContext(
                SAA_in              const std::string&                  privateKeyPem,
                SAA_in              const std::string&                  certificatePem
                )
                -> cpp::SafeUniquePtr< asio::ssl::context >
            {
                init();

                return detail::CryptoInit::createAsioSslServerContext( privateKeyPem, certificatePem );
            }

            static bool allowUntrustedCertificates() NOEXCEPT
            {
                /*
                 * We need to allow SSL connections to servers which provide certificates
                 * that cannot be trusted because we don't want to make the app non-operational
                 * as a hard policy - e.g. if some certificate expires that might be
                 * considered soft error, prompt the user to continue, etc (like in the
                 * browsers)
                 */

                return true;
            }

            static bool hasUntrustedEndpoints() NOEXCEPT
            {
                return detail::CryptoInit::hasUntrustedEndpoints();
            }

            static auto getUntrustedEndpointsInfo() -> std::map< std::string, std::string >
            {
                return detail::CryptoInit::getUntrustedEndpointsInfo();
            }

            static void setUntrustedEndpointInfo(
                SAA_in              std::string&&                      endpointId,
                SAA_in              std::string&&                      info
                )
            {
                detail::CryptoInit::setUntrustedEndpointInfo( BL_PARAM_FWD( endpointId ), BL_PARAM_FWD( info ) );
            }

            static void clearUntrustedEndpointInfo( SAA_in const std::string& endpointId )
            {
                detail::CryptoInit::clearUntrustedEndpointInfo( endpointId );
            }

            static void isEnableTlsV10( SAA_in const bool isEnableTlsV10 )
            {
                detail::CryptoInit::isEnableTlsV10( isEnableTlsV10 );
            }
        };

        BL_DEFINE_STATIC_MEMBER( CryptoBaseT, bool, g_dllsPinned ) = false;

        template
        <
            typename E
        >
        os::mutex
        CryptoBaseT< E >::g_lock;

        template
        <
            typename E
        >
        bool
        CryptoBaseT< E >::g_initialized = false;

        typedef CryptoBaseT<> CryptoBase;

    } // crypto

} // bl

#endif /* __BL_CRYPTO_CRYPTOBASE_H_ */
