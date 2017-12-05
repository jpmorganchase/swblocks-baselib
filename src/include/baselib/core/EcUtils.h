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

#ifndef __BL_ECUTILS_H_
#define __BL_ECUTILS_H_

#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace eh
    {
        /**
         * @brief class EcUtils. Contains utility functions for error handling
         * using error codes instead of exceptions.
         */

        template
        <
            typename E = void
        >
        class EcUtilsT
        {
            BL_DECLARE_STATIC( EcUtilsT )

        public:

            static int getErrorCode( SAA_in const cpp::void_callback_t& callback ) NOEXCEPT
            {
                int error = eh::errc::success;

                try
                {
                    callback();
                }
                catch( std::exception& e )
                {
                    BL_LOG_MULTILINE(
                        Logging::error(),
                        BL_MSG()
                            << e.what()
                        );

                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "\nUnhandled exception was encountered:\n"
                            << eh::diagnostic_information( e )
                        );

                    const int* errNo = eh::get_error_info< eh::errinfo_errno >( e );

                    if( errNo )
                    {
                        error = *errNo;
                    }

                    if( ! error )
                    {
                        const auto* errorCode = eh::get_error_info< eh::errinfo_error_code >( e );

                        if( errorCode && errorCode -> category() == eh::generic_category() )
                        {
                            error = errorCode -> value();
                        }
                        else
                        {
                            error = eh::errc::no_message;
                        }
                    }
                }

                return error;
            }

            static void checkErrorCode( SAA_in const int rc )
            {
                if( rc != 0 )
                {
                    eh::error_code code( rc, eh::generic_category() );

                    /*
                     * The function that returned a non-zero error code is
                     * responsible for logging any exception details
                     * (see getErrorCode above)
                     */

                    BL_THROW(
                        UnexpectedException()
                            << eh::errinfo_errno( code.value() )
                            << eh::errinfo_error_code( code ),
                        BL_MSG()
                            << "An unexpected error occurred: '"
                            << code.message()
                            << "'"
                        );
                }
            }
        };

    typedef EcUtilsT<> EcUtils;

    } // eh

} // bl

#endif /* __BL_ECUTILS_H_ */
