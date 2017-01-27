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

#ifndef __BL_NETUTILS_H_
#define __BL_NETUTILS_H_

#include <baselib/core/OS.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace net
    {
        using asio::ip::tcp;

        typedef cpp::SafeUniquePtr< tcp::socket > socket_ref;

        namespace detail
        {
            /**
             * @brief class NetUtils - networking utility APIs
             */

            template
            <
                typename E = void
            >
            class NetUtilsT FINAL
            {
                BL_DECLARE_STATIC( NetUtilsT )

            private:

                enum
                {
                    /*
                     * 3 seconds ( 100 x 30 milliseconds ) total delay
                     * should be hopefully enough to workaround this
                     * known issue (see comments below)
                     */

                    ENDPOINT_QUERY_WAIT_IN_MILLISECONDS = 100,
                    MAX_ENDPOINT_QUERY_RETRIES = 30,
                };

                template
                <
                    typename T
                >
                static auto safeRemoteEndpoint( SAA_in const T& socket ) -> typename T::endpoint_type
                {
                    tcp::endpoint remoteEndpoint;

                    for( std::size_t i = 0; i < MAX_ENDPOINT_QUERY_RETRIES; ++i )
                    {

                        /*
                         * Save the remote endpoint, so the getHost(), getPort()
                         * and getRemoteEndpoint() APIs below can work safely.
                         */

                        try
                        {
                            remoteEndpoint = socket.remote_endpoint();

                            break;
                        }
                        catch( eh::system_error& e )
                        {
                            /*
                             * We have seen cases, specifically when using RHEL5
                             * generated libs running on RHEL6, where
                             * 'remote_endpoint' is called and it returns not
                             * connected. This occurs in spite of the connection
                             * already being established as the present callback
                             * is invoked after a connection has been established.
                             *
                             * Googling around, others have reported sporadic issues
                             * like this one as a result of 'getpeername' (used by
                             * Boost) returning ENOTCONN. Workarounds include
                             * retrying the call and writing to the socket.
                             */

                            if( asio::error::not_connected != e.code() || ( MAX_ENDPOINT_QUERY_RETRIES - 1 ) == i )
                            {
                                throw;
                            }

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "failure to retrieve remote endpoint information -- retry #"
                                    << i + 1
                                    << " out of "
                                    << MAX_ENDPOINT_QUERY_RETRIES
                            );

                            os::sleep( time::milliseconds( ENDPOINT_QUERY_WAIT_IN_MILLISECONDS ) );
                        }
                    }

                    return remoteEndpoint;
                }

            public:

                template
                <
                    typename T
                >
                static std::string safeRemoteEndpointId( SAA_in const T& socket )
                {
                    try
                    {
                        return formatEndpointId( safeRemoteEndpoint( socket ) );
                    }
                    catch( eh::system_error& e )
                    {
                        if( asio::error::not_connected != e.code() )
                        {
                            throw;
                        }
                    }

                    return "<unknown_host_name>:<unknown_port>";
                }

                template
                <
                    typename T
                >
                static std::string formatEndpointId( SAA_in const T& endpoint )
                {
                    return formatEndpointId( endpoint.address().to_string(), endpoint.port() );
                }

                static std::string formatEndpointId(
                    SAA_in      const std::string&                      host,
                    SAA_in      const os::port_t                        port
                    )
                {
                    cpp::SafeOutputStringStream output;

                    output
                        << host
                        << ":"
                        << port;

                    return output.str();
                }

                static std::string getCanonicalHostName( SAA_in const std::string& hostName )
                {
                    using namespace asio::ip;

                    asio::io_service ioService;
                    tcp::resolver resolver( ioService );
                    eh::error_code ec;

                    tcp::resolver::query query(
                        hostName                            /* host_name */,
                        str::empty()                        /* service_name */,
                        resolver_query_base::canonical_name /* flags */
                        );

                    const auto endpoints = resolver.resolve( query, ec );

                    BL_CHK_EC_USER_FRIENDLY(
                        ec,
                        BL_MSG()
                            << "Host '"
                            << hostName
                            << "' look-up has failed"
                        );

                    const decltype( endpoints ) end;

                    /*
                     * All host names in the returned endpoints are identical. Use the first one.
                     *
                     * Even if the getaddrinfo function succeeds, the host name may be empty.
                     */

                    BL_CHK_USER_FRIENDLY(
                        true,
                        endpoints == end || endpoints -> host_name().empty(),
                        BL_MSG()
                            << "Host '"
                            << hostName
                            << "' has no canonical name"
                        );

                    return endpoints -> host_name();
                }
            };

            typedef NetUtilsT<> NetUtils;

        } // detail

        template
        <
            typename T
        >
        inline std::string safeRemoteEndpointId( SAA_in const T& socket )
        {
            return detail::NetUtils::safeRemoteEndpointId< T >( socket );
        }

        template
        <
            typename T
        >
        inline std::string formatEndpointId( SAA_in const T& endpoint )
        {
            return detail::NetUtils::formatEndpointId< T >( endpoint );
        }

        inline std::string formatEndpointId(
            SAA_in      const std::string&                      host,
            SAA_in      const os::port_t                        port
            )
        {
            return detail::NetUtils::formatEndpointId( host, port );
        }

        inline std::string getShortHostName( SAA_in_opt std::string&& hostName = asio::ip::host_name() )
        {
            BL_CHK_ARG( ! hostName.empty(), hostName );

            const auto pos = hostName.find( '.' );

            if( pos == std::string::npos )
            {
                return BL_PARAM_FWD( hostName );
            }
            else
            {
                return hostName.substr( 0, pos );
            }
        }

        inline std::string getCanonicalHostName( SAA_in_opt std::string&& hostName = asio::ip::host_name() )
        {
            BL_CHK_ARG( ! hostName.empty(), hostName );

            return detail::NetUtils::getCanonicalHostName( BL_PARAM_FWD( hostName ) );
        }

        /*
         * @brief: Extract host and port from string in "host:port" format
         */

        inline bool tryParseEndpoint(
            SAA_in      const std::string&      endpoint,
            SAA_inout   std::string&            host,
            SAA_inout   os::port_t&             port
            )
        {
            const auto pos = endpoint.find( ':' );

            if( pos == std::string::npos )
            {
                return false;
            }

            if( pos == 0U || pos + 1U == endpoint.size() )
            {
                return false;
            }

            if( endpoint.size() > pos + 6U )
            {
                return false;
            }

            unsigned long value = 0U;

            try
            {
                value = std::stoul( std::string( endpoint.begin() + pos + 1U, endpoint.end() ) );
            }
            catch( std::invalid_argument& )
            {
                return false;
            }

            if( value > std::numeric_limits< os::port_t >::max() )
            {
                return false;
            }

            host.assign( endpoint.begin(), endpoint.begin() + pos );
            port = static_cast< os::port_t >( value );

            return true;
        }

        /*
         * Network protocol header wrappers
         */

        /**
         * @brief Packet header for IPv4
         *
         * The wire format of an IPv4 header is:
         *
         * @pre
         * 0               8               16                             31
         * +-------+-------+---------------+------------------------------+      ---
         * |       |       |               |                              |       ^
         * |version|header |    type of    |    total length in bytes     |       |
         * |  (4)  | length|    service    |                              |       |
         * +-------+-------+---------------+-+-+-+------------------------+       |
         * |                               | | | |                        |       |
         * |        identification         |0|D|M|    fragment offset     |       |
         * |                               | |F|F|                        |       |
         * +---------------+---------------+-+-+-+------------------------+       |
         * |               |               |                              |       |
         * | time to live  |   protocol    |       header checksum        |   20 bytes
         * |               |               |                              |       |
         * +---------------+---------------+------------------------------+       |
         * |                                                              |       |
         * |                      source IPv4 address                     |       |
         * |                                                              |       |
         * +--------------------------------------------------------------+       |
         * |                                                              |       |
         * |                   destination IPv4 address                   |       |
         * |                                                              |       v
         * +--------------------------------------------------------------+      ---
         * |                                                              |       ^
         * |                                                              |       |
         * /                        options (if any)                      /    0 - 40
         * /                                                              /     bytes
         * |                                                              |       |
         * |                                                              |       v
         * +--------------------------------------------------------------+      ---
         */

        class Ipv4Header FINAL
        {
            std::uint8_t                                                    m_data[ 60 ];

        public:

            Ipv4Header()
            {
                std::fill( m_data, m_data + sizeof( m_data ), 0 );
            }

            std::uint8_t version() const NOEXCEPT                           { return ( m_data[ 0 ] >> 4 ) & 0xF; }
            std::uint16_t headerLength() const NOEXCEPT                     { return ( m_data[ 0 ] & 0xF ) * 4; }
            std::uint8_t typeOfService() const NOEXCEPT                     { return m_data[ 1 ]; }
            std::uint16_t totalLength() const NOEXCEPT                      { return decode( 2, 3 ); }
            std::uint16_t identification() const NOEXCEPT                   { return decode( 4, 5 ); }
            bool dontFragment() const NOEXCEPT                              { return ( m_data[ 6 ] & 0x40 ) != 0; }
            bool moreFragments() const NOEXCEPT                             { return ( m_data[ 6 ] & 0x20 ) != 0; }
            std::uint16_t fragmentOffset() const NOEXCEPT                   { return decode( 6, 7 ) & 0x1FFF; }
            std::uint8_t timeToLive() const NOEXCEPT                        { return m_data[ 8 ]; }
            std::uint8_t protocol() const NOEXCEPT                          { return m_data[ 9 ]; }
            std::uint16_t headerChecksum() const NOEXCEPT                   { return decode( 10, 11 ); }

            asio::ip::address_v4 sourceAddress() const NOEXCEPT
            {
                const asio::ip::address_v4::bytes_type bytes = {
                    { m_data[ 12 ], m_data[ 13 ], m_data[ 14 ], m_data[ 15 ] }
                    };

                return asio::ip::address_v4( bytes );
            }

            asio::ip::address_v4 destinationAddress() const NOEXCEPT
            {
                const asio::ip::address_v4::bytes_type bytes = {
                    { m_data[ 16 ], m_data[ 17 ], m_data[ 18 ], m_data[ 19 ] }
                    };

                return asio::ip::address_v4( bytes );
            }

            friend std::istream& operator>>(
                SAA_inout   std::istream&                                   is,
                SAA_inout   Ipv4Header&                                     header
                )
            {
                is.read( reinterpret_cast< char* >( header.m_data ), 20 );

                if( header.version() != 4 )
                {
                    is.setstate( std::ios::failbit );
                }

                const std::streamsize optionsLength = header.headerLength() - 20;

                if( optionsLength < 0 || optionsLength > 40 )
                {
                    is.setstate( std::ios::failbit );
                }
                else
                {
                    is.read( reinterpret_cast< char* >( header.m_data ) + 20, optionsLength );
                }

                return is;
            }

        private:

            std::uint16_t decode(
                SAA_in      const std::size_t                               a,
                SAA_in      const std::size_t                               b
                ) const NOEXCEPT
            {
                return ( m_data[ a ] << 8 ) + m_data[ b ];
            }
        };

        /**
         * @brief ICMP header for both IPv4 and IPv6
         *
         * The wire format of an ICMP header is:
         *
         * @pre
         * 0               8               16                             31
         * +---------------+---------------+------------------------------+      ---
         * |               |               |                              |       ^
         * |     type      |     code      |          checksum            |       |
         * |               |               |                              |       |
         * +---------------+---------------+------------------------------+    8 bytes
         * |                               |                              |       |
         * |          identifier           |       sequence number        |       |
         * |                               |                              |       v
         * +-------------------------------+------------------------------+      ---
         */

        class IcmpHeader FINAL
        {
            std::uint8_t                                                    m_data[ 8 ];

        public:

            enum : std::uint8_t
            {
                ICMP_ECHO_REPLY                 = 0,
                ICMP_DESTINATION_UNREACHABLE    = 3,
                ICMP_SOURCE_QUENCH              = 4,
                ICMP_REDIRECT                   = 5,
                ICMP_ECHO_REQUEST               = 8,
                ICMP_TIME_EXCEEDED              = 11,
                ICMP_PARAMETER_PROBLEM          = 12,
                ICMP_TIMESTAMP_REQUEST          = 13,
                ICMP_TIMESTAMP_REPLY            = 14,
                ICMP_INFO_REQUEST               = 15,
                ICMP_INFO_REPLY                 = 16,
                ICMP_ADDRESS_REQUEST            = 17,
                ICMP_ADDRESS_REPLY              = 18,
            };

            IcmpHeader()
            {
                std::fill( m_data, m_data + sizeof( m_data ), 0 );
            }

            std::uint8_t type() const NOEXCEPT                              { return m_data[ 0 ]; }
            std::uint8_t code() const NOEXCEPT                              { return m_data[ 1 ]; }
            std::uint16_t checksum() const NOEXCEPT                         { return decode( 2, 3 ); }
            std::uint16_t identifier() const NOEXCEPT                       { return decode( 4, 5 ); }
            std::uint16_t sequenceNumber() const NOEXCEPT                   { return decode( 6, 7 ); }

            void type( SAA_in const std::uint8_t n ) NOEXCEPT               { m_data[ 0 ] = n; }
            void code( SAA_in const std::uint8_t n ) NOEXCEPT               { m_data[ 1 ] = n; }
            void checksum( SAA_in const std::uint16_t n ) NOEXCEPT          { encode( 2, 3, n ); }
            void identifier( SAA_in const std::uint16_t n ) NOEXCEPT        { encode( 4, 5, n ); }
            void sequenceNumber( SAA_in const std::uint16_t n ) NOEXCEPT    { encode( 6, 7, n ); }

            friend std::istream& operator>>(
                SAA_inout   std::istream&                                   is,
                SAA_inout   IcmpHeader&                                     header
                )
            {
                return is.read( reinterpret_cast< char* >( header.m_data ), 8 );
            }

            friend std::ostream& operator<<(
                SAA_inout   std::ostream&                                   os,
                SAA_in      const IcmpHeader&                               header
                )
            {
                return os.write( reinterpret_cast< const char* >( header.m_data ), 8 );
            }

            template
            <
                typename Iterator
            >
            void computeChecksum(
                SAA_in      const Iterator                                  bodyBegin,
                SAA_in      const Iterator                                  bodyEnd
                )
            {
                std::uint32_t sum = ( type() << 8 ) + code() + identifier() + sequenceNumber();

                Iterator iter = bodyBegin;
                while( iter != bodyEnd )
                {
                    sum += static_cast< std::uint8_t >( *iter++ ) << 8;

                    if( iter != bodyEnd )
                    {
                        sum += static_cast< std::uint8_t >( *iter++ );
                    }
                }

                sum = ( sum >> 16 ) + ( sum & 0xFFFF );
                sum += ( sum >> 16 );

                checksum( static_cast< std::uint16_t >( ~sum ) );
            }

        private:

            std::uint16_t decode(
                SAA_in      const std::size_t                               a,
                SAA_in      const std::size_t                               b
                ) const NOEXCEPT
            {
                return ( m_data[ a ] << 8 ) + m_data[ b ];
            }

            void encode(
                SAA_in      const std::size_t                               a,
                SAA_in      const std::size_t                               b,
                SAA_in      const std::uint16_t                             n
                ) NOEXCEPT
            {
                m_data[ a ] = static_cast< std::uint8_t >( n >> 8 );
                m_data[ b ] = static_cast< std::uint8_t >( n & 0xFF );
            }
        };

    } // net

} // bl

#endif /* __BL_NETUTILS_H_ */
