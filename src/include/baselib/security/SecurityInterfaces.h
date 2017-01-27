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

#ifndef __BL_SECURITY_SECURITYINTERFACES_H_
#define __BL_SECURITY_SECURITYINTERFACES_H_

#include <baselib/data/DataBlock.h>

#include <baselib/tasks/Task.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>

#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( AuthorizationCache, "0cd2e7f0-c2cd-4396-acac-680ca8ce581a" )

namespace bl
{
    namespace security
    {
        /**
         * @brief The security principal data object
         */

        template
        <
            typename E = void
        >
        class SecurityPrincipalT : public om::ObjectDefaultBase
        {
        protected:

            const std::string                                                       m_secureIdentity;
            const std::string                                                       m_givenName;
            const std::string                                                       m_familyName;
            const std::string                                                       m_email;
            const std::string                                                       m_typeId;
            const om::ObjPtr< data::DataBlock >                                     m_authenticationToken;

            SecurityPrincipalT(
                SAA_in              std::string&&                                   secureIdentity,
                SAA_in              std::string&&                                   givenName,
                SAA_in              std::string&&                                   familyName,
                SAA_in              std::string&&                                   email,
                SAA_in              std::string&&                                   typeId,
                SAA_in              om::ObjPtr< data::DataBlock >&&                 authenticationToken
                ) NOEXCEPT
                :
                m_secureIdentity( BL_PARAM_FWD( secureIdentity ) ),
                m_givenName( BL_PARAM_FWD( givenName ) ),
                m_familyName( BL_PARAM_FWD( familyName ) ),
                m_email( BL_PARAM_FWD( email ) ),
                m_typeId( BL_PARAM_FWD( typeId ) ),
                m_authenticationToken( BL_PARAM_FWD( authenticationToken ) )
            {
            }

        public:

            auto secureIdentity() const NOEXCEPT -> const std::string&
            {
                return m_secureIdentity;
            }

            auto givenName() const NOEXCEPT -> const std::string&
            {
                return m_givenName;
            }

            auto familyName() const NOEXCEPT -> const std::string&
            {
                return m_familyName;
            }

            auto email() const NOEXCEPT -> const std::string&
            {
                return m_email;
            }

            auto typeId() const NOEXCEPT -> const std::string&
            {
                return m_typeId;
            }

            auto authenticationToken() const NOEXCEPT -> const om::ObjPtr< data::DataBlock >&
            {
                return m_authenticationToken;
            }
        };

        typedef om::ObjectImpl< SecurityPrincipalT<> > SecurityPrincipal;

        /**
         * @brief The authorization cache interface
         */

        class AuthorizationCache : public om::Object
        {
            BL_DECLARE_INTERFACE( AuthorizationCache )

        public:

            /**
             * @brief Just a small helper to create an authentication token data block from a string buffer
             */

            inline static auto createAuthenticationToken( SAA_in const std::string& tokenAsString )
                -> om::ObjPtr< data::DataBlock >
            {
                auto authenticationToken = data::DataBlock::createInstance( tokenAsString.size() /* capacity */ );

                authenticationToken -> setSize( tokenAsString.size() );

                std::copy_n( tokenAsString.c_str(), tokenAsString.size(), authenticationToken -> begin() );

                return authenticationToken;
            }

            /**
             * @brief Calls tryGetAuthorizedPrinciplal( ... ) and if the token is not in the cache then
             * it calls tryUpdate( ... ) synchronously
             */

            inline auto tryGetAuthorizedPrinciplalOrUpdate(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken
                )
                -> om::ObjPtr< SecurityPrincipal >
            {
                auto principal = tryGetAuthorizedPrinciplal( authenticationToken );

                if( ! principal )
                {
                    principal = tryUpdate( authenticationToken );
                }

                return principal;
            }

            /**
             * @brief Calls tryGetAuthorizedPrinciplal( ... ) and if the token is not in the cache then
             * it calls update( ... ) synchronously
             */

            inline auto getAuthorizedPrinciplalOrUpdate(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken
                )
                -> om::ObjPtr< SecurityPrincipal >
            {
                auto principal = tryGetAuthorizedPrinciplal( authenticationToken );

                if( ! principal )
                {
                    principal = update( authenticationToken );
                }

                return principal;
            }

            /**
             * @brief Returns the expected authentication token type
             */

            virtual auto tokenType() const NOEXCEPT -> const std::string& = 0;

            /**
             * @brief Configure the freshness interval - i.e. how long an authorization result will be considered valid
             * before hitting the authorization service again
             *
             * If special value is provided (e.g. time::neg_infin) then it configures an implementation specific default
             */

            virtual void configureFreshnessInterval(
                SAA_in_opt          const time::time_duration&                      freshnessInterval = time::neg_infin
                ) = 0;

            /**
             * @brief Attempts to perform authorization locally within the cache
             *
             * If the authentication token does not exist in the cache or the cache is not fresh enough it will return nullptr
             * to indicate that update is necessary (via createAuthorizationTask / tryUpdate / update APIs)
             */

            virtual auto tryGetAuthorizedPrinciplal(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken
                )
                -> om::ObjPtr< SecurityPrincipal > = 0;

            /**
             * @brief This to create the authorization task which when executed will perform authorization for the specified
             * security principal
             *
             * This API is necessary to enable asynchronous authorization execution and full control of when and how the
             * authorization operation is executed by the caller
             */

            virtual auto createAuthorizationTask(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken
                )
                -> om::ObjPtr< tasks::Task > = 0;

            /**
             * @brief This is called after the authorization task is executed to update the cache and obtain the freshly
             * authorized security principal
             *
             * If the authorization was not successful for some reason (typically the authentication token is invalid or
             * it has expired) then this API returns nullptr to indicate this case
             *
             * If authorizationTask parameter was not provided then it will be created internally and executed synchronously
             * by the API, but if it is provided then it needs to be a task created by createAuthorizationTask API and also
             * it needs to be in executed state
             */

            virtual auto tryUpdate(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken,
                SAA_in_opt          const om::ObjPtr< tasks::Task >&                authorizationTask = nullptr
                )
                -> om::ObjPtr< SecurityPrincipal > = 0;

            /**
             * @brief This is called after the authorization task is executed to update the cache and obtain the freshly
             * authorized security principal
             *
             * If the authorization was not successful for some reason (typically the authentication token is invalid or
             * it has expired) then this API will throw SecurityException
             *
             * If authorizationTask parameter was not provided then it will be created internally and executed synchronously
             * by the API, but if it is provided then it needs to be a task created by createAuthorizationTask API and also
             * it needs to be in executed state
             */

            virtual auto update(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken,
                SAA_in_opt          const om::ObjPtr< tasks::Task >&                authorizationTask = nullptr
                )
                -> om::ObjPtr< SecurityPrincipal > = 0;

            /**
             * @brief Evicts the authorization info from the cache (if any)
             */

            virtual void evict(
                SAA_in              const om::ObjPtr< data::DataBlock >&            authenticationToken
                ) = 0;
        };

    } // security

} // bl

#endif /* __BL_SECURITY_SECURITYINTERFACES_H_ */
