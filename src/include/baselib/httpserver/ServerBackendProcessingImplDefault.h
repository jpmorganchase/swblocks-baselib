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

#ifndef __BL_HTTPSERVER_SERVERBACKENDPROCESSINGIMPLDEFAULT_H_
#define __BL_HTTPSERVER_SERVERBACKENDPROCESSINGIMPLDEFAULT_H_

#include <baselib/data/eh/ServerErrorHelpers.h>

#include <baselib/httpserver/ServerBackendProcessing.h>

#include <baselib/httpserver/Request.h>
#include <baselib/httpserver/Response.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace httpserver
    {
        template
        <
            typename BACKENDSTATE
        >
        class HttpServerProcessingTaskDefault : public tasks::WrapperTaskBase
        {
            BL_DECLARE_OBJECT_IMPL( HttpServerProcessingTaskDefault )

        protected:

            typedef HttpServerProcessingTaskDefault< BACKENDSTATE > this_type;
            typedef tasks::SimpleTaskImpl                           SimpleTaskImpl;
            typedef tasks::Task                                     Task;
            typedef http::Parameters::HttpStatusCode                HttpStatusCode;

            const om::ObjPtr< BACKENDSTATE >                        m_backendState;
            cpp::ScalarTypeIniter< HttpStatusCode >                 m_statusCode;
            const om::ObjPtr< Request >                             m_request;
            std::string                                             m_responseType;
            std::string                                             m_response;
            http::HeadersMap                                        m_responseHeaders;

            HttpServerProcessingTaskDefault(
                SAA_in          om::ObjPtr< Request >&&             request,
                SAA_in_opt      om::ObjPtr< BACKENDSTATE >&&        backendState = nullptr
                )
                :
                m_backendState( BL_PARAM_FWD( backendState ) ),
                m_statusCode( HttpStatusCode::HTTP_CLIENT_ERROR_NOT_FOUND ),
                m_request( BL_PARAM_FWD( request ) ),
                m_responseType( http::HttpHeader::g_contentTypeJsonUtf8 )
            {
                m_wrappedTask = SimpleTaskImpl::createInstance< Task >(
                    cpp::bind( &this_type::requestProcessing, this )
                    );
            }

            virtual void requestProcessing()
            {
                BL_THROW_EC(
                    eh::errc::make_error_code( eh::errc::no_such_file_or_directory ),
                    BL_MSG()
                        << "Content not found!"
                    );
            }

        public:

            auto getResponseHeadersLvalue() NOEXCEPT -> http::HeadersMap&
            {
                return m_responseHeaders;
            }

            auto getStatusCode() const NOEXCEPT -> HttpStatusCode
            {
                return m_statusCode;
            }

            auto getResponseLvalue() -> std::string&
            {
                return m_response;
            }

            auto getResponseTypeLvalue() -> std::string&
            {
                return m_responseType;
            }

            virtual om::ObjPtr< Task > continuationTask() OVERRIDE
            {
                return nullptr;
            }
        };

        template
        <
            typename BACKENDSTATE,
            template < typename > class TASKIMPL = HttpServerProcessingTaskDefault
        >
        class ServerBackendProcessingImplDefault : public ServerBackendProcessing
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( ServerBackendProcessingImplDefault, ServerBackendProcessing )

        protected:

            const om::ObjPtr< BACKENDSTATE >                                                m_backendState;

            ServerBackendProcessingImplDefault( SAA_in om::ObjPtr< BACKENDSTATE >&& backendState = nullptr )
                :
                m_backendState( BL_PARAM_FWD( backendState ) )
            {
            }

        public:

            typedef om::ObjectImpl< TASKIMPL< BACKENDSTATE > >                              task_t;

            virtual auto getProcessingTask( SAA_in om::ObjPtr< Request >&& request )
                -> om::ObjPtr< Task > OVERRIDE
            {
                return task_t::template createInstance< tasks::Task >(
                    BL_PARAM_FWD( request ),
                    om::copy( m_backendState )
                    );
            }

            virtual auto getResponse( SAA_in const om::ObjPtr< Task >& task )
                -> om::ObjPtr< Response > OVERRIDE
            {
                const auto taskImpl = om::qi< task_t >( task );

                return Response::createInstance(
                    taskImpl -> getStatusCode()                                             /* httpStatusCode */,
                    std::move( taskImpl -> getResponseLvalue() )                            /* content */,
                    std::move( taskImpl -> getResponseTypeLvalue() )                        /* contentType */,
                    std::move( taskImpl -> getResponseHeadersLvalue() )                     /* customHeaders */
                    );
            }

            virtual auto getStdErrorResponse(
                SAA_in const HttpStatusCode                                                 httpStatusCode,
                SAA_in const std::exception_ptr&                                            eptr
                )
                -> om::ObjPtr< Response > OVERRIDE
            {
                return Response::createInstance(
                    httpStatusCode                                                          /* httpStatusCode */,
                    dm::ServerErrorHelpers::getServerErrorAsJson( eptr )                    /* content */,
                    cpp::copy( http::HttpHeader::g_contentTypeJsonUtf8 )                    /* contentType */
                    );
            }
        };

    } // httpserver

} // bl

#endif /* __BL_HTTPSERVER_SERVERBACKENDPROCESSINGIMPLDEFAULT_H_ */
