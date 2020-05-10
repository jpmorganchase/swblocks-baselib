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

#include <apps/bl-tool/BaselibToolApp.h>
#include<bits/stdc++.h>
#include <baselib/core/Logging.h>
#include <baselib/core/BaseIncludes.h>

extern "C" int main(
    SAA_in                      int                         argc,
    SAA_in_ecount( argc )       const char* const*          argv
    )
{
    BL_LOG_MULTILINE(
        bl::Logging::notify(),
        BL_MSG()
            << "Baselib Tool\n"
        );

    return bltool::BaselibToolApp::main( argc, argv );
}
