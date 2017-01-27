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

#include <utests/baselib/TestAuthorizationCacheImplUtils.h>

namespace utest
{
    namespace security
    {
        template
        <
            typename E = void
        >
        class TestAuthorizationServiceImplT : public bl::om::ObjectDefaultBase
        {
            BL_CTR_DEFAULT( TestAuthorizationServiceImplT, protected )

        private:

            static const std::string                            g_tokenType;
            static const std::string                            g_tokenUniqueSeedSeparator;

        public:

            typedef bl::tasks::SimpleHttpSslGetTaskImpl         task_impl_t;

            auto getTokenType() NOEXCEPT -> const std::string&
            {
                return g_tokenType;
            }

            auto createAuthorizationTask( SAA_in const bl::om::ObjPtr< bl::data::DataBlock >& authenticationToken )
                -> bl::om::ObjPtr< bl::tasks::Task >
            {
                using namespace bl;
                using namespace bl::tasks;
                using namespace utest::http;

                typename task_impl_t::HeadersMap headers;

                headers[ task_impl_t::HttpHeader::g_contentType ] = task_impl_t::HttpHeader::g_contentTypeDefault;

                const std::string tokenData( authenticationToken -> begin(), authenticationToken -> end() );

                const auto pos = tokenData.find( g_tokenUniqueSeedSeparator );

                auto taskImpl = task_impl_t::createInstance(
                    cpp::copy( test::UtfArgsParser::host() ),
                    test::UtfArgsParser::port(),
                    pos == std::string::npos ?
                        tokenData : tokenData.substr( pos + g_tokenUniqueSeedSeparator.size() ),
                    uuids::uuid2string( uuids::create() ) /* content */,
                    std::move( headers )
                );

                /*
                 * Ensure that sensitive secure information is omitted
                 */

                taskImpl -> isSecureMode( true );

                return om::moveAs< Task >( taskImpl );
            }

            auto extractSecurityPrincipal( SAA_in const bl::om::ObjPtr< bl::tasks::Task >& executedAuthorizationTask )
                -> bl::om::ObjPtr< bl::security::SecurityPrincipal >
            {
                using namespace bl;
                using namespace bl::tasks;
                using namespace bl::security;

                BL_ASSERT(
                    Task::Created != executedAuthorizationTask -> getState() &&
                    Task::Running != executedAuthorizationTask -> getState()
                    );

                BL_ASSERT( ! executedAuthorizationTask -> isFailed() );

                const auto taskImpl = om::qi< task_impl_t >( executedAuthorizationTask );

                const auto response = taskImpl -> getResponse();

                auto authenticationToken =
                    AuthorizationCache::createAuthenticationToken(
                        resolveMessage(
                            BL_MSG()
                                << "{"
                                << taskImpl -> getContent()
                                << "}"
                                << g_tokenUniqueSeedSeparator
                                << taskImpl -> getPath()
                            )
                        );

                return bl::security::SecurityPrincipal::createInstance(
                    cpp::copy( taskImpl -> getResponse() )          /* secureIdentity */,
                    "givenName"                                     /*givenName*/,
                    "familyName"                                    /* familyName */,
                    "user@host.com"                                 /* email */,
                    "type_0"                                        /* typeId */,
                    std::move( authenticationToken )
                    );
            }
        };

        BL_DEFINE_STATIC_CONST_STRING( TestAuthorizationServiceImplT, g_tokenType ) = "dummyTokenType";
        BL_DEFINE_STATIC_CONST_STRING( TestAuthorizationServiceImplT, g_tokenUniqueSeedSeparator ) = "::::";

        typedef bl::om::ObjectImpl< TestAuthorizationServiceImplT<> > TestAuthorizationServiceImpl;

    } // security

} // utest

UTF_AUTO_TEST_CASE( AuthorizationCacheImplBasicTests )
{
    using namespace utest;
    using namespace utest::http;
    using namespace utest::security;

    TestAuthorizationCacheImplUtils::basicTests< TestAuthorizationServiceImpl /* SERVICE */ >(
        g_requestUri            /* validAuthenticationTokenData1 */,
        g_desiredResult         /* expectedSecureIdentity1 */
        );
}

UTF_AUTO_TEST_CASE( AuthorizationCacheImplFullTests )
{
    using namespace utest;
    using namespace utest::http;
    using namespace utest::security;

    TestAuthorizationCacheImplUtils::fullTests< TestAuthorizationServiceImpl /* SERVICE */ >(
        g_requestUri            /* validAuthenticationTokenData1 */,
        g_requestPerfUri        /* validAuthenticationTokenData2 */,
        g_notFoundUri           /* invalidAuthenticationTokenData */,
        g_desiredResult         /* expectedSecureIdentity1 */,
        g_desiredPerfResult     /* expectedSecureIdentity2 */
        );
}

