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
        utest::TestRestUtils::httpRestWithMessagingBackendTests(
            om::copy( controlToken ),
            test::UtfArgsParser::host()                     /* brokerHostName */,
            test::UtfArgsParser::port()                     /* brokerInboundPort */,
            test::UtfArgsParser::connections()              /* noOfConnections */,
            "sid1234"                                       /* expectedSecurityId */
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
