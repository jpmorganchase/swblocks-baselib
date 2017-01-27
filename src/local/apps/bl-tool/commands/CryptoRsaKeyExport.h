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

#ifndef __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYEXPORT_H_
#define __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYEXPORT_H_

#include <apps/bl-tool/commands/CryptoRsaKeyCommandBase.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class CryptoRsaKeyExport
         */

        template
        <
            typename E = void
        >
        class CryptoRsaKeyExportT : public CryptoRsaKeyCommandBase
        {
            BL_NO_COPY_OR_MOVE( CryptoRsaKeyExportT )

        protected:

            typedef CryptoRsaKeyCommandBase               base_type;

            BL_CMDLINE_OPTION(
                m_encrypt,
                BoolSwitch,
                "encrypt",
                "If specified the exported RSA key will be encrypted"
                )

            BL_CMDLINE_OPTION(
                m_decrypt,
                BoolSwitch,
                "decrypt",
                "If specified the the source RSA key needs to be decrypted"
                )

            BL_CMDLINE_OPTION(
                m_sourcePassword,
                StringOption,
                "sourcepassword",
                "The password to use for decryption of the input RSA key"
                )

            BL_CMDLINE_OPTION(
                m_destinationPassword,
                StringOption,
                "destinationpassword",
                "The password to use for encryption of the output RSA key"
                )

            BL_CMDLINE_OPTION(
                m_sourcePemFormat,
                BoolSwitch,
                "sourcepemformat",
                "If specified we are exporting from a PEM key type format"
                )

            BL_CMDLINE_OPTION(
                m_destinationPemFormat,
                BoolSwitch,
                "destinationpemformat",
                "If specified the exported RSA key will be in PEM key type format"
                )

            BL_CMDLINE_OPTION(
                m_sourcePath,
                StringOption,
                "sourcepath",
                "A file path where the input RSA key should be read from",
                bl::cmdline::Required
                )

            BL_CMDLINE_OPTION(
                m_destinationPath,
                StringOption,
                "destinationpath",
                "A file path to where the output RSA key should be written"
                )

            BL_CMDLINE_OPTION(
                m_publicKeyOnly,
                BoolSwitch,
                "publickeyonly",
                "If specified only the public key data will be exported"
                )

        public:

            CryptoRsaKeyExportT(
                SAA_inout    bl::cmdline::CommandBase*    parent,
                SAA_in       const GlobalOptions&         globalOptions
                )
                :
                base_type( parent, globalOptions, "rsakeyexport", "bl-tool @FULLNAME@ [options]" )
            {
                addOption( m_encrypt );
                addOption( m_decrypt );
                addOption( m_sourcePassword );
                addOption( m_destinationPassword );
                addOption( m_sourcePemFormat );
                addOption( m_destinationPemFormat );
                addOption( m_sourcePath );
                addOption( m_destinationPath );
                addOption( m_publicKeyOnly );

                //  |0123456789>123456789>123456789>123456789>123456789>123456789>123456789>123456789|
                setHelpMessage(
                    "Converts an RSA key pair from one type to another (or exports a public key from\n"
                    "an RSA key pair).\n"
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

                om::ObjPtr< RsaKey > rsaSourceKey;

                {
                    /*
                     * First we read the source key as text
                     *
                     * Normalize the path to handle relative paths
                     */

                    const auto keyAsText = encoding::readTextFile( fs::normalize( m_sourcePath.getValue() ) );

                    if( m_sourcePemFormat.getValue() )
                    {
                        /*
                         * The source key is in PEM format (text)
                         */

                        const std::string password =
                            m_sourcePassword.hasValue() ?
                                m_sourcePassword.getValue() :
                                ( m_decrypt.hasValue() ?
                                    os::readPasswordFromInput( "Decryption password for source RSA key" /* prompt */ ) : bl::str::empty() );

                        if( m_decrypt.hasValue() && password.empty() )
                        {
                            BL_THROW_USER(
                                BL_MSG()
                                    << "If decryption was requested --sourcepassword parameter cannot be empty"
                                );
                        }

                        rsaSourceKey = JsonSecuritySerialization::loadPrivateKeyFromPemString( keyAsText, password );
                    }
                    else
                    {
                        /*
                         * The source key is assumed to be in JOSE format (JSON)
                         */

                        if( m_sourcePassword.hasValue() || m_decrypt.hasValue() )
                        {
                            BL_THROW_USER(
                                BL_MSG()
                                    << "Decryption with a password is not supported for JOSE type keys"
                                );
                        }

                        rsaSourceKey = JsonSecuritySerialization::loadPrivateKeyFromJsonString( keyAsText );
                    }
                }

                /*
                 * At this point the input key has been read
                 *
                 * Let's generate the output key now
                 */

                if( m_publicKeyOnly.hasValue() && ( m_destinationPassword.hasValue() || m_encrypt.hasValue() ) )
                {
                    BL_THROW_USER(
                        BL_MSG()
                            << "Encryption to a password is not supported for public keys"
                        );
                }

                {
                    std::string keyAsText;

                    if( m_publicKeyOnly.hasValue() )
                    {
                        /*
                         * The key was requested to be exported as a public key
                         */

                        const auto publicKey = JsonSecuritySerialization::getPublicKeyAsJsonObject( rsaSourceKey );

                        keyAsText = m_destinationPemFormat.getValue() ?
                            JsonSecuritySerialization::getPublicKeyAsPemString(
                                JsonSecuritySerialization::loadPublicKeyFromJsonObject( publicKey )
                                ) :
                            BL_DM_GET_AS_PRETTY_JSON_STRING( publicKey );
                    }
                    else
                    {
                        /*
                         * The key was requested to be exported as a private key
                         */

                        keyAsText = getKeyAsText(
                            "Encryption password for exported RSA key" /* prompt */,
                            rsaSourceKey,
                            m_destinationPemFormat,
                            m_encrypt,
                            m_destinationPassword
                            );
                    }

                    writeKey( m_destinationPath, keyAsText, "exported" /* verb */ );
                }

                return 0;
            }
        };

        typedef CryptoRsaKeyExportT<> CryptoRsaKeyExport;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYEXPORT_H_ */
