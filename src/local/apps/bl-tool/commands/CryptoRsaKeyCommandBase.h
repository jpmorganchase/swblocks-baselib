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

#ifndef __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYCOMMANDBASE_H_
#define __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYCOMMANDBASE_H_

#include <baselib/security/JsonSecuritySerialization.h>

#include <apps/bl-tool/GlobalOptions.h>

#include <baselib/cmdline/CommandBase.h>

#include <baselib/crypto/RsaKey.h>

#include <baselib/core/FileEncoding.h>
#include <baselib/core/SecureStringWrapper.h>
#include <baselib/core/BaseIncludes.h>

namespace bltool
{
    namespace commands
    {
        /**
         * @brief class CryptoRsaKeyCommandBase
         */

        template
        <
            typename E = void
        >
        class CryptoRsaKeyCommandBaseT : public bl::cmdline::CommandBase
        {
            BL_NO_COPY_OR_MOVE( CryptoRsaKeyCommandBaseT )

        protected:

            typedef bl::cmdline::CommandBase              base_type;

            const GlobalOptions& m_globalOptions;

            static void writeKey(
                SAA_in              const bl::cmdline::StringOption&                        path,
                SAA_in              const std::string&                                      keyAsText,
                SAA_in              const std::string&                                      verb
                )
            {
                using namespace bl;
                using namespace bl::crypto;
                using namespace bl::security;

                if( path.hasValue() )
                {
                    /*
                     * The key is expected to go to the specified file
                     */

                    const auto filePath = fs::normalize( path.getValue() );

                    encoding::writeTextFile(
                        filePath,
                        keyAsText,
                        encoding::TextFileEncoding::Ascii
                        );

                    BL_LOG(
                        Logging::notify(),
                        BL_MSG()
                            << "RSA key was "
                            << verb
                            << " and saved in "
                            << fs::normalizePathParameterForPrint( filePath )
                        );
                }
                else
                {
                    /*
                     * The key is expected to go to STDOUT
                     */

                    BL_LOG(
                        Logging::notify(),
                        keyAsText
                        );
                }
            }

            static std::string getKeyAsText(
                SAA_in              const std::string&                                      prompt,
                SAA_in              const bl::om::ObjPtr< bl::crypto::RsaKey >&             rsaKey,
                SAA_in              const bl::cmdline::BoolSwitch&                          pemFormat,
                SAA_in              const bl::cmdline::BoolSwitch&                          encrypt,
                SAA_in              const bl::cmdline::StringOption&                        password
                )
            {
                using namespace bl;
                using namespace bl::crypto;
                using namespace bl::security;

                if( pemFormat.getValue() )
                {
                    str::SecureStringWrapper passwordResolved;

                    if( password.hasValue() )
                    {
                        passwordResolved.append( password.getValue() );
                    }
                    else if( encrypt.hasValue() )
                    {
                        passwordResolved = os::readPasswordFromInput( prompt );
                    }

                    if( encrypt.hasValue() && passwordResolved.empty() )
                    {
                        BL_THROW_USER(
                            BL_MSG()
                                << "If encryption was requested password cannot be empty"
                            );
                    }

                    return JsonSecuritySerialization::getPrivateKeyAsPemString(
                        rsaKey,
                        passwordResolved.getAsNonSecureString()
                        );
                }
                else
                {
                    if( password.hasValue() || encrypt.hasValue() )
                    {
                        BL_THROW_USER(
                            BL_MSG()
                                << "Encryption to a password is not supported for JOSE type keys"
                            );
                    }

                    return JsonSecuritySerialization::getPrivateKeyAsJsonString( rsaKey );
                }
            }

        public:

            CryptoRsaKeyCommandBaseT(
                SAA_inout           bl::cmdline::CommandBase*       parent,
                SAA_in              const GlobalOptions&            globalOptions,
                SAA_inout           std::string&&                   commandName,
                SAA_inout_opt       std::string&&                   helpCaption = ""

                )
                :
                base_type( parent, BL_PARAM_FWD( commandName ), BL_PARAM_FWD( helpCaption ) ),
                m_globalOptions( globalOptions )
            {
            }
        };

        typedef CryptoRsaKeyCommandBaseT<> CryptoRsaKeyCommandBase;

    } // commands

} // bltool

#endif /* __APPS_BLTOOL_COMMANDS_CRYPTORSAKEYCOMMANDBASE_H_ */
