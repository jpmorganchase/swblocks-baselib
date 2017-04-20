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

#ifndef __BL_TASKS_SIMPLEHTTPSSLTASK_H_
#define __BL_TASKS_SIMPLEHTTPSSLTASK_H_

#include <baselib/http/SimpleHttpTask.h>
#include <baselib/http/Globals.h>

#include <baselib/tasks/TcpSslBaseTasks.h>
#include <baselib/tasks/TcpBaseTasks.h>

#include <baselib/core/AsioSSL.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/Utils.h>
#include <baselib/core/NetUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <sstream>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class SimpleHttpSslTask
         */

        template
        <
            typename E = void
        >
        class SimpleHttpSslTaskT : public SimpleHttpTaskT< TcpSslSocketAsyncBase >
        {
            BL_DECLARE_OBJECT_IMPL( SimpleHttpSslTaskT )

        public:

            typedef SimpleHttpSslTaskT< E >                                         this_type;
            typedef SimpleHttpTaskT< TcpSslSocketAsyncBase >                        base_type;

            typedef http::HeadersMap                                                HeadersMap;

        protected:

            static const std::string                                                g_protocolHttps;

            SimpleHttpSslTaskT(
                SAA_in          std::string&&               host,
                SAA_in          const unsigned short        port,
                SAA_in          const std::string&          path,
                SAA_in          const std::string&          action,
                SAA_in          const std::string&          content,
                SAA_in          HeadersMap&&                requestHeaders
                )
                :
                base_type(
                    BL_PARAM_FWD( host ),
                    port,
                    path,
                    action,
                    content,
                    BL_PARAM_FWD( requestHeaders )
                    )
            {
            }

            virtual const std::string& getProtocol() const NOEXCEPT OVERRIDE
            {
                return g_protocolHttps;
            }
        };

        template
        <
            typename E
        >
        const std::string
        SimpleHttpSslTaskT< E >::g_protocolHttps( "https" );

        typedef SimpleHttpSslTaskT<> SimpleHttpSslTask;

        typedef om::ObjectImpl< SimpleHttpSslTaskT<> > SimpleHttpSslTaskImpl;

    } // tasks

} // bl

#define BL_TASKS_DECLARE_HTTPS_TASK_IMPL_BEGIN( verbId ) \
    namespace bl \
    { \
        namespace tasks \
        { \
            \
            template \
            < \
                typename E = void \
            > \
            class SimpleHttpSsl ## verbId ## TaskT : public SimpleHttpSslTask \
            { \
                BL_DECLARE_OBJECT_IMPL( SimpleHttpSsl ## verbId ##  TaskT ) \
            \
            protected: \
            \
                SimpleHttpSsl ## verbId ##  TaskT( \
                    SAA_in          std::string&&               host, \
                    SAA_in          const unsigned short        port, \
                    SAA_in          const std::string&          path, \

#define BL_TASKS_DECLARE_HTTPS_TASK_IMPL_END( verbId, verbStr, contentValue ) \
                    SAA_in_opt      HeadersMap&&                requestHeaders = HeadersMap() \
                    ) \
                    : \
                    SimpleHttpSslTask( \
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
            typedef om::ObjectImpl< SimpleHttpSsl ## verbId ##  TaskT<> > SimpleHttpSsl ## verbId ##  TaskImpl; \
        } \
    } \


#define BL_TASKS_DECLARE_HTTPS_TASK_NO_CONTENT( verbId, verbStr ) \
    BL_TASKS_DECLARE_HTTPS_TASK_IMPL_BEGIN( verbId ) \
    BL_TASKS_DECLARE_HTTPS_TASK_IMPL_END( verbId, verbStr, "" ) \

#define BL_TASKS_DECLARE_HTTPS_TASK_WITH_CONTENT( verbId, verbStr ) \
        BL_TASKS_DECLARE_HTTPS_TASK_IMPL_BEGIN( verbId ) \
        SAA_in const std::string& content, \
        BL_TASKS_DECLARE_HTTPS_TASK_IMPL_END( verbId, verbStr, content ) \

BL_TASKS_DECLARE_HTTPS_TASK_WITH_CONTENT( Get, "GET" )
BL_TASKS_DECLARE_HTTPS_TASK_NO_CONTENT( Delete, "DELETE" )
BL_TASKS_DECLARE_HTTPS_TASK_WITH_CONTENT( Put, "PUT" )
BL_TASKS_DECLARE_HTTPS_TASK_WITH_CONTENT( Post, "POST" )

#endif /* __BL_TASKS_SIMPLEHTTPSSLTASK_H_ */
