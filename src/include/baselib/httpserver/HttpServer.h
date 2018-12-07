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

#ifndef __BL_HTTPSERVER_HTTPSERVER_H_
#define __BL_HTTPSERVER_HTTPSERVER_H_

#include <baselib/tasks/TcpBaseTasks.h>
#include <baselib/tasks/TcpSslBaseTasks.h>

#include <baselib/httpserver/Parser.h>
#include <baselib/httpserver/Response.h>
#include <baselib/httpserver/ServerBackendProcessing.h>
#include <baselib/httpserver/detail/ParserHelpers.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace httpserver
    {
        /********************************************************
         * The receive request task
         */

        template
        <
            typename STREAM
        >
        class HttpServerReceiveRequestTask:
            public STREAM
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( HttpServerReceiveRequestTask )

        protected:

            typedef HttpServerReceiveRequestTask< STREAM >                                      this_type;
            typedef STREAM                                                                      base_type;

            typedef httpserver::detail::ServerResult                                            ServerResult;
            typedef httpserver::detail::ParserHelpers                                           ParserHelpers;
            typedef httpserver::detail::HttpParserResult                                        HttpParserResult;

            om::ObjPtr< data::DataBlock >                                                       m_buffer;
            const om::ObjPtr< Parser >                                                          m_parser;
            om::ObjPtr< Request >                                                               m_request;

            ServerResult                                                                        m_parsingStatus;
            cpp::ScalarTypeIniter< bool >                                                       m_isStreamTruncationError;

        protected:

            HttpServerReceiveRequestTask( SAA_in typename base_type::stream_ref&& connectedStream )
                :
                m_buffer( data::DataBlock::createInstance( 512U ) ),
                m_parser( Parser::createInstance() ),
                m_parsingStatus( ParserHelpers::serverResult( HttpParserResult::MORE_DATA_REQUIRED ) )
            {
                base_type::attachStream( BL_PARAM_FWD( connectedStream ) );
            }

            void scheduleRead()
            {
                base_type::getStream().async_read_some(
                    asio::buffer( m_buffer -> pv(), m_buffer -> size() ),
                    cpp::bind(
                        &this_type::handleRead,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );
            }

            void handleRead(
                SAA_in      const eh::error_code&                                               ec,
                SAA_in      const std::size_t                                                   bytesTransferred
                ) NOEXCEPT
            {
                if( base_type::isStreamTruncationError( ec ) )
                {
                    m_isStreamTruncationError = true;
                }

                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                m_parsingStatus = m_parser -> parse(
                    reinterpret_cast< char* >( m_buffer -> pv() ),
                    reinterpret_cast< char* >( m_buffer -> pv() ) + bytesTransferred
                    );

                const auto parserResult = m_parsingStatus.first;

                if( HttpParserResult::MORE_DATA_REQUIRED == parserResult )
                {
                    scheduleRead();

                    return;
                }

                if( HttpParserResult::PARSED == parserResult )
                {
                    m_request = m_parser -> buildRequest();

                    BL_CHK(
                        false,
                        nullptr != m_request,
                        BL_MSG()
                            << "Parser::buildRequest() should not return nullptr if parse status is PARSED"
                        );
                }
                else
                {
                    /*
                     * If we are here the result can only be HttpParserResult::PARSING_ERROR and
                     * in this case we check to throw the exception or if exception was not
                     * provided by the parser then we throw UnexpectedException
                     */

                    BL_ASSERT( HttpParserResult::PARSING_ERROR == parserResult );

                    if( m_parsingStatus.second /* exception */ )
                    {
                        cpp::safeRethrowException( m_parsingStatus.second /* exception */ );
                    }

                    BL_THROW(
                        UnexpectedException(),
                        BL_MSG()
                            << "HTTP request parser returned unexpected parsing error "
                            << static_cast< std::size_t >( parserResult )
                            << " and no error details"
                        );
                }

                BL_TASKS_HANDLER_END()
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< tasks::ExecutionQueue >& eq ) OVERRIDE
            {
                /*
                 * Save the thread pool, create the timer and schedule it immediately
                 */

                BL_ASSERT( eq );

                base_type::ensureChannelIsOpen();

                BL_UNUSED( eq );

                scheduleRead();
            }

        public:

            auto request() NOEXCEPT -> const om::ObjPtr< Request >&
            {
                return m_request;
            }

            auto parsingStatus() const NOEXCEPT -> const ServerResult&
            {
                return m_parsingStatus;
            }

            auto isStreamTruncationError() const NOEXCEPT -> bool
            {
                return m_isStreamTruncationError;
            }
        };

        template
        <
            typename STREAM
        >
        using HttpServerReceiveRequestTaskImpl = om::ObjectImpl< HttpServerReceiveRequestTask< STREAM > >;

        /********************************************************
         * The send response task
         */

        template
        <
            typename STREAM
        >
        class HttpServerSendResponseTask:
            public STREAM
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( HttpServerSendResponseTask )

        protected:

            typedef HttpServerSendResponseTask< STREAM >                                        this_type;
            typedef STREAM                                                                      base_type;

            const om::ObjPtr< Response >                                                        m_response;

        protected:

            HttpServerSendResponseTask(
                SAA_in      typename base_type::stream_ref&&                                    connectedStream,
                SAA_in      om::ObjPtr< Response >&&                                            response
                )
                :
                m_response( BL_PARAM_FWD( response ) )
            {
                base_type::attachStream( BL_PARAM_FWD( connectedStream ) );
                base_type::isCloseStreamOnTaskFinish( true );
            }

            void scheduleWrite()
            {
                asio::async_write(
                    base_type::getStream(),
                    asio::buffer( m_response -> getSerialized().c_str(), m_response -> getSerialized().size() ),
                    boost::bind(
                        &this_type::handleWrite,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );
            }

            void handleWrite(
                SAA_in      const eh::error_code&                                               ec,
                SAA_in      const std::size_t                                                   bytesTransferred
                ) NOEXCEPT
            {
                BL_UNUSED( bytesTransferred );

                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                BL_TASKS_HANDLER_END()
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< tasks::ExecutionQueue >& eq ) OVERRIDE
            {
                /*
                 * Save the thread pool, create the timer and schedule it immediately
                 */

                BL_ASSERT( eq );

                base_type::ensureChannelIsOpen();

                BL_UNUSED( eq );

                scheduleWrite();
            }
        };

        template
        <
            typename STREAM
        >
        using HttpServerSendResponseTaskImpl = om::ObjectImpl< HttpServerSendResponseTask< STREAM > >;

        /********************************************************
         * The main connection task (wrapper with continuations)
         */

        /**
         * @brief class HttpServerConnection
         */

        template
        <
            typename STREAM
        >
        class HttpServerConnection : public tasks::WrapperTaskBase
        {
            BL_DECLARE_OBJECT_IMPL( HttpServerConnection )

        public:

            typedef HttpServerConnection< STREAM >                                              this_type;
            typedef tasks::WrapperTaskBase                                                      base_type;

            typedef httpserver::detail::HttpParserResult                                        HttpParserResult;

        protected:

            typedef HttpServerReceiveRequestTaskImpl< STREAM >                                  receive_task_t;
            typedef HttpServerSendResponseTaskImpl< STREAM >                                    send_task_t;

            enum State : std::size_t
            {
                RECEIVE,
                PROCESS,
                RESPOND,
            };

            om::ObjPtr< receive_task_t >                                                        m_receiveRequestTask;
            om::ObjPtr< tasks::Task >                                                           m_processingTask;
            om::ObjPtr< send_task_t >                                                           m_sendResponseTask;

            const om::ObjPtr< ServerBackendProcessing >                                         m_backend;
            State                                                                               m_state;

            HttpServerConnection(
                SAA_in          om::ObjPtr< ServerBackendProcessing >&&                         backend,
                SAA_in          typename STREAM::stream_ref&&                                   connectedStream
                )
                :
                m_backend( BL_PARAM_FWD( backend ) ),
                m_state( RECEIVE )
            {
                m_receiveRequestTask = receive_task_t::createInstance( BL_PARAM_FWD( connectedStream ) );

                m_wrappedTask = om::qi< tasks::Task >( m_receiveRequestTask );
            }

            void scheduleStdErrorResponse( SAA_in const std::exception_ptr& eptr )
            {
                const auto remoteEndpointId = m_receiveRequestTask -> safeRemoteEndpointId();

                auto response = m_backend -> getStdErrorResponse(
                    http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST,
                    eptr
                    );

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "An HTTP request failed from the following endpoint: "
                        << remoteEndpointId
                        << "; error response details:\n"
                        << response -> getSerialized()
                    );

                m_sendResponseTask = send_task_t::createInstance(
                    m_receiveRequestTask -> detachStream(),
                    std::move( response )
                    );

                m_wrappedTask = om::qi< tasks::Task >( m_sendResponseTask );
                m_state = RESPOND;
            }

        public:

            virtual om::ObjPtr< Task > continuationTask() OVERRIDE
            {
                auto task = base_type::handleContinuationForward();

                if( task )
                {
                    return task;
                }

                const auto eptr = exception();

                BL_MUTEX_GUARD( m_lock );

                if( eptr )
                {
                    if( RESPOND == m_state )
                    {
                        return nullptr;
                    }

                    if( RECEIVE == m_state && m_receiveRequestTask -> isStreamTruncationError() )
                    {
                        /*
                         * Some browsers (e.g. Chrome) create empty requests for SSL connection
                         * and then terminate them abruptly, so they can get the server certificate
                         * and check it separately which can be slow (as it may require user input)
                         * and they don't want to keep the connection open
                         *
                         * We should simply ignore these requests and don't return anything
                         *
                         * For more details see the following post:
                         * https://security.stackexchange.com/questions/35833/multiple-ssl-connections-in-a-single-https-web-request
                         */

                        return nullptr;
                    }

                    scheduleStdErrorResponse( eptr );
                    return om::copyAs< Task >( this );
                }

                switch( m_state )
                {
                    case RECEIVE:
                        {
                            const auto parsingStatus = m_receiveRequestTask -> parsingStatus();

                            if( parsingStatus.first != HttpParserResult::PARSED )
                            {
                                scheduleStdErrorResponse( parsingStatus.second /* exception */ );
                            }
                            else
                            {
                                m_processingTask = m_backend -> getProcessingTask(
                                    om::copy( m_receiveRequestTask -> request() )
                                    );
                                m_wrappedTask = om::qi< tasks::Task >( m_processingTask );
                                m_state = PROCESS;
                            }
                        }
                        break;

                    case PROCESS:
                        {
                            m_sendResponseTask = send_task_t::createInstance(
                                BL_PARAM_FWD( m_receiveRequestTask -> detachStream() ),
                                std::move( m_backend -> getResponse( m_processingTask ) )
                                );
                            m_wrappedTask = om::qi< tasks::Task >( m_sendResponseTask );
                            m_state = RESPOND;
                        }
                        break;

                    case RESPOND:

                        /*
                         * We are done
                         */

                        return nullptr;

                    default:

                        BL_ASSERT( false );
                }

                return om::copyAs< Task >( this );
            }
        };

        template
        <
            class STREAM
        >
        using HttpServerConnectionImpl = om::ObjectImpl< HttpServerConnection< STREAM > >;

        /********************************************************
         * The Server (acceptor)
         */

        template
        <
            typename STREAM,
            typename SERVERPOLICY = tasks::TcpServerPolicyVerbose
        >
        class HttpServerT:
            public tasks::TcpServerBase< STREAM, SERVERPOLICY >
        {
            BL_DECLARE_OBJECT_IMPL( HttpServerT )

        public:

            typedef tasks::TcpServerBase< STREAM, SERVERPOLICY >                                base_type;
            typedef typename STREAM::stream_ref                                                 stream_ref;

            const om::ObjPtr< ServerBackendProcessing >                                         m_backend;

        protected:

            HttpServerT(
                SAA_in      om::ObjPtr< ServerBackendProcessing >&&                             backend,
                SAA_in      const om::ObjPtr< tasks::TaskControlTokenRW >&                      controlToken,
                SAA_in      std::string&&                                                       host,
                SAA_in      const unsigned short                                                port,
                SAA_in      const std::string&                                                  privateKeyPem,
                SAA_in      const std::string&                                                  certificatePem
                )
                :
                base_type( controlToken, BL_PARAM_FWD( host ), port, privateKeyPem, certificatePem ),
                m_backend( BL_PARAM_FWD( backend ) )
            {
            }

            virtual om::ObjPtr< tasks::Task > createConnection( SAA_inout stream_ref&& connectedStream ) OVERRIDE
            {
                const auto connection = HttpServerConnectionImpl< STREAM >::createInstance(
                    om::copy( m_backend ),
                    BL_PARAM_FWD( connectedStream )
                    );

                return om::qi< tasks::Task >( connection );
            }
        };

        typedef om::ObjectImpl< HttpServerT< tasks::TcpSocketAsyncBase > >                                  HttpServer;
        typedef om::ObjectImpl< HttpServerT< tasks::TcpSocketAsyncBase, tasks::TcpServerPolicyQuiet > >     HttpServerQuiet;
        typedef om::ObjectImpl< HttpServerT< tasks::TcpSslSocketAsyncBase > >                               HttpSslServer;
        typedef om::ObjectImpl< HttpServerT< tasks::TcpSslSocketAsyncBase, tasks::TcpServerPolicyQuiet > >  HttpSslServerQuiet;

    } // httpserver

} // bl

#endif /* __BL_HTTPSERVER_HTTPSERVER_H_ */
