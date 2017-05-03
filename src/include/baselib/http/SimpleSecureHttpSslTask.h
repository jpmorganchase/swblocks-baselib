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

#ifndef __BL_TASKS_SIMPLESECUREHTTPSSLTASK_H_
#define __BL_TASKS_SIMPLESECUREHTTPSSLTASK_H_

#include <baselib/http/SimpleHttpSslTask.h>

#include <baselib/core/SecureStringWrapper.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class SimpleSecureHttpSslTask
         */

        template
        <
            typename E = void
        >
        class SimpleSecureHttpSslTaskT : public SimpleHttpSslTask
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( SimpleSecureHttpSslTaskT )

        public:

            typedef SimpleSecureHttpSslTaskT< E >                                   this_type;
            typedef SimpleHttpSslTask                                               base_type;

        protected:

            SimpleSecureHttpSslTaskT(
                SAA_in          std::string&&                                       host,
                SAA_in          const os::port_t                                    port,
                SAA_in          const std::string&                                  path,
                SAA_in          const std::string&                                  action,
                SAA_in          const str::SecureStringWrapper&                     content,
                SAA_in          HeadersMap&&                                        requestHeaders
                )
                :
                base_type(
                    BL_PARAM_FWD( host ),
                    port,
                    path,
                    action,
                    str::empty() /* content */,
                    BL_PARAM_FWD( requestHeaders )
                    ),
                m_secureContentIn( &this -> m_contentIn )
            {
                m_secureContentIn = content;

                base_type::m_isSecureMode = true;
            }

            ~SimpleSecureHttpSslTaskT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                base_type::m_request.consume( base_type::m_request.size() );
                base_type::m_response.consume( base_type::m_response.size() );

                BL_NOEXCEPT_END()
            }

        protected:

            str::SecureStringWrapper                                                m_secureContentIn;

        };

        typedef SimpleSecureHttpSslTaskT<> SimpleSecureHttpSslTask;

        typedef om::ObjectImpl< SimpleSecureHttpSslTaskT<> > SimpleSecureHttpSslTaskImpl;

    } // tasks

} // bl

#define BL_TASKS_DECLARE_SECURE_HTTPS_TASK_IMPL_BEGIN( verbId ) \
    namespace bl \
    { \
        namespace tasks \
        { \
            \
            template \
            < \
                typename E = void \
            > \
            class SimpleSecureHttpSsl ## verbId ## TaskT : public SimpleSecureHttpSslTask \
            { \
                BL_DECLARE_OBJECT_IMPL( SimpleSecureHttpSsl ## verbId ##  TaskT ) \
            \
            protected: \
            \
                SimpleSecureHttpSsl ## verbId ##  TaskT( \
                    SAA_in          std::string&&               host, \
                    SAA_in          const os::port_t            port, \
                    SAA_in          const std::string&          path, \

#define BL_TASKS_DECLARE_SECURE_HTTPS_TASK_IMPL_END( verbId, verbStr, contentValue ) \
                    SAA_in_opt      HeadersMap&&                requestHeaders = HeadersMap() \
                    ) \
                    : \
                    SimpleSecureHttpSslTask( \
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
            typedef om::ObjectImpl< SimpleSecureHttpSsl ## verbId ##  TaskT<> > SimpleSecureHttpSsl ## verbId ##  TaskImpl; \
        } \
    } \


#define BL_TASKS_DECLARE_SECURE_HTTPS_TASK_NO_CONTENT( verbId, verbStr) \
    BL_TASKS_DECLARE_SECURE_HTTPS_TASK_IMPL_BEGIN( verbId ) \
    BL_TASKS_DECLARE_SECURE_HTTPS_TASK_IMPL_END( verbId, verbStr, str::SecureStringWrapper() ) \

#define BL_TASKS_DECLARE_SECURE_HTTPS_TASK_WITH_CONTENT( verbId, verbStr) \
        BL_TASKS_DECLARE_SECURE_HTTPS_TASK_IMPL_BEGIN( verbId ) \
        SAA_in const str::SecureStringWrapper& content, \
        BL_TASKS_DECLARE_SECURE_HTTPS_TASK_IMPL_END( verbId, verbStr, content ) \

BL_TASKS_DECLARE_SECURE_HTTPS_TASK_WITH_CONTENT( Get, "GET" )
BL_TASKS_DECLARE_SECURE_HTTPS_TASK_NO_CONTENT( Delete, "DELETE" )
BL_TASKS_DECLARE_SECURE_HTTPS_TASK_WITH_CONTENT( Put, "PUT" )
BL_TASKS_DECLARE_SECURE_HTTPS_TASK_WITH_CONTENT( Post, "POST" )

#endif /* __BL_TASKS_SIMPLESECUREHTTPSSLTASK_H_ */
