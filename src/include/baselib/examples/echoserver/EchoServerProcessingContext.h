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

#ifndef __BL_EXAMPLES_ECHOSERVER_ECHOSERVERPROCESSINGCONTEXT_H_
#define __BL_EXAMPLES_ECHOSERVER_ECHOSERVERPROCESSINGCONTEXT_H_

#include <baselib/rest/BaseRestServerProcessingContext.h>

#include <baselib/messaging/ForwardingBackendProcessingImpl.h>

#include <baselib/data/models/Http.h>

#include <baselib/http/Globals.h>

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/ExecutionQueue.h>

#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace echo
    {
        /**
         * class EchoServerProcessingContext - an echo server message processing context implementation
         */

        template
        <
            typename E = void
        >
        class EchoServerProcessingContextT :
            public rest::BaseRestServerProcessingContext< EchoServerProcessingContextT< E > >
        {
            BL_DECLARE_OBJECT_IMPL( EchoServerProcessingContextT )

        protected:

            typedef EchoServerProcessingContextT< E >                       this_type;
            typedef om::ObjPtrCopyable< dm::messaging::BrokerProtocol >     broker_protocol_ptr_t;
            typedef dm::messaging::BrokerProtocol                           BrokerProtocol;

            typedef rest::BaseRestServerProcessingContext
            <
                EchoServerProcessingContextT< E >
            >
            base_type;

            using base_type::getResponseBrokerProtocolString;

            const bool                                                      m_isQuietMode;
            const unsigned long                                             m_maxProcessingDelayInMicroseconds;

            EchoServerProcessingContextT(
                SAA_in      const bool                                      isQuietMode,
                SAA_in      const unsigned long                             maxProcessingDelayInMicroseconds,
                SAA_in      const bool                                      isGraphQLServer,
                SAA_in      const bool                                      isAuthnticationAlwaysRequired,
                SAA_in      std::string&&                                   requiredContentType,
                SAA_in      om::ObjPtr< data::datablocks_pool_type >&&      dataBlocksPool,
                SAA_in      om::ObjPtr< om::Proxy >&&                       backendReference,
                SAA_in      std::string&&                                   tokenType,
                SAA_in_opt  std::string&&                                   tokenData = std::string()
                )
                :
                base_type(
                    isGraphQLServer,
                    isAuthnticationAlwaysRequired,
                    BL_PARAM_FWD( requiredContentType ),
                    BL_PARAM_FWD( dataBlocksPool ),
                    BL_PARAM_FWD( backendReference ),
                    BL_PARAM_FWD( tokenType ),
                    BL_PARAM_FWD( tokenData )
                    ),
                m_isQuietMode( isQuietMode ),
                m_maxProcessingDelayInMicroseconds( maxProcessingDelayInMicroseconds )
            {
            }

        public:

            void checkToRefreshToken()
            {
            }

            auto processingSync(
                SAA_in      const om::ObjPtr< BrokerProtocol >&             brokerProtocolIn,
                SAA_in      const om::ObjPtrCopyable< data::DataBlock >&    dataBlock
                )
                -> om::ObjPtr< dm::http::HttpResponseMetadata >
            {
                using namespace bl::tasks;
                using namespace bl::messaging;

                const std::string message = "Echo server processing";

                const auto targetPeerId = uuids::string2uuid( brokerProtocolIn -> targetPeerId() );

                if( m_maxProcessingDelayInMicroseconds )
                {
                    const auto randomDelay = random::getUniformRandomUnsignedValue< unsigned long >(
                        m_maxProcessingDelayInMicroseconds
                        );

                    os::sleep( time::microseconds( numbers::safeCoerceTo< long >( randomDelay ) ) );
                }

                /*
                 * Prepare the HTTP response metadata to pass it as pass through user data
                 * in the broker protocol message part
                 */

                auto responseMetadata = dm::http::HttpResponseMetadata::createInstance();

                responseMetadata -> httpStatusCode( http::Parameters::HTTP_SUCCESS_OK );
                responseMetadata -> contentType( http::HttpHeader::g_contentTypeJsonUtf8 );

                responseMetadata -> headersLvalue()[ http::HttpHeader::g_setCookie ] =
                    "responseCookieName=responseCookieValue;";

                /*
                 * Now examine the input request metadata to figure out what to do and what
                 * to return, etc
                 */

                const auto& passThroughUserData = brokerProtocolIn -> passThroughUserData();

                std::string method;
                std::string urlPath;

                if( passThroughUserData )
                {
                    const auto payload =
                        dm::DataModelUtils::castTo< dm::http::HttpRequestMetadataPayload >( passThroughUserData );

                    if( payload -> httpRequestMetadata() )
                    {
                        const auto& requestMetadata = payload -> httpRequestMetadata();

                        /*
                         * Save the method and urlPath to make decisions based on them later on
                         */

                        method = requestMetadata -> method();
                        urlPath = requestMetadata -> urlPath();

                        if( ! m_isQuietMode )
                        {
                            auto messageAsText = resolveMessage(
                                BL_MSG()
                                    << "\nContent size:"
                                    << dataBlock -> offset1()
                                );

                            const auto pos = requestMetadata -> headers().find( http::HttpHeader::g_contentType );

                            if( pos != std::end( requestMetadata -> headers() ) )
                            {
                                const auto& contentType = pos -> second;

                                messageAsText.append(
                                    resolveMessage(
                                        BL_MSG()
                                            << "\nContent type:"
                                            << contentType
                                        )
                                    );

                                if( dataBlock -> offset1() )
                                {
                                    if(
                                        contentType == http::HttpHeader::g_contentTypeJsonUtf8 ||
                                        contentType == http::HttpHeader::g_contentTypeJsonIso8859_1
                                        )
                                    {
                                        /*
                                         * The content type is JSON, format as pretty JSON
                                         */

                                        const auto pair = MessagingUtils::deserializeBlockToObjects( dataBlock );

                                        messageAsText.append( "\n\n" );

                                        messageAsText.append(
                                            dm::DataModelUtils::getDocAsPrettyJsonString( pair.second /* payload */ )
                                            );
                                    }

                                    if(
                                        contentType == http::HttpHeader::g_contentTypeXml ||
                                        contentType == http::HttpHeader::g_contentTypePlainText ||
                                        contentType == http::HttpHeader::g_contentTypePlainTextUtf8
                                        )
                                    {
                                        /*
                                         * The content type is printed as text
                                         */

                                        messageAsText.append( "\n\n" );

                                        messageAsText.append(
                                            dataBlock -> begin(),
                                            dataBlock -> begin() + dataBlock -> offset1()
                                            );
                                    }
                                }
                                else
                                {
                                    /*
                                     * A content type header was provided, but no actual content
                                     * which is an invalid case
                                     *
                                     * Make sure we return an error, so the appropriate test fail
                                     */

                                    responseMetadata -> httpStatusCode( http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST );
                                }
                            }

                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "\n**********************************************\n\n"
                                    << message
                                    << "\n\nTarget peer id: "
                                    << uuids::uuid2string( targetPeerId )
                                    << "\n\nBroker protocol message:\n"
                                    << dm::DataModelUtils::getDocAsPrettyJsonString( brokerProtocolIn )
                                    << "\n\nPayload message:\n"
                                    << messageAsText
                                    << "\n\n"
                                );
                        }
                    }
                }

                /*
                 * By default the response body will be the same as the request body (i.e. echo)
                 * unless there is an error / problem and / or if the urlPath has a special URL
                 * which requests a specific data in the response or as a header (e.g.
                 * "requestMetadata" || "responseMetadata", /cookie/..., /error/... etc)
                 *
                 * If the responseBody variable is left empty then it is the echo case and if
                 * not then we need to re-wrote the payload part of the message as the response
                 * body is expected to be something else
                 */

                std::string responseBody;
                bool responseMetadataRequested = false;

                if( http::Parameters::HTTP_SUCCESS_OK != responseMetadata -> httpStatusCode() )
                {
                    responseMetadata -> contentType( http::HttpHeader::g_contentTypeJsonUtf8 );

                    responseBody = dm::ServerErrorHelpers::getServerErrorAsJson(
                        std::make_exception_ptr(
                            BL_EXCEPTION(
                                SystemException::create(
                                    eh::errc::make_error_code( eh::errc::invalid_argument ),
                                    BL_SYSTEM_ERROR_DEFAULT_MSG
                                    ),
                                BL_SYSTEM_ERROR_DEFAULT_MSG
                                )
                            )
                        );
                }
                else if( method == "GET" )
                {
                    /*
                     * Check if a specific response cookie was requested
                     */

                    if( str::starts_with( urlPath, "/cookie/" ) )
                    {
                        responseMetadata -> headersLvalue()[ http::HttpHeader::g_setCookie ] =
                            resolveMessage(
                                BL_MSG()
                                    << "responseCookieName="
                                    << urlPath
                                    << ";"
                                );

                        responseMetadataRequested = true;
                    }

                    /*
                     * Check if some specific status code was requested
                     */

                    if( urlPath == "/error/HTTP_CLIENT_ERROR_UNAUTHORIZED" )
                    {
                        responseMetadata -> httpStatusCode( http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED );
                    }
                    else if( urlPath == "/error/HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE" )
                    {
                        responseMetadata -> httpStatusCode( http::Parameters::HTTP_SERVER_ERROR_SERVICE_UNAVAILABLE );
                    }
                    else if( urlPath == "/error/HTTP_SERVER_ERROR_INTERNAL" )
                    {
                        responseMetadata -> httpStatusCode( http::Parameters::HTTP_SERVER_ERROR_INTERNAL );
                    }
                    else if( urlPath == "/error/HTTP_CLIENT_ERROR_NOT_FOUND" )
                    {
                        responseMetadata -> httpStatusCode( http::Parameters::HTTP_CLIENT_ERROR_NOT_FOUND );
                    }
                    else if( urlPath == "/error/HTTP_SERVER_ERROR_NOT_IMPLEMENTED" )
                    {
                        responseMetadata -> httpStatusCode( http::Parameters::HTTP_SERVER_ERROR_NOT_IMPLEMENTED );
                    }
                    else if( urlPath == "/error/HTTP_CLIENT_ERROR_FORBIDDEN" )
                    {
                        responseMetadata -> httpStatusCode( http::Parameters::HTTP_CLIENT_ERROR_FORBIDDEN );
                    }

                    if( http::Parameters::HTTP_SUCCESS_OK != responseMetadata -> httpStatusCode() )
                    {
                        responseMetadata -> contentType( http::HttpHeader::g_contentTypeJsonUtf8 );

                        responseBody = dm::ServerErrorHelpers::getServerErrorAsJson(
                            std::make_exception_ptr(
                                BL_EXCEPTION(
                                    SystemException::create(
                                        eh::errc::make_error_code( eh::errc::bad_file_descriptor ),
                                        BL_SYSTEM_ERROR_DEFAULT_MSG
                                        ),
                                    BL_SYSTEM_ERROR_DEFAULT_MSG
                                    )
                                )
                            );
                    }
                    else if( urlPath == "/requestMetadata" )
                    {
                        responseBody = dm::DataModelUtils::getDocAsPrettyJsonString( brokerProtocolIn );
                    }
                    else if( urlPath == "/responseMetadata" )
                    {
                        responseMetadataRequested = true;
                    }
                }

                {
                    if( responseMetadataRequested )
                    {
                        responseBody =
                            getResponseBrokerProtocolString( brokerProtocolIn, om::copy( responseMetadata ) );
                    }

                    if( responseBody.empty() )
                    {
                        /*
                         * We simply return the original data -- i.e. "echo" the request as a response
                         *
                         * The current size also includes the broker protocol data of the request, so
                         * we have to simply truncate it before we return
                         */

                        dataBlock -> setSize( dataBlock -> offset1() );
                    }
                    else
                    {
                        /*
                         * If new response body was provided we need to re-write the payload
                         * part of the message
                         */

                        dataBlock -> reset();
                        dataBlock -> write( responseBody.c_str(), responseBody.size() );
                    }
                }

                return responseMetadata;
            }

        };

        typedef om::ObjectImpl< EchoServerProcessingContextT<> > EchoServerProcessingContext;

    } // echo

} // bl

#endif /* __BL_EXAMPLES_ECHOSERVER_ECHOSERVERPROCESSINGCONTEXT_H_ */
