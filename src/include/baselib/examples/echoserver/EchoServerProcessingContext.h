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
            public messaging::AsyncBlockDispatcher,
            public om::Disposable
        {
            BL_CTR_DEFAULT( EchoServerProcessingContextT, protected )

            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( EchoServerProcessingContextT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( messaging::AsyncBlockDispatcher )
                BL_QITBL_ENTRY( om::Disposable )
            BL_QITBL_END( messaging::AsyncBlockDispatcher )

        protected:

            typedef EchoServerProcessingContextT< E >                       this_type;
            typedef om::ObjPtrCopyable< dm::messaging::BrokerProtocol >     broker_protocol_ptr_t;

            const om::ObjPtr< tasks::ExecutionQueue >                       m_eqProcessingQueue;
            const bool                                                      m_isQuietMode;
            const unsigned long                                             m_maxProcessingDelayInMicroseconds;
            const std::string                                               m_tokenType;
            const std::string                                               m_tokenData;
            const om::ObjPtr< data::datablocks_pool_type >                  m_dataBlocksPool;
            const om::ObjPtr< om::Proxy >                                   m_backendReference;

            os::mutex                                                       m_lock;
            cpp::ScalarTypeIniter< bool >                                   m_isDisposed;
            std::atomic< unsigned long >                                    m_messagesProcessed;

            EchoServerProcessingContextT(
                SAA_in      const bool                                      isQuietMode,
                SAA_in      const unsigned long                             maxProcessingDelayInMicroseconds,
                SAA_in      std::string&&                                   tokenType,
                SAA_in      std::string&&                                   tokenData,
                SAA_in      om::ObjPtr< data::datablocks_pool_type >&&      dataBlocksPool,
                SAA_in      om::ObjPtr< om::Proxy >&&                       backendReference
                ) NOEXCEPT
                :
                m_eqProcessingQueue(
                    tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                        tasks::ExecutionQueue::OptionKeepNone
                        )
                    ),
                m_isQuietMode( isQuietMode ),
                m_maxProcessingDelayInMicroseconds( maxProcessingDelayInMicroseconds ),
                m_tokenType( BL_PARAM_FWD( tokenType ) ),
                m_tokenData( BL_PARAM_FWD( tokenData ) ),
                m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) ),
                m_backendReference( BL_PARAM_FWD( backendReference ) ),
                m_messagesProcessed( 0UL )
            {
            }

            ~EchoServerProcessingContextT() NOEXCEPT
            {
                if( ! m_isDisposed )
                {
                    BL_LOG(
                        Logging::warning(),
                        BL_MSG()
                            << "~EchoServerProcessingContextT() is being called without"
                            << " the object being disposed"
                        );
                }

                disposeInternal();
            }

            void disposeInternal() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( m_isDisposed )
                {
                    return;
                }

                m_eqProcessingQueue -> forceFlushNoThrow();
                m_eqProcessingQueue -> dispose();

                m_isDisposed = true;

                BL_NOEXCEPT_END()
            }

            void chkIfDisposed()
            {
                BL_CHK_T(
                    true,
                    m_isDisposed.value(),
                    UnexpectedException(),
                    BL_MSG()
                        << "The server processing context was disposed already"
                    );
            }

            void processingImpl(
                SAA_in      const std::string&                              message,
                SAA_in      const uuid_t&                                   targetPeerId,
                SAA_in      const broker_protocol_ptr_t&                    brokerProtocolIn,
                SAA_in      const om::ObjPtrCopyable< data::DataBlock >&    dataBlock
                )
            {
                using namespace bl;
                using namespace bl::tasks;
                using namespace bl::messaging;

                BL_MUTEX_GUARD( m_lock );

                chkIfDisposed();

                ++m_messagesProcessed;

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
                    /*
                     * At this point we are ready to (re-)write the broker protocol part of the message
                     */

                    const auto conversationId = uuids::string2uuid( brokerProtocolIn -> conversationId() );

                    const auto brokerProtocol = MessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        m_tokenType,
                        m_tokenData
                        );

                    const auto responseMetadataPayload = dm::http::HttpResponseMetadataPayload::createInstance();

                    responseMetadataPayload -> httpResponseMetadata( std::move( responseMetadata ) );

                    brokerProtocol -> passThroughUserData(
                        dm::DataModelUtils::castTo< dm::Payload >( responseMetadataPayload )
                        );

                    const auto protocolDataString =
                        dm::DataModelUtils::getDocAsPrettyJsonString( brokerProtocol );

                    if( responseMetadataRequested )
                    {
                        responseBody = protocolDataString;
                    }

                    if( ! responseBody.empty() )
                    {
                        /*
                         * If new response body was provided we need to re-write the payload
                         * part of the message
                         */

                        dataBlock -> setOffset1( 0U );
                        dataBlock -> setSize( 0U );
                        dataBlock -> write( responseBody.c_str(), responseBody.size() );
                        dataBlock -> setOffset1( dataBlock -> size() );
                    }

                    /*
                     * Re-write / update the broker protocol part of the message in the data block
                     */

                    dataBlock -> setSize( dataBlock -> offset1() );

                    dataBlock -> write( protocolDataString.c_str(), protocolDataString.size() );
                }
            }

            auto createProcessingTaskInternal(
                SAA_in      const std::string&                              message,
                SAA_in      const uuid_t&                                   targetPeerId,
                SAA_in      const broker_protocol_ptr_t&                    brokerProtocolIn,
                SAA_in      const om::ObjPtrCopyable< data::DataBlock >&    dataBlock
                )
                -> om::ObjPtr< tasks::SimpleTaskImpl >
            {
                return tasks::SimpleTaskImpl::createInstance(
                    cpp::bind(
                        &this_type::processingImpl,
                        om::ObjPtrCopyable< this_type, om::Disposable >::acquireRef( this ),
                        message,
                        targetPeerId,
                        brokerProtocolIn,
                        dataBlock
                        )
                    );
            }

            auto createServerProcessingAndResponseTask(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  const om::ObjPtrCopyable< data::DataBlock >&        data
                )
                -> om::ObjPtr< tasks::Task >
            {
                using namespace bl::messaging;
                using namespace bl::tasks;

                BL_MUTEX_GUARD( m_lock );

                chkIfDisposed();

                os::mutex_unique_lock guard;

                const auto backend =
                    m_backendReference -> tryAcquireRef< BackendProcessing >( BackendProcessing::iid(), &guard );

                BL_CHK(
                    nullptr,
                    backend,
                    BL_MSG()
                        << "Backend was not connected"
                    );

                const auto pair = MessagingUtils::deserializeBlockToObjects( data, true /* brokerProtocolOnly */ );

                const auto& brokerProtocolIn = pair.first;

                auto messageResponseTask = om::ObjPtrCopyable< Task >(
                    backend -> createBackendProcessingTask(
                        BackendProcessing::OperationId::Put,
                        BackendProcessing::CommandId::None,
                        uuids::nil()                                                    /* sessionId */,
                        BlockTransferDefs::chunkIdDefault(),
                        uuids::string2uuid( brokerProtocolIn -> targetPeerId() )        /* sourcePeerId */,
                        uuids::string2uuid( brokerProtocolIn -> sourcePeerId() )        /* targetPeerId */,
                        data
                        )
                    );

                /*
                 * Processing is required - create a processing task and then
                 * set the message response task as a continuation task
                 */

                auto processingTask = createProcessingTaskInternal(
                    "Echo server processing",
                    targetPeerId,
                    broker_protocol_ptr_t( brokerProtocolIn ),
                    om::ObjPtrCopyable< data::DataBlock >( data )
                    );

                processingTask -> setContinuationCallback(
                    [ = ]( SAA_inout Task* finishedTask ) -> om::ObjPtr< Task >
                    {
                        BL_UNUSED( finishedTask );

                        return om::copy( messageResponseTask );
                    }
                    );

                return om::moveAs< Task >( processingTask );
            }

            void scheduleIncomingRequest(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  const om::ObjPtrCopyable< data::DataBlock >&        data
                )
            {
                BL_MUTEX_GUARD( m_lock );

                chkIfDisposed();

                const auto dataCopy = data::DataBlock::copy( data, m_dataBlocksPool );

                m_eqProcessingQueue -> push_back(
                    tasks::SimpleTaskWithContinuation::createInstance< tasks::Task >(
                        cpp::bind(
                            &this_type::createServerProcessingAndResponseTask,
                            om::ObjPtrCopyable< this_type, om::Disposable >::acquireRef( this ),
                            targetPeerId,
                            om::ObjPtrCopyable< data::DataBlock >( dataCopy )
                            )
                        )
                    );
            }

        public:

            static void waitOnForwardingBackend(
                SAA_in                  const om::ObjPtr< tasks::TaskControlTokenRW >&      controlToken,
                SAA_in                  const om::ObjPtr< messaging::BackendProcessing >&   forwardingBackend
                )
            {
                using namespace bl::messaging;
                using namespace bl::tasks;

                scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                    {
                        eq -> setOptions( ExecutionQueue::OptionKeepNone );

                        /*
                         * Schedule a simple timer to request shutdown if the backend gets disconnected
                         */

                        const long disconnectedTimerFrequencyInSeconds = 5L;

                        const auto onTimer = [ disconnectedTimerFrequencyInSeconds ](
                            SAA_in          const om::ObjPtrCopyable< Task >&                       shutdownWatcher,
                            SAA_in          const om::ObjPtrCopyable< TaskControlTokenRW >&         controlToken,
                            SAA_in          const om::ObjPtrCopyable< BackendProcessing >&          backend
                            )
                            -> time::time_duration
                        {
                            if( ! backend -> isConnected() )
                            {
                                controlToken -> requestCancel();
                                shutdownWatcher -> requestCancel();
                            }

                            return time::seconds( disconnectedTimerFrequencyInSeconds );
                        };

                        /*
                         * Just create a CTRL-C shutdown watcher task and wait for the server
                         * to be shutdown gracefully
                         */

                        const auto shutdownWatcher = ShutdownTaskImpl::createInstance< Task >();

                        SimpleTimer timer(
                            cpp::bind< time::time_duration >(
                                onTimer,
                                om::ObjPtrCopyable< Task >::acquireRef( shutdownWatcher.get() ),
                                om::ObjPtrCopyable< TaskControlTokenRW >::acquireRef( controlToken.get() ),
                                om::ObjPtrCopyable< BackendProcessing >::acquireRef( forwardingBackend.get() )
                                ),
                            time::seconds( disconnectedTimerFrequencyInSeconds )            /* defaultDuration */,
                            time::seconds( 0L )                                             /* initDelay */
                            );

                        eq -> push_back( shutdownWatcher );
                        eq -> wait( shutdownWatcher );
                    });
            }

            auto messagesProcessed() const NOEXCEPT -> unsigned long
            {
                return m_messagesProcessed;
            }

            /*
             * om::Disposable
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                disposeInternal();

                BL_NOEXCEPT_END()
            }

            /*
             * messaging::AsyncBlockDispatcher
             */

            virtual auto getAllActiveQueuesIds() -> std::unordered_set< uuid_t > OVERRIDE
            {
                return std::unordered_set< uuid_t >();
            }

            virtual auto tryGetMessageBlockCompletionQueue( SAA_in const uuid_t& targetPeerId )
                -> om::ObjPtr< messaging::MessageBlockCompletionQueue > OVERRIDE
            {
                BL_UNUSED( targetPeerId );

                return nullptr;
            }

            virtual auto createDispatchTask(
                SAA_in                  const uuid_t&                                       targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&                data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkIfDisposed();

                return tasks::SimpleTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &this_type::scheduleIncomingRequest,
                        om::ObjPtrCopyable< this_type, om::Disposable >::acquireRef( this ),
                        targetPeerId,
                        om::ObjPtrCopyable< data::DataBlock >( data )
                        )
                    );
            }
        };

        typedef om::ObjectImpl< EchoServerProcessingContextT<> > EchoServerProcessingContext;

    } // echo

} // bl

#endif /* __BL_EXAMPLES_ECHOSERVER_ECHOSERVERPROCESSINGCONTEXT_H_ */
