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

#ifndef __BL_CMDLINE_EHUTILS_H_
#define __BL_CMDLINE_EHUTILS_H_

#include <baselib/crypto/ErrorHandling.h>

#include <baselib/core/AsioSSL.h>
#include <baselib/core/OS.h>
#include <baselib/core/FileEncoding.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace cmdline
    {
        /**
         * @brief class EhUtils - command line error handling utility code
         */

        template
        <
            typename E = void
        >
        class EhUtilsT
        {
            BL_DECLARE_STATIC( EhUtilsT )

        public:

            static void asioErrorCallback(
                SAA_in          const eh::error_code&       ec,
                SAA_in          const char*                 location
                )
            {
                const auto msg = resolveMessage(
                    BL_MSG()
                        << ( location ? location : "Boost ASIO" )
                    );

                /*
                 * Check if it is an SSL error vs. normal system error
                 * and use the appropriate error handling macros
                 */

                if( ec.category() == asio::error::get_ssl_category() )
                {
                    const eh::error_code firstEC( ec.value(), crypto::detail::getErrorCategory() );

                    BL_THROW( crypto::getException( msg, firstEC ), msg );
                }
                else
                {
                    BL_THROW_EC( ec, msg );
                }
            }

            static void processException(
                SAA_in          const std::exception&       exception,
                SAA_in          const std::string&          errorMessage,
                SAA_in          const std::string&          debugMessage,
                SAA_in_opt      std::string&&               exceptionDump = std::string()
                ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( exceptionDump.empty() )
                {
                    exceptionDump = eh::diagnostic_information( exception );
                }

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "\n"
                        << debugMessage
                        << ":\n"
                        << exceptionDump
                    );

                try
                {
                    const auto fileName =
                        "error." +
                        uuids::uuid2string( uuids::create() ) +
                        "." +
                        std::to_string( os::getPid() ) +
                        ".txt";

                    const auto filePath = fs::temp_directory_path() / fileName;

                    encoding::writeTextFile(
                        filePath,
                        resolveMessage(
                            BL_MSG()
                                << debugMessage
                                << ":\n"
                                << exceptionDump
                            ),
                        encoding::TextFileEncoding::Ascii
                        );

                    BL_LOG_MULTILINE(
                        Logging::error(),
                        BL_MSG()
                            << errorMessage
                            << "\n"
                            << "For technical details please see the following file: "
                            << fs::normalizePathParameterForPrint( filePath )
                        );
                }
                catch( std::exception& )
                {
                    BL_LOG(
                        Logging::error(),
                        BL_MSG()
                            << errorMessage
                            << ": "
                            << exception.what()
                        );
                }

                BL_NOEXCEPT_END()
            }
        };

        typedef EhUtilsT<> EhUtils;

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_EHUTILS_H_ */
