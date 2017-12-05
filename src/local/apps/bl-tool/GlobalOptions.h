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

#ifndef __APPS_BLTOOL_GLOBALOPTIONS_H_
#define __APPS_BLTOOL_GLOBALOPTIONS_H_

#include <baselib/core/BaseIncludes.h>

#include <baselib/cmdline/Option.h>

namespace bltool
{
    /**
     * @brief class GlobalOptions
     */

    template
    <
        typename E = void
    >
    struct GlobalOptionsT
    {
        bl::cmdline::HelpSwitch             m_help;
        bl::cmdline::VerboseSwitch          m_verbose;
        bl::cmdline::DebugSwitch            m_debug;
    };

    typedef GlobalOptionsT<> GlobalOptions;

} // bltool

#endif /* __APPS_BLTOOL_GLOBALOPTIONS_H_ */
