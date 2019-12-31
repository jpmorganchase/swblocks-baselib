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

#ifndef __BL_REST_BASERESTSERVERPROCESSINGCONTEXT_H_
#define __BL_REST_BASERESTSERVERPROCESSINGCONTEXT_H_

#include <baselib/rest/RestUtils.h>

#include <baselib/messaging/ForwardingBackendProcessingImpl.h>
#include <baselib/messaging/GraphQLErrorHelpers.h>

#include <baselib/data/models/Http.h>
#include <baselib/data/DataModelObject.h>

#include <baselib/http/Globals.h>

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/ExecutionQueue.h>

#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace rest
    {
        /**
         * class BaseRestServerProcessingContext< IMPL > - A base REST server message processing
         * context implementation
         */

        template
        <
            typename IMPL
        >
        class BaseRestServerProcessingContext :
            public messaging::AsyncBlockDispatcher,
            public om::Disposable
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( BaseRestServerProcessingContext )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( messaging::AsyncBlockDispatcher )
                BL_QITBL_ENTRY( om::Disposable )
            BL_QITBL_END( messaging::AsyncBlockDispatcher )

        protected:

            typedef BaseRestServerProcessingContext< IMPL >                         this_type;

            typedef dm::messaging::BrokerProtocol                                   BrokerProtocol;

            static const std::string                                                g_healthCheckUri;

            const bool                                                              m_isGraphQLServer;
            const bool                                                              m_isAuthnticationAlwaysRequired;
            const std::string                                                       m_requiredContentType;
            const om::ObjPtr< data::datablocks_pool_type >                          m_dataBlocksPool;
            const om::ObjPtr< om::Proxy >                                           m_backendReference;
            const std::string                                                       m_tokenType;
            std::string                                                             m_tokenData;

            os::mutex                                                               m_disposeLock;
            cpp::ScalarTypeIniter< bool >                                           m_isDisposed;
            std::atomic< unsigned long >                                            m_messagesProcessed;
            om::ObjPtr< tasks::ExecutionQueue >                                     m_eqProcessingQueue;

            BaseRestServerProcessingContext(
                SAA_in      const bool                                              isGraphQLServer,
                SAA_in      const bool                                              isAuthnticationAlwaysRequired,
                SAA_in      std::string&&                                           requiredContentType,
                SAA_in      om::ObjPtr< data::datablocks_pool_type >&&              dataBlocksPool,
                SAA_in      om::ObjPtr< om::Proxy >&&                               backendReference,
                SAA_in      std::string&&                                           tokenType,
                SAA_in_opt  std::string&&                                           tokenData = std::string()
                )
                :
                m_isGraphQLServer( isGraphQLServer ),
                m_isAuthnticationAlwaysRequired( isAuthnticationAlwaysRequired ),
                m_requiredContentType( BL_PARAM_FWD( requiredContentType ) ),
                m_dataBlocksPool( BL_PARAM_FWD( dataBlocksPool ) ),
                m_backendReference( BL_PARAM_FWD( backendReference ) ),
                m_tokenType( BL_PARAM_FWD( tokenType ) ),
                m_tokenData( BL_PARAM_FWD( tokenData ) ),
                m_messagesProcessed( 0UL )
            {
                /*
                 * Disposable members are initialized in the body instead of in the initialization block.
                 * These members are usually disposed in the destructor - see disposeInternal.
                 * But in case of error in the constructor initialization block the object's destructor
                 * won't be called and disposable object destructors could assert.
                 */

                m_eqProcessingQueue = tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                    tasks::ExecutionQueue::OptionKeepNone
                    );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Backend server health check URI: "
                        << g_healthCheckUri
                    );
            }

            ~BaseRestServerProcessingContext() NOEXCEPT
            {
                if( ! m_isDisposed )
                {
                    BL_LOG(
                        Logging::warning(),
                        BL_MSG()
                            << "~BaseRestServerProcessingContext<...>() is being called without"
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
                BL_CHK(
                    true,
                    m_isDisposed.value(),
                    BL_MSG()
                        << "The server processing context was disposed already"
                    );
            }

            void validateRequestBrokerProtocol(
                SAA_in      const om::ObjPtr< BrokerProtocol >&     brokerProtocol,
                SAA_out     bool&                                   isHealthCheckRequest
                )
            {
                using HttpHeader = http::HttpHeader;

                BL_CHK(
                    nullptr,
                    brokerProtocol -> passThroughUserData(),
                    BL_MSG()
                        << "Required field "
                        << str::quoteString( BrokerProtocol::passThroughUserDataToString() )
                        << " not found in messaging broker protocol data"
                    );

                const auto payload = dm::DataModelUtils::castTo< dm::http::HttpRequestMetadataPayload >(
                    brokerProtocol -> passThroughUserData()
                    );

                const auto& metadata = payload -> httpRequestMetadata();

                BL_CHK(
                    nullptr,
                    metadata,
                    BL_MSG()
                        << "Required field "
                        << str::quoteString( dm::http::HttpRequestMetadataPayload::httpRequestMetadataToString() )
                        << " not found in messaging broker protocol user data"
                    );

                isHealthCheckRequest = str::iequals( metadata -> urlPath(), g_healthCheckUri );

                if( isHealthCheckRequest )
                {
                    return;
                }

                if( m_isAuthnticationAlwaysRequired )
                {
                    const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();

                    if(
                        ! principalIdentityInfo ||
                        ! principalIdentityInfo -> securityPrincipal() ||
                        principalIdentityInfo -> securityPrincipal() -> sid().empty()
                        )
                    {
                        const auto errorMessage =
                            "Authentication information is required for all requests";

                        BL_THROW_USER_FRIENDLY(
                            SystemException::create(
                                eh::errc::make_error_code( eh::errc::permission_denied ),
                                errorMessage
                                ),
                            BL_MSG()
                                << errorMessage
                            );
                    }
                }

                if( ! m_requiredContentType.empty() )
                {
                    const auto pos = std::find_if(
                        metadata -> headers().begin(),
                        metadata -> headers().end(),
                        []( SAA_in const std::pair<std::string, std::string>& pair ) -> bool
                        {
                            return str::iequals( pair.first, HttpHeader::g_contentType );
                        }
                        );

                    BL_CHK_USER(
                        std::end( metadata -> headers() ),
                        pos,
                        BL_MSG()
                            << "Required header "
                            << str::quoteString( HttpHeader::g_contentType )
                            << " not found in messaging broker HTTP request metadata"
                        );

                    const auto& contentType = pos -> second;

                    BL_CHK_USER(
                        false,
                        str::starts_with( contentType, HttpHeader::g_contentTypeJson ),
                        BL_MSG()
                            << "Unsupported  "
                            << str::quoteString( HttpHeader::g_contentType )
                            << " value "
                            << str::quoteString( contentType )
                            << ". Supported is "
                            << str::quoteString( HttpHeader::g_contentTypeJson )
                        );
                }
            }

            auto getResponseBrokerProtocolString(
                SAA_in      const om::ObjPtr< BrokerProtocol >&                 brokerProtocolIn,
                SAA_in_opt  om::ObjPtr< dm::http::HttpResponseMetadata >&&      responseMetadata = nullptr
                )
                -> std::string
            {
                const auto brokerProtocol = messaging::MessagingUtils::createBrokerProtocolMessage(
                    messaging::MessageType::AsyncRpcDispatch,
                    uuids::string2uuid( brokerProtocolIn -> conversationId() ),
                    m_tokenType,
                    m_tokenData
                    );

                if( ! responseMetadata )
                {
                    /*
                     * If not provided then prepare the HTTP response metadata to pass it as pass through
                     * user data in the broker protocol message part
                     */

                    responseMetadata = dm::http::HttpResponseMetadata::createInstance();
                    responseMetadata -> httpStatusCode( http::Parameters::HTTP_SUCCESS_OK );
                    responseMetadata -> contentType( http::HttpHeader::g_contentTypeJsonUtf8 );
                }

                const auto responseMetadataPayload = dm::http::HttpResponseMetadataPayload::createInstance();

                responseMetadataPayload -> httpResponseMetadata( std::move( responseMetadata ) );

                brokerProtocol -> passThroughUserData(
                    dm::DataModelUtils::castTo< dm::Payload >( responseMetadataPayload )
                    );

                return dm::DataModelUtils::getDocAsPackedJsonString( brokerProtocol );
            }

            void processingImpl(
                SAA_in      const om::ObjPtr< BrokerProtocol >&                 brokerProtocolIn,
                SAA_in      const om::ObjPtrCopyable< data::DataBlock >&        data
                )
            {
                om::ObjPtr< dm::http::HttpResponseMetadata > responseMetadata;

                ++m_messagesProcessed;

                try
                {
                    bool isHealthCheckRequest;
                    validateRequestBrokerProtocol( brokerProtocolIn, isHealthCheckRequest );

                    if( isHealthCheckRequest )
                    {
                        /*
                         * Support for non-authenticated client health check request at predefined URI.
                         * Ensures backend server connected to HTTP gateway can serve requests.
                         */

                        const auto reponseString =
                            httpserver::Response::createInstance( http::Parameters::HTTP_SUCCESS_OK ) -> content();

                        data -> reset();

                        data -> write( reponseString.c_str(), reponseString.size() );
                    }
                    else
                    {
                        responseMetadata = static_cast< IMPL* >( this ) -> processingSync( brokerProtocolIn, data );
                    }

                }
                catch( std::exception& exception )
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "Request failed with the following exception:\n"
                            << eh::diagnostic_information( exception )
                        );

                    const auto errorString =
                        m_isGraphQLServer ?
                            messaging::GraphQLErrorHelpers::getServerErrorAsGraphQL( std::current_exception() ) :
                            dm::ServerErrorHelpers::getServerErrorAsJson( std::current_exception() );

                    responseMetadata = dm::http::HttpResponseMetadata::createInstance();
                    responseMetadata -> contentType( http::HttpHeader::g_contentTypeJsonUtf8 );

                    auto httpStatusCode = http::Parameters::HTTP_CLIENT_ERROR_BAD_REQUEST;
                    RestUtils::updateHttpStatusFromException( exception, httpStatusCode /* INOUT */ );
                    responseMetadata -> httpStatusCode( httpStatusCode );

                    data -> reset();

                    data -> write( errorString.c_str(), errorString.size() );
                }

                data-> setOffset1( data -> size() );

                const auto protocolDataString =
                    getResponseBrokerProtocolString( brokerProtocolIn, std::move( responseMetadata ) );

                data -> write( protocolDataString.c_str(), protocolDataString.size() );
            }

            auto createProcessingAndResponseTask( SAA_in const om::ObjPtrCopyable< data::DataBlock >& data )
                -> om::ObjPtr< tasks::Task >
            {
                using BackendProcessing = messaging::BackendProcessing;

                BL_MUTEX_GUARD( m_disposeLock );

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

                const auto pair =
                    messaging::MessagingUtils::deserializeBlockToObjects( data, true /* brokerProtocolOnly */ );

                const auto& brokerProtocolIn = pair.first;

                auto messageResponseTask = om::ObjPtrCopyable< tasks::Task >(
                    backend -> createBackendProcessingTask(
                        BackendProcessing::OperationId::Put,
                        BackendProcessing::CommandId::None,
                        uuids::nil()                                                    /* sessionId */,
                        tasks::BlockTransferDefs::chunkIdDefault(),
                        uuids::string2uuid( brokerProtocolIn -> targetPeerId() )        /* sourcePeerId */,
                        uuids::string2uuid( brokerProtocolIn -> sourcePeerId() )        /* targetPeerId */,
                        data
                        )
                    );

                auto processingTask = tasks::SimpleTaskImpl::createInstance(
                    cpp::bind(
                        &this_type::processingImpl,
                        om::ObjPtrCopyable< this_type, om::Disposable >::acquireRef( this ),
                        om::ObjPtrCopyable< BrokerProtocol >( brokerProtocolIn ),
                        om::ObjPtrCopyable< data::DataBlock >( data )
                        )
                    );

                processingTask -> setContinuationCallback(
                    [ = ]( SAA_inout tasks::Task* finishedTask ) -> om::ObjPtr< tasks::Task >
                    {
                        BL_UNUSED( finishedTask );

                        return om::copy( messageResponseTask );
                    }
                    );

                return om::moveAs< tasks::Task >( processingTask );
            }

            void scheduleIncomingRequest( SAA_in const om::ObjPtrCopyable< data::DataBlock >& data )
            {
                BL_MUTEX_GUARD( m_disposeLock );

                chkIfDisposed();

                const auto dataCopy = data::DataBlock::copy( data, m_dataBlocksPool );

                m_eqProcessingQueue -> push_back(
                    tasks::SimpleTaskWithContinuation::createInstance< tasks::Task >(
                        cpp::bind(
                            &this_type::createProcessingAndResponseTask,
                            om::ObjPtrCopyable< this_type, om::Disposable >::acquireRef( this ),
                            om::ObjPtrCopyable< data::DataBlock >( dataCopy )
                            )
                        )
                    );
            }

        public:

            static void waitOnForwardingBackend(
                SAA_in      const om::ObjPtr< tasks::TaskControlTokenRW >&      controlToken,
                SAA_in      const om::ObjPtr< messaging::BackendProcessing >&   forwardingBackend
                )
            {
                using BackendProcessing = messaging::BackendProcessing;

                tasks::scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                    {
                        eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                        /*
                         * Schedule a simple timer to request shutdown if the backend gets disconnected
                         */

                        const long disconnectedTimerFrequencyInSeconds = 5L;

                        const auto onTimer = [](
                            SAA_in          const om::ObjPtrCopyable< tasks::Task >&                shutdownWatcher,
                            SAA_in          const om::ObjPtrCopyable< tasks::TaskControlTokenRW >&  controlToken,
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

                        const auto shutdownWatcher = tasks::ShutdownTaskImpl::createInstance< tasks::Task >();

                        tasks::SimpleTimer timer(
                            cpp::bind< time::time_duration >(
                                onTimer,
                                om::ObjPtrCopyable< tasks::Task >::acquireRef( shutdownWatcher.get() ),
                                om::ObjPtrCopyable< tasks::TaskControlTokenRW >::acquireRef( controlToken.get() ),
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

                BL_MUTEX_GUARD( m_disposeLock );

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
                SAA_in      const uuid_t&                                       targetPeerId,
                SAA_in      const om::ObjPtr< data::DataBlock >&                data
                )
                -> om::ObjPtr< tasks::Task > OVERRIDE
            {
                BL_UNUSED( targetPeerId );

                BL_MUTEX_GUARD( m_disposeLock );

                chkIfDisposed();

                return tasks::SimpleTaskImpl::createInstance< tasks::Task >(
                    cpp::bind(
                        &this_type::scheduleIncomingRequest,
                        om::ObjPtrCopyable< this_type, om::Disposable >::acquireRef( this ),
                        om::ObjPtrCopyable< data::DataBlock >( data )
                        )
                    );
            }
        };

        BL_DEFINE_STATIC_CONST_STRING( BaseRestServerProcessingContext, g_healthCheckUri ) = "/backendhealth";

    } // rest

} // bl

#endif /* __BL_REST_BASERESTSERVERPROCESSINGCONTEXT_H_ */
