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

#ifndef __BL_MESSAGING_BROKERERRORCODES_H_
#define __BL_MESSAGING_BROKERERRORCODES_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        BL_UUID_DECLARE( ErrorUuidNotConnectedToBroker,     "11420a1f-6e7d-4cd1-997a-5360ba74890e" )
        BL_UUID_DECLARE( ErrorUuidResponseTimeout,          "d2a4064d-66fe-473d-a134-bfaf54bb0114" )

        template
        <
            typename E = void
        >
        class BrokerErrorCodesT
        {
            BL_DECLARE_STATIC( BrokerErrorCodesT )

        private:

            static auto initExpectedErrorsGlobalMap() -> const std::unordered_map< int, std::string >&
            {
                static std::unordered_map< int, std::string > map;

                map.emplace( AuthorizationFailed, eh::errc::make_error_code( AuthorizationFailed ).message() );
                map.emplace( ProtocolValidationFailed, eh::errc::make_error_code( ProtocolValidationFailed ).message() );

                map.emplace( TargetPeerNotFound, "The server is currently unavailable" );
                map.emplace( TargetPeerQueueFull, "The server is too busy" );

                return map;
            };

        public:

            static const eh::errc::errc_t AuthorizationFailed           = eh::errc::permission_denied;
            static const eh::errc::errc_t ProtocolValidationFailed      = eh::errc::invalid_argument;

            /*
             * Since the values of the error codes below are not stable for each platform (even across
             * the POSIX platforms - see links below) we need to hard-code the values for these to
             * something stable; in our case we are going to use the Linux values
             * (the Linux values can be found here: http://www.virtsync.com/c-error-codes-include-errno)
             *
             * http://www.ioplex.com/~miallen/errcmp.html
             * http://www.ioplex.com/~miallen/errcmpp.html
             */

            static const eh::errc::errc_t TargetPeerNotFound            =
                static_cast< eh::errc::errc_t >( 99 ) /* EADDRNOTAVAIL / eh::errc::address_not_available */;

            static const eh::errc::errc_t TargetPeerQueueFull           =
                static_cast< eh::errc::errc_t >( 105 ) /* ENOBUFS / eh::errc::no_buffer_space */;

            static bool isExpectedErrorCode( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                if( ec.category() != eh::generic_category() )
                {
                    return false;
                }

                switch( ec.value() )
                {
                    case AuthorizationFailed:
                    case ProtocolValidationFailed:
                    case TargetPeerNotFound:
                    case TargetPeerQueueFull:

                        return true;
                }

                return false;
            }

            static auto tryGetExpectedErrorMessage( SAA_in const eh::error_code& ec ) NOEXCEPT -> const std::string&
            {
                if( ! isExpectedErrorCode( ec ) )
                {
                    return bl::str::empty();
                }

                /*
                 * The global error codes map will be initialized once on the first call
                 */

                static const auto& g_map = initExpectedErrorsGlobalMap();

                const auto pos = g_map.find( ec.value() );

                if( pos != g_map.end() )
                {
                    return pos -> second;
                }

                return bl::str::empty();
            }

            static void rethrowIfNotExpectedException( SAA_in const std::exception_ptr& exception )
            {
                try
                {
                    cpp::safeRethrowException( exception );
                }
                catch( ServerErrorException& e )
                {
                    const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );

                    if( ec && isExpectedErrorCode( *ec ) )
                    {
                        return;
                    }

                    throw;
                }
            }

            static bool isExpectedException( SAA_in const std::exception_ptr& exception ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                try
                {
                    rethrowIfNotExpectedException( exception );

                    return true;
                }
                catch( std::exception& )
                {
                    /*
                     * Nothing to do, just avoiding irrelevant exception to escape
                     */
                }

                BL_NOEXCEPT_END()

                return false;
            }
        };

        typedef BrokerErrorCodesT<> BrokerErrorCodes;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BROKERERRORCODES_H_ */
