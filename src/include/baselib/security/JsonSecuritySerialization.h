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

#ifndef __BL_SECURITY_JSONSECURITYSERIALIZATION_H_
#define __BL_SECURITY_JSONSECURITYSERIALIZATION_H_

#include <baselib/data/models/Jose.h>

#include <baselib/security/JsonSecuritySerializationImpl.h>

#include <baselib/crypto/RsaKey.h>

#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace security
    {
        namespace detail
        {
            class JoseTypesPolicy
            {
                BL_DECLARE_STATIC( JoseTypesPolicy )

            public:

                typedef bl::dm::jose::RsaPublicKey                                  RsaPublicKey;
                typedef bl::dm::jose::RsaPrivateKey                                 RsaPrivateKey;
                typedef bl::dm::jose::KeyType                                       KeyType;
                typedef bl::dm::jose::SigningAlgorithm                              SigningAlgorithm;
                typedef bl::dm::jose::PublicKeyUse                                  PublicKeyUse;

            };
        }

        template
        <
            typename E = void
        >
        class JsonSecuritySerializationT : public JsonSecuritySerializationImpl< detail::JoseTypesPolicy >
        {
            BL_DECLARE_STATIC( JsonSecuritySerializationT )
        };

        typedef JsonSecuritySerializationT<> JsonSecuritySerialization;

    } // security

} // bl

#endif /* __BL_SECURITY_JSONSECURITYSERIALIZATION_H_ */
