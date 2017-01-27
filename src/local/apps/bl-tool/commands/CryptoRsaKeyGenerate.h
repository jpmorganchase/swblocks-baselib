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

#ifndef __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYGENERATE_H_
#define __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYGENERATE_H_

#include <apps/bl-tool/commands/CryptoRsaKeyCommandBase.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class CryptoRsaKeyGenerate
         */

        template
        <
            typename E = void
        >
        class CryptoRsaKeyGenerateT : public CryptoRsaKeyCommandBase
        {
            BL_NO_COPY_OR_MOVE( CryptoRsaKeyGenerateT )

        private:

            typedef CryptoRsaKeyCommandBase               base_type;

            BL_CMDLINE_OPTION(
                m_encrypt,
                BoolSwitch,
                "encrypt",
                "If specified the generated key will be encrypted"
                )

            BL_CMDLINE_OPTION(
                m_password,
                StringOption,
                "password",
                "The password to use for encryption of the RSA key"
                )

            BL_CMDLINE_OPTION(
                m_pemFormat,
                BoolSwitch,
                "pemformat",
                "If specified the generated key will be in PEM key type format"
                )

            BL_CMDLINE_OPTION(
                m_path,
                StringOption,
                "path",
                "A file path where the output RSA key should be written to"
                )

        public:

            CryptoRsaKeyGenerateT(
                SAA_inout    bl::cmdline::CommandBase*    parent,
                SAA_in       const GlobalOptions&         globalOptions
                )
                :
                base_type( parent, globalOptions, "rsakeygenerate", "bl-tool @FULLNAME@ [options]" )
            {
                addOption( m_encrypt );
                addOption( m_password );
                addOption( m_pemFormat );
                addOption( m_path );

                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Generates an RSA key pair into the specified file format (JOSE or PEM).\n"
                    "The default key format is JOSE. If PEM format is requested then the key can\n"
                    "also be encrypted to a password (if provided).\n"
                    "\n"
                    "Usage: @CAPTION@\n"
                    "\n"
                    "Options:\n"
                    "@OPTIONS@\n"
                    );
            }

            virtual bl::cmdline::Result execute() OVERRIDE
            {
                using namespace bl;
                using namespace bl::crypto;
                using namespace bl::security;

                const auto rsaKey = RsaKey::createInstance();
                rsaKey -> generate();

                const auto keyAsText = getKeyAsText(
                    "Encryption password for the generated RSA key" /* prompt */,
                    rsaKey,
                    m_pemFormat,
                    m_encrypt,
                    m_password
                    );

                writeKey( m_path, keyAsText, "generated" /* verb */ );

                return 0;
            }
        };

        typedef CryptoRsaKeyGenerateT<> CryptoRsaKeyGenerate;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYGENERATE_H_ */
