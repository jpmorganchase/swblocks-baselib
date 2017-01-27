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

#ifndef __BL_HTTPSERVER_SERVERBACKENDPROCESSING_H_
#define __BL_HTTPSERVER_SERVERBACKENDPROCESSING_H_

#include <baselib/httpserver/Request.h>
#include <baselib/httpserver/Response.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( ServerBackendProcessing, "8dbdc6b8-815a-4920-b662-7957f416c3fd" )

namespace bl
{
    namespace httpserver
    {
        /**
         * @brief class ServerBackendProcessing
         */

        class ServerBackendProcessing : public om::Object
        {
            BL_DECLARE_INTERFACE( ServerBackendProcessing )

        public:

            typedef http::Parameters::HttpStatusCode                HttpStatusCode;
            typedef tasks::Task                                     Task;

            virtual om::ObjPtr< Task > getProcessingTask( SAA_in om::ObjPtr< Request >&& request ) = 0;

            virtual om::ObjPtr< Response > getResponse( SAA_in const om::ObjPtr< Task >& task ) = 0;

            virtual om::ObjPtr< Response > getStdErrorResponse(
                SAA_in const HttpStatusCode                     httpStatusCode,
                SAA_in const std::exception_ptr&                exception
                ) = 0;
        };

    } // httpserver

} // bl

#endif /* __BL_HTTPSERVER_SERVERBACKENDPROCESSING_H_ */
