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

#ifndef __BL_TASKS_TCPSSLBASETASKS_H_
#define __BL_TASKS_TCPSSLBASETASKS_H_

#include <baselib/tasks/TcpBaseTasks.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TasksIncludes.h>

#include <baselib/tasks/AsioSslStreamWrapper.h>

namespace bl
{
    namespace tasks
    {
        /******************************************************************************************
         * ================================ TcpSslSocketAsyncBase =================================
         */

        /**
         * @brief TcpSslSocketAsyncBase
         */

        template
        <
            typename E = void
        >
        class TcpSslSocketAsyncBaseT :
            public TcpSocketCommonBase
        {
            BL_CTR_DEFAULT( TcpSslSocketAsyncBaseT, protected )
            BL_DECLARE_OBJECT_IMPL( TcpSslSocketAsyncBaseT )

        public:

            typedef TcpSslSocketAsyncBaseT< E >                                         this_type;
            typedef TcpSocketCommonBase                                                 base_type;

            using base_type::m_isCloseStreamOnTaskFinish;

            typedef AsioSslStreamWrapper                                                stream_t;
            typedef typename stream_t::lowest_layer_type                                socket_t;
            typedef typename stream_t::completion_callback_t                            completion_callback_t;
            typedef cpp::SafeUniquePtr< socket_t >                                      socket_ref;
            typedef cpp::SafeUniquePtr< AsioSslStreamWrapper >                          stream_ref;

            enum : bool
            {
                isProtocolHandshakeNeeded = true
            };

        protected:

            static const eh::error_code                                                 g_sslErrorShortRead;

            stream_ref                                                                  m_sslStream;
            std::exception_ptr                                                          m_originalException;
            cpp::ScalarTypeIniter< bool >                                               m_scheduledForShutdown;
            cpp::SafeUniquePtr< asio::ssl::context >                                    m_serverContext;
            cpp::ScalarTypeIniter< bool >                                               m_isHandshakeCompleted;

            /*
             * The following functions below are the static interface:
             *
             * isProtocolHandshakeNeeded: public bool const as it is used in template class specialization
             * resetStreamState() NOEXCEPT
             * isChannelOpen() const NOEXCEPT
             * isSocketCreated() const NOEXCEPT
             * ensureChannelIsOpen() const
             * initServerContext( ... )
             * createSocket( ... )
             * getSocket() NOEXCEPT
             * getStream() NOEXCEPT
             * beginProtocolHandshake( ... )
             * isProtocolHandshakeRetryableError( const std::exception_ptr& )
             * scheduleProtocolOperations( const std::shared_ptr< ExecutionQueue >& )
             */

            void resetStreamState() NOEXCEPT
            {
                m_sslStream.reset();
                m_originalException = std::exception_ptr();
                m_scheduledForShutdown = false;
            }

            bool isChannelOpen() const NOEXCEPT
            {
                return m_sslStream && m_sslStream -> isChannelOpen();
            }

            bool isSocketCreated() const NOEXCEPT
            {
                return nullptr != m_sslStream;
            }

            void ensureChannelIsOpen() const
            {
                BL_CHK(
                    false,
                    isChannelOpen(),
                    BL_MSG()
                        << "SSL socket was closed unexpectedly"
                    );
            }

            void initServerContext(
                SAA_in          const std::string&                                  privateKeyPem,
                SAA_in          const std::string&                                  certificatePem
                )
            {
                m_serverContext =
                    crypto::CryptoBase::createAsioSslServerContext( privateKeyPem, certificatePem );
            }

            void createSocket(
                SAA_inout       asio::io_service&                                   aioService,
                SAA_in          const std::string&                                  hostName,
                SAA_in          const std::string&                                  serviceName
                )
            {
                m_sslStream.reset(
                    new stream_t(
                        aioService,
                        hostName,
                        serviceName,
                        m_serverContext ? m_serverContext.get() : nullptr
                        )
                    );
            }

            socket_t& getSocket() const NOEXCEPT
            {
                BL_ASSERT( m_sslStream );

                return m_sslStream -> getSocket();
            }

            stream_t& getStream() NOEXCEPT
            {
                BL_ASSERT( m_sslStream );

                return *m_sslStream;
            }

            bool beginProtocolHandshake( SAA_in const cpp::bool_callback_t& continueCallback )
            {
                getStream().beginProtocolHandshake(
                    cpp::bind(
                        &this_type::onHandshakeCompleted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        continueCallback,
                        asio::placeholders::error
                        )
                    );

                return true;
            }

            bool isProtocolHandshakeRetryableError( SAA_in const std::exception_ptr& eptr )
            {
                try
                {
                    cpp::safeRethrowException( eptr );
                }
                catch( eh::system_error& e )
                {
                    return ( e.code() == g_sslErrorShortRead || e.code() == asio::error::eof );
                }
                catch( std::exception& )
                {
                    /*
                     * Ignore other exceptions
                     */
                }

                return false;
            }

            void scheduleProtocolOperations( SAA_in const std::shared_ptr< ExecutionQueue >& eq )
            {
                BL_UNUSED( eq );

                /*
                 * This task can be scheduled directly only to cover two cases:
                 * 1) Perform SSL handshake only (if needed)
                 * 2) Perform SSL shutdown only (if needed & not invoked already)
                 * All other cases are invalid and the method will throw
                 */

                if( ! hasHandshakeCompletedSuccessfully() )
                {
                    beginProtocolHandshake(
                        []() NOEXCEPT -> bool
                        {
                            /*
                             * Returning false means task should complete once handshake is done
                             */

                            return false;
                        }
                        );

                    return;
                }

                if( isShutdownNeeded() && ! wasShutdownInvoked() )
                {
                    beginProtocolShutdown( nullptr /* eptrIn */ );

                    return;
                }

                BL_THROW(
                    UnexpectedException(),
                    BL_MSG()
                        << "TcpSslSocketAsyncBase task can only be scheduled directly to do handshake or shutdown"
                    );
            }

            virtual void cancelTask() OVERRIDE
            {
                if( isChannelOpen() )
                {
                    TcpSocketCommonBase::shutdownSocket( getSocket(), true /* force */ );

                    base_type::m_wasSocketShutdownForcefully = true;
                }
            }

            virtual ThreadPoolId getThreadPoolId() const NOEXCEPT OVERRIDE
            {
                /*
                 * By default all TCP tasks use the I/O only thread pool
                 */

                return ThreadPoolId::NonBlocking;
            }

            /**
             * @brief To be called when the stream is about to change
             *
             * This allows the derived class to reset session state associated with the stream
             */

            virtual void onStreamChanging( SAA_in const stream_ref& /* streamNew */ ) NOEXCEPT
            {
                /*
                 * NOP by default
                 */
            }

            virtual void enhanceException( SAA_in eh::exception& exception ) const OVERRIDE
            {
                base_type::enhanceException( exception );

                /*
                 * Check to enhance the exception with the SSL verify error info if
                 * such is available
                 */

                if( m_sslStream )
                {
                    m_sslStream -> enhanceException( exception );
                }
            }

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT OVERRIDE
            {
                if( isExpectedSslException( eptr, exception, ec ) )
                {
                    return true;
                }

                return base_type::isExpectedException( eptr, exception, ec );
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                scheduleProtocolOperations( eq );
            }

            virtual bool scheduleTaskFinishContinuation( SAA_in_opt const std::exception_ptr& eptrIn = nullptr ) OVERRIDE
            {
                BL_UNUSED( eptrIn );

                const auto isShutdownCompletedOrNotNeeded =
                    base_type::m_wasSocketShutdownForcefully ||
                    m_scheduledForShutdown ||
                    ! m_isCloseStreamOnTaskFinish ||
                    ! isShutdownNeeded();

                if( isShutdownCompletedOrNotNeeded )
                {
                    if( m_scheduledForShutdown )
                    {
                        /*
                         * If we were invoked after the completion of the shutdown task then
                         * check to (re-)throw the original exception (if any)
                         *
                         * If the shutdown continuation part of the task has generated an exception
                         * then this exception will be chained in the original exception and if not
                         * then it will be logged as a warning and then discarded -- this will be
                         * done by the core task code in notifyReady()
                         */

                        if( m_originalException )
                        {
                            cpp::safeRethrowException( m_originalException );
                        }
                    }

                    return false;
                }

                /*
                 * If we are here that means shutdown of the stream is needed
                 * and m_isCloseStreamOnTaskFinish is set to true, so we must
                 * perform the shutdown
                 *
                 * We also save the original exception, so we can propagate it
                 * later on when the protocol shutdown has completed
                 */

                beginProtocolShutdown( eptrIn );

                return true;
            }

            virtual auto onTaskStoppedNothrow( SAA_in_opt const std::exception_ptr& eptrIn ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                if( m_isCloseStreamOnTaskFinish && m_sslStream )
                {
                    TcpSocketCommonBase::shutdownSocket( getSocket() );
                }

                BL_NOEXCEPT_END()

                return base_type::onTaskStoppedNothrow( eptrIn );
            }

            void beginProtocolShutdown( SAA_in_opt const std::exception_ptr& eptrIn )
            {
                /*
                 * We need to shutdown the SSL stream first
                 * and then the doShutdown handler will end
                 * the task
                 */

                getStream().beginProtocolShutdown(
                    cpp::bind(
                        &this_type::onShutdownCompleted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );

                m_originalException = eptrIn ? eptrIn : m_exception;

                m_scheduledForShutdown = true;
            }

            void onHandshakeCompleted(
                SAA_in          const cpp::bool_callback_t&                         continueCallback,
                SAA_in          const eh::error_code&                               ec
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                m_isHandshakeCompleted = true;

                getStream().notifyOnSuccessfulHandshakeOrShutdown( true /* isHandshake */ );

                if( continueCallback() )
                {
                    /*
                     * If continueCallback returns true it means the task should not terminate
                     */

                    return;
                }

                BL_TASKS_HANDLER_END()
            }

            void onShutdownCompleted( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                /*
                 * Check to see if we are doing SSL shutdown from a failure position
                 * (i.e. we already have original exception)
                 *
                 * In that case the SSL shutdown is really best effort and very
                 * likely to fail since the socket is in unknown state and in this
                 * case we should simply ignore any errors coming from SSL shutdown
                 *
                 * i.e. we basically only check to throw the respective error code
                 * exception if there is no original exception already
                 */

                /*
                 * "eof" is expected error during SSL shutdown as explained at
                 * http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                 */

                if( asio::error::eof != ec )
                {
                    if( ! isExpectedSslErrorCode( ec ) )
                    {
                        if( m_originalException )
                        {
                            /*
                             * Just log the extra exception as we want to propagate the original one
                             */

                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "Unexpected exception during SSL shutdown; exception details:\n"
                                    << eh::diagnostic_information(
                                        SystemException::create( ec, BL_SYSTEM_ERROR_DEFAULT_MSG )
                                        )
                                );
                        }
                        else
                        {
                            BL_TASKS_HANDLER_CHK_EC( ec )
                        }
                    }
                }

                getStream().notifyOnSuccessfulHandshakeOrShutdown( false /* isHandshake */ );

                BL_TASKS_HANDLER_END()
            }

        public:

            bool hasHandshakeCompletedSuccessfully() const NOEXCEPT
            {
                return m_sslStream && m_sslStream -> hasHandshakeCompletedSuccessfully();
            }

            bool hasShutdownCompletedSuccessfully() const NOEXCEPT
            {
                return m_sslStream && m_sslStream -> hasShutdownCompletedSuccessfully();
            }

            bool wasShutdownInvoked() const NOEXCEPT
            {
                return m_sslStream && m_sslStream -> wasShutdownInvoked();
            }

            bool isShutdownNeeded() const NOEXCEPT
            {
                if( base_type::m_wasSocketShutdownForcefully )
                {
                    return false;
                }

                return hasHandshakeCompletedSuccessfully() && ! wasShutdownInvoked();
            }

            std::string safeRemoteEndpointId() const
            {
                return net::safeRemoteEndpointId( getSocket() );
            }

            void attachStream( SAA_in stream_ref&& stream ) NOEXCEPT
            {
                onStreamChanging( stream );

                m_sslStream = BL_PARAM_FWD( stream );
            }

            stream_ref detachStream() NOEXCEPT
            {
                onStreamChanging( nullptr );

                auto stream = std::move( m_sslStream );

                return stream;
            }

            void chkToCloseSocket()
            {
                if( isChannelOpen() )
                {
                    TcpSocketCommonBase::shutdownSocket( getSocket() );
                }
            }

            static bool isExpectedSslErrorCode( SAA_in_opt const eh::error_code& ec ) NOEXCEPT
            {
                /*
                 * A new SSL error category and a new error code were defined in the latest ASIO
                 * (asio::ssl::error::stream_category and asio::ssl::error::stream_truncated),
                 * but only when compiled with more recent SSL library which allows to better
                 * deal with and better handle SSL stream truncation errors, but we don't want
                 * to use these directly yet as that would make baselib incompatible with older
                 * versions of SSL and ASIO thus the hard-coding below
                 *
                 * TODO: at some point when we no longer want to support older versions of
                 * SSL and ASIO this hard-coded check can be removed
                 */

                if(
                    std::string( "asio.ssl.stream" ) == ec.category().name() &&
                    ec.value() == 1
                    )
                {
                    return true;
                }

                return ec == g_sslErrorShortRead;
            }

            static bool isExpectedSslException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT
            {
                BL_UNUSED( eptr );
                BL_UNUSED( exception );

                return ec && isExpectedSslErrorCode( *ec );
            }

            static bool isExpectedProtocolException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT
            {
                return isExpectedSslException( eptr, exception, ec );
            }
        };

        typedef TcpSslSocketAsyncBaseT<> TcpSslSocketAsyncBase;


        /*
         * Newer versions of OpenSSL do not define this value (as it is not used there) and
         * thus we need to check and define it ourselves to make the work uniformly and keep
         * it backward compatible with the older versions of OpenSSL
         */

		#ifndef SSL_R_SHORT_READ
		#define SSL_R_SHORT_READ 219
		#endif

        BL_DEFINE_STATIC_MEMBER( TcpSslSocketAsyncBaseT, const eh::error_code, g_sslErrorShortRead ) =
            eh::error_code( ERR_PACK( ERR_LIB_SSL, 0, SSL_R_SHORT_READ ), asio::error::get_ssl_category() );

    } // tasks

} // bl

#endif /* __BL_TASKS_TCPSSLBASETASKS_H_ */
