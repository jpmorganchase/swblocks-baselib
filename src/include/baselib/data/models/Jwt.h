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

#ifndef __BL_DATA_MODELS_JWT_H_
#define __BL_DATA_MODELS_JWT_H_

#include <baselib/data/DataModelObjectDefs.h>

namespace bl
{
    namespace dm
    {
        namespace jwt
        {
            BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( TokenTypeValues, "TokenTypeValues",
                ( ( Default, "JWT" ) )
                ( ( Alternative, "urn:ietf:params:oauth:token-type:jwt" ) )
                )

            /*
             * @brief Class SafeToReplicateCoreClaimsSet
             */

            BL_DM_DEFINE_CLASS_BEGIN( SafeToReplicateCoreClaimsSet )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( issuer, "iss" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( subject, "sub" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( audience, "aud" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( issuer )
                    BL_DM_IMPL_PROPERTY( subject )
                    BL_DM_IMPL_PROPERTY( audience )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( SafeToReplicateCoreClaimsSet )

            BL_DM_DEFINE_PROPERTY( SafeToReplicateCoreClaimsSet, issuer )
            BL_DM_DEFINE_PROPERTY( SafeToReplicateCoreClaimsSet, subject )
            BL_DM_DEFINE_PROPERTY( SafeToReplicateCoreClaimsSet, audience )

            /*
             * @brief Class CoreClaimsSet
             */

            BL_DM_DEFINE_CLASS_BEGIN( CoreClaimsSet )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( issuer, "iss" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( subject, "sub" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( audience, "aud" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( expiresAt, "exp" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( notBefore, "nbf" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( issuedAt, "iat" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( tokenId, "jti" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( issuer )
                    BL_DM_IMPL_PROPERTY( subject )
                    BL_DM_IMPL_PROPERTY( audience )
                    BL_DM_IMPL_PROPERTY( expiresAt )
                    BL_DM_IMPL_PROPERTY( notBefore )
                    BL_DM_IMPL_PROPERTY( issuedAt )
                    BL_DM_IMPL_PROPERTY( tokenId )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( CoreClaimsSet )

            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, issuer )
            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, subject )
            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, audience )
            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, expiresAt )
            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, notBefore )
            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, issuedAt )
            BL_DM_DEFINE_PROPERTY( CoreClaimsSet, tokenId )

            /*
             * @brief Class WellKnownClaimsSet
             */

            BL_DM_DEFINE_CLASS_BEGIN( WellKnownClaimsSet )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( issuer, "iss" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( subject, "sub" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( audience, "aud" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( expiresAt, "exp" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( notBefore, "nbf" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( issuedAt, "iat" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( tokenId, "jti" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( accountTypeId, "accountTypeId" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( actor, "actort" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( birthDate, "birthdate" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( email, "email" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( familyName, "family_name" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( gender, "gender" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( givenName, "given_name" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( nameId, "nameid" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( principal, "prn" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( uniqueName, "unique_name" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( website, "website" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( issuer )
                    BL_DM_IMPL_PROPERTY( subject )
                    BL_DM_IMPL_PROPERTY( audience )
                    BL_DM_IMPL_PROPERTY( expiresAt )
                    BL_DM_IMPL_PROPERTY( notBefore )
                    BL_DM_IMPL_PROPERTY( issuedAt )
                    BL_DM_IMPL_PROPERTY( tokenId )
                    BL_DM_IMPL_PROPERTY( accountTypeId )
                    BL_DM_IMPL_PROPERTY( actor )
                    BL_DM_IMPL_PROPERTY( birthDate )
                    BL_DM_IMPL_PROPERTY( email )
                    BL_DM_IMPL_PROPERTY( familyName )
                    BL_DM_IMPL_PROPERTY( gender )
                    BL_DM_IMPL_PROPERTY( givenName )
                    BL_DM_IMPL_PROPERTY( nameId )
                    BL_DM_IMPL_PROPERTY( principal )
                    BL_DM_IMPL_PROPERTY( uniqueName )
                    BL_DM_IMPL_PROPERTY( website )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( WellKnownClaimsSet )

            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, issuer )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, subject )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, audience )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, expiresAt )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, notBefore )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, issuedAt )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, tokenId )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, accountTypeId )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, actor )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, birthDate )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, email )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, familyName )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, gender )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, givenName )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, nameId )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, principal )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, uniqueName )
            BL_DM_DEFINE_PROPERTY( WellKnownClaimsSet, website )

            /*
             * @brief Class ClaimsSetBase
             */

            BL_DM_DEFINE_CLASS_BEGIN( ClaimsSetBase )

                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( issuer, "iss" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( subject, "sub" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( audience, "aud" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( expiresAt, "exp" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( notBefore, "nbf" )
                BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( issuedAt, "iat" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( tokenId, "jti" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( accountTypeId, "accountTypeId" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( actor, "actort" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( birthDate, "birthdate" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( email, "email" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( familyName, "family_name" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( gender, "gender" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( givenName, "given_name" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( nameId, "nameid" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( principal, "prn" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( uniqueName, "unique_name" )
                BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( website, "website" )

                BL_DM_PROPERTIES_IMPL_BEGIN()
                    BL_DM_IMPL_PROPERTY( issuer )
                    BL_DM_IMPL_PROPERTY( subject )
                    BL_DM_IMPL_PROPERTY( audience )
                    BL_DM_IMPL_PROPERTY( expiresAt )
                    BL_DM_IMPL_PROPERTY( notBefore )
                    BL_DM_IMPL_PROPERTY( issuedAt )
                    BL_DM_IMPL_PROPERTY( tokenId )
                    BL_DM_IMPL_PROPERTY( accountTypeId )
                    BL_DM_IMPL_PROPERTY( actor )
                    BL_DM_IMPL_PROPERTY( birthDate )
                    BL_DM_IMPL_PROPERTY( email )
                    BL_DM_IMPL_PROPERTY( familyName )
                    BL_DM_IMPL_PROPERTY( gender )
                    BL_DM_IMPL_PROPERTY( givenName )
                    BL_DM_IMPL_PROPERTY( nameId )
                    BL_DM_IMPL_PROPERTY( principal )
                    BL_DM_IMPL_PROPERTY( uniqueName )
                    BL_DM_IMPL_PROPERTY( website )
                BL_DM_PROPERTIES_IMPL_END()

            BL_DM_DEFINE_CLASS_END( ClaimsSetBase )

            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, issuer )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, subject )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, audience )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, expiresAt )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, notBefore )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, issuedAt )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, tokenId )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, accountTypeId )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, actor )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, birthDate )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, email )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, familyName )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, gender )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, givenName )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, nameId )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, principal )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, uniqueName )
            BL_DM_DEFINE_PROPERTY( ClaimsSetBase, website )

        } // jwt

    } // dm

} // bl

#endif /* __BL_DATA_MODELS_JWT_H_ */
