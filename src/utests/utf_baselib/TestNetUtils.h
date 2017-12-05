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

#include <baselib/core/NetUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <string>

#include <utests/baselib/Utf.h>

UTF_AUTO_TEST_CASE( NetUtils_tryParseEndpointTest )
{
    bl::os::port_t port;
    std::string host;

    UTF_CHECK( ! bl::net::tryParseEndpoint( ":", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( "host:", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( ":port", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( "hostport", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( "host:65536", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( "host:123456", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( "host:-123", host, port ) );
    UTF_CHECK( ! bl::net::tryParseEndpoint( "host:port", host, port ) );

    UTF_CHECK( bl::net::tryParseEndpoint( "host:6", host, port ) );
    UTF_CHECK_EQUAL( host, "host" );
    UTF_CHECK_EQUAL( port, 6 );

    UTF_CHECK( bl::net::tryParseEndpoint( "host:65", host, port ) );
    UTF_CHECK_EQUAL( host, "host" );
    UTF_CHECK_EQUAL( port, 65 );

    UTF_CHECK( bl::net::tryParseEndpoint( "host:655", host, port ) );
    UTF_CHECK_EQUAL( host, "host" );
    UTF_CHECK_EQUAL( port, 655 );

    UTF_CHECK( bl::net::tryParseEndpoint( "host:6553", host, port ) );
    UTF_CHECK_EQUAL( host, "host" );
    UTF_CHECK_EQUAL( port, 6553 );

    UTF_CHECK( bl::net::tryParseEndpoint( "host:65535", host, port ) );
    UTF_CHECK_EQUAL( host, "host" );
    UTF_CHECK_EQUAL( port, 65535 );
}
