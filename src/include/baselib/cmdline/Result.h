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

#ifndef __BL_CMDLINE_RESULT_H_
#define __BL_CMDLINE_RESULT_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace cmdline
    {
        /**
         * @brief class Result - returned by CommandBaseT::execute
         */

        template
        <
            typename E = void
        >
        class ResultT
        {
        private:

            int                         m_returnCode;
            std::string                 m_message;

        public:

            ResultT(
                SAA_in_opt  const int       returnCode = 0,
                SAA_in_opt  std::string&&   message = ""
                )
                :
                m_returnCode( returnCode ),
                m_message( BL_PARAM_FWD( message ) )
            {
            }

            template
            <
                typename T
            >
            static ResultT create( SAA_in T&& apiResult )
            {
                return ResultT(
                    apiResult.returnCode,
                    BL_PARAM_FWD( apiResult.message )
                    );
            }

            int getReturnCode() const NOEXCEPT
            {
                return m_returnCode;
            }

            void setReturnCode( SAA_in const int returnCode ) NOEXCEPT
            {
                m_returnCode = returnCode;
            }

            const std::string& getMessage() const
            {
                return m_message;
            }

            void setMessage( SAA_inout std::string&& message )
            {
                m_message = BL_PARAM_FWD( message );
            }
        };

        typedef ResultT<> Result;

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_RESULT_H_ */
