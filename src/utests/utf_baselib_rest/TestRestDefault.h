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

#include <utests/baselib/TestRestUtils.h>

UTF_AUTO_TEST_CASE( RestServiceSslBackendTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto callbackTests = [ & ]() -> void
    {
        std::unordered_set< std::string > tokenCookieNames;
        tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

        utest::TestRestUtils::httpRestWithMessagingBackendTests(
            false                                                           /* waitOnServer */,
            false                                                           /* isQuietMode */,
            1U                                                              /* requestsCount */,
            uuids::create()                                                 /* serverPeerId */,
            om::copy( controlToken )                                        /* controlToken */,
            test::UtfArgsParser::host()                                     /* brokerHostName */,
            test::UtfArgsParser::port()                                     /* brokerInboundPort */,
            test::UtfArgsParser::connections()                              /* noOfConnections */,
            cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
            std::move( tokenCookieNames )                                   /* tokenCookieNames */,
            cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1U                    /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests,
        om::copy( controlToken )
        );
}

UTF_AUTO_TEST_CASE( RestServiceSslBackendPerfTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto callbackTests = [ & ]() -> void
    {
        std::unordered_set< std::string > tokenCookieNames;
        tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

        utest::TestRestUtils::httpRestWithMessagingBackendTests(
            false                                                           /* waitOnServer */,
            true                                                            /* isQuietMode */,
            50U                                                             /* requestsCount */,
            uuids::create()                                                 /* serverPeerId */,
            om::copy( controlToken )                                        /* controlToken */,
            test::UtfArgsParser::host()                                     /* brokerHostName */,
            test::UtfArgsParser::port()                                     /* brokerInboundPort */,
            test::UtfArgsParser::connections()                              /* noOfConnections */,
            cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
            std::move( tokenCookieNames )                                   /* tokenCookieNames */,
            cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1U                    /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests,
        om::copy( controlToken )
        );
}

UTF_AUTO_TEST_CASE( RestServiceSslHttpGatewayOnlyTests )
{
    using namespace bl;
    using namespace bl::tasks;

    if( ! test::UtfArgsParser::isServer() )
    {
        /*
         * This is a manual run test
         */

        return;
    }

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    std::unordered_set< std::string > tokenCookieNames;
    tokenCookieNames.emplace( utest::DummyAuthorizationCache::dummyCookieName() );

    utest::TestRestUtils::httpRestWithMessagingBackendTests(
        true                                                            /* waitOnServer */,
        true                                                            /* isQuietMode */,
        0U                                                              /* requestsCount */,
        uuids::create()                                                 /* serverPeerId */,
        om::copy( controlToken )                                        /* controlToken */,
        test::UtfArgsParser::host()                                     /* brokerHostName */,
        test::UtfArgsParser::port()                                     /* brokerInboundPort */,
        test::UtfArgsParser::connections()                              /* noOfConnections */,
        cpp::copy( utest::DummyAuthorizationCache::dummySid() )         /* expectedSecurityId */,
        std::move( tokenCookieNames )                                   /* tokenCookieNames */,
        cpp::copy( utest::DummyAuthorizationCache::dummyTokenType() )   /* tokenTypeDefault */
        );
}

UTF_AUTO_TEST_CASE( RestServiceSslBackendHttpOnlyTests )
{
    if( ! test::UtfArgsParser::isClient() )
    {
        /*
         * This is a manual run test
         */

        return;
    }

    if( test::UtfArgsParser::tokenData().empty() )
    {
        UTF_FAIL( "--token-data is a required parameter for this test" );

        return;
    }

    utest::TestRestUtils::httpRunSimpleRequest(
        test::UtfArgsParser::port()                 /* httpPort */,
        test::UtfArgsParser::tokenData()            /* tokenData */,
        test::UtfArgsParser::connections()          /* requestsCount */,
        true                                        /* isQuietMode */
        );

}
