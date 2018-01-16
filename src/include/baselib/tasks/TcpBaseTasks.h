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

#ifndef __BL_TASKS_TCPBASETASKS_H_
#define __BL_TASKS_TCPBASETASKS_H_

#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TaskControlToken.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/TasksIncludes.h>

#include <vector>
#include <unordered_map>

namespace bl
{
    namespace tasks
    {
        using asio::ip::tcp;

        /******************************************************************************************
         * ================================== TcpSocketCommonBase =================================
         */

        /**
         * @brief TcpSocketCommonBase
         */

        template
        <
            typename E = void
        >
        class TcpSocketCommonBaseT :
            public cpp::noncopyable,
            public TaskBase
        {
            BL_CTR_DEFAULT( TcpSocketCommonBaseT, protected )
            BL_DECLARE_OBJECT_IMPL( TcpSocketCommonBaseT )

        protected:

            typedef TcpSocketCommonBaseT< E >                                           this_type;
            typedef TaskBase                                                            base_type;

            typedef decltype( asio::transfer_all() )                                    completion_t;

            enum : long
            {
                SET_OPTION_RETRY_TIMEOUT_IN_MILLISECONDS = 500,
            };

            cpp::ScalarTypeIniter< bool >                                               m_isCloseStreamOnTaskFinish;
            cpp::ScalarTypeIniter< bool >                                               m_wasSocketShutdownForcefully;

            std::size_t untilCanceledImpl(
                SAA_in                  completion_t                                    completionDefault,
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               bytesTransferred
                )
            {
                if( base_type::isCanceled() )
                {
                    /*
                     * Return zero to indicate the operation should be canceled
                     */

                    return 0U;
                }

                return completionDefault( ec, bytesTransferred );
            }

            typedef cpp::function
                <
                    std::size_t
                    (
                        SAA_in          const eh::error_code&                           ec,
                        SAA_in          const std::size_t                               bytesTransferred
                    )
                    NOEXCEPT
                >
                completion_handler_t;

            completion_handler_t untilCanceled()
            {
                return cpp::bind(
                    &this_type::untilCanceledImpl,
                    om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    asio::transfer_all(),
                    asio::placeholders::error,
                    asio::placeholders::bytes_transferred
                    );
            }

            virtual auto onTaskStoppedNothrow(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_inout_opt           bool*                                       isExpectedException = nullptr
                ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                std::exception_ptr eptr;

                BL_NOEXCEPT_BEGIN()

                eptr = base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );

                if( base_type::isCanceled() && m_wasSocketShutdownForcefully )
                {
                    const auto exception = SystemException::create(
                        asio::error::operation_aborted,
                        BL_SYSTEM_ERROR_DEFAULT_MSG
                        );

                    if( eptr )
                    {
                        exception
                            << eh::errinfo_nested_exception_ptr( eptr );
                    }

                    eptr  = std::make_exception_ptr( exception );

                    if( isExpectedException )
                    {
                        *isExpectedException = this -> isExpectedException( eptr, exception, &exception.code() );
                    }
                }

                BL_NOEXCEPT_END()

                return eptr;
            }

        public:

            static bool isExpectedSocketException(
                SAA_in                  const bool                                      isCancelExpected,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT
            {
                if( ec )
                {
                    if( isCancelExpected && asio::error::operation_aborted == *ec )
                    {
                        /*
                         * If the task was cancelled explicitly then
                         * asio::error::operation_aborted is normally expected
                         */

                        return true;
                    }

                    if( asio::error::eof == *ec )
                    {
                        /*
                         * EOF is an expected error condition which can occur on both sides
                         * if one of then decides to disconnect first, so it doesn't need
                         * to be reported normally
                         */

                        return true;
                    }

                    /*
                     * Some exceptions are very common and 100% expected due to the client
                     * closing the sockets or some other expected situations (e.g. timed out)
                     *
                     * This is always handled and does not need to be dumped in the server
                     * logs as the exceptions will happen often and are expected
                     */

                    if(
                        eh::isErrorCondition( eh::errc::not_connected, *ec ) ||
                        eh::isErrorCondition( eh::errc::connection_aborted, *ec ) ||
                        eh::isErrorCondition( eh::errc::connection_reset, *ec ) ||
                        eh::isErrorCondition( eh::errc::connection_already_in_progress, *ec ) ||
                        eh::isErrorCondition( eh::errc::connection_refused, *ec ) ||
                        eh::isErrorCondition( eh::errc::broken_pipe, *ec ) ||
                        eh::isErrorCondition( eh::errc::timed_out, *ec ) ||
                        eh::isErrorCondition( eh::errc::host_unreachable, *ec )
                        )
                    {
                        return true;
                    }

                    if( os::onWindows() )
                    {
                        /*
                         * On Windows the error codes above are different and we need to handle these separately
                         *
                         * The values for the WSA* error codes are from here:
                         * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%28v=vs.85%29.aspx
                         */

                        const int errorValueWsaENOTCONN = 10057;
                        const int errorValueWsaECONNABORTED = 10053;
                        const int errorValueWsaECONNRESET = 10054;
                        const int errorValueWsaEALREADY = 10037;
                        const int errorValueWsaECONNREFUSED = 10061;
                        const int errorValueWsaETIMEDOUT = 10060;
                        const int errorValueWsaEHOSTUNREACH = 10065;

                        if(
                            eh::error_code( errorValueWsaENOTCONN, eh::system_category() ) == *ec ||
                            eh::error_code( errorValueWsaECONNABORTED, eh::system_category() ) == *ec ||
                            eh::error_code( errorValueWsaECONNRESET, eh::system_category() ) == *ec ||
                            eh::error_code( errorValueWsaEALREADY, eh::system_category() ) == *ec ||
                            eh::error_code( errorValueWsaECONNREFUSED, eh::system_category() ) == *ec ||
                            eh::error_code( errorValueWsaETIMEDOUT, eh::system_category() ) == *ec ||
                            eh::error_code( errorValueWsaEHOSTUNREACH, eh::system_category() ) == *ec
                            )
                        {
                            return true;
                        }
                    }
                }

                return false;
            }

            template
            <
                typename T
            >
            static void shutdownSocket(
                SAA_inout                   T&                              socket,
                SAA_in_opt                  const bool                      force = false
                ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( ! socket.is_open() )
                {
                    return;
                }

                const auto checkSocketError = []( SAA_in const eh::error_code& ec ) -> void
                {
                    if( ! ec )
                    {
                        return;
                    }

                    bool isBadSocket = false;

                    if( eh::isErrorCondition( eh::errc::bad_file_descriptor, ec ) )
                    {
                        isBadSocket = true;
                    }
                    else if( os::onWindows() )
                    {
                        /*
                         * On Windows the error codes above are different and we need to handle these separately
                         *
                         * The values for the WSA* error codes are from here:
                         * http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%28v=vs.85%29.aspx
                         */

                        const int errorValueWsaEBADF = 10009;

                        if( eh::error_code( errorValueWsaEBADF, eh::system_category() ) == ec )
                        {
                            isBadSocket = true;
                        }
                    }

                    if( isBadSocket )
                    {
                        BL_RIP_MSG( "Fatal error (bad_file_descriptor) during socket shutdown" );
                    }
                    else
                    {
                        if( ! isExpectedSocketException( false /* isCancelExpected */, &ec ) )
                        {
                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Non-fatal error during socket shutdown: "
                                    << eh::errorCodeToString( ec )
                                    << " "
                                    << str::quoteString( ec.message() )
                                );
                        }
                    }
                };

                /*
                 * shutdown() will prevent new read/write requests
                 * and cancel() will stop existing such requests
                 */

                if( force )
                {
                    /*
                     * If the socket is being forcefully shutdown (e.g. as part of
                     * canceling an I/O task) then we should set the linger timeout
                     * to zero so the socket is closed immediately as we don't have
                     * to worry about errors at this point
                     */

                    eh::error_code ec;
                    socket.set_option( asio::socket_base::linger( false, 0 ), ec );
                    checkSocketError( ec );
                }

                {
                    eh::error_code ec;
                    socket.shutdown( tcp::socket::shutdown_both, ec );
                    checkSocketError( ec );
                }

                {
                    eh::error_code ec;
                    socket.cancel( ec );
                    checkSocketError( ec );
                }

                BL_NOEXCEPT_END()
            }

            template
            <
                typename STREAM
            >
            static bool tryConfigureConnectedStream( SAA_inout STREAM& stream )
            {
                return utils::tryCatchLog< bool, eh::system_error >(
                    "tcp::socket::set_option failed with an exception",
                    [ & ]() -> bool
                    {
                        utils::retryOnError< eh::system_error >(
                            [ & ]() -> void
                            {
                                /*
                                 * Disable Nagle's algorithm which buffers small packages before sending
                                 * them over the network. We want to avoid potential delays introduced by the
                                 * algorithm as in general the code doesn't send small packages frequently.
                                 * Also the introduced delay could break handshake in case SSL is used.
                                 */

                                stream.lowest_layer().set_option( tcp::no_delay( true ) );

                                /*
                                 * In some cases connections may be held idle for a longer period of time and
                                 * we want to ensure that they won't be discarded without having to rely
                                 * on how the kernel is configured by default
                                 *
                                 * Not being able to set the keep-alive option is not considered a fatal
                                 * issue since it might fail in a number of expected cases
                                 *
                                 * Also note that occasionally we encounter some weird one-off failures
                                 * with invalid parameter which seem to be timing-related and somewhat
                                 * transient, so to avoid these errors we will do several retries
                                 */

                                stream.lowest_layer().set_option( tcp::socket::keep_alive( true ) );
                            },
                            5U /* retryCount */,
                            time::milliseconds( SET_OPTION_RETRY_TIMEOUT_IN_MILLISECONDS ) /* retryTimeout */
                            );

                            return true;
                    },
                    [ & ]() -> bool
                    {
                        return false;
                    },
                    utils::LogFlags::DEBUG_ONLY
                    );
            }

            bool isCloseStreamOnTaskFinish() const NOEXCEPT
            {
                return m_isCloseStreamOnTaskFinish;
            }

            void isCloseStreamOnTaskFinish( SAA_in const bool isCloseStreamOnTaskFinish ) NOEXCEPT
            {
                m_isCloseStreamOnTaskFinish = isCloseStreamOnTaskFinish;
            }

            bool wasSocketShutdownForcefully() const NOEXCEPT
            {
                return m_wasSocketShutdownForcefully;
            }
        };

        typedef TcpSocketCommonBaseT<> TcpSocketCommonBase;

        /******************************************************************************************
         * ================================== TcpSocketAsyncBase ==================================
         */

        /**
         * @brief TcpSocketAsyncBase
         */

        template
        <
            typename E = void
        >
        class TcpSocketAsyncBaseT :
            public TcpSocketCommonBase
        {
            BL_CTR_DEFAULT( TcpSocketAsyncBaseT, protected )
            BL_DECLARE_OBJECT_IMPL( TcpSocketAsyncBaseT )

        public:

            using TcpSocketCommonBase::m_isCloseStreamOnTaskFinish;

            typedef TcpSocketCommonBase                                                 base_type;

            typedef tcp::socket                                                         socket_t;
            typedef tcp::socket                                                         stream_t;
            typedef cpp::SafeUniquePtr< socket_t >                                      socket_ref;
            typedef cpp::SafeUniquePtr< stream_t >                                      stream_ref;

            enum : bool
            {
                isProtocolHandshakeNeeded = false
            };

        private:

            socket_ref                                                                  m_socket;

        protected:

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
             * isProtocolHandshakeRetryableError( const std::exception_ptr& ) NOEXCEPT
             * isStreamTruncationError( const eh::error_code& errorCode )
             * scheduleProtocolOperations( const std::shared_ptr< ExecutionQueue >& )
             */

            void resetStreamState() NOEXCEPT
            {
                m_socket.reset();
            }

            bool isChannelOpen() const NOEXCEPT
            {
                return m_socket && m_socket -> is_open();
            }

            bool isSocketCreated() const NOEXCEPT
            {
                return nullptr != m_socket;
            }

            void ensureChannelIsOpen() const
            {
                BL_CHK(
                    false,
                    isChannelOpen(),
                    BL_MSG()
                        << "Socket was closed unexpectedly"
                    );
            }

            void initServerContext(
                SAA_in          const std::string&                                  privateKeyPem,
                SAA_in          const std::string&                                  certificatePem
                )
            {
                BL_UNUSED( privateKeyPem );
                BL_UNUSED( certificatePem );

                /*
                 * No specific server context required for non-secure connection
                 */
            }

            void createSocket(
                SAA_inout       asio::io_service&                                   aioService,
                SAA_in          const std::string&                                  hostName,
                SAA_in          const std::string&                                  serviceName
                )
            {
                BL_UNUSED( hostName );
                BL_UNUSED( serviceName );

                m_socket.reset( new tcp::socket( aioService ) );
            }

            socket_t& getSocket() const NOEXCEPT
            {
                BL_ASSERT( m_socket );

                return *m_socket;
            }

            stream_t& getStream() NOEXCEPT
            {
                BL_ASSERT( m_socket );

                return *m_socket;
            }

            bool beginProtocolHandshake( SAA_in const cpp::bool_callback_t& continueCallback )
            {
                /*
                 * There is no protocol handshake here - we simply invoke the continuation callback
                 */

                return continueCallback();
            }

            bool isProtocolHandshakeRetryableError( SAA_in const std::exception_ptr& eptr )
            {
                BL_UNUSED( eptr );

                return false;
            }

            bool isStreamTruncationError( const eh::error_code& errorCode ) NOEXCEPT
            {
                BL_UNUSED( errorCode );

                return false;
            }

            void scheduleProtocolOperations( SAA_in const std::shared_ptr< ExecutionQueue >& eq )
            {
                BL_UNUSED( eq );

                BL_THROW(
                    UnexpectedException(),
                    BL_MSG()
                        << "TcpSocketAsyncBase::scheduleProtocolOperations( ... ) is not supported"
                    );
            }

            virtual void cancelTask() OVERRIDE
            {
                if( isChannelOpen() )
                {
                    TcpSocketCommonBase::shutdownSocket( *m_socket, true /* force */ );

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

            virtual auto onTaskStoppedNothrow(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_inout_opt           bool*                                       isExpectedException = nullptr
                ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                if( m_isCloseStreamOnTaskFinish && m_socket )
                {
                    TcpSocketCommonBase::shutdownSocket( getSocket() );
                }

                BL_NOEXCEPT_END()

                return base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );
            }

        public:

            std::string safeRemoteEndpointId() const
            {
                return net::safeRemoteEndpointId( *m_socket );
            }

            void attachStream( SAA_in stream_ref&& stream ) NOEXCEPT
            {
                onStreamChanging( stream );

                m_socket = BL_PARAM_FWD( stream );
            }

            stream_ref detachStream() NOEXCEPT
            {
                onStreamChanging( nullptr );

                return std::move( m_socket );
            }

            void chkToCloseSocket()
            {
                if( isChannelOpen() )
                {
                    TcpSocketCommonBase::shutdownSocket( getSocket() );
                }
            }

            bool hasHandshakeCompletedSuccessfully() const NOEXCEPT
            {
                return false;
            }

            bool hasShutdownCompletedSuccessfully() const NOEXCEPT
            {
                return false;
            }

            bool isShutdownNeeded() const NOEXCEPT
            {
                return false;
            }

            static bool isExpectedProtocolException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT
            {
                BL_UNUSED( eptr );
                BL_UNUSED( exception );
                BL_UNUSED( ec );

                return false;
            }
        };

        typedef TcpSocketAsyncBaseT<> TcpSocketAsyncBase;

        /******************************************************************************************
         * ============================= TcpConnectionEstablisherBase =============================
         */

        /**
         * @brief TcpConnectionEstablisherBase class
         */

        template
        <
            typename STREAM
        >
        class TcpConnectionEstablisherBase :
            public STREAM
        {
            BL_DECLARE_OBJECT_IMPL( TcpConnectionEstablisherBase )

        protected:

            typedef STREAM                                                              base_type;
            typedef TcpConnectionEstablisherBase< STREAM >                              this_type;

            tcp::resolver::query                                                        m_query;
            cpp::SafeUniquePtr< tcp::resolver >                                         m_resolver;
            tcp::resolver::endpoint_type                                                m_endpoint;

            TcpConnectionEstablisherBase(
                SAA_in                              std::string&&                       host,
                SAA_in                              const unsigned short                port
                )
                :
                m_query(
                    host,
                    utils::lexical_cast< std::string >( port ),
                    asio::ip::resolver_query_base::all_matching
                    )
            {
            }

            virtual void enhanceException( SAA_in eh::exception& exception ) const OVERRIDE
            {
                base_type::enhanceException( exception );

                exception << eh::errinfo_host_name( m_query.host_name() );
                exception << eh::errinfo_service_name( m_query.service_name() );
                exception << eh::errinfo_endpoint_address( m_endpoint.address().to_string() );
                exception << eh::errinfo_endpoint_port( m_endpoint.port() );
            }

            virtual void cancelTask() OVERRIDE
            {
                if( m_resolver )
                {
                    m_resolver -> cancel();
                }

                base_type::cancelTask();
            }

            static auto getEndpoint( SAA_in tcp::resolver::iterator endpoints ) -> decltype( endpoints -> endpoint() )
            {
                const decltype( endpoints ) end;
                BL_ASSERT( end != endpoints );

                return endpoints -> endpoint();
            }

            void onResolved(
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  tcp::resolver::iterator                         endpoints
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                /*
                 * Note: If resolve has succeeded we still need to check if the task was
                 * cancelled because if the task was cancelled, but the cancellation was
                 * not fast enough and the resolve has succeeded already then we still do
                 * not want to potentially continue the task by invoking
                 * continueAfterResolved since from user's perspective the async_resolve
                 * / async_connect (or alternatively async_resolve / async_accept if the
                 * derived class is acceptor) is considered a single transaction / task
                 * and if the task was cancelled the user is expecting it to terminate as
                 * soon as possible
                 *
                 * If we don't do this then if someone calls requestCancel() on an acceptor
                 * task and waits for it to terminate it may never terminate and cause a hang
                 * (as the acceptor will continue accepting and there won't be way to cancel
                 * it since it was already marked as cancelled and thus cancelTask() will
                 * never be called to trigger the cancellation)
                 */

                if( base_type::isCanceled() )
                {
                    BL_THROW_EC( asio::error::operation_aborted, BL_SYSTEM_ERROR_DEFAULT_MSG );
                }

                m_endpoint = getEndpoint( endpoints );

                BL_LOG(
                    Logging::trace(),
                    BL_MSG()
                        << "Endpoint resolved: [ host_name='"
                        << m_query.host_name()
                        << "', endpoint_address: '"
                        << net::formatEndpointId( m_endpoint )
                        << "' ]"
                    );

                if( continueAfterResolved( endpoints ) )
                {
                    /*
                     * If connection establishing has commenced we must
                     * return to ensure we don't mark the ask as completed
                     */

                    return;
                }

                BL_TASKS_HANDLER_END()
            }

            void startConnectionEstablishingInternal()
            {
                BL_CHK(
                    false,
                    nullptr == m_resolver,
                    BL_MSG()
                        << "Connection establisher task can be executed only once"
                    );

                /*
                 * We need to initialize the resolver and do async_resolve first
                 *
                 * All TCP tasks should always be scheduled on the I/O thread pool
                 */

                const auto threadPool = ThreadPoolDefault::getDefault( base_type::getThreadPoolId() );
                BL_ASSERT( threadPool );

                m_resolver.reset( new tcp::resolver( threadPool -> aioService() ) );

                m_resolver -> async_resolve(
                    m_query,
                    cpp::bind(
                        &this_type::onResolved,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::iterator
                        )
                    );
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                BL_ASSERT( eq );
                BL_UNUSED( eq );

                startConnectionEstablishingInternal();
            }

            /**
             * @brief This will commence establishing the connection in the derived class -
             * either via async_connect or via async_accept call
             */

            virtual bool continueAfterResolved( SAA_in tcp::resolver::iterator /* endpoints */ )
            {
                return false;
            }
        };

        /******************************************************************************************
         * =========================== TcpConnectionEstablisherAcceptor ===========================
         */

        /**
         * @brief TcpConnectionEstablisherAcceptor class
         */

        template
        <
            typename STREAM
        >
        class TcpConnectionEstablisherAcceptor :
            public TcpConnectionEstablisherBase< STREAM >
        {
            BL_DECLARE_OBJECT_IMPL( TcpConnectionEstablisherAcceptor )

        protected:

            typedef TcpConnectionEstablisherBase< STREAM >                              base_type;
            typedef TcpConnectionEstablisherAcceptor< STREAM >                          this_type;
            typedef typename STREAM::stream_ref                                         stream_ref;

            cpp::SafeUniquePtr< tcp::acceptor >                                         m_acceptor;
            tcp::endpoint                                                               m_localEndpoint;
            eh::error_code                                                              m_errorCode;

            TcpConnectionEstablisherAcceptor(
                SAA_in                              std::string&&                       host,
                SAA_in                              const unsigned short                port
                )
                :
                base_type( std::forward< std::string >( host ), port )
            {
                TaskBase::m_name = "success:TcpTask_Acceptor";
            }

            virtual auto onTaskStoppedNothrow(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_inout_opt           bool*                                       isExpectedException = nullptr
                ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                if( base_type::isSocketCreated() )
                {
                    TcpSocketCommonBase::shutdownSocket( base_type::getSocket() );
                }

                m_acceptor.reset();

                BL_NOEXCEPT_END()

                return base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );
            }

            void startAccept()
            {
                BL_ASSERT( m_acceptor );

                /*
                 * All TCP tasks should always be scheduled on the I/O thread pool
                 */

                const auto threadPool = ThreadPoolDefault::getDefault( base_type::getThreadPoolId() );
                BL_ASSERT( threadPool );

                base_type::createSocket(
                    threadPool -> aioService(),
                    base_type::m_query.host_name(),
                    base_type::m_query.service_name()
                    );

                m_acceptor -> async_accept(
                    base_type::getSocket(),
                    cpp::bind(
                        &this_type::onConnectionAccepted,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );
            }

            /**
             * @brief This will allow the derived class to continue the shutdown
             * process after the acceptor has stopped accepting connections
             * (e.g. shutdown outstanding connections, etc)
             */

            virtual bool continueAfterStoppedAccepting()
            {
                return false;
            }

            virtual void processIncomingConnection( SAA_inout stream_ref&& /* connectedStream */ )
            {
                /*
                 * NOP; to be overridden in the derived class
                 */
            }

            void onConnectionAccepted( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                BL_ASSERT( m_acceptor );

                const bool isSocketClosed = ! base_type::getSocket().is_open();
                const bool isAcceptorClosed = ! m_acceptor -> is_open();

                if( ec && asio::error::operation_aborted != ec )
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "Error while accepting connections in task '"
                            << TaskBase::m_name
                            << "'\n[error_code] = "
                            << ec.value()
                            << "\n[error_code_message] = "
                            << ec.message()
                            << "\n[category_name] = "
                            << ec.category().name()
                            << "\n"
                        );

                    m_errorCode = ec;
                }

                /*
                 * Note: It is very important to check for base_type::isCanceled() here as
                 * the acceptor is cancelled externally, but the request cancel happened
                 * to be called while we were not waiting on async_accept then the
                 * onConnectionAccepted( ... ) callback will be called without the
                 * asio::error::operation_aborted error code (and without the socket or
                 * the acceptor being closed) which is the state that is expected to
                 * initiate the shutdown process
                 */

                if(
                    base_type::isCanceled() ||
                    asio::error::operation_aborted == ec ||
                    isSocketClosed ||
                    isAcceptorClosed
                    )
                {
                    /*
                     * The acceptor stopped accepting connections or either the acceptor or the
                     * socket was closed (likely because cancel was requested)
                     */

                    if( continueAfterStoppedAccepting() )
                    {
                        /*
                         * Close the acceptor, so we no longer listen on the port and confuse
                         * clients while waiting for connections to be canceled
                         */

                        m_acceptor.reset();

                        return;
                    }

                    /*
                     * No further cancellation / shutdown processing
                     * is necessary. The task is done.
                     */

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Stopped accepting connections at the following endpoint: "
                            << net::formatEndpointId( m_localEndpoint )
                        );

                    BL_TASKS_HANDLER_CHK_EC( m_errorCode );
                }
                else
                {
                    /*
                     * We don't want failures when establishing individual connections to take down
                     * the server, so we catch and dump the exception here, but we continue accepting
                     * connections
                     */

                    utils::tryCatchLog(
                        "Failed to establish a connection with endpoint; exception details",
                        [ & ]() -> void
                        {
                            processIncomingConnection( base_type::detachStream() );
                        }
                        );

                    /*
                     * Continue accepting incoming connections
                     */

                    startAccept();

                    return;
                }

                BL_TASKS_HANDLER_END()
            }

            virtual void cancelTask() OVERRIDE
            {
                if( m_acceptor && m_acceptor -> is_open() )
                {
                    m_acceptor -> cancel();
                }

                base_type::cancelTask();
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                if( m_acceptor )
                {
                    /*
                     * Acceptor is ready; just continue accepting connections
                     */

                    startAccept();

                    return;
                }

                /*
                 * If we're here that means the acceptor needs to be resolved and
                 * initialized via the base class implementation of scheduleTask()
                 */

                base_type::scheduleTask( eq );
            }

            virtual bool continueAfterResolved( SAA_in tcp::resolver::iterator endpoints ) OVERRIDE
            {
                BL_ASSERT( ! m_acceptor );

                /*
                 * All TCP tasks should always be scheduled on the I/O thread pool
                 */

                const auto threadPool = ThreadPoolDefault::getDefault( base_type::getThreadPoolId() );
                BL_ASSERT( threadPool );

                m_acceptor.reset( new tcp::acceptor( threadPool -> aioService() ) );

                const auto endpoint = base_type::getEndpoint( endpoints );

                /*
                 * Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR)
                 *
                 * Also set the linger option to false and zero timeout to ensure the acceptor
                 * is closed promptly once the task is terminated (to have predictable behavior
                 * for unit tests and in general)
                 */

                m_acceptor -> open( endpoint.protocol() );
                m_acceptor -> set_option( tcp::acceptor::reuse_address( true ) );
                m_acceptor -> set_option( asio::socket_base::linger( false, 0 ) );
                m_acceptor -> bind( endpoint );
                m_acceptor -> listen();

                /*
                 * We're ready to start accepting connections now (async)
                 */

                m_localEndpoint = endpoint;

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Begin accepting connections at the following endpoint: "
                        << net::formatEndpointId( m_localEndpoint )
                    );

                /*
                 * Just dump the alternative endpoints returned by
                 * resolve for this query
                 */

                ++endpoints;

                const decltype( endpoints ) end;

                while( end != endpoints )
                {
                    const auto altEndpoint = endpoints -> endpoint();

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Acceptor: alternative endpoint: "
                            << net::formatEndpointId( altEndpoint )
                        );

                    ++endpoints;
                }

                startAccept();

                return true;
            }
        };

        /******************************************************************************************
         * =========================== TcpConnectionEstablisherConnector ==========================
         */

        /**
         * @brief TcpConnectionEstablisherConnector class
         */

        template
        <
            typename STREAM
        >
        class TcpConnectionEstablisherConnector :
            public TcpConnectionEstablisherBase< STREAM >
        {
            BL_DECLARE_OBJECT_IMPL( TcpConnectionEstablisherConnector )

        protected:

            enum : std::size_t
            {
                MAX_RETRY_COUNT = 5,
            };

            typedef TcpConnectionEstablisherBase< STREAM >                              base_type;
            typedef TcpConnectionEstablisherConnector< STREAM >                         this_type;

            cpp::ScalarTypeIniter< std::size_t >                                        m_maxRetryCount;
            cpp::ScalarTypeIniter< std::size_t >                                        m_retries;
            cpp::ScalarTypeIniter< bool >                                               m_isSocketConnected;

            TcpConnectionEstablisherConnector(
                SAA_in                              std::string&&                       host,
                SAA_in                              const unsigned short                port,
                SAA_in                              const bool                          logExceptions = true
                )
                :
                base_type( BL_PARAM_FWD( host ), port ),
                m_maxRetryCount( MAX_RETRY_COUNT )
            {
                if( logExceptions )
                {
                    /*
                     * Setting task name will cause exceptions to be logged (see TaskBase::notifyReady)
                     */

                    TaskBase::m_name = "TcpTask_Connector";
                }
            }

            /**
             * @brief This will commence the async operations on the socket (if any)
             */

            virtual bool continueAfterConnected()
            {
                return false;
            }

            void onConnectionEstablished(
                SAA_in                              const eh::error_code&               ec,
                SAA_in                              tcp::resolver::iterator             endpoints
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                const decltype( endpoints ) end;

                if( endpoints != end && false == TaskBase::isCanceled() )
                {
                    m_isSocketConnected = true;

                    base_type::ensureChannelIsOpen();

                    ( void ) base_type::tryConfigureConnectedStream( base_type::getStream() );

                    const bool continueTask =
                        base_type::beginProtocolHandshake(
                            cpp::bind(
                                &this_type::continueAfterConnected,
                                om::ObjPtrCopyable< this_type >::acquireRef( this )
                                )
                            );

                    if( continueTask )
                    {
                        /*
                         * The task has started has started async operations on
                         * the socket; we're not ready to finish yet
                         */

                        return;
                    }
                }

                /*
                 * Just fall through which will mark the task as completed
                 */

                BL_TASKS_HANDLER_END()
            }

            virtual bool continueAfterResolved( SAA_in tcp::resolver::iterator endpoints ) OVERRIDE
            {
                /*
                 * We're ready to start connecting now
                 *
                 * All TCP tasks should always be scheduled on the I/O thread pool
                 */

                const auto threadPool = ThreadPoolDefault::getDefault( base_type::getThreadPoolId() );
                BL_ASSERT( threadPool );

                base_type::createSocket(
                    threadPool -> aioService(),
                    base_type::m_query.host_name(),
                    base_type::m_query.service_name()
                    );

                asio::async_connect(
                    base_type::getSocket(),
                    endpoints,
                    cpp::bind(
                        &this_type::onConnectionEstablished,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::iterator
                        )
                    );

                return true;
            }

            virtual bool scheduleTaskFinishContinuation( SAA_in_opt const std::exception_ptr& eptrIn = nullptr ) OVERRIDE
            {
                if(
                    eptrIn &&
                    m_retries < m_maxRetryCount &&
                    base_type::isChannelOpen() &&
                    base_type::isProtocolHandshakeNeeded &&
                    ! base_type::hasHandshakeCompletedSuccessfully() &&
                    base_type::isProtocolHandshakeRetryableError( eptrIn )
                    )
                {
                    /*
                     * This is a transient error encountered as part of the protocol handshake
                     * and we should do a few retries to see if it will get resolved (i.e. start
                     * over the full resolve/connect/handshake transaction)
                     */

                    base_type::resetStreamState();
                    base_type::m_resolver.reset();

                    base_type::startConnectionEstablishingInternal();

                    ++m_retries.lvalue();

                    return true;
                }

                /*
                 * It is important to invoke the base class implementation because
                 * it is normally expected to handle the protocol shutdown part
                 */

                return base_type::scheduleTaskFinishContinuation( eptrIn );
            }

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT OVERRIDE
            {
                if( ec )
                {
                    if( base_type::isCanceled() && asio::error::operation_aborted == *ec )
                    {
                        /*
                         * If the task was cancelled explicitly then
                         * asio::error::operation_aborted is normally expected
                         */

                        return true;
                    }
                }

                return base_type::isExpectedException( eptr, exception, ec );
            }

        public:

            auto endpointId() const -> std::string
            {
                return resolveMessage(
                    BL_MSG()
                        << base_type::m_query.host_name()
                        << ":"
                        << base_type::m_query.service_name()
                        );
            }

            auto endpointHost() const -> std::string
            {
                return base_type::m_query.host_name();
            }

            auto endpointPort() const -> unsigned short
            {
                return utils::lexical_cast< unsigned short >( base_type::m_query.service_name() );
            }
        };

        template
        <
            typename STREAM
        >
        using TcpConnectionEstablisherConnectorImpl = om::ObjectImpl< TcpConnectionEstablisherConnector< STREAM > >;

        /******************************************************************************************
         * =============================== TcpServerPolicyFixedT ==================================
         */

        template
        <
            bool ParamIsLogOnConnectAndDisconnect
        >
        class TcpServerPolicyFixedT
        {
            BL_CTR_DEFAULT( TcpServerPolicyFixedT, protected )
            BL_NO_COPY_OR_MOVE( TcpServerPolicyFixedT )

        public:

            static bool isLogOnConnect( SAA_in const std::size_t noOfConnections )
            {
                BL_UNUSED( noOfConnections );

                return ParamIsLogOnConnectAndDisconnect;
            }

            static bool isLogOnDisconnect( SAA_in const std::size_t noOfConnections )
            {
                BL_UNUSED( noOfConnections );

                return ParamIsLogOnConnectAndDisconnect;
            }
        };

        /******************************************************************************************
         * =============================== TcpServerPolicySmoothT =================================
         */

        template
        <
            typename E = void
        >
        class TcpServerPolicySmoothT
        {
            BL_NO_COPY_OR_MOVE( TcpServerPolicySmoothT )

        protected:

            enum : std::size_t
            {
                NO_OF_CONNECTIONS_MIN_DELTA_DEFAULT = 100U,
            };

            const std::size_t                                       m_noOfConnectionsMinDelta;
            std::atomic< std::size_t >                              m_noOfConnectionsLastLogged;

            TcpServerPolicySmoothT(
                SAA_in_opt const std::size_t noOfConnectionsMinDelta = NO_OF_CONNECTIONS_MIN_DELTA_DEFAULT
                ) NOEXCEPT
                :
                m_noOfConnectionsMinDelta( noOfConnectionsMinDelta ),
                m_noOfConnectionsLastLogged( 0U )
            {
            }

            bool isLogInternal( SAA_in const std::size_t noOfConnections ) NOEXCEPT
            {
                auto noOfConnectionsLastLoggedCurrent = m_noOfConnectionsLastLogged.load();

                /*
                 * We need to calculate the delta this way as we are dealing with unsigned
                 * values and we can't just subtract and get the absolute value
                 */

                const auto newDelta =
                    noOfConnections > noOfConnectionsLastLoggedCurrent ?
                        noOfConnections - noOfConnectionsLastLoggedCurrent
                        :
                        noOfConnectionsLastLoggedCurrent - noOfConnections;

                if(
                    0U == noOfConnectionsLastLoggedCurrent ||
                    0U == noOfConnections ||
                    newDelta >= m_noOfConnectionsMinDelta
                    )
                {
                    return m_noOfConnectionsLastLogged.compare_exchange_strong(
                        noOfConnectionsLastLoggedCurrent,
                        noOfConnections
                        );
                }

                return false;
            }

        public:

            bool isLogOnConnect( SAA_in const std::size_t noOfConnections ) NOEXCEPT
            {
                return isLogInternal( noOfConnections );
            }

            bool isLogOnDisconnect( SAA_in const std::size_t noOfConnections ) NOEXCEPT
            {
                return isLogInternal( noOfConnections );
            }
        };

        /******************************************************************************************
         * ============================ TcpServerPolicy* definitions ==============================
         */

        typedef TcpServerPolicyFixedT
        <
            true        /* ParamIsLogOnConnectAndDisconnect */
        >
        TcpServerPolicyVerbose;

        typedef TcpServerPolicyFixedT
        <
            false       /* ParamIsLogOnConnectAndDisconnect */
        >
        TcpServerPolicyQuiet;

        typedef TcpServerPolicySmoothT<> TcpServerPolicySmooth;

        typedef TcpServerPolicySmooth TcpServerPolicyDefault;

        namespace detail
        {
            template
            <
                typename STREAM,
                bool ProtocolHandshakeNeeded = STREAM::isProtocolHandshakeNeeded
            >
            class HandshakeTaskHelper;

            template
            <
                typename STREAM
            >
            class HandshakeTaskHelper< STREAM, true >
            {
                typedef om::ObjectImpl< STREAM >                        task_impl;

            public:

                static auto createTask( SAA_inout typename STREAM::stream_ref&& connectedStream ) -> om::ObjPtr< Task >
                {
                    auto task = task_impl::createInstance( "SSL_Handshake_Task" /* taskName */ );

                    task -> attachStream( BL_PARAM_FWD( connectedStream ) );

                    return om::qi< Task >( task );
                }

                static auto getStream( const om::ObjPtr< Task >& task ) -> typename STREAM::stream_ref
                {
                    return om::qi< task_impl >( task ) -> detachStream();
                }
            };

            template
            <
                typename STREAM
            >
            class HandshakeTaskHelper< STREAM, false >
            {
            public:

                static auto createTask( SAA_inout typename STREAM::stream_ref&& connectedStream ) -> om::ObjPtr< Task >
                {
                    BL_UNUSED( connectedStream );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "HandshakeTaskHelper::createTask not supported for the supplied STREAM type"
                        );
                }

                static auto getStream( const om::ObjPtr< Task >& task ) -> typename STREAM::stream_ref
                {
                    BL_UNUSED( task );

                    BL_THROW(
                        NotSupportedException(),
                        BL_MSG()
                            << "HandshakeTaskHelper::getStream not supported for the supplied STREAM type"
                        );
                }
            };
        }

        /******************************************************************************************
         * ==================================== TcpServerBase =====================================
         */

        /**
         * @brief class TcpServerBase
         */

        template
        <
            typename STREAM,
            typename SERVERPOLICY = TcpServerPolicyDefault
        >
        class TcpServerBase :
            public TcpConnectionEstablisherAcceptor< STREAM >,
            public ExecutionQueueNotifyBase,
            public SERVERPOLICY
        {
        public:

            typedef TcpServerBase< STREAM, SERVERPOLICY >                                       this_type;
            typedef TcpConnectionEstablisherAcceptor< STREAM >                                  base_type;
            typedef typename STREAM::stream_ref                                                 stream_ref;
            typedef SERVERPOLICY                                                                server_policy_t;

        private:

            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( TcpServerBase )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY_CHAIN_BASE( base_type )
                BL_QITBL_ENTRY_CHAIN_BASE( ExecutionQueueNotifyBase )
            BL_QITBL_END( Task )

        protected:

            const om::ObjPtr< TaskControlTokenRW >                                              m_controlToken;
            om::ObjPtrDisposable< ExecutionQueue >                                              m_eqConnections;
            bool                                                                                m_forceShutdown;
            cpp::SafeUniquePtr< asio::deadline_timer >                                          m_timer;
            cpp::SafeUniquePtr< asio::strand >                                                  m_strand;
            om::ObjPtr< om::Proxy >                                                             m_notifyCB;
            std::unordered_map< Task*, std::string >                                            m_activeEndpoints;

            om::ObjPtr< om::Proxy >                                                             m_hostServices;
            om::ObjPtr< om::Proxy >                                                             m_executionServices;
            om::ObjPtrDisposable< ExecutionQueue >                                              m_eqSupportingTasks;

            TcpServerBase(
                SAA_in                  const om::ObjPtr< TaskControlTokenRW >&                 controlToken,
                SAA_in                  std::string&&                                           host,
                SAA_in                  const unsigned short                                    port,
                SAA_in                  const std::string&                                      privateKeyPem,
                SAA_in                  const std::string&                                      certificatePem
                )
                :
                base_type( std::forward< std::string >( host ), port ),
                m_controlToken( om::copy( controlToken ) ),
                m_forceShutdown( true )
            {
                if( base_type::isProtocolHandshakeNeeded )
                {
                    BL_ASSERT( ! privateKeyPem.empty() && ! certificatePem.empty() );

                    base_type::initServerContext( privateKeyPem, certificatePem );
                }
            }

            ~TcpServerBase() NOEXCEPT
            {
                /*
                 * Last ditch to disconnect all callbacks
                 */

                BL_NOEXCEPT_BEGIN()

                if( m_hostServices )
                {
                    m_hostServices -> disconnect();
                }

                if( m_executionServices )
                {
                    m_executionServices -> disconnect();
                }

                if( m_notifyCB )
                {
                    m_notifyCB -> disconnect();
                }

                BL_NOEXCEPT_END()
            }

            /**
             * @brief The derived classes will override this to create the server connection task
             */

            virtual om::ObjPtr< Task > createConnection( SAA_inout stream_ref&& connectedStream ) = 0;

            /**
             * @brief The derived classes could override this method if they want to control that aspect
             *
             * By default we simply use the HandshakeTaskHelper< ... > template to create a handshake task
             */

            virtual om::ObjPtr< Task > createProtocolHandshakeTask( SAA_inout stream_ref&& connectedStream )
            {
                return detail::HandshakeTaskHelper< STREAM >::createTask( BL_PARAM_FWD( connectedStream ) );
            }

            virtual auto onTaskStoppedNothrow(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_inout_opt           bool*                                       isExpectedException = nullptr
                ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                if( eptrIn )
                {
                    /*
                     * Note that the task lock isn't held while this callback is called,
                     * so we can safely call cancelAll() with wait=true
                     */

                    if( m_hostServices )
                    {
                        m_hostServices -> disconnect();
                    }

                    if( m_executionServices )
                    {
                        m_executionServices -> disconnect();
                    }

                    m_eqSupportingTasks -> forceFlushNoThrow( true /* wait */ );

                    m_eqConnections -> forceFlushNoThrow( true /* wait */ );

                    m_notifyCB -> disconnect();
                }
                else
                {
                    /*
                     * If we're terminating without an error everything is
                     * expected to be shutdown already (i.e. no outstanding connections)
                     */

                    if( ! m_eqConnections -> isEmpty() || ! m_eqSupportingTasks -> isEmpty() )
                    {
                        BL_RIP_MSG(
                            "Forcefully terminating a server task while there are "
                            "outstanding connections or supporting tasks"
                            );
                    }
                }

                if( m_controlToken )
                {
                    m_controlToken -> unregisterCancelableTask( om::ObjPtrCopyable< Task >::acquireRef( this ) );
                }

                return base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );
            }

            virtual void onEvent(
                SAA_in                      const ExecutionQueueNotify::EventId     eventId,
                SAA_in_opt                  const om::ObjPtrCopyable< Task >&       task
                ) NOEXCEPT OVERRIDE
            {
                BL_ASSERT( ExecutionQueueNotify::TaskDiscarded == eventId || ExecutionQueueNotify::AllTasksCompleted == eventId );

                if( ExecutionQueueNotify::AllTasksCompleted == eventId )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "No outstanding connections currently open for "
                            << net::formatEndpointId( base_type::m_localEndpoint )
                        );

                    return;
                }

                BL_ASSERT( task );

                BL_MUTEX_GUARD( base_type::m_lock );

                const auto pos = m_activeEndpoints.find( task.get() );

                if( pos == m_activeEndpoints.end() )
                {
                    /*
                     * Handshake task has completed, schedule the actual processing now
                     *
                     * If the handshake task has failed we simply ignore as nothing much
                     * can be done in this case - the task itself would report the error
                     * information in the server logs
                     */

                    if( task -> isFailed() )
                    {
                        return;
                    }

                    BL_WARN_NOEXCEPT_BEGIN()

                    processConfiguredConnection(
                        detail::HandshakeTaskHelper< STREAM >::getStream( task )
                        );

                    BL_WARN_NOEXCEPT_END( "TcpServerBase::onEvent() - processConfiguredConnection" )

                    return;
                }

                /*
                 * We need to be very careful what code we execute here because the task maybe in shutdown
                 * already disconnected, completed, disposed, etc.
                 */

                const auto& remoteEndpointId = pos -> second;

                auto& loggingChannel =
                    server_policy_t::isLogOnDisconnect( m_eqConnections -> size() ) ?
                        Logging::debug() : Logging::trace();

                BL_LOG(
                    loggingChannel,
                    BL_MSG()
                        << "Connection terminated with: "
                        << remoteEndpointId
                        << "; total # of connections for "
                        << net::formatEndpointId( base_type::m_localEndpoint )
                        << " is "
                        << m_eqConnections -> size()
                    );

                m_activeEndpoints.erase( pos );

                BL_WARN_NOEXCEPT_BEGIN()

                onTaskTerminated( task );

                BL_WARN_NOEXCEPT_END( "TcpServerBase::onEvent() - onTaskTerminated" )
            }

            /**
             * @brief The derived classes can override this to take action upon tearing down a
             * connection
             */

            virtual void onTaskTerminated( const om::ObjPtrCopyable< Task >& task )
            {
                BL_UNUSED( task );

                /*
                 * NOP by default
                 */
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                if( ! m_eqConnections )
                {
                    /*
                     * The execution queue has ExecutionQueue::OptionKeepNone to ensure that
                     * the connections are discarded automatically as they're closed
                     */

                    m_eqConnections =
                        ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepNone );
                }

                if( ! m_eqSupportingTasks )
                {
                    /*
                     * The execution queue has ExecutionQueue::OptionKeepNone to ensure that
                     * the supporting tasks are discarded automatically as they're completed
                     */

                    m_eqSupportingTasks =
                        ExecutionQueueImpl::createInstance< ExecutionQueue >( ExecutionQueue::OptionKeepNone );

                    m_executionServices = om::ProxyImpl::createInstance< om::Proxy >();
                    m_executionServices -> connect( m_eqSupportingTasks.get() );
                }

                m_notifyCB = om::ProxyImpl::createInstance< om::Proxy >();
                m_notifyCB -> connect( static_cast< Task* >( this ) );

                m_eqConnections -> setNotifyCallback(
                    om::copy( m_notifyCB ),
                    tasks::ExecutionQueueNotify::AllEvents /* eventsMask */
                    );

                base_type::scheduleTask( eq );

                if( m_controlToken )
                {
                    m_controlToken -> registerCancelableTask( om::ObjPtrCopyable< Task >::acquireRef( this ) );
                }
            }

            void onTimer( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()

                if( asio::error::operation_aborted != ec )
                {
                    if( m_eqConnections -> isEmpty() && m_eqSupportingTasks -> isEmpty() )
                    {
                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "All outstanding connections and supporting tasks for server "
                                << this
                                << " were closed / completed successfully"
                            );

                        m_notifyCB -> disconnect();
                    }
                    else
                    {
                        /*
                         * There are still outstanding connections; we should continue waiting,
                         * but we should try to cancel connections again
                         */

                        if( m_forceShutdown )
                        {
                            m_eqConnections -> cancelAll( false /* wait */ );

                            m_eqSupportingTasks -> cancelAll( false /* wait */ );
                        }

                        scheduleTimer();

                        return;
                    }
                }

                BL_TASKS_HANDLER_CHK_EC( base_type::m_errorCode );

                BL_TASKS_HANDLER_END()
            }

            void scheduleTimer()
            {
                m_timer -> expires_from_now( time::milliseconds( 50 ) );

                m_timer -> async_wait(
                    m_strand -> wrap(
                        cpp::bind(
                            &this_type::onTimer,
                            om::ObjPtrCopyable< this_type, Task >::acquireRef( this ),
                            asio::placeholders::error
                            )
                        )
                    );
            }

            virtual bool continueAfterStoppedAccepting() OVERRIDE
            {
                /*
                 * Close the acceptor to stop accepting connections immediately
                 *
                 * Then also notify the control token to cancel anything that is
                 * pending and disconnect the host services immediately
                 */

                base_type::m_acceptor -> close();

                if( m_controlToken )
                {
                    /*
                     * Invoke unregisterCancelableTask( ... ) here to ensure that we are not going to call
                     * requestCancel() on ourselves otherwise we can have an exception from trying to
                     * re-acquire the non-recursive lock (because we are holding the task lock already)
                     */

                    m_controlToken -> unregisterCancelableTask( om::ObjPtrCopyable< Task >::acquireRef( this ) );

                    m_controlToken -> requestCancel();
                }

                os::mutex_unique_lock guard;

                if( m_hostServices )
                {
                    m_hostServices -> disconnect( &guard );
                }

                if( m_executionServices )
                {
                    m_executionServices -> disconnect( &guard );
                }

                const bool areConnectionsPending =
                    m_eqConnections && false == m_eqConnections -> isEmpty();

                const bool areSupportingTasksPending =
                    m_eqSupportingTasks && false == m_eqSupportingTasks -> isEmpty();

                if( areConnectionsPending )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Stopped accepting from server "
                            << this
                            << " and waiting for "
                            << m_eqConnections -> size()
                            << " outstanding connections to shutdown [forceShutdown="
                            << m_forceShutdown
                            << "]"
                        );
                }

                if( areSupportingTasksPending )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Stopped accepting from server "
                            << this
                            << " and waiting for "
                            << m_eqSupportingTasks -> size()
                            << " outstanding supporting tasks to complete [forceShutdown="
                            << m_forceShutdown
                            << "]"
                        );
                }

                if( areConnectionsPending || areSupportingTasksPending )
                {
                    if( m_forceShutdown )
                    {
                        m_eqConnections -> cancelAll( false /* wait */ );

                        m_eqSupportingTasks -> cancelAll( false /* wait */ );
                    }

                    const auto threadPool = ThreadPoolDefault::getDefault( base_type::getThreadPoolId() );
                    BL_ASSERT( threadPool );

                    m_timer.reset( new asio::deadline_timer( threadPool -> aioService() ) );

                    m_strand.reset( new asio::strand( threadPool -> aioService() ) );

                    scheduleTimer();

                    return true;
                }

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Stopped accepting from server "
                        << this
                        << "; no outstanding connections or supporting tasks pending [forceShutdown="
                        << m_forceShutdown
                        << "]"
                    );

                m_notifyCB -> disconnect();

                return false;
            }

            virtual void processIncomingConnection( SAA_inout stream_ref&& connectedStream ) OVERRIDE
            {
                ( void ) base_type::tryConfigureConnectedStream( *connectedStream );

                if( base_type::isProtocolHandshakeNeeded )
                {
                    /*
                     * Schedule the handshake task
                     */

                    m_eqConnections -> push_back(
                        createProtocolHandshakeTask( BL_PARAM_FWD( connectedStream ) )
                        );
                }
                else
                {
                    /*
                     * No handshake required, schedule the actual processing
                     */

                    processConfiguredConnection( BL_PARAM_FWD( connectedStream ) );
                }
            }

            void processConfiguredConnection( SAA_inout stream_ref&& connectedStream )
            {
                /*
                 * Connection was established. Let's move the connected socket
                 * into a connection task object implemented by the server (via createConnection)
                 * and then move the connection task into m_eqConnections
                 */

                BL_ASSERT( m_eqConnections );

                std::string remoteEndpointId;

                try
                {
                    /*
                     * net::safeRemoteEndpointId isn't used below to avoid retries leading to server log
                     * clutter and processing delays in case the client has closed the socket prematurely
                     */

                    remoteEndpointId = net::formatEndpointId( connectedStream -> lowest_layer().remote_endpoint() );
                }
                catch( eh::system_error& e )
                {
                    if( asio::error::not_connected == e.code() )
                    {
                        /*
                         * Expected error in case other side closed the socket,
                         * just return here to avoid logging the exception
                         */

                        return;
                    }

                    throw;
                }

                const auto connection = createConnection( BL_PARAM_FWD( connectedStream ) );

                if( connection )
                {
                    m_activeEndpoints[ connection.get() ] = remoteEndpointId;

                    try
                    {
                        /*
                         * Start the connection task
                         */

                        m_eqConnections -> push_back( om::qi< Task >( connection ) );
                    }
                    catch( std::exception& )
                    {
                        const auto pos = m_activeEndpoints.find( connection.get() );

                        if( pos != m_activeEndpoints.end() )
                        {
                            m_activeEndpoints.erase( pos );
                        }

                        throw;
                    }

                    auto& loggingChannel =
                        server_policy_t::isLogOnConnect( m_eqConnections -> size() ) ?
                            Logging::debug() : Logging::trace();

                    BL_LOG(
                        loggingChannel,
                        BL_MSG()
                            << "Connection established from: "
                            << remoteEndpointId
                            << "; total # of connections for "
                            << net::formatEndpointId( base_type::m_localEndpoint )
                            << " is "
                            << m_eqConnections -> size()
                        );
                }
            }

        public:

            void setHostServices( SAA_in om::ObjPtr< om::Proxy >&& hostServices ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( base_type::m_lock );

                m_hostServices = BL_PARAM_FWD( hostServices );

                BL_NOEXCEPT_END()
            }

            auto executionQueue() const NOEXCEPT -> const om::ObjPtr< ExecutionQueue >&
            {
                return m_eqConnections;
            }

            auto activeEndpoints() -> std::vector< om::ObjPtr< Task > >
            {
                std::vector< om::ObjPtr< Task > > result;

                BL_MUTEX_GUARD( base_type::m_lock );

                result.reserve( m_activeEndpoints.size() );

                for( const auto& pair : m_activeEndpoints )
                {
                    result.push_back( om::copy( pair.first ) /* endpoint task */ );
                }

                return result;
            }
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_TCPBASETASKS_H_ */
