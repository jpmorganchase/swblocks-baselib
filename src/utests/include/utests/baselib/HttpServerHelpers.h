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

#ifndef __UTEST_HTTPSERVERHELPERS_H_
#define __UTEST_HTTPSERVERHELPERS_H_

#include <baselib/data/eh/ServerErrorHelpers.h>

#include <baselib/httpserver/ServerBackendProcessingImplDefault.h>
#include <baselib/httpserver/ServerBackendProcessing.h>

#include <baselib/httpserver/HttpServer.h>
#include <baselib/httpserver/Parser.h>
#include <baselib/httpserver/Request.h>
#include <baselib/httpserver/Response.h>

#include <baselib/http/SimpleHttpTask.h>
#include <baselib/http/SimpleHttpSslTask.h>
#include <baselib/http/SimpleSecureHttpSslTask.h>
#include <baselib/http/Globals.h>

#include <baselib/tasks/TasksUtils.h>
#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/utils/ScanDirectoryTask.h>
#include <baselib/tasks/SimpleTaskControlToken.h>

#include <baselib/core/NumberUtils.h>
#include <baselib/core/BoxedObjects.h>
#include <baselib/core/OS.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/PathUtils.h>
#include <baselib/core/Utils.h>
#include <baselib/core/Random.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/MachineGlobalTestLock.h>
#include <utests/baselib/TestTaskUtils.h>
#include <utests/baselib/TestFsUtils.h>
#include <utests/baselib/UtfCrypto.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

#include <cstdlib>
#include <cstdint>
#include <unordered_map>

namespace utest
{
    namespace http
    {
        using bl::httpserver::Request;
        using bl::httpserver::Response;
        using bl::httpserver::ServerBackendProcessing;
        using bl::httpserver::ServerBackendProcessingImplDefault;

        using bl::tasks::TaskControlTokenRW;

        typedef bl::http::Parameters::HttpStatusCode                                    HttpStatusCode;
        typedef bl::http::Parameters::HttpHeader                                        HttpHeader;

        const std::string g_requestUri = "/request-uri";
        const std::string g_requestPerfUri = "/request-perf-uri";
        const std::string g_redirectedRequestUri = "/redirected-request-uri";
        const std::string g_notFoundUri = "/invalid-request-uri";
        const std::string g_desiredResult = "\r\nHTTP server successfully processed a normal request!\r\n";
        const std::string g_desiredPerfResult = "\r\nHTTP server successfully processed a perf request!\r\n";
        const std::string g_redirectedResult = "\r\nHTTP server successfully processed a redirected request!\r\n";
        const std::string g_notFoundResult = "\r\nHTTP page not found!\r\n";

        template
        <
            typename BACKENDSTATE
        >
        class TestHttpServerProcessingTask :
            public bl::httpserver::HttpServerProcessingTaskDefault< BACKENDSTATE >
        {
            BL_DECLARE_OBJECT_IMPL( TestHttpServerProcessingTask )

        protected:

            typedef bl::httpserver::HttpServerProcessingTaskDefault< BACKENDSTATE >     base_type;
            typedef bl::http::Parameters::HttpStatusCode                                HttpStatusCode;

            using base_type::m_statusCode;
            using base_type::m_request;
            using base_type::m_response;
            using base_type::m_responseHeaders;

            TestHttpServerProcessingTask(
                SAA_in          bl::om::ObjPtr< Request >&&                             request,
                SAA_in_opt      bl::om::ObjPtr< BACKENDSTATE >&&                        backendState = nullptr
                )
                :
                base_type( BL_PARAM_FWD( request ), BL_PARAM_FWD( backendState ) )
            {
            }

            virtual void requestProcessing() OVERRIDE
            {
                if( m_request -> uri() == g_requestUri || m_request -> uri() == g_requestPerfUri )
                {
                    m_response = m_request -> uri() == g_requestUri ? g_desiredResult : g_desiredPerfResult;
                    m_statusCode = HttpStatusCode::HTTP_SUCCESS_OK;
                }
                else if( m_request -> uri() == g_redirectedRequestUri )
                {
                    m_response = g_redirectedResult;
                    m_statusCode = HttpStatusCode::HTTP_REDIRECT_PERMANENTLY;
                }
                else
                {
                    m_response = g_notFoundResult;
                    m_statusCode = HttpStatusCode::HTTP_CLIENT_ERROR_NOT_FOUND;
                }

                m_responseHeaders.clear();

                const auto pos = m_request -> headers().find( http::HttpHeader::g_userAgent );

                if( pos != m_request -> headers().end() )
                {
                    m_responseHeaders[ "request-user-agent-id" ] = pos -> second;
                }

                if( m_request -> uri() != g_requestPerfUri )
                {
                    bl::os::sleep( bl::time::milliseconds( 20 /* millisecondsWait */ ) );
                }
            };
        };

        class DummyBackendState : public bl::om::ObjectDefaultBase
        {
        };

        typedef bl::om::ObjectImpl< DummyBackendState > DummyBackendStateImpl;

        typedef bl::om::ObjectImpl
        <
            ServerBackendProcessingImplDefault< DummyBackendStateImpl, TestHttpServerProcessingTask >
        >
        ServerBackendProcessingImplTest;

        template
        <
            typename E = void
        >
        class HttpServerHelpersT
        {
            BL_DECLARE_STATIC( HttpServerHelpersT )

        public:

            typedef std::tuple
            <
                bool                                                /* exceptionExpected */,
                bl::http::Parameters::HttpStatusCode                /* statusCodeExpected */,
                std::string                                         /* contentExpected */
            >
            completion_result_t;

            typedef std::unordered_map
            <
                bl::om::ObjPtr< bl::tasks::Task >,
                completion_result_t
            >
            completion_results_map_t;

            template
            <
                typename TASKIMPL = bl::tasks::SimpleHttpPutTaskImpl
            >
            static void sendHttpRequestAndVerifyTheResult(
                SAA_in          const bl::om::ObjPtr< bl::tasks::ExecutionQueue >&      eq,
                SAA_in          const std::string&                                      uri,
                SAA_in          const std::string&                                      content,
                SAA_in          const bool                                              exceptionExpected,
                SAA_in          const bl::http::Parameters::HttpStatusCode              statusCodeExpected,
                SAA_in          const std::string                                       contentExpected = std::string(),
                SAA_in_opt      completion_results_map_t*                               completionResults = nullptr
                )
            {
                using namespace bl;

                MessageBuffer request;

                request
                    << content;

                const auto taskImpl = TASKIMPL::template createInstance(
                    cpp::copy( test::UtfArgsParser::host() ),
                    test::UtfArgsParser::port(),
                    uri, /* URI */
                    resolveMessage( request ) /* content */
                    );

                const auto task = om::qi< bl::tasks::Task >( taskImpl );
                eq -> push_back( task );

                if( completionResults )
                {
                    completionResults -> emplace(
                        bl::om::copy( task ),
                        std::make_tuple( exceptionExpected, statusCodeExpected, contentExpected )
                        );

                    return;
                }

                if( exceptionExpected )
                {
                    eq -> wait( task, false /* cancel */ );

                    UTF_REQUIRE( task -> isFailed() );
                }
                else
                {
                    eq -> waitForSuccess( task, false /* cancel */ );
                }

                const auto status = taskImpl -> getHttpStatus();

                const auto& response = taskImpl -> getResponse();

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "HTTP status is "
                        << status
                        << "; body:\n\n"
                        << response
                    );

                UTF_REQUIRE( status == statusCodeExpected )

                if( ! contentExpected.empty() )
                {
                    UTF_REQUIRE( response.find( contentExpected ) != std::string::npos );
                }
            }

            template
            <
                typename TASKIMPL = bl::tasks::SimpleHttpPutTaskImpl
            >
            static void sendAndVerifyAssortedHttpRequests(
                SAA_in          const bl::om::ObjPtr< bl::tasks::ExecutionQueue >&      eq,
                SAA_in_opt      completion_results_map_t*                               completionResults = nullptr
                )
            {
                using namespace utest::http;

                /*
                 * Test with a valid request
                 */

                sendHttpRequestAndVerifyTheResult< TASKIMPL >(
                    eq,
                    g_requestUri, /* URI */
                    "0123456789", /* content */
                    false, /* exceptionExpected */
                    bl::http::Parameters::HTTP_SUCCESS_OK, /* statusCodeExpected */
                    g_desiredResult /* contentExpected */,
                    completionResults
                    );

                /*
                 * Test with a redirected request
                 */

                sendHttpRequestAndVerifyTheResult< TASKIMPL >(
                    eq,
                    g_redirectedRequestUri, /* URI */
                    "0123456789", /* content */
                    true, /* exceptionExpected */
                    bl::http::Parameters::HTTP_REDIRECT_PERMANENTLY, /* statusCodeExpected */
                    g_redirectedResult /* contentExpected */,
                    completionResults
                    );

                /*
                 * Test with a invalid URI path request
                 */

                sendHttpRequestAndVerifyTheResult< TASKIMPL >(
                    eq,
                    g_notFoundUri, /* URI */
                    "0123456789", /* content */
                    true, /* exceptionExpected */
                    bl::http::Parameters::HTTP_CLIENT_ERROR_NOT_FOUND, /* statusCodeExpected */
                    g_notFoundResult /* contentExpected */,
                    completionResults
                    );

                /*
                 * Test with a bad request (no URI)
                 */

                sendHttpRequestAndVerifyTheResult< TASKIMPL >(
                    eq,
                    std::string(), /* URI */
                    "0123456789", /* content */
                    true, /* exceptionExpected */
                    bl::http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST, /* statusCodeExpected */
                    std::string() /* contentExpected */,
                    completionResults
                    );
            }

            template
            <
                typename SERVERIMPL = bl::httpserver::HttpServer
            >
            static void startHttpServerAndExecuteCallback(
                SAA_in          const bl::cpp::void_callback_t&                                         callback,
                SAA_in_opt      bl::om::ObjPtr< bl::httpserver::ServerBackendProcessing >&&             backend = nullptr,
                SAA_in_opt      const bl::om::ObjPtr< TaskControlTokenRW >&                             controlToken = nullptr
                )
            {
                using namespace bl;
                using namespace bl::tasks;

                test::MachineGlobalTestLock lock;

                {
                    if( ! backend )
                    {
                        backend = ServerBackendProcessingImplTest::createInstance< httpserver::ServerBackendProcessing >();
                    }

                    const auto acceptor = SERVERIMPL::template createInstance(
                        BL_PARAM_FWD( backend ),
                        controlToken,
                        "0.0.0.0"                                           /* host */,
                        test::UtfArgsParser::port(),
                        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
                        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */
                        );

                    utest::TestTaskUtils::startAcceptorAndExecuteCallback( callback, acceptor );
                }
            }
        };

        typedef HttpServerHelpersT<> HttpServerHelpers;

    } // http

} // utest

#endif /* __UTEST_HTTPSERVERHELPERS_H_ */
