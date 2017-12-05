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

#ifndef __UTEST_LOGGERUTILS_H_
#define __UTEST_LOGGERUTILS_H_

#include <utests/baselib/Utf.h>

#include <baselib/core/BaseIncludes.h>

namespace utest
{

    /*
     * These loggers override error/warning messages as debug ones
     * so tests can finish successfully despite such messages
     * being generated in code.
     */

    inline void toDebugLineLogger(
        SAA_in      const std::string&                prefix,
        SAA_in      const std::string&                text,
        SAA_in      const bool                        /* enableTimestamp */,
        SAA_in      const bl::Logging::Level          actualLevel,
        SAA_in      const bl::Logging::Level          expectedLevel
        )
    {
        using namespace bl;

        bl::cpp::SafeOutputStringStream os;

        if( actualLevel == expectedLevel )
        {
            os << Logging::debug().prefix();
        }

        Logging::defaultLineLoggerNoLock(
            prefix,
            text,
            true /* enableTimestamp */,
            actualLevel,
            false /* addNewLine */,
            os
            );

        os.flush();

        BL_STDIO_TEXT(
            {
                UTF_MESSAGE( os.str() );
            }
            );
    }

    inline void warningToDebugLineLogger(
        SAA_in      const std::string&                prefix,
        SAA_in      const std::string&                text,
        SAA_in      const bool                        enableTimestamp,
        SAA_in      const bl::Logging::Level          level
        )
    {
        toDebugLineLogger( prefix, text, enableTimestamp, level, bl::Logging::LL_WARNING );
    }

    inline void errorToDebugLineLogger(
        SAA_in      const std::string&                prefix,
        SAA_in      const std::string&                text,
        SAA_in      const bool                        enableTimestamp,
        SAA_in      const bl::Logging::Level          level
        )
    {
        toDebugLineLogger( prefix, text, enableTimestamp, level, bl::Logging::LL_ERROR );
    }

} // test

#endif /* __UTEST_LOGGERUTILS_H_ */
