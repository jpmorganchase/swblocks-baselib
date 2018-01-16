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

#ifndef __BL_BASELIB_TASKS_ASIOSSLSTREAMWRAPPER_H_
#define __BL_BASELIB_TASKS_ASIOSSLSTREAMWRAPPER_H_

#include <baselib/crypto/CryptoBase.h>

#include <baselib/core/AsioSSL.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class AsioSslStreamWrapper - a wrapper for Boost ASIO SSL stream class that will
         * carry other useful state necessary to manage the stream and dispose of it
         */

        template
        <
            typename E = void
        >
        class AsioSslStreamWrapperT :
            public crypto::CryptoBase
        {
            BL_NO_COPY_OR_MOVE( AsioSslStreamWrapperT )

        public:

            typedef AsioSslStreamWrapperT< E >                                          this_type;

            typedef cpp::function< void ( SAA_in const eh::error_code& ec ) NOEXCEPT >  completion_callback_t;

            typedef cpp::function
            <
                bool (
                    SAA_in          const std::string&                              hostName,
                    SAA_in          const bool                                      preVerified,
                    SAA_inout       asio::ssl::verify_context&                      ctx
                    ) NOEXCEPT
            >
            rfc2818_verify_callback_t;

            static auto rfc2818NoVerifyCallback() NOEXCEPT -> rfc2818_verify_callback_t
            {
                return &rfc2818NoVerifyImplementation;
            }

            typedef asio::ssl::stream< asio::ip::tcp::socket >                      sslstream_t;
            typedef typename sslstream_t::lowest_layer_type                         lowest_layer_type;

            static rfc2818_verify_callback_t                                        g_rfc2818VerifyCallback;

        protected:

            const std::string                                                       m_hostName;
            const std::string                                                       m_serviceName;
            const cpp::ScalarTypeIniter< bool >                                     m_isServer;
            const cpp::SafeUniquePtr< sslstream_t >                                 m_sslStream;

            cpp::ScalarTypeIniter< bool >                                           m_hasHandshakeCompletedSuccessfully;
            cpp::ScalarTypeIniter< bool >                                           m_hasShutdownCompletedSuccessfully;
            cpp::ScalarTypeIniter< bool >                                           m_wasShutdownInvoked;

            cpp::ScalarTypeIniter< bool >                                           m_verifyFailed;
            cpp::ScalarTypeIniter< int >                                            m_lastVerifyError;
            std::string                                                             m_lastVerifyErrorString;
            std::string                                                             m_lastVerifyErrorMessage;
            std::string                                                             m_lastVerifySubjectName;

            static bool rfc2818NoVerifyImplementation(
                SAA_in          const std::string&                                  hostName,
                SAA_in          const bool                                          preVerified,
                SAA_inout       asio::ssl::verify_context&                          ctx
                ) NOEXCEPT
            {
                BL_UNUSED( hostName );
                BL_UNUSED( preVerified );
                BL_UNUSED( ctx );

                return true;
            }

            static bool verifyCertificateDummy(
                SAA_in          const bool                                          preVerified,
                SAA_inout       asio::ssl::verify_context&                          ctx
                ) NOEXCEPT
            {
                BL_UNUSED( preVerified );
                BL_UNUSED( ctx );

                return false;
            }

            bool verifyCertificate(
                SAA_in          const asio::ssl::rfc2818_verification&              rfc2818,
                SAA_in          const bool                                          preVerified,
                SAA_inout       asio::ssl::verify_context&                          ctx
                ) NOEXCEPT
            {
                bool ok = false;

                BL_WARN_NOEXCEPT_BEGIN()

                const int depth = ::X509_STORE_CTX_get_error_depth( ctx.native_handle() );

                {
                    X509* cert = ::X509_STORE_CTX_get_current_cert( ctx.native_handle() );
                    BL_CHK_CRYPTO_API_NM( cert );

                    X509_NAME* subjectName = ::X509_get_subject_name( cert );
                    BL_CHK_CRYPTO_API_NM( subjectName );

                    char subjectNameBuffer[ 1024 ];

                    BL_CHK_CRYPTO_API_NM(
                        ::X509_NAME_oneline( subjectName, subjectNameBuffer, BL_ARRAY_SIZE( subjectNameBuffer ) )
                        );

                    m_lastVerifySubjectName = subjectNameBuffer;
                }

                if( preVerified )
                {
                    /*
                     * Verify the host name matches what is specified in the certificate
                     *
                     * Note that this will only match the certificate at the end of the
                     * chain (i.e. when depth = 0)
                     */

                    if( rfc2818( preVerified, ctx ) )
                    {
                        ok = true;
                    }
                    else
                    {
                        m_verifyFailed = true;
                        m_lastVerifyErrorMessage =
                            "RFC2818 verification failed due to the subject name not matching the host name";

                        BL_LOG(
                            Logging::trace(),
                            BL_MSG()
                                << "RFC2818 verification of subject name '"
                                << m_lastVerifySubjectName
                                << "' has failed for host name '"
                                << m_hostName
                                << "'; [depth="
                                << depth
                                << "]"
                            );
                    }
                }
                else
                {
                    /*
                     * preVerified is false when the certificate cannot be checked against
                     * well known root certificates or if some intermediate certificate is
                     * not found in the store, but also not sent from server as well
                     *
                     * In this case we just log the verify error and the subject name and
                     * return false
                     */

                    BL_LOG(
                        Logging::trace(),
                        BL_MSG()
                            << "Pre-verifying of subject name has failed: '"
                            << m_lastVerifySubjectName
                            << "'; [depth="
                            << depth
                            << "]"
                        );

                    m_verifyFailed = true;
                    m_lastVerifyError = ::X509_STORE_CTX_get_error( ctx.native_handle() );

                    if( X509_V_OK != m_lastVerifyError )
                    {
                        const char* errorString = ::X509_verify_cert_error_string( m_lastVerifyError );
                        BL_CHK_CRYPTO_API_NM( errorString );

                        m_lastVerifyErrorString = errorString;
                    }

                    m_lastVerifyErrorMessage =
                        resolveMessage(
                            BL_MSG()
                                << "SSL verify error: "
                                << m_lastVerifyError
                                << "; ['"
                                << (
                                        m_lastVerifyErrorString.empty() ?
                                            std::string( "<N/A>" ) : m_lastVerifyErrorString
                                    )
                                << "']; [depth="
                                << depth
                                << "]"
                            );

                    BL_LOG(
                        Logging::trace(),
                        BL_MSG()
                            << m_lastVerifyErrorMessage
                        );
                }

                BL_WARN_NOEXCEPT_END( "AsioSslStreamWrapperT<...>::verifyCertificate" )

                /*
                 * We check allowUntrustedCertificates() and return true in this case
                 * as we don't want to block on expired certificate or other such
                 * certificate issues
                 *
                 * The caller code will check for untrusted certificate issues and they
                 * will report a loud warning to the user, prompt the user, etc
                 */

                return ( ok || allowUntrustedCertificates() );
            }

            void onHandshakeInternal(
                SAA_in          const completion_callback_t&                        transferCallback,
                SAA_in          const eh::error_code&                               ec
                ) NOEXCEPT
            {
                /*
                 * Record successful handshake and transfer to the callback
                 */

                m_hasHandshakeCompletedSuccessfully = ! ec;

                transferCallback( ec );
            }

            void onShutdownInternal(
                SAA_in          const completion_callback_t&                        transferCallback,
                SAA_in          const eh::error_code&                               ec
                ) NOEXCEPT
            {
                /*
                 * Record successful shutdown and transfer to the callback
                 */

                /*
                 * "eof" is expected error during SSL shutdown as explained at
                 * http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                 */

                m_hasShutdownCompletedSuccessfully = asio::error::eof == ec ? true : ! ec;

                m_wasShutdownInvoked = true;

                transferCallback( ec );
            }

        public:

            AsioSslStreamWrapperT(
                SAA_inout       asio::io_service&                                   aioService,
                SAA_in          const std::string&                                  hostName,
                SAA_in          const std::string&                                  serviceName,
                SAA_in_opt      asio::ssl::context*                                 sslServerContextPtr
                )
                :
                m_hostName( hostName ),
                m_serviceName( serviceName ),
                m_isServer( sslServerContextPtr != nullptr ),
                m_sslStream(
                    cpp::SafeUniquePtr< sslstream_t >::attach(
                        new sslstream_t(
                            aioService,
                            sslServerContextPtr ? *sslServerContextPtr : crypto::CryptoBase::getAsioSslContext()
                            )
                        )
                    )
            {
            }

            ~AsioSslStreamWrapperT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                /*
                 * Before we get destroyed we have to reset back
                 * the verify callback to verifyCertificateDummy to
                 * ensure that the callback into the internal SSL
                 * socket object no longer refers to the 'this'
                 * pointer of this specific HTTP task object
                 */

                m_sslStream -> set_verify_callback( &this_type::verifyCertificateDummy );

                BL_NOEXCEPT_END()
            }

            template
            <
                typename MutableBufferSequence,
                typename ReadHandler
            >
            void async_read_some(
                SAA_in          const MutableBufferSequence&                        buffers,
                SAA_in          ReadHandler                                         handler
                )
            {
                getStream().async_read_some( buffers, handler );
            }

            template
            <
                typename ConstBufferSequence,
                typename WriteHandler
            >
            void async_write_some(
                SAA_in          const ConstBufferSequence&                          buffers,
                SAA_in          WriteHandler                                        handler
                )
            {
                getStream().async_write_some( buffers, handler );
            }

            bool isChannelOpen() const NOEXCEPT
            {
                return m_sslStream && m_sslStream -> lowest_layer().is_open();
            }

            lowest_layer_type& getSocket() const NOEXCEPT
            {
                BL_ASSERT( m_sslStream );

                return m_sslStream -> lowest_layer();
            }

            lowest_layer_type& lowest_layer() const NOEXCEPT
            {
                return getSocket();
            }

            sslstream_t& getStream() const NOEXCEPT
            {
                BL_ASSERT( m_sslStream );

                return *m_sslStream;
            }

            std::string endpointId() const
            {
                return resolveMessage(
                    BL_MSG()
                        << m_hostName
                        << ":"
                        << m_serviceName
                        );
            }

            bool hasHandshakeCompletedSuccessfully() const NOEXCEPT
            {
                return m_hasHandshakeCompletedSuccessfully;
            }

            bool hasShutdownCompletedSuccessfully() const NOEXCEPT
            {
                return m_hasShutdownCompletedSuccessfully;
            }

            bool wasShutdownInvoked() const NOEXCEPT
            {
                return m_wasShutdownInvoked;
            }

            void enhanceException( SAA_in eh::exception& exception ) const
            {
                /*
                 * Check to enhance the exception with the SSL verify error info if
                 * such is available
                 */

                if( m_verifyFailed )
                {
                    exception
                        << eh::errinfo_ssl_is_verify_failed( m_verifyFailed.value() )
                        << eh::errinfo_ssl_is_verify_error( m_lastVerifyError.value() )
                        << eh::errinfo_ssl_is_verify_error_string( m_lastVerifyErrorString )
                        << eh::errinfo_ssl_is_verify_error_message( m_lastVerifyErrorMessage )
                        << eh::errinfo_ssl_is_verify_subject_name( m_lastVerifySubjectName )
                        ;
                }
            }

            void beginProtocolHandshake( SAA_in const completion_callback_t& transferCallback )
            {
                /*
                 * Before we attempt to download data we need to configure the SSL
                 * stream and do the SSL handshake
                 */

                getStream().set_verify_mode( asio::ssl::verify_peer );

                /*
                 * Clear the last verify error info state in case the object has been reused
                 */

                m_hasHandshakeCompletedSuccessfully = false;
                m_hasShutdownCompletedSuccessfully = false;
                m_wasShutdownInvoked = false;

                m_verifyFailed = false;
                m_lastVerifyError = 0;
                m_lastVerifyErrorString.clear();
                m_lastVerifyErrorMessage.clear();
                m_lastVerifySubjectName.clear();

                /*
                 * If global verify callback is not provided then we use the std
                 * rfc2818 verify code provided by Boost ASIO
                 *
                 * The verify callback can acquire a weak reference to the 'this'
                 * pointer as its lifetime is going to be the same as the
                 * underlying SSL stream object associated with this task
                 *
                 * We actually cannot acquire a strong reference to 'this' pointer
                 * here because this will cause a circular reference and because
                 * the object can't be copied
                 *
                 * After we are done with the connection we will reset back the
                 * callback to this_type::verifyCertificateDummy which always will
                 * return false and won't hold weak reference to the object
                 */

                if( g_rfc2818VerifyCallback )
                {
                    getStream().set_verify_callback(
                        cpp::bind(
                            g_rfc2818VerifyCallback,
                            m_hostName,
                            _1 /* preVerified */,
                            _2 /* ctx */
                            )
                        );
                }
                else
                {
                    getStream().set_verify_callback(
                        cpp::bind(
                            &this_type::verifyCertificate,
                            this,
                            asio::ssl::rfc2818_verification( m_hostName ),
                            _1 /* preVerified */,
                            _2 /* ctx */
                            )
                        );
                }

                getStream().async_handshake(
                    m_isServer ? sslstream_t::server : sslstream_t::client,
                    cpp::bind(
                        &this_type::onHandshakeInternal,
                        this,
                        transferCallback,
                        asio::placeholders::error
                        )
                    );
            }

            void beginProtocolShutdown( SAA_in const completion_callback_t& transferCallback )
            {
                /*
                 * This should only be called if the protocol handshake has completed
                 * successfully
                 */

                BL_CHK(
                    false,
                    m_hasHandshakeCompletedSuccessfully.value(),
                    BL_MSG()
                        << "Protocol shutdown should not be called on a stream which did not complete handshake"
                    );

                BL_CHK(
                    true,
                    m_wasShutdownInvoked.value(),
                    BL_MSG()
                        << "Protocol shutdown should not be called more than once"
                    );

                getStream().async_shutdown(
                    cpp::bind(
                        &this_type::onShutdownInternal,
                        this,
                        transferCallback,
                        asio::placeholders::error
                        )
                    );
            }

            void notifyOnSuccessfulHandshakeOrShutdown( SAA_in const bool isHandshake )
            {
                BL_UNUSED( isHandshake );

                /*
                 * On successful shutdown we need to set or clear the untrusted
                 * endpoint info for that endpoint
                 *
                 * This method is to be called by the shutdown handler of the caller
                 */

                if( m_verifyFailed )
                {
                    crypto::CryptoBase::setUntrustedEndpointInfo(
                        endpointId(),
                        cpp::copy( m_lastVerifyErrorMessage )
                        );
                }
                else
                {
                    crypto::CryptoBase::clearUntrustedEndpointInfo( endpointId() );
                }
            }
        };

        BL_DEFINE_STATIC_MEMBER(
            AsioSslStreamWrapperT,
            typename AsioSslStreamWrapperT< TCLASS >::rfc2818_verify_callback_t,
            g_rfc2818VerifyCallback );

        typedef AsioSslStreamWrapperT<> AsioSslStreamWrapper;

    } // tasks

} // bl

#endif /* __BL_BASELIB_TASKS_ASIOSSLSTREAMWRAPPER_H_ */
