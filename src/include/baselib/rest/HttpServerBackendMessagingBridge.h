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

#ifndef __BL_REST_HTTPSERVERBACKENDMESSAGINGBRIDGE_H_
#define __BL_REST_HTTPSERVERBACKENDMESSAGINGBRIDGE_H_

#include <baselib/messaging/ForwardingBackendProcessingFactory.h>
#include <baselib/messaging/ForwardingBackendSharedState.h>
#include <baselib/messaging/MessagingUtils.h>
#include <baselib/messaging/MessagingClientObject.h>
#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/BrokerErrorCodes.h>
#include <baselib/messaging/AsyncBlockDispatcher.h>
#include <baselib/messaging/MessagingClientBlockDispatch.h>
#include <baselib/messaging/MessagingClientFactory.h>

#include <baselib/httpserver/ServerBackendProcessing.h>
#include <baselib/httpserver/Request.h>
#include <baselib/httpserver/Response.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/TasksUtils.h>

#include <baselib/data/eh/ServerErrorHelpers.h>
#include <baselib/data/models/JsonMessaging.h>
#include <baselib/data/models/Http.h>
#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace rest
    {
        template
        <
            typename E = void
        >
        class HttpServerBackendMessagingBridgeT :
            public httpserver::ServerBackendProcessing,
            public messaging::AsyncBlockDispatcher,
            public om::Disposable
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( HttpServerBackendMessagingBridgeT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( httpserver::ServerBackendProcessing )
                BL_QITBL_ENTRY( messaging::AsyncBlockDispatcher )
                BL_QITBL_ENTRY( om::Disposable )
            BL_QITBL_END( httpserver::ServerBackendProcessing )

        protected:

            enum : long
            {
                TIMEOUT_PRUNE_TIMER_IN_SECONDS = 5L,
            };

            template
            <
                typename E2 = void
            >
            class SharedStateT : public om::DisposableObjectBase
            {
            protected:

                struct Info
                {
                    tasks::CompletionCallback                                   callback;
                    om::ObjPtr< data::DataBlock >                               response;
                    time::ptime                                                 utcRegisteredAt;
                };

                typedef std::unordered_map
                <
                    uuid_t                          /* conversationId */,
                    Info                            /* info */
                >
                requests_map_t;

                typedef om::ObjPtrCopyable< data::DataBlock >                   data_ptr_t;

                const time::time_duration                                       m_requestTimeout;
                requests_map_t                                                  m_requestsInFlight;
                os::mutex                                                       m_lock;
                cpp::ScalarTypeIniter< bool >                                   m_isDisposed;

                void chkIfDisposed()
                {
                    BL_CHK_T(
                        true,
                        m_isDisposed.value(),
                        UnexpectedException(),
                        BL_MSG()
                            << "The backend shared state was disposed already"
                        );
                }

                static void cancelRequestsNoThrow( SAA_inout requests_map_t&& requests ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    if( requests.empty() )
                    {
                        return;
                    }

                    const auto eptr = std::make_exception_ptr(
                        SystemException::create( asio::error::operation_aborted, BL_SYSTEM_ERROR_DEFAULT_MSG )
                        );

                    for( auto& pair : requests )
                    {
                        auto& info = pair.second;

                        if( info.callback )
                        {
                            info.callback( eptr );
                        }

                        info.response.reset();
                        info.utcRegisteredAt = time::neg_infin;
                    }

                    BL_NOEXCEPT_END()
                }

                auto getRequestsToDisposeInternal() NOEXCEPT -> requests_map_t
                {
                    requests_map_t requestsInFlight;

                    if( ! m_isDisposed )
                    {
                        m_requestsInFlight.swap( requestsInFlight );

                        m_isDisposed = true;
                    }

                    return requestsInFlight;
                }

                auto getExpiredRequestsInternal() -> requests_map_t
                {
                    requests_map_t expiredRequests;

                    BL_MUTEX_GUARD( m_lock );

                    for( auto& pair : m_requestsInFlight )
                    {
                        const auto elapsed = time::microsec_clock::universal_time() - pair.second.utcRegisteredAt;

                        if( elapsed > m_requestTimeout )
                        {
                            expiredRequests.emplace( pair.first, std::move( pair.second ) );
                        }
                    }

                    if( expiredRequests.empty() )
                    {
                        for( const auto& pair : expiredRequests )
                        {
                            m_requestsInFlight.erase( pair.first );
                        }
                    }

                    return expiredRequests;
                }

                auto completeRequestInternal(
                    SAA_in              const bool                              discardInfo,
                    SAA_in              const uuid_t&                           conversationId,
                    SAA_in_opt          const data_ptr_t&                       dataBlock,
                    SAA_in_opt          const std::exception_ptr&               eptr
                    )
                    -> om::ObjPtr< data::DataBlock >
                {
                    om::ObjPtr< data::DataBlock > result;
                    tasks::CompletionCallback onReady;

                    {
                        BL_MUTEX_GUARD( m_lock );

                        chkIfDisposed();

                        auto pair = m_requestsInFlight.emplace( conversationId, Info() );

                        auto& info = pair.first -> second;

                        if( pair.second /* bool=true if inserted */ )
                        {
                            /*
                             * The info was inserted successfully and that means the data
                             * has arrived before the wait task had a chance to register
                             * for waiting
                             *
                             * In this case we simply update info.utcRegisteredAt and
                             * save the data, but we won't invoke the completion callback
                             * as none would be available
                             *
                             * In this case the registerRequest method below would return
                             * false to make the wait task to complete immediately /
                             * synchronously
                             */

                            info.utcRegisteredAt = time::microsec_clock::universal_time();
                        }

                        result = std::move( info.response );
                        info.response = om::copy( dataBlock );
                        onReady = std::move( info.callback );

                        if( discardInfo )
                        {
                            m_requestsInFlight.erase( pair.first /* pos */ );
                        }
                    }

                    if( onReady )
                    {
                        onReady( eptr );
                    }

                    return result;
                }

                SharedStateT( SAA_in const time::time_duration& requestTimeout )
                    :
                    m_requestTimeout( requestTimeout )
                {
                }

                ~SharedStateT() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    if( ! m_isDisposed )
                    {
                        BL_LOG(
                            Logging::warning(),
                            BL_MSG()
                                << "~SharedStateT() state is being called without the object being disposed"
                            );
                    }

                    cancelRequestsNoThrow( getRequestsToDisposeInternal() );

                    BL_NOEXCEPT_END()
                }

            public:

                /*
                 * om::Disposable
                 */

                virtual void dispose() NOEXCEPT OVERRIDE
                {
                    BL_NOEXCEPT_BEGIN()

                    requests_map_t requestsInFlight;

                    {
                        BL_MUTEX_GUARD( m_lock );

                        getRequestsToDisposeInternal().swap( requestsInFlight );
                    }

                    cancelRequestsNoThrow( std::move( requestsInFlight ) );

                    BL_NOEXCEPT_END()
                }

                bool registerRequest(
                    SAA_in              const uuid_t&                           conversationId,
                    SAA_in              const tasks::CompletionCallback&        onReady
                    )
                {
                    BL_MUTEX_GUARD( m_lock );

                    chkIfDisposed();

                    auto pair = m_requestsInFlight.emplace( conversationId, Info() );

                    if( ! pair.second )
                    {
                        /*
                         * The data was already received and completeRequest has been
                         * called already
                         *
                         * Return false to let the wait task complete synchronously
                         */

                        return false;
                    }

                    auto& info = pair.first -> second;

                    info.callback = onReady;
                    info.utcRegisteredAt = time::microsec_clock::universal_time();

                    return true;
                }

                void completeRequest(
                    SAA_in              const uuid_t&                           conversationId,
                    SAA_in_opt          const data_ptr_t&                       dataBlock,
                    SAA_in_opt          const std::exception_ptr&               eptr
                    )
                {
                    ( void ) completeRequestInternal( false /* discardInfo */, conversationId, dataBlock, eptr );
                }

                bool cancelRequest( SAA_in const uuid_t& conversationId ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    completeRequest(
                        conversationId,
                        nullptr /* dataBlock */,
                        std::make_exception_ptr(
                            SystemException::create( asio::error::operation_aborted, BL_SYSTEM_ERROR_DEFAULT_MSG )
                            )
                        );

                    BL_NOEXCEPT_END()

                    /*
                     * The operation is always considered cancel-able
                     */

                    return true;
                }

                auto closeRequest( SAA_in const uuid_t& conversationId ) NOEXCEPT
                    -> om::ObjPtr< data::DataBlock >
                {
                    om::ObjPtr< data::DataBlock > result;

                    BL_NOEXCEPT_BEGIN()

                    result = completeRequestInternal(
                        true                        /* discardInfo */,
                        conversationId,
                        nullptr                     /* dataBlock */,
                        std::make_exception_ptr(
                            SystemException::create( asio::error::operation_aborted, BL_SYSTEM_ERROR_DEFAULT_MSG )
                            )
                        );

                    BL_NOEXCEPT_END()

                    return result;
                }

                auto onTimer() -> time::time_duration
                {
                    cancelRequestsNoThrow( getExpiredRequestsInternal() );

                    return time::seconds( TIMEOUT_PRUNE_TIMER_IN_SECONDS );
                }
            };

            typedef om::ObjectImpl< SharedStateT<> >                            shared_state_t;

            template
            <
                typename E2 = void
            >
            class LocalDispatchingTaskT : public tasks::WrapperTaskBase
            {
                BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( LocalDispatchingTaskT )

            protected:

                typedef tasks::WrapperTaskBase                                  base_type;

                const uuid_t                                                    m_conversationId;
                const om::ObjPtr< shared_state_t >                              m_sharedState;
                const om::ObjPtr< tasks::Task >                                 m_sendMessageTask;
                const om::ObjPtr< tasks::Task >                                 m_waitingResponse;

                LocalDispatchingTaskT(
                    SAA_in              const uuid_t&                           conversationId,
                    SAA_in              om::ObjPtr< shared_state_t >&&          sharedState,
                    SAA_in              om::ObjPtr< tasks::Task >&&             sendMessageTask,
                    SAA_in_opt          om::ObjPtr< tasks::Task >&&             waitingResponse
                    )
                    :
                    m_conversationId( conversationId ),
                    m_sharedState( BL_PARAM_FWD( sharedState ) ),
                    m_sendMessageTask( BL_PARAM_FWD( sendMessageTask ) ),
                    m_waitingResponse( BL_PARAM_FWD( waitingResponse ) )
                {
                    m_wrappedTask = om::copy( m_sendMessageTask );
                }

                ~LocalDispatchingTaskT() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    /*
                     * The returned data block is simply discarded
                     */

                    ( void ) m_sharedState -> closeRequest( m_conversationId );

                    BL_NOEXCEPT_END()
                }

                virtual om::ObjPtr< tasks::Task > continuationTask() OVERRIDE
                {
                    auto task = base_type::handleContinuationForward();

                    if( task )
                    {
                        return task;
                    }

                    const auto eptr = exception();

                    if( eptr || om::areEqual( m_wrappedTask, m_waitingResponse ) )
                    {
                        return nullptr;
                    }

                    /*
                     * If we are here that means that the send block task has not been executed yet
                     * and we don't have
                     */

                    m_wrappedTask = om::copy( m_waitingResponse );

                    return om::copyAs< tasks::Task >( this );
                }

            public:

                auto conversationId() const NOEXCEPT -> const uuid_t&
                {
                    return m_conversationId;
                }
            };

            enum : long
            {
                DEFAULT_REQUEST_TIMEOUT_IN_SECONDS = 2L * 60L,
            };

            typedef om::ObjectImpl< LocalDispatchingTaskT<> >                   task_t;

            const om::ObjPtr< om::Proxy >                                       m_hostServices;
            const om::ObjPtr< shared_state_t >                                  m_sharedState;
            const om::ObjPtr< messaging::BackendProcessing >                    m_messagingBackend;
            const uuid_t                                                        m_sourcePeerId;
            const uuid_t                                                        m_targetPeerId;
            const om::ObjPtr< data::datablocks_pool_type >                      m_dataBlocksPool;
            const std::unordered_set< std::string >                             m_tokenCookieNames;
            const std::string                                                   m_tokenTypeDefault;
            const std::string                                                   m_tokenDataDefault;

            os::mutex                                                           m_lock;
            cpp::ScalarTypeIniter< bool >                                       m_isDisposed;
            tasks::SimpleTimer                                                  m_timer;

            HttpServerBackendMessagingBridgeT(
                SAA_in          om::ObjPtr< messaging::BackendProcessing >&&    messagingBackend,
                SAA_in          const uuid_t&                                   sourcePeerId,
                SAA_in          const uuid_t&                                   targetPeerId,
                SAA_in          om::ObjPtr< data::datablocks_pool_type >&&      dataBlocksPool,
                SAA_in          std::unordered_set< std::string >&&             tokenCookieNames,
                SAA_in_opt      std::string&&                                   tokenTypeDefault = std::string(),
                SAA_in_opt      std::string&&                                   tokenDataDefault = std::string(),
                SAA_in_opt      const time::time_duration&                      requestTimeout = time::neg_infin
                )
                :
                m_hostServices( om::ProxyImpl::createInstance< om::Proxy >( false /* strongRef */ ) ),
                m_sharedState(
                    shared_state_t::createInstance(
                        requestTimeout.is_special() ?
                            time::seconds( DEFAULT_REQUEST_TIMEOUT_IN_SECONDS ) : requestTimeout
                        )
                    ),
                m_messagingBackend( BL_PARAM_FWD( messagingBackend ) ),
                m_sourcePeerId( sourcePeerId ),
                m_targetPeerId( targetPeerId ),
                m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) ),
                m_tokenCookieNames( BL_PARAM_FWD( tokenCookieNames ) ),
                m_tokenTypeDefault( BL_PARAM_FWD( tokenTypeDefault ) ),
                m_tokenDataDefault( BL_PARAM_FWD( tokenDataDefault ) ),
                m_timer(
                    cpp::bind(
                        &shared_state_t::onTimer,
                        om::ObjPtrCopyable< shared_state_t >::acquireRef( m_sharedState.get() )
                        ),
                    time::seconds( TIMEOUT_PRUNE_TIMER_IN_SECONDS )         /* defaultDuration */,
                    time::seconds( 0L )                                     /* initDelay */
                    )
            {
                m_messagingBackend -> setHostServices( om::copy( m_hostServices ) );

                m_hostServices -> connect( static_cast< httpserver::ServerBackendProcessing* >( this ) );
            }

            ~HttpServerBackendMessagingBridgeT() NOEXCEPT
            {
                if( ! m_isDisposed )
                {
                    BL_LOG(
                        Logging::warning(),
                        BL_MSG()
                            << "~~HttpServerBackendMessagingBridgeT() state is being called without"
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

                m_timer.stop();
                m_hostServices -> disconnect();
                m_sharedState -> dispose();
                m_messagingBackend -> dispose();

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
                        << "The backend was disposed already"
                    );
            }

        public:

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
             * httpserver::ServerBackendProcessing
             */

            virtual auto getProcessingTask( SAA_in om::ObjPtr< httpserver::Request >&& request )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                using namespace bl::messaging;
                using namespace bl::tasks;

                chkIfDisposed();

                const auto dataBlock = data::DataBlock::get( m_dataBlocksPool );

                const auto& requestBody = request -> body();

                if( requestBody.size() )
                {
                    dataBlock -> write( requestBody.c_str(), requestBody.size() );
                    dataBlock -> setOffset1( dataBlock -> size() );
                }

                std::string tokenData;

                const auto& requestHeaders = request -> headers();

                const auto pos = requestHeaders.find( http::HttpHeader::g_cookie );

                if( pos != std::end( requestHeaders ) )
                {
                    const auto allCookies = str::parsePropertiesList( pos -> second );

                    std::unordered_map< std::string, std::string > tokenProperties;

                    std::copy_if(
                        std::begin( allCookies ),
                        std::end( allCookies ),
                        std::inserter( tokenProperties, tokenProperties.end() ),
                        [ & ]( const std::pair< std::string, std::string >& pair ) -> bool
                        {
                            return m_tokenCookieNames.find( pair.first ) != std::end( m_tokenCookieNames );
                        }
                        );

                    tokenData = str::joinFormattedImpl(
                        tokenProperties,
                        ";",                    /* separator */
                        str::empty(),           /* lastSeparator */
                        [](
                            SAA_inout   std::ostream&                               stream,
                            SAA_in      const std::pair< std::string, std::string>& pair
                        )
                        -> void
                        {
                            stream
                                << pair.first
                                << '='
                                << pair.second
                                << ';';
                        }
                        );
                }
                else
                {
                    tokenData = m_tokenDataDefault;
                }

                const auto conversationId = uuids::create();

                const auto brokerProtocol = MessagingUtils::createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    m_tokenTypeDefault,
                    tokenData
                    );

                /*
                 * Prepare the HTTP request metadata and send it as pass through data in the
                 * broker protocol part of the message
                 */

                const auto requestMetadata = dm::http::HttpRequestMetadata::createInstance();

                requestMetadata -> method( request -> method() );
                requestMetadata -> urlPath( request -> uri() );

                for( const auto& header : requestHeaders )
                {
                    auto pair = dm::NameValueStringsPair::createInstance();

                    pair -> name( header.first );
                    pair -> value( header.second );

                    requestMetadata -> headersLvalue().push_back( std::move( pair ) );
                }

                brokerProtocol -> passThroughUserData(
                    dm::DataModelUtils::castTo< bl::dm::Payload >( requestMetadata )
                    );

                const auto protocolDataString =
                    dm::DataModelUtils::getDocAsPackedJsonString( brokerProtocol );

                dataBlock -> write( protocolDataString.c_str(), protocolDataString.size() );

                auto sendMessageTask = m_messagingBackend -> createBackendProcessingTask(
                    BackendProcessing::OperationId::Put,
                    BackendProcessing::CommandId::None,
                    uuids::nil()                                    /* sessionId */,
                    BlockTransferDefs::chunkIdDefault(),
                    m_sourcePeerId,
                    m_targetPeerId,
                    dataBlock
                    );

                return task_t::template createInstance< tasks::Task >(
                    conversationId,
                    om::copy( m_sharedState ),
                    std::move( sendMessageTask ),
                    ExternalCompletionTaskIfImpl::createInstance< Task >(
                        cpp::bind(
                            &shared_state_t::registerRequest,
                            om::ObjPtrCopyable< shared_state_t >::acquireRef( m_sharedState.get() ),
                            conversationId,
                            _1 /* onReady */
                            ),
                        cpp::bind(
                            &shared_state_t::cancelRequest,
                            om::ObjPtrCopyable< shared_state_t >::acquireRef( m_sharedState.get() ),
                            conversationId
                            )
                        )
                    );
            }

            virtual auto getResponse( SAA_in const om::ObjPtr< Task >& task )
                -> om::ObjPtr< httpserver::Response > OVERRIDE
            {
                chkIfDisposed();

                const auto taskImpl = om::qi< task_t >( task );

                const auto dataBlock = m_sharedState -> closeRequest( taskImpl -> conversationId() );

                /*
                 * If the processing task has completed successfully a data block must be available
                 */

                BL_CHK(
                    nullptr,
                    dataBlock.get(),
                    BL_MSG()
                        << "HttpServerBackendMessagingBridge::getResponse(): data block not available"
                    );

                std::string contentResponse(
                    dataBlock -> begin(),
                    dataBlock -> begin() + dataBlock -> offset1()
                    );

                HttpStatusCode statusCode = http::Parameters::HTTP_SUCCESS_OK;
                std::string contentType = bl::http::HttpHeader::g_contentTypeDefault;
                http::HeadersMap responseHeaders;

                /*
                 * Check to parse and consume the HTTP response metadata if provided by the server
                 */

                const auto pair =
                    messaging::MessagingUtils::deserializeBlockToObjects( dataBlock, true /* brokerProtocolOnly */ );

                const auto& brokerProtocol = pair.first;
                const auto& passThroughUserData = brokerProtocol -> passThroughUserData();

                if( passThroughUserData )
                {
                    const auto responseMetadata =
                        dm::DataModelUtils::castTo< dm::http::HttpResponseMetadata >( passThroughUserData );

                    if( responseMetadata -> statusCode() )
                    {
                        statusCode = static_cast< HttpStatusCode >( responseMetadata -> statusCode() );
                    }

                    contentType = responseMetadata -> contentType();

                    for( const auto& headerInfo : responseMetadata -> headers() )
                    {
                        if( headerInfo -> name() == bl::http::HttpHeader::g_contentType )
                        {
                            BL_CHK(
                                false,
                                headerInfo -> value() == contentType,
                                BL_MSG()
                                    << "The content type returned from the server should match "
                                    << "the value in the headers"
                                );

                            /*
                             * Skip this as content type is not expected to be passed as
                             * a custom header
                             */

                            continue;
                        }

                        responseHeaders[ headerInfo -> name() ] = headerInfo -> value();
                    }
                }

                return httpserver::Response::createInstance(
                    statusCode                                      /* httpStatusCode */,
                    std::move( contentResponse )                    /* content */,
                    std::move( contentType )                        /* contentType */,
                    std::move( responseHeaders )                    /* customHeaders */
                    );
            }

            virtual auto getStdErrorResponse(
                SAA_in const HttpStatusCode                                     httpStatusCode,
                SAA_in const std::exception_ptr&                                exception
                )
                -> om::ObjPtr< httpserver::Response > OVERRIDE
            {
                chkIfDisposed();

                return httpserver::Response::createInstance(
                    httpStatusCode                                              /* httpStatusCode */,
                    dm::ServerErrorHelpers::getServerErrorAsJson( exception )   /* content */,
                    cpp::copy( http::HttpHeader::g_contentTypeJsonUtf8 )        /* contentType */
                    );
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
                SAA_in                  const uuid_t&                           targetPeerId,
                SAA_in                  const om::ObjPtr< data::DataBlock >&    data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                BL_UNUSED( targetPeerId );

                BL_ASSERT( m_sourcePeerId == targetPeerId );

                const auto pair =
                    messaging::MessagingUtils::deserializeBlockToObjects( data, true /* brokerProtocolOnly */ );

                const auto& brokerProtocol = pair.first;

                const auto conversationId = uuids::string2uuid( brokerProtocol -> conversationId() );

                const auto dataCopy = data::DataBlock::copy( data, m_dataBlocksPool );

                return tasks::SimpleTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &shared_state_t::completeRequest,
                        om::ObjPtrCopyable< shared_state_t >::acquireRef( m_sharedState.get() ),
                        conversationId,
                        om::ObjPtrCopyable< data::DataBlock >( dataCopy ),
                        std::exception_ptr()
                        )
                    );
            }
        };

        typedef om::ObjectImpl< HttpServerBackendMessagingBridgeT<> > HttpServerBackendMessagingBridge;

    } // rest

} // bl

#endif /* __BL_REST_HTTPSERVERBACKENDMESSAGINGBRIDGE_H_ */
