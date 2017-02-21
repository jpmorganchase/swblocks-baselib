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

#ifndef __BL_TASKS_UTILS_PINGER_H_
#define __BL_TASKS_UTILS_PINGER_H_

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/TaskBase.h>

#include <baselib/core/NetUtils.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/OS.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class PingerTask - common pinger implementation base
         */

        template
        <
            typename E = void
        >
        class PingerTaskT : public SimpleTaskBase
        {
            BL_DECLARE_OBJECT_IMPL( PingerTaskT )

        protected:

            typedef SimpleTaskBase                              base_type;

            const std::string                                   m_host;
            const int                                           m_timeoutMs;
            cpp::ScalarTypeIniter< bool >                       m_isReachable;
            cpp::ScalarTypeIniter< double >                     m_roundTripTimeMs;

        protected:

            PingerTaskT(
                SAA_in              std::string&&               host,
                SAA_in              const int                   timeoutInMilliseconds
                )
                :
                m_host( BL_PARAM_FWD( host ) ),
                m_timeoutMs( timeoutInMilliseconds )
            {
            }

            virtual void enhanceException( SAA_in eh::exception& exception ) const OVERRIDE
            {
                base_type::enhanceException( exception );

                exception << eh::errinfo_host_name( m_host );
            }

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                   eptr,
                SAA_in                  const std::exception&                       exception,
                SAA_in_opt              const eh::error_code*                       ec
                ) NOEXCEPT OVERRIDE
            {
                BL_UNUSED( eptr );
                BL_UNUSED( exception );
                BL_UNUSED( ec );

                /*
                 * Pinger tasks can fail normally and these failures are expected
                 *
                 * Always return true here, so no unnecessary noise in the logs is generated
                 */

                return true;
            }

        public:

            const std::string& host() const NOEXCEPT
            {
                return m_host;
            }

            int timeoutMs() const NOEXCEPT
            {
                return m_timeoutMs;
            }

            bool isReachable() const NOEXCEPT
            {
                return m_isReachable;
            }

            bool isUnreachable() const NOEXCEPT
            {
                return ! m_isReachable || isFailed();
            }

            /**
             * @return Ping round-trip time in milliseconds
             */

            double roundTripTimeMs() const NOEXCEPT
            {
                return m_roundTripTimeMs;
            }

            /**
             * @brief Compare pingers by host reachability and round-trip time
             * @return true if lhs < rhs
             */

            static bool less(
                SAA_in      const PingerTaskT&                  lhs,
                SAA_in      const PingerTaskT&                  rhs
                ) NOEXCEPT
            {
                const auto lhsUnreachable = lhs.isUnreachable();
                const auto rhsUnreachable = rhs.isUnreachable();

                /*
                 * Failed tasks are always considered after non-failed tasks
                 */

                if( ! lhsUnreachable && rhsUnreachable )
                {
                    return true;
                }

                if( lhsUnreachable && ! rhsUnreachable )
                {
                    return false;
                }

                if( lhsUnreachable && rhsUnreachable )
                {
                    /*
                     * If both are not reachable then we simply order them by host
                     * name alphabetically
                     */

                     return lhs.host() < rhs.host();
                }

                /*
                 * Both are reachable - order them by round trip time
                 * (which is the order that we want)
                 */

                BL_ASSERT( ! lhsUnreachable && ! rhsUnreachable );

                return lhs.roundTripTimeMs() < rhs.roundTripTimeMs();
            }

            static void sortPingerTasksByRoundTripTime( SAA_inout std::vector< om::ObjPtr< PingerTaskT > >& list )
            {
                const auto cbComparer = [](
                    SAA_in      const om::ObjPtr< PingerTaskT >&        lhs,
                    SAA_in      const om::ObjPtr< PingerTaskT >&        rhs
                    ) -> bool
                {
                    return less( *lhs, *rhs );
                };

                std::sort( list.begin(), list.end(), cbComparer );
            }
        };

        typedef PingerTaskT<> PingerTask;

        /**
         * @brief class ProcessPingerTask - a simple implementation of pinger task which
         * will execute the ping command line utility and parse out the round trip time
         *
         * The ping command line utility on each platform does not require admin rights, but
         * this is because there is workaround on each:
         *
         * -- On Windows it uses ICMPSendEcho{,2,2Ex} APIs which can work as non-admin
         * -- On Linux, ping is owned by root and has the setuid bit set so that RAW
         *    sockets work for anyone
         *
         *    You can see more information in the following stack overflow article:
         *    http://stackoverflow.com/questions/1623455/sendto-fails-with-a-non-administrator-user-with-errorcode-10013/1623502
         *
         * Since we want this command to work for non-admin and for now not to deal
         * with these issues we will just shell out ping{.exe} and parse out the output
         * that we care about
         *
         * @warning This is a blocking task, consuming up to 3 threads from the thread pool.
         * Executing too many process pingers in parallel is known to cause deadlocks due to
         * thread pool exhaustion.
         */

        template
        <
            typename E = void
        >
        class ProcessPingerTaskT : public PingerTask
        {
        private:

            static const str::regex                             g_patternAvgRtt;

        protected:

            ProcessPingerTaskT(
                SAA_in              std::string&&               host,
                SAA_in              const int                   timeoutInMilliseconds
                )
                :
                PingerTask( BL_PARAM_FWD( host ), timeoutInMilliseconds )
            {
                m_name = ( BL_MSG()
                    << "Process_PING "
                    << m_host
                    ).text();
            }

        private:

            virtual void onExecute() NOEXCEPT OVERRIDE
            {
                BL_TASKS_HANDLER_BEGIN()
                BL_TASKS_HANDLER_CHK_CANCEL_IMPL()

                const auto cbStdIo = [ this ](
                    SAA_in              const os::process_handle_t  process,
                    SAA_in              std::istream&               out
                    ) -> void
                {
                    scheduleAndExecuteInParallel(
                        [ this, &process, &out ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                        {
                            eq -> push_back(
                                [ this, &process ]() -> void
                                {
                                    timedWait( process );
                                }
                                );

                            eq -> push_back(
                                [ this, &process, &out ]() -> void
                                {
                                    parseData( process, out );
                                }
                                );
                        }
                        );
                };

                /*
                 * Send just 1 ping packet and wait up to 3 seconds for a response
                 */

                std::vector< std::string > cmdLine;
                cmdLine.reserve( 6 );

                if( os::onWindows() )
                {
                    cmdLine.emplace_back( "ping.exe" );
                    cmdLine.emplace_back( "-n" );
                    cmdLine.emplace_back( "1" );
                    cmdLine.emplace_back( "-w" );
                    cmdLine.emplace_back( std::to_string( m_timeoutMs ) );
                }
                else
                {
                    cmdLine.emplace_back( "ping" );
                    cmdLine.emplace_back( "-c" );
                    cmdLine.emplace_back( "1" );
                    cmdLine.emplace_back( os::onLinux() ? "-w" : "-t" );
                    cmdLine.emplace_back( std::to_string( ( m_timeoutMs + 500 ) / 1000 ) );
                }

                cmdLine.emplace_back( m_host );

                /*
                 * Just shell out ping{.exe} and parse the output
                 */

                const auto processRef = os::createRedirectedProcessMergeOutputAndWait( cmdLine, cbStdIo );

                BL_TASKS_HANDLER_END()
            }

            void parseData(
                SAA_in              const os::process_handle_t  process,
                SAA_in              std::istream&               out
                )
            {
                cpp::ScalarTypeIniter< bool >                   allPacketsArrived;
                cpp::ScalarTypeIniter< double >                 roundTripTimeMs;

                BL_UNUSED( process );

                while( ! out.eof() )
                {
                    std::string line;
                    std::getline( out, line );

                    BL_LOG(
                        Logging::trace(),
                        BL_MSG()
                            << m_name
                            << ": "
                            << line
                        );

                    if( ! allPacketsArrived )
                    {
                        allPacketsArrived = matchPacketsArrived( line );
                        continue;
                    }

                    if( matchAverageRoundTripTime( line, &roundTripTimeMs.lvalue() ) )
                    {
                        m_isReachable = true;
                        m_roundTripTimeMs = roundTripTimeMs;
                        return;
                    }
                }
            }

            void timedWait( SAA_in const os::process_handle_t process )
            {
                /*
                 * Just give the ping process limited time to respond, so
                 * we can detect all the cases where it hangs or some other
                 * such issue
                 */

                try
                {
                    const auto exitCode =
                        os::tryAwaitTermination( process, m_timeoutMs );

                    if( exitCode )
                    {
                        BL_THROW(
                            UnexpectedException(),
                            BL_MSG()
                                << "Ping failed with non-zero exit code: "
                                << exitCode
                            );
                    }
                }
                catch( std::exception& )
                {
                    os::terminateProcess( process );
                    throw;
                }
            }

        public:

            /*
            On Windows:
            ---------------------------------------------------------

            Reply from 10.83.36.194: bytes=32 time=1ms TTL=125

            Ping statistics for <$host>:
                Packets: Sent = 1, Received = 1, Lost = 0 (0% loss),
            Approximate round trip times in milli-seconds:
                Minimum = 1ms, Maximum = 1ms, Average = 1ms

            On Linux:
            ---------------------------------------------------------

            64 bytes from 169.70.41.12: icmp_req=1 ttl=57 time=2.08 ms

            --- <$host> statistics ---
            1 packets transmitted, 1 received, 0% packet loss, time 3005ms
            rtt min/avg/max/mdev = 2.069/2.161/2.324/0.105 ms

            On Darwin:
            ---------------------------------------------------------

            PING localhost (127.0.0.1): 56 data bytes
            64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.048 ms

            --- localhost ping statistics ---
            1 packets transmitted, 1 packets received, 0.0% packet loss
            round-trip min/avg/max/stddev = 0.048/0.048/0.048/0.000 ms
             */

            static bool matchPacketsArrived( SAA_in const std::string& line )
            {
                return cpp::contains(
                    line,
                    os::onWindows() ?
                        "Packets: Sent = 1, Received = 1, Lost = 0" :
                        (
                            os::onLinux() ?
                                "1 packets transmitted, 1 received, 0% packet loss"
                                :
                                "1 packets transmitted, 1 packets received, 0.0% packet loss"
                        )
                    );
            }

            static bool matchAverageRoundTripTime(
                SAA_in              const std::string&          line,
                SAA_out             double*                     roundTripTimeMs
                )
            {
                str::smatch results;

                if( str::regex_search( line, results, g_patternAvgRtt ) )
                {
                    const auto& expression = results[ os::onWindows() ? 1 : 2 ];

                    if( expression.matched )
                    {
                        *roundTripTimeMs = utils::lexical_cast< double >( expression.str() );
                        return true;
                    }
                }

                return false;
            }
        };

        BL_DEFINE_STATIC_MEMBER( ProcessPingerTaskT, const str::regex, g_patternAvgRtt )(
            os::onWindows() ? "Average = ([^m]+)ms.*" :
                (
                    os::onLinux() ?
                        "rtt min/avg/max/mdev = ([^/]+)/([^/]+)/.*"
                        :
                        "round-trip min/avg/max/stddev = ([^/]+)/([^/]+)/.*"
                )
            );

        typedef om::ObjectImpl< ProcessPingerTaskT<> > ProcessPingerTaskImpl;

        /**
         * @brief ICMP ping implementation using Boost ASIO:
         * http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/icmp/ping.cpp
         *
         * It needs to open the socket in 'raw mode' which requires admin / root access
         * on both Windows and Linux platforms
         *
         * This implementation sends only 1 ICMP echo request
         */

        template
        <
            typename E = void
        >
        class IcmpPingerTaskT : public PingerTask
        {
        private:

            typedef asio::ip::icmp                              icmp;
            typedef PingerTask                                  base_type;
            typedef IcmpPingerTaskT                             this_type;

            const std::uint16_t                                 m_identifier;
            const std::uint16_t                                 m_sequenceNumber;

            cpp::SafeUniquePtr< icmp::socket >                  m_socket;
            cpp::SafeUniquePtr< icmp::resolver >                m_resolver;
            icmp::endpoint                                      m_destination;
            cpp::SafeUniquePtr< asio::deadline_timer >          m_timer;

            std::string                                         m_requestId;
            time::ptime                                         m_timeSent;
            asio::streambuf                                     m_replyBuffer;

            static std::atomic< std::uint32_t >                 g_sequenceCounter;

        protected:

            IcmpPingerTaskT(
                SAA_in              std::string&&               host,
                SAA_in              const int                   timeoutInMilliseconds
                )
                :
                PingerTask( BL_PARAM_FWD( host ), timeoutInMilliseconds ),
                m_identifier( createPingerId() ),
                m_sequenceNumber( static_cast< std::uint16_t >( ++g_sequenceCounter ) )
            {
                m_name = ( BL_MSG()
                    << "ICMP_PING "
                    << m_host
                    << " ["
                    << m_identifier
                    << "/"
                    << m_sequenceNumber
                    << "]"
                    ).text();
            }

        private:

            static std::uint16_t createPingerId()
            {
                /*
                 * Each ICMP request must use a 'unique' 16-bit identifier.
                 * Use the bottom 16 bits of the process ID.
                 */

                return static_cast< std::uint16_t >( os::getPid() );
            }

            virtual void onExecute() NOEXCEPT OVERRIDE
            {
                BL_TASKS_HANDLER_BEGIN()
                BL_TASKS_HANDLER_CHK_CANCEL_IMPL()

                auto& aioService = getThreadPool() -> aioService();

                m_socket.reset( new icmp::socket( aioService, icmp::v4() ) );

                m_timer.reset( new asio::deadline_timer( aioService ) );

                /*
                 * Resolve the host address asynchronously
                 */

                m_resolver.reset( new icmp::resolver( aioService ) );

                icmp::resolver::query query( icmp::v4(), m_host, str::empty() /* service */ );

                m_resolver -> async_resolve(
                    query,
                    cpp::bind(
                        &this_type::onResolved,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        asio::placeholders::iterator
                        )
                    );

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void onResolved(
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const icmp::resolver::iterator                  endpoints
                ) NOEXCEPT
            {
                BL_TASKS_HANDLER_BEGIN()
                BL_TASKS_HANDLER_CHK_CANCEL_IMPL()

                BL_CHK_EC_USER_FRIENDLY(
                    ec,
                    BL_MSG()
                        << "Host '"
                        << m_host
                        << "' look-up has failed"
                    );

                const decltype( endpoints ) end;

                BL_ASSERT( end != endpoints );

                m_destination = endpoints -> endpoint();

                BL_LOG(
                    Logging::trace(),
                    BL_MSG()
                        << m_name
                        << ": host resolved: "
                        << m_destination.address()
                    );

                /*
                 * Start listening for ICMP packets and send an ICMP echo request
                 */

                receiveReply();
                sendRequest();

                BL_TASKS_HANDLER_END_NOTREADY()
            }

            void sendRequest()
            {
                /*
                 * Create an ICMP header for an echo request
                 *
                 * Send a unique ID in the request body (needed to match the responses due to a Linux bug)
                 */

                m_requestId = uuids::uuid2string( uuids::create() );

                net::IcmpHeader request;
                const auto& body = m_requestId;

                request.type( net::IcmpHeader::ICMP_ECHO_REQUEST );
                request.code( 0 );
                request.identifier( m_identifier );
                request.sequenceNumber( m_sequenceNumber );
                request.computeChecksum( body.begin(), body.end() );

                /*
                 * Encode the request packet
                 */

                asio::streambuf requestBuffer;

                std::ostream os( &requestBuffer );
                os << request << body;

                /*
                 * Send the request
                 */

                BL_LOG(
                    Logging::trace(),
                    BL_MSG()
                        << m_name
                        << ": sending echo request to "
                        << m_destination.address()
                        << ", body="
                        << body
                    );

                BL_ASSERT( m_socket.get() );

                m_timeSent = time::microsec_clock::universal_time();

                eh::error_code ec;

                m_socket -> send_to( requestBuffer.data(), m_destination, 0 /* message_flags */, ec );

                BL_CHK_EC(
                    ec,
                    BL_MSG()
                        << "Failed to send an ICMP request to "
                        << m_destination
                    );

                /*
                 * Wait for a reply
                 */

                BL_ASSERT( m_timer.get() );

                m_timer -> expires_at( m_timeSent + time::milliseconds( m_timeoutMs ));
                m_timer -> async_wait(
                    cpp::bind(
                        &this_type::onTimeout,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error
                        )
                    );
            }

            void receiveReply()
            {
                /*
                 * Discard any data already in the buffer
                 */

                m_replyBuffer.consume( m_replyBuffer.size() );

                /*
                 * Wait for a reply. Prepare the buffer to receive up to 64KB
                 */

                BL_ASSERT( m_socket.get() );

                m_socket -> async_receive(
                    m_replyBuffer.prepare( 65536 ),
                    cpp::bind(
                        &this_type::onReply,
                        om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        asio::placeholders::error,
                        _2
                        )
                    );
            }

            void onReply(
                SAA_in                  const eh::error_code&                           ec,
                SAA_in                  const std::size_t                               length
                )
            {
                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                const auto now = time::microsec_clock::universal_time();

                /*
                 * The actual number of bytes received is committed to the buffer so that we
                 * can extract it using a std::istream object
                 */

                m_replyBuffer.commit( length );

                /*
                 * Decode the reply packet
                 */

                std::istream is( &m_replyBuffer );
                net::Ipv4Header ipHeader;
                net::IcmpHeader reply;
                std::string body;

                is >> ipHeader >> reply;

                std::getline( is, body, '\0' );

                /*
                 * We can receive all ICMP packets received by the host, so we need to
                 * filter out only the echo replies that match our requests
                 *
                 * Unfortunately, Linux seems to return the *most recent* ICMP identifier and
                 * sequence number sent to the network interface in *every* received ICMP response.
                 * This means that if we send multiple ICMP requests in parallel, we cannot match
                 * the responses using ICMP id/sequence number but we have to encode a unique
                 * request ID in the ICMP body instead.
                 */

                if( ! is )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << m_name
                            << ": got invalid reply"
                            << ": length=" << length
                        );
                }
                else if( reply.type() == net::IcmpHeader::ICMP_ECHO_REPLY )
                {
                    if( body == m_requestId )
                    {
                        /*
                         * Finished
                         */

                        m_isReachable = true;
                        m_roundTripTimeMs = ( now - m_timeSent ).total_microseconds() / 1000.0;

                        BL_LOG(
                            Logging::trace(),
                            BL_MSG()
                                << m_name
                                << ": got reply from " << ipHeader.sourceAddress()
                                << ": length=" << length
                                << ", icmp_id=" << reply.identifier()
                                << ", icmp_seq=" << reply.sequenceNumber()
                                << ", ttl=" << static_cast< unsigned int >( ipHeader.timeToLive() )
                                << ", time=" << m_roundTripTimeMs << " ms"
                                << ", body=" << body
                            );

                        /*
                         * Cancel the timeout
                         */

                        BL_ASSERT( m_timer.get() );

                        m_timer -> cancel();
                    }
                    else
                    {
                        BL_LOG(
                            Logging::trace(),
                            BL_MSG()
                                << m_name
                                << ": got unexpected reply from " << ipHeader.sourceAddress()
                                << ": length=" << length
                                << ", icmp_id=" << reply.identifier()
                                << ", icmp_seq=" << reply.sequenceNumber()
                                << ", ttl=" << static_cast< unsigned int >( ipHeader.timeToLive() )
                                << ", body=" << body
                            );
                    }
                }

                if( ! m_isReachable )
                {
                    /*
                     * Wait for another ICMP packet
                     */

                    receiveReply();

                    return;
                }

                BL_TASKS_HANDLER_END()
            }

            void onTimeout( SAA_in const eh::error_code& ec )
            {
                if( asio::error::operation_aborted == ec )
                {
                    return;
                }

                BL_TASKS_HANDLER_BEGIN_CHK_EC()

                if( ! m_isReachable )
                {
                    BL_LOG(
                        Logging::trace(),
                        BL_MSG()
                            << m_name
                            << ": timed out"
                        );
                }

                BL_TASKS_HANDLER_END()
            }

            virtual void cancelTask() OVERRIDE
            {
                if( m_resolver )
                {
                    m_resolver -> cancel();
                }

                if( m_timer )
                {
                    m_timer -> cancel();
                }

                base_type::cancelTask();
            }

            virtual void enhanceException( SAA_in eh::exception& exception ) const OVERRIDE
            {
                base_type::enhanceException( exception );

                if( m_destination != icmp::endpoint() )
                {
                    exception << eh::errinfo_endpoint_address( m_destination.address().to_string() );
                }
            }
        };

        BL_DEFINE_STATIC_MEMBER( IcmpPingerTaskT, std::atomic< std::uint32_t >, g_sequenceCounter )( 0 );

        typedef om::ObjectImpl< IcmpPingerTaskT<> > IcmpPingerTaskImpl;

        /**
         * @brief Pinger task factory
         */

        template
        <
            typename E = void
        >
        class PingerTaskFactoryT
        {
        private:

            enum
            {
                /*
                 * The maximum number of concurrent non-blocking ICMP pinger tasks
                 */

                MAX_ICMP_PING_TASKS = 256,

                /*
                 * Avoid deadlocks by running too many process pinger tasks in parallel
                 *
                 * Increase the level of parallelism in unit tests
                 */

                MAX_PROCESS_PING_TASKS = 5,
                MAX_PROCESS_PING_TASKS_FOR_UNIT_TESTS = 15,

                /*
                 * The maximum amount of time we're going to wait for a ping response
                 */

                TIMEOUT_IN_MILLISECONDS_DEFAULT = 3 * 1000,
                TIMEOUT_IN_MILLISECONDS_DEFAULT_FOR_UNIT_TESTS = 10 * 1000,
            };

            const bool                                          m_isAdmin;

        public:

            PingerTaskFactoryT()
                :
                m_isAdmin( os::onUNIX() ? os::isUserAdministrator() : false )
            {
            }

            om::ObjPtr< PingerTask > createInstance(
                SAA_in              std::string&&               host,
                SAA_in_opt          const int                   timeoutInMilliseconds = timeoutDefault()
                ) const
            {
                if( m_isAdmin )
                {
                    return IcmpPingerTaskImpl::createInstance< PingerTask >(
                        BL_PARAM_FWD( host ),
                        timeoutInMilliseconds
                        );
                }
                else
                {
                    return ProcessPingerTaskImpl::createInstance< PingerTask >(
                        BL_PARAM_FWD( host ),
                        timeoutInMilliseconds
                        );
                }
            }

            int maxExecutingTasks() const NOEXCEPT
            {
                if( m_isAdmin )
                {
                    return MAX_ICMP_PING_TASKS;
                }

                if( os::onUNIX() )
                {
                    /*
                     * Ping utility on Linux cannot be executed concurrently because
                     * Linux kernel seems to mess with received ICMP sequence numbers
                     * and you end up with duplicate packets like the following:
                     *
                     * 64 bytes from 169.114.17.244: icmp_req=1 ttl=57 time=39.8 ms (DUP!)
                     */

                    return 1;
                }
                else
                {
                    return
                    (
                        global::isInUnitTest() ?
                            MAX_PROCESS_PING_TASKS_FOR_UNIT_TESTS :
                            MAX_PROCESS_PING_TASKS
                    );
                }
            }

            static int timeoutDefault() NOEXCEPT
            {
                return
                (
                    global::isInUnitTest() ?
                        TIMEOUT_IN_MILLISECONDS_DEFAULT_FOR_UNIT_TESTS :
                        TIMEOUT_IN_MILLISECONDS_DEFAULT
                );
            }

            /**
             * @brief The process pinger tasks are not safe to execute concurrently on Linux
             * and we need to obtain a global machine-wide lock before such are executed
             *
             * This method will return the name of this global machine-wide lock
             */

            static const char* getGlobalLockName() NOEXCEPT
            {
                return "app-global-lock-ping-ec0d8f9a-7a3e-4ee3-a727-5069a5bbf55c";
            }
        };

        typedef PingerTaskFactoryT<> PingerTaskFactory;

    } // tasks

} // bl

#endif /* __BL_TASKS_UTILS_PINGER_H_ */
