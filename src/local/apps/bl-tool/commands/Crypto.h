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

#ifndef __APPS_BLTOOL_COMMANDS_CRYPTO_H_
#define __APPS_BLTOOL_COMMANDS_CRYPTO_H_

#include <apps/bl-tool/commands/CryptoRsaKeyExport.h>
#include <apps/bl-tool/commands/CryptoRsaKeyGenerate.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class Crypto
         */

        template
        <
            typename E = void
        >
        class CryptoT : public bl::cmdline::CommandBase
        {
            /*
             * These members must be ordered alphabetically
             */

            CryptoRsaKeyExport               m_cryptoRsaKeyExport;
            CryptoRsaKeyGenerate             m_cryptoRsaKeyGenerate;

        public:

            CryptoT(
                SAA_inout       bl::cmdline::CommandBase*       parent,
                SAA_in          const GlobalOptions&            globalOptions
                )
                :
                bl::cmdline::CommandBase( parent, "crypto" ),
                m_cryptoRsaKeyExport( this, globalOptions ),
                m_cryptoRsaKeyGenerate( this, globalOptions )
            {
                setHelpMessage(
                    "Usage: @CAPTION@\n"
                    "Supported sub-commands:\n"
                    "@COMMANDS@\n"
                    );
            }
        };

        typedef CryptoT<> Crypto;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_CRYPTO_H_ */
