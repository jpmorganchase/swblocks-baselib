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

#ifndef __UTESTS_BASELIB_TESTAUTHORIZATIONCACHEIMPLUTILS_H_
#define __UTESTS_BASELIB_TESTAUTHORIZATIONCACHEIMPLUTILS_H_

#include <baselib/security/AuthorizationCacheImpl.h>

#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/HttpServerHelpers.h>
#include <utests/baselib/UtfArgsParser.h>
#include <utests/baselib/Utf.h>

namespace utest
{
    /**
     * @brief class TestAuthorizationCacheImplUtils - utility test code to test
     * AuthorizationCacheImpl class instantiations
     */

    template
    <
        typename E = void
    >
    class TestAuthorizationCacheImplUtilsT
    {
        BL_DECLARE_STATIC( TestAuthorizationCacheImplUtilsT )

    public:

        typedef bl::httpserver::ServerBackendProcessing                         ServerBackendProcessing;

        template
        <
            typename SERVICE
        >
        static void basicTests(
            SAA_in              const std::string&                              validAuthenticationTokenData1,
            SAA_in              const std::string&                              expectedSecureIdentity1,
            SAA_in_opt          bl::om::ObjPtr< ServerBackendProcessing >&&     backend = nullptr,
            SAA_in_opt          bl::om::ObjPtr< SERVICE >&&                     authorizationService =
                SERVICE::template createInstance()
            )
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::security;

            typedef om::ObjectImpl< AuthorizationCacheImpl< SERVICE > > cache_t;

            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n***** Starting test: TestAuthorizationCacheImplUtilsT<>::basicTests() *****\n"
                    );

            utest::http::HttpServerHelpers::startHttpServerAndExecuteCallback< bl::httpserver::HttpSslServer >(
                [ & ]() -> void
                {
                    scheduleAndExecuteInParallel(
                        [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                        {
                            const auto cache = cache_t::template createInstance< AuthorizationCache >(
                                bl::om::copy( authorizationService )
                                );

                            UTF_REQUIRE_EQUAL( cache -> tokenType(), authorizationService -> getTokenType() );

                            const auto authenticationToken =
                                AuthorizationCache::createAuthenticationToken( validAuthenticationTokenData1 );

                            UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken ) );

                            const auto task = cache -> createAuthorizationTask( authenticationToken );
                            eq -> push_back( task );
                            eq -> wait( task );

                            const auto principal = cache -> update( authenticationToken, task );
                            UTF_REQUIRE( principal );
                            UTF_REQUIRE_EQUAL( principal -> secureIdentity(), expectedSecureIdentity1 );

                            const auto cachedPrincipal = cache -> tryGetAuthorizedPrinciplal( authenticationToken );
                            UTF_REQUIRE( cachedPrincipal );
                            UTF_REQUIRE_EQUAL( principal, cachedPrincipal );

                            cache -> evict( authenticationToken );
                            UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken ) );
                        });
                },
                BL_PARAM_FWD( backend )
                );
        }

        template
        <
            typename SERVICE
        >
        static void fullTests(
            SAA_in              const std::string&                              validAuthenticationTokenData1,
            SAA_in              const std::string&                              validAuthenticationTokenData2,
            SAA_in              const std::string&                              invalidAuthenticationTokenData,
            SAA_in              const std::string&                              expectedSecureIdentity1,
            SAA_in              const std::string&                              expectedSecureIdentity2,
            SAA_in_opt          bl::om::ObjPtr< ServerBackendProcessing >&&     backend = nullptr,
            SAA_in_opt          bl::om::ObjPtr< SERVICE >&&                     authorizationService =
                SERVICE::template createInstance()
            )
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::security;

            typedef om::ObjectImpl< AuthorizationCacheImpl< SERVICE > > cache_t;

            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n***** Starting test: TestAuthorizationCacheImplUtilsT<>::fullTests() *****\n"
                    );

            utest::http::HttpServerHelpers::startHttpServerAndExecuteCallback< bl::httpserver::HttpSslServer >(
                [ & ]() -> void
                {
                    typedef data::DataBlock DataBlock;

                    const auto createToken = [ & ]( SAA_in const std::string& tokenText ) -> om::ObjPtr< DataBlock >
                    {
                        return AuthorizationCache::createAuthenticationToken( tokenText );
                    };

                    const auto requireBlocksNotEqual = [](
                        SAA_in              const om::ObjPtr< DataBlock >&      block1,
                        SAA_in              const om::ObjPtr< DataBlock >&      block2
                        ) -> void
                    {
                        if( block1 -> size() == block2 -> size() )
                        {
                            UTF_REQUIRE( 0 != std::memcmp( block1 -> pv(), block2 -> pv(), block1 -> size() ) );
                        }
                    };

                    const auto requireBlocksEqual = [](
                        SAA_in              const om::ObjPtr< DataBlock >&      block1,
                        SAA_in              const om::ObjPtr< DataBlock >&      block2
                        ) -> void
                    {
                        UTF_REQUIRE_EQUAL( block1 -> size(), block2 -> size() );

                        UTF_REQUIRE( 0 == std::memcmp( block1 -> pv(), block2 -> pv(), block1 -> size() ) );
                    };

                    const auto authenticationToken1 = createToken( validAuthenticationTokenData1 /* tokenText */ );
                    const auto authenticationToken2 = createToken( validAuthenticationTokenData2 /* tokenText */ );

                    const auto invalidAuthenticationToken =
                        createToken( invalidAuthenticationTokenData /* tokenText */ );

                    requireBlocksNotEqual( authenticationToken1, authenticationToken2 );

                    const auto validatePrincipal = [ & ]( SAA_in const om::ObjPtr< SecurityPrincipal >& principal ) -> void
                    {
                        /*
                         * Ensure they are all non-empty and different values and then ensure the
                         * token is valid and the SID is the expected one
                         *
                         * We allow for the given name and the family name to be the same as they
                         * are in fact the same for our test account ("TEST USER")
                         */

                        UTF_REQUIRE( principal );

                        UTF_REQUIRE( ! principal -> secureIdentity().empty() );
                        UTF_REQUIRE( ! principal -> givenName().empty() );
                        UTF_REQUIRE( ! principal -> familyName().empty() );
                        UTF_REQUIRE( ! principal -> email().empty() );
                        UTF_REQUIRE( ! principal -> typeId().empty() );

                        UTF_REQUIRE( principal -> secureIdentity() != principal -> givenName() );
                        UTF_REQUIRE( principal -> givenName() != principal -> email() );
                        UTF_REQUIRE( principal -> email() != principal -> typeId() );

                        UTF_REQUIRE(
                            principal -> secureIdentity() == expectedSecureIdentity1 ||
                            principal -> secureIdentity() == expectedSecureIdentity2
                            );

                        UTF_REQUIRE( principal -> authenticationToken() );
                        UTF_REQUIRE( principal -> authenticationToken() -> pv() );
                        UTF_REQUIRE( principal -> authenticationToken() -> size() );
                    };

                    const auto printPrincipal = [ & ]( SAA_in const om::ObjPtr< SecurityPrincipal >& principal ) -> void
                    {
                        validatePrincipal( principal );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "\nSID: "
                                << principal -> secureIdentity()
                                << "\nFirst: "
                                << principal -> givenName()
                                << "\nLast: "
                                << principal -> familyName()
                                << "\nEmail: "
                                << principal -> email()
                                << "\nTypeId: "
                                << principal -> typeId()
                            );
                    };

                    const auto cache = cache_t::template createInstance< AuthorizationCache >(
                        bl::om::copy( authorizationService )
                        );

                    UTF_REQUIRE_EQUAL( cache -> tokenType(), authorizationService -> getTokenType() );

                    /*
                     * Basic caching logic tests and validation of the returned data from the APIs
                     */

                    UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );

                    validatePrincipal( cache -> tryUpdate( authenticationToken1 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );
                    validatePrincipal( cache -> tryUpdate( authenticationToken1 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );

                    validatePrincipal( cache -> tryGetAuthorizedPrinciplalOrUpdate( authenticationToken2 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );

                    cache -> evict( authenticationToken1 );
                    cache -> evict( authenticationToken2 );

                    UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );

                    validatePrincipal( cache -> update( authenticationToken1 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );

                    validatePrincipal( cache -> getAuthorizedPrinciplalOrUpdate( authenticationToken2 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    validatePrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );

                    printPrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                    printPrincipal( cache -> tryGetAuthorizedPrinciplal( authenticationToken2 ) );

                    {
                        /*
                         * Test the caching logic - i.e. when in the cache the returned principal is the same
                         */

                        const auto principal1 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        const auto principal2 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );

                        UTF_REQUIRE( om::areEqual( principal1, principal2 ) );
                    }

                    {
                        /*
                         * Test the eviction logic - i.e. after the eviction tryUpdate will re-authorize
                         */

                        const auto principal1 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        cache -> evict( authenticationToken1 );
                        const auto principal2 = cache -> tryUpdate( authenticationToken1 );

                        UTF_REQUIRE( ! om::areEqual( principal1, principal2 ) );
                        requireBlocksNotEqual( principal1 -> authenticationToken(), principal2 -> authenticationToken() );
                    }

                    {
                        /*
                         * Test the freshness interval logic
                         */

                        const auto principal1 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        validatePrincipal( principal1 );
                        cache -> configureFreshnessInterval( time::seconds( 1 ) );
                        os::sleep( time::seconds( 2L ) );
                        UTF_REQUIRE( ! cache -> tryGetAuthorizedPrinciplal( authenticationToken1 ) );
                        const auto principal2 = cache -> getAuthorizedPrinciplalOrUpdate( authenticationToken1 );
                        validatePrincipal( principal2 );

                        UTF_REQUIRE( ! om::areEqual( principal1, principal2 ) );
                        requireBlocksNotEqual( principal1 -> authenticationToken(), principal2 -> authenticationToken() );

                        cache -> configureFreshnessInterval( time::minutes( 15 ) );
                    }

                    {
                        /*
                         * Test the createAuthorizationTask logic
                         *
                         * Verify that the task being executed invariant is enforced
                         */

                        auto principal1 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        validatePrincipal( principal1 );

                        const auto authorizationTask = cache -> createAuthorizationTask( authenticationToken1 );

                        UTF_REQUIRE_THROW_MESSAGE(
                            cache -> update( authenticationToken1, authorizationTask ),
                            UnexpectedException,
                            "The provided authorization task is not ready as it has not been executed"
                            );

                        tasks::scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                            {
                                /*
                                 * Note that the authorization task can fail and the failure
                                 * case will be handled from the cache -> update() call
                                 */

                                eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                                eq -> push_back( authorizationTask );
                            }
                            );

                        auto principal2 = cache -> update( authenticationToken1, authorizationTask );
                        validatePrincipal( principal2 );

                        UTF_REQUIRE( ! om::areEqual( principal1, principal2 ) );
                        requireBlocksNotEqual( principal1 -> authenticationToken(), principal2 -> authenticationToken() );

                        principal1 = cache -> update( authenticationToken1, authorizationTask );
                        validatePrincipal( principal1 );

                        /*
                         * The principal blocks will be different, but the authentication blocks will be the same
                         */

                        UTF_REQUIRE( ! om::areEqual( principal1, principal2 ) );
                        requireBlocksEqual( principal1 -> authenticationToken(), principal2 -> authenticationToken() );

                        /*
                         * Test the tryUpdate vs. update logic when we set the task artificially to failed and
                         * then also for the case where the Janus response will be failed (we can simulate failure
                         * by sending empty tickets)
                         */

                        typedef typename SERVICE::task_impl_t  task_impl_t;

                        const auto taskImpl = om::qi< task_impl_t >( authorizationTask );

                        taskImpl -> exception(
                            BL_MAKE_EXCEPTION_PTR(
                                UnexpectedException(),
                                BL_MSG()
                                    << "HTTP task has failed unexpectedly"
                                )
                            );

                        UTF_REQUIRE( nullptr == cache -> tryUpdate( authenticationToken1, authorizationTask ) );

                        UTF_REQUIRE_THROW_MESSAGE(
                            cache -> update( authenticationToken1, authorizationTask ),
                            SecurityException,
                            "Authorization request to the authorization service has failed"
                            );

                        /*
                         * Verify that the above failed attempts did not modify the internal state of the cache
                         */

                        principal2 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        validatePrincipal( principal2 );

                        UTF_REQUIRE_EQUAL( principal1, principal2 );

                        /*
                         * Try the invalid cookies case
                         */

                        utils::tryCatchLog(
                            "Expected security exception",
                            [ & ]() -> void
                            {
                                cache -> update( invalidAuthenticationToken );
                            },
                            cpp::void_callback_t(),
                            utils::LogFlags::DEBUG_ONLY
                            );

                        UTF_REQUIRE_THROW_MESSAGE(
                            cache -> update( invalidAuthenticationToken ),
                            SecurityException,
                            "Authorization request to the authorization service has failed"
                            );

                        principal1 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        validatePrincipal( principal1 );

                        UTF_REQUIRE_EQUAL( principal1, principal2 );

                        cache -> update( authenticationToken1 );

                        principal2 = cache -> tryGetAuthorizedPrinciplal( authenticationToken1 );
                        validatePrincipal( principal2 );

                        UTF_REQUIRE( ! om::areEqual( principal1, principal2 ) );
                        requireBlocksNotEqual( principal1 -> authenticationToken(), principal2 -> authenticationToken() );
                    }

                    /*
                     * Use an invalid authentication token to simulate authorization service error
                     * and then ensure that the content of the exception dump doesn't contain the
                     * sensitive data (e.g. the authentication token or other such data is replaced
                     * with [REDACTED])
                     */

                    try
                    {
                        const auto authorizationTask = cache -> createAuthorizationTask( invalidAuthenticationToken );

                        tasks::scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                            {
                                eq -> push_back( authorizationTask );
                            }
                            );

                        UTF_FAIL( "The statement above is expected to throw" );
                    }
                    catch( std::exception& e )
                    {
                        const auto exceptionDump = eh::diagnostic_information( e );

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "Expected exception:\n"
                                << exceptionDump
                                );

                        /*
                         * Ensure that we can't find the security sensitive info in the exception dump and
                         * instead we find [REDACTED]
                         */

                        UTF_REQUIRE( ! cpp::contains( exceptionDump, invalidAuthenticationTokenData ) );

                        UTF_REQUIRE( cpp::contains( exceptionDump, "[REDACTED]" ) );
                    }
                },
                BL_PARAM_FWD( backend )
                );
        }
    };

    typedef TestAuthorizationCacheImplUtilsT<> TestAuthorizationCacheImplUtils;

} // utest

#endif /* __UTESTS_BASELIB_TESTAUTHORIZATIONCACHEIMPLUTILS_H_ */
