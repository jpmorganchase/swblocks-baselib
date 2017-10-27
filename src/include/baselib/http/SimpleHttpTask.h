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

#ifndef __BL_TASKS_SIMPLEHTTPTASK_H_
#define __BL_TASKS_SIMPLEHTTPTASK_H_

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/TcpBaseTasks.h>

#include <baselib/http/Globals.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/TlsState.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/Utils.h>
#include <baselib/core/NetUtils.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <sstream>
#include <set>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class SimpleHttpTask
         */

        template
        <
            typename STREAM = TcpSocketAsyncBase
        >
        class SimpleHttpTaskT :
            public http::Parameters,
            public TcpConnectionEstablisherConnector< STREAM >
        {
            BL_DECLARE_OBJECT_IMPL( SimpleHttpTaskT )

        public:

            typedef SimpleHttpTaskT< STREAM >                                       this_type;
            typedef TcpConnectionEstablisherConnector< STREAM >                     base_type;
            typedef asio::ip::tcp                                                   tcp;

            typedef http::HeadersMap                                                HeadersMap;

        protected:

            enum
            {
                MAX_DUMP_STRING_LENGTH = 2048
            };

            static const std::string                                                g_protocolDefault;

            static const str::regex                                                 g_hrefRegex;
            static const str::regex                                                 g_charsetRegex;

            const std::string                                                       m_path;
            const std::string                                                       m_action;
            std::string                                                             m_contentIn;
            std::string                                                             m_contentOut;
            cpp::SafeOutputStringStream                                             m_contentOutStream;
            asio::streambuf                                                         m_request;
            asio::streambuf                                                         m_response;
            const om::ObjPtr< data::DataBlock >                                     m_contentBuffer;
            const HeadersMap                                                        m_requestHeaders;
            HeadersMap                                                              m_responseHeaders;
            size_t                                                                  m_responseLength;
            unsigned int                                                            m_httpStatus;
            std::set< unsigned int >                                                m_expectedHttpStatuses;
            std::string                                                             m_remoteEndpointId;
            cpp::SafeUniquePtr< asio::deadline_timer >                              m_timer;
            cpp::ScalarTypeIniter< bool >                                           m_timedOut;
            time::time_duration                                                     m_timeout;
            cpp::ScalarTypeIniter< bool >                                           m_isSecureMode;
            cpp::ScalarTypeIniter< bool >                                           m_isExpectUtf8Content;

            SimpleHttpTaskT(
                SAA_in          std::string&&               host,
                SAA_in          const unsigned short        port,
                SAA_in          const std::string&          path,
                SAA_in          const std::string&          action,
                SAA_in          const std::string&          content,
                SAA_in          HeadersMap&&                requestHeaders
                )
                :
                base_type( BL_PARAM_FWD( host ), port ),
                m_path( path ),
                m_action( action ),
                m_contentIn( content ),
                m_contentBuffer( data::DataBlock::createInstance( 2048U /* capacity */ ) ),
                m_requestHeaders( BL_PARAM_FWD( requestHeaders ) ),
                m_responseLength( -1 ),
                m_httpStatus( HTTP_STATUS_UNDEFINED ),
                m_timeout(
                    time::seconds(
                        "GET" == action ?
                            http::Parameters::timeoutInSecondsGet() :
                            http::Parameters::timeoutInSecondsOther()
                        )
                    )
            {
                /*
                 * Request the socket to be closed when the task finishes which will ensure that
                 * the protocol shutdown is also executed
                 */

                base_type::isCloseStreamOnTaskFinish( true );
            }

            void cancelTimer()
            {
                if( m_timer )
                {
                    m_timer -> cancel();
                }
            }

            std::string createUrl() const
            {
                return resolveMessage(
                    BL_MSG()
                        << getProtocol()
                        << "://"
                        << base_type::m_query.host_name()
                        << ":"
                        << base_type::m_query.service_name()
                        << ( m_isSecureMode ? "[REDACTED]" : m_path )
                    );
            }

            std::string createTimeoutMessage()
            {
                return resolveMessage(
                    BL_MSG()
                        << "HTTP "
                        << m_action
                        << " request to '"
                        << createUrl()
                        << "' has timed out ("
                        << m_timeout
                        << ")"
                    );
            }

            void chkHttpResponse(
                SAA_in          const bool                  cond,
                SAA_in          const std::string&          info
                )
            {
                if( ! cond )
                {
                    BL_THROW(
                        createException< UnexpectedException >( false /* isExpected */ ),
                        BL_MSG()
                            << "HTTP "
                            << m_action
                            << " request to '"
                            << createUrl()
                            << "' returned invalid response: "
                            << ( m_isSecureMode ? "[REDACTED]" : info )
                        );
                }
            }

            virtual const std::string& getProtocol() const NOEXCEPT
            {
                return g_protocolDefault;
            }

            virtual auto onTaskStoppedNothrow(
                SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
                SAA_inout_opt           bool*                                       isExpectedException = nullptr
                ) NOEXCEPT
                -> std::exception_ptr OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                cancelTimer();

                if( TaskBase::isCanceled() && m_timedOut )
                {
                    if( eptrIn )
                    {
                        return std::make_exception_ptr(
                            BL_EXCEPTION(
                                createException< TimeoutException >( true /* isExpected */ )
                                    << eh::errinfo_nested_exception_ptr( eptrIn ),
                                createTimeoutMessage()
                                )
                            );
                    }

                    return std::make_exception_ptr(
                        BL_EXCEPTION(
                            createException< TimeoutException >( true /* isExpected */ ),
                            createTimeoutMessage()
                            )
                        );
                }

                BL_NOEXCEPT_END()

                return base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );
            }

            virtual void cancelTask() OVERRIDE
            {
                cancelTimer();

                base_type::cancelTask();
            }

            virtual void scheduleTask( SAA_in const std::shared_ptr< ExecutionQueue >& eq ) OVERRIDE
            {
                initRequest();

                base_type::scheduleTask( eq );
            }

            void scheduleTimer()
            {
                m_timer -> expires_from_now( m_timeout );

                m_timer -> async_wait(
                    cpp::bind(
                        &this_type::onTimer,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );
            }

            virtual bool continueAfterConnected() OVERRIDE
            {
                base_type::ensureChannelIsOpen();

                m_remoteEndpointId = net::safeRemoteEndpointId( base_type::getSocket() );

                m_timer.reset(
                    new asio::deadline_timer(
                        base_type::m_resolver -> get_io_service(),
                        time::milliseconds( 0 )
                        )
                    );

                scheduleTimer();

                asio::async_write(
                    base_type::getStream(),
                    m_request,
                    cpp::bind(
                        &this_type::doRequest,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );

                return true;
            }

            void onTimer( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                if( asio::error::operation_aborted == ec )
                {
                    return;
                }

                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( TaskBase::m_lock );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "HTTP task timeout timer kicked in"
                        << "; error code is "
                        << eh::errorCodeToString( ec )
                        << "; m_timedOut is "
                        << m_timedOut.value()
                        << "; task state is "
                        << TaskBase::m_state
                        << "; channel open is "
                        << base_type::isChannelOpen()
                    );

                if( ! ec && TaskBase::Running == TaskBase::m_state && base_type::isChannelOpen() )
                {
                    m_timedOut = true;

                    base_type::requestCancelInternal();
                }

                BL_NOEXCEPT_END()
            }

            void chk2EnhanceException( SAA_in eh::exception& exception ) const
            {
                if( ! eh::get_error_info< eh::errinfo_http_response_headers >( exception ) )
                {
                    cpp::SafeOutputStringStream headersBuffer;

                    if( m_isSecureMode )
                    {
                        headersBuffer
                            << "[REDACTED]";
                    }
                    else
                    {
                        const auto& names = errorResponseHeaderNamesLvalue();

                        for( const auto& name : names )
                        {
                            const auto value = tryGetResponseHeader( name );

                            if( value )
                            {
                                headersBuffer
                                    << name
                                    << ": "
                                    << *value
                                    << "; ";
                            }
                        }
                    }

                    auto headers = headersBuffer.str();

                    if( ! headers.empty() )
                    {
                        exception << eh::errinfo_http_response_headers( std::move( headers ) );
                    }
                }

                if( ! eh::get_error_info< eh::errinfo_http_request_details >( exception ) )
                {
                    cpp::SafeOutputStringStream buffer;

                    if( m_isSecureMode )
                    {
                        buffer
                            << "[REDACTED]";
                    }
                    else
                    {
                        buffer
                            << "HTTP action: "
                            << m_action
                            << "\nHTTP request path: "
                            << getPath()
                            << "\nHTTP request content:\n"
                            << ( m_contentIn.size() < MAX_DUMP_STRING_LENGTH ?
                                    m_contentIn :
                                    m_contentIn.substr( 0, MAX_DUMP_STRING_LENGTH ) + "..."
                                    )
                            << "\nHTTP request response:\n"
                            << ( m_contentOut.size() < MAX_DUMP_STRING_LENGTH ?
                                    m_contentOut :
                                    m_contentOut.substr( 0, MAX_DUMP_STRING_LENGTH ) + "..."
                                    );
                    }

                    exception << eh::errinfo_http_request_details( buffer.str() );
                }
            }

            virtual void enhanceException( SAA_in eh::exception& exception ) const OVERRIDE
            {
                base_type::enhanceException( exception );

                chk2EnhanceException( exception );
            }

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                   eptr,
                SAA_in                  const std::exception&                       exception,
                SAA_in_opt              const eh::error_code*                       ec
                ) NOEXCEPT OVERRIDE
            {
                if( ! m_expectedHttpStatuses.empty() )
                {
                    const auto* httpStatus = eh::get_error_info< eh::errinfo_http_status_code >( exception );

                    if( httpStatus && cpp::contains( m_expectedHttpStatuses, *httpStatus ) )
                    {
                        return true;
                    }
                }

                return base_type::isExpectedException( eptr, exception, ec );
            }

        public:

            const std::string& getRemoteEndpointId() const NOEXCEPT
            {
                return m_remoteEndpointId;
            }

            const std::string& getPath() const NOEXCEPT
            {
                return m_path;
            }

            const std::string& getContent() const NOEXCEPT
            {
                return m_contentIn;
            }

            const std::string& getResponse() const NOEXCEPT
            {
                return m_contentOut;
            }

            std::string& getResponseLvalue() NOEXCEPT
            {
                return m_contentOut;
            }

            const HeadersMap& getResponseHeaders() const NOEXCEPT
            {
                return m_responseHeaders;
            }

            os::string_ptr tryGetResponseHeader( SAA_in const std::string& name ) const
            {
                os::string_ptr value;

                const auto pos = m_responseHeaders.find( str::to_lower_copy( name ) );

                if( pos != m_responseHeaders.end() )
                {
                    value = os::string_ptr::attach( new std::string( pos -> second ) );
                }

                return value;
            }

            unsigned int getHttpStatus() const NOEXCEPT
            {
                return m_httpStatus;
            }

            const time::time_duration& getTimeout() const NOEXCEPT
            {
                return m_timeout;
            }

            void setTimeout( SAA_in const time::time_duration& timeout ) NOEXCEPT
            {
                m_timeout = timeout;
            }

            bool isTimedOut() const NOEXCEPT
            {
                return m_timedOut;
            }

            bool isSecureMode() const NOEXCEPT
            {
                return m_isSecureMode;
            }

            void isSecureMode( SAA_in const bool isSecureMode ) NOEXCEPT
            {
                m_isSecureMode = isSecureMode;
            }

            bool isExpectUtf8Content() const NOEXCEPT
            {
                return m_isExpectUtf8Content;
            }

            void isExpectUtf8Content( SAA_in const bool isExpectUtf8Content ) NOEXCEPT
            {
                m_isExpectUtf8Content = isExpectUtf8Content;
            }

            void addExpectedHttpStatuses( SAA_in const http::StatusesList& expectedHttpStatuses )
            {
                m_expectedHttpStatuses.insert( expectedHttpStatuses.begin(), expectedHttpStatuses.end() );
            }

            void throwHttpException( SAA_in_opt const std::string& message = str::empty() )
            {
                if( m_httpStatus == HttpStatusCode::HTTP_SUCCESS_OK )
                {
                    /*
                     * This function may be called when m_httpStatus == HttpStatusCode::HTTP_SUCCESS_OK
                     * because of the way the server may stream results. When processing complex queries
                     * the server may stream HttpStatusCode::HTTP_SUCCESS_OK if the operation generally
                     * succeeded but then report errors in the response document if the one or more of
                     * the sub-operations have failed
                     */

                    m_httpStatus = HttpStatusCode::HTTP_SERVER_ERROR_INTERNAL;
                }

                BL_THROW(
                    createHttpException(),
                    BL_MSG()
                        << "HTTP "
                        << m_action
                        << " request to '"
                        << createUrl()
                        << "' returned error status code: "
                        << m_httpStatus
                        << message
                    );
            }

        private:

            void initRequest()
            {
                std::ostream rstream( &m_request );

                rstream
                    << m_action
                    << " "
                    << m_path
                    << " HTTP/1.0\r\nHost: "
                    << base_type::m_query.host_name()
                    << "\r\nAccept: */*\r\nConnection: close";

                for( const auto& headerPair : m_requestHeaders )
                {
                    rstream
                        << "\r\n"
                        << headerPair.first
                        << HttpHeader::g_nameSeparator
                        << headerPair.second;
                }

                if( m_contentIn.size() )
                {
                    rstream
                        << "\r\nContent-Length: "
                        << m_contentIn.size();
                }

                rstream << "\r\n\r\n";

                if( m_contentIn.size() )
                {
                    rstream << m_contentIn;
                }
            }

            void doRequest( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                asio::async_read_until(
                    base_type::getStream(),
                    m_response,
                    "\r\n",
                    cpp::bind(
                        &this_type::doReadStatus,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );

                scheduleTimer();

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void doReadStatus( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                std::istream rstream( &m_response );

                std::string httpVersion;
                rstream >> httpVersion;

                chkHttpResponse(
                    rstream && httpVersion.substr( 0, 5 ) == "HTTP/",
                    httpVersion
                    );

                unsigned int statusCode;
                rstream >> statusCode;

                chkHttpResponse(
                    rstream && 0 != statusCode,
                    "HTTP status code is zero or invalid"
                    );

                std::string statusMsg;
                std::getline( rstream, statusMsg );

                chkHttpResponse(
                    rstream && false == statusMsg.empty(),
                    statusMsg
                    );

                m_httpStatus = statusCode;

                // Read the response headers, which are terminated by a blank line.
                asio::async_read_until(
                    base_type::getStream(),
                    m_response,
                    "\r\n\r\n",
                    cpp::bind(
                        &this_type::doReadHeaders,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );

                scheduleTimer();

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void doReadHeaders( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                std::istream rstream( &m_response );
                std::string header;

                cpp::SafeOutputStringStream cookiesBuffer;

                while( std::getline( rstream, header ) && header != "\r" )
                {
                    const auto pos = header.find( HttpHeader::g_nameSeparator );

                    if( pos == std::string::npos || pos == 0 )
                    {
                        continue;
                    }

                    auto name = header.substr( 0, pos );
                    auto value = header.substr( pos + 1 );

                    str::trim( name );
                    str::trim( value );

                    if( str::iequals( name, HttpHeader::g_setCookie) )
                    {
                        cookiesBuffer
                            << value
                            << HttpHeader::g_cookieSeparator;
                    }
                    else
                    {
                        str::to_lower( name );

                        m_responseHeaders.emplace( std::move( name ), std::move( value ) );
                    }
                }

                const auto pos = m_responseHeaders.find(
                    str::to_lower_copy( http::HttpHeader::g_contentLength )
                    );

                if( pos != m_responseHeaders.end() )
                {
                    m_responseLength = std::stoul( pos -> second );
                }

                auto cookies = cookiesBuffer.str();

                if( ! cookies.empty() )
                {
                    m_responseHeaders.emplace( str::to_lower_copy( HttpHeader::g_cookie ), std::move( cookies ) );
                }

                if( m_response.size() > 0 )
                {
                    // get current content
                    m_contentOutStream << &m_response;
                }

                // Start reading remaining data
                base_type::getStream().async_read_some(
                    asio::buffer( m_contentBuffer -> pv(), m_contentBuffer -> size() ),
                    cpp::bind(
                        &this_type::doReadContent,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );

                scheduleTimer();

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void doReadContent(
                SAA_in      const eh::error_code&                   ec,
                SAA_in      const std::size_t                       bytesTransferred
                ) NOEXCEPT

            {
                /*
                 * We can finish reading in two cases:
                 * 1. EOF which means that the whole response has been read
                 * 2. Expected protocol error/exception (short read for SSL), which can
                 * be a genuine error or indicate that a server is not shutting down the
                 * protocol before closing the socket. In this case, we can compare
                 * the expected size of the response to the number of bytes received,
                 * to distinguish between failure and bad behavior. For it to work,
                 * the 'Content-Length' header has to be present in HTTP response,
                 * otherwise this will always fail on protocol exception.
                 */

                if( asio::error::eof == ec ||
                    ( base_type::isExpectedProtocolException( nullptr, std::exception(), &ec ) &&
                      m_responseLength == m_contentOutStream.str().size() )
                  )
                {
                    BL_TASKS_HANDLER_BEGIN()

                    m_contentOut = decodeContent();

                    m_contentOutStream.str( std::string() );

                    if( HTTP_SUCCESS_OK != m_httpStatus )
                    {
                        throwHttpException();
                    }

                    BL_TASKS_HANDLER_END()

                    return;
                }

                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                // store any content from response
                m_contentOutStream
                    << std::string( reinterpret_cast< char* >( m_contentBuffer -> pv() ), bytesTransferred );

                // Continue reading remaining data until EOF.
                base_type::getStream().async_read_some(
                    asio::buffer( m_contentBuffer -> pv(), m_contentBuffer -> size() ),
                    cpp::bind(
                        &this_type::doReadContent,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::bytes_transferred
                        )
                    );

                scheduleTimer();

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            template
            <
                typename EXCEPTION
            >
            EXCEPTION createException( SAA_in const bool isExpected ) const
            {
                auto exception = EXCEPTION() << eh::errinfo_http_url( createUrl() );

                if( isExpected )
                {
                    exception << eh::errinfo_is_expected( true );
                }

                chk2EnhanceException( exception );

                return exception;
            }

            HttpException createHttpException() const
            {
                auto exception = createException< HttpException >( true /* isExpected */ )
                    << eh::errinfo_http_status_code( m_httpStatus );

                if(
                    m_httpStatus >= HTTP_REDIRECT_START_RANGE &&
                    m_httpStatus <= HTTP_REDIRECT_END_RANGE
                    )
                {
                    /*
                     * Try to extract the re-direct URL and add it to the exception
                     */

                    str::smatch results;

                    if( str::regex_search( m_contentOut, results, g_hrefRegex ) )
                    {
                        const auto& expression = results[ 1 ];

                        if( expression.matched )
                        {
                            exception
                                << eh::errinfo_http_redirect_url( expression );
                        }
                    }
                }

                return exception;
            }

            /**
             * @brief Convert received content from the response charset into ISO-8859-1
             *
             * If the content is UTF-8 and m_isExpectUtf8Content is 'true' then it won't
             * be converted to ISO-8859-1 and be left as is
             */

            std::string decodeContent()
            {
                auto content = m_contentOutStream.str();

                const auto charset = tryGetContentCharset();

                if( ! charset.empty() )
                {
                    if(
                        HttpHeader::g_iso8859_1   == charset ||
                        HttpHeader::g_latin1      == charset ||
                        HttpHeader::g_windows1252 == charset
                        )
                    {
                    }
                    else if( HttpHeader::g_utf8 == charset )
                    {
                        /*
                         * If the charset is UTF-8 and the m_isExpectUtf8Content is true
                         * then the content is expected to be UTF-8 and to contain non-ASCII
                         * characters, so in this case we don't attempt to convert it to
                         * ISO-8859-1 since that would fail
                         */

                        if( ! m_isExpectUtf8Content )
                        {
                            return str::from_utf( content, HttpHeader::g_iso8859_1, str::method_type::stop );
                        }
                    }
                    else
                    {
                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "Unsupported "
                                << HttpHeader::g_contentType
                                << " charset: "
                                << charset
                            );
                    }
                }

                return content;
            }

            /**
             * @brief Extract charset=value from the Content-Type header field
             * @returns Charset name in uppercase or an empty string
             */

            std::string tryGetContentCharset()
            {
                const auto contentType = tryGetResponseHeader( HttpHeader::g_contentType );

                if( contentType && ! contentType -> empty() )
                {
                    str::smatch results;

                    if( str::regex_search( *contentType, results, g_charsetRegex ) )
                    {
                        const auto& expression = results[ 1 ];

                        if( expression.matched )
                        {
                            auto charset = expression.str();

                            str::to_upper( charset );

                            return charset;
                        }
                    }
                }

                return std::string();
            }
        };

        BL_DEFINE_STATIC_CONST_STRING( SimpleHttpTaskT, g_protocolDefault ) = "http";

        BL_DEFINE_STATIC_MEMBER( SimpleHttpTaskT, const str::regex, g_hrefRegex )             ( "\\bhref\\s*=\\s*[\"']([^\"']+)[\"']", str::regex::icase );
        BL_DEFINE_STATIC_MEMBER( SimpleHttpTaskT, const str::regex, g_charsetRegex )          ( "\\bcharset\\s*=\\s*([^;]+)\\b", str::regex::icase );

        typedef SimpleHttpTaskT<> SimpleHttpTask;

        typedef om::ObjectImpl< SimpleHttpTaskT<> > SimpleHttpTaskImpl;

    } // tasks

} // bl

#define BL_TASKS_DECLARE_HTTP_TASK_IMPL_BEGIN( verbId ) \
    namespace bl \
    { \
        namespace tasks \
        { \
            \
            template \
            < \
                typename E = void \
            > \
            class SimpleHttp ## verbId ## TaskT : public SimpleHttpTask \
            { \
                BL_DECLARE_OBJECT_IMPL( SimpleHttp ## verbId ##  TaskT ) \
            \
            protected: \
            \
                SimpleHttp ## verbId ##  TaskT( \
                    SAA_in          std::string&&               host, \
                    SAA_in          const unsigned short        port, \
                    SAA_in          const std::string&          path, \

#define BL_TASKS_DECLARE_HTTP_TASK_IMPL_END( verbId, verbStr, contentValue ) \
                    SAA_in_opt      HeadersMap&&                requestHeaders = HeadersMap() \
                    ) \
                    : \
                    SimpleHttpTask( \
                        BL_PARAM_FWD( host ), \
                        port, \
                        path, \
                        verbStr, \
                        contentValue, \
                        BL_PARAM_FWD( requestHeaders ) ) \
                { \
                } \
            }; \
            \
            typedef om::ObjectImpl< SimpleHttp ## verbId ## TaskT<> > SimpleHttp ## verbId ## TaskImpl; \
        } \
    } \


#define BL_TASKS_DECLARE_HTTP_TASK_NO_CONTENT( verbId, verbStr ) \
    BL_TASKS_DECLARE_HTTP_TASK_IMPL_BEGIN( verbId ) \
    BL_TASKS_DECLARE_HTTP_TASK_IMPL_END( verbId, verbStr, "" ) \

#define BL_TASKS_DECLARE_HTTP_TASK_WITH_CONTENT( verbId, verbStr ) \
    BL_TASKS_DECLARE_HTTP_TASK_IMPL_BEGIN( verbId ) \
    SAA_in const std::string& content, \
    BL_TASKS_DECLARE_HTTP_TASK_IMPL_END( verbId, verbStr, content ) \

BL_TASKS_DECLARE_HTTP_TASK_NO_CONTENT( Get, "GET" )
BL_TASKS_DECLARE_HTTP_TASK_NO_CONTENT( Delete, "DELETE" )
BL_TASKS_DECLARE_HTTP_TASK_WITH_CONTENT( GetWithContent, "GET" )
BL_TASKS_DECLARE_HTTP_TASK_WITH_CONTENT( Put, "PUT" )
BL_TASKS_DECLARE_HTTP_TASK_WITH_CONTENT( Post, "POST" )

#endif /* __BL_TASKS_SIMPLEHTTPTASK_H_ */
