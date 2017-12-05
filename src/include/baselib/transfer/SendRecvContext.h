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

#ifndef __BL_TRANSFER_SENDRECVCONTEXT_H_
#define __BL_TRANSFER_SENDRECVCONTEXT_H_

#include <baselib/security/SecurityInterfaces.h>

#include <baselib/tasks/TcpBaseTasks.h>
#include <baselib/tasks/TcpSslBaseTasks.h>

#include <baselib/core/Pool.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/EndpointSelector.h>
#include <baselib/core/BaseIncludes.h>

#include <map>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class SendRecvContext
         */
        template
        <
            typename STREAM
        >
        class SendRecvContextT :
            public om::ObjectDefaultBase
        {
            BL_DECLARE_OBJECT_IMPL( SendRecvContextT )

        private:

            typedef typename STREAM::stream_t                       stream_t;
            typedef typename STREAM::stream_ref                     stream_ref;

            typedef om::ObjectImpl
            <
                TaggedPool< std::string, stream_ref, SimplePoolCheckerNaiveImpl >
            >
            connections_pool_type;

            enum : long
            {
                /*
                 * The maximum time a connection can be kept in the pool
                 *
                 * Keeping the connection longer may hit the keep alive timeouts and it is
                 * not desirable
                 */

                MAX_TIMEOUT_IN_SECONDS = 60 * 5,
            };

            const om::ObjPtr< EndpointSelector >                    m_endpointSelector;
            const om::ObjPtr< data::datablocks_pool_type >          m_dataBlocksPool;
            const om::ObjPtr< connections_pool_type >               m_connectionsPool;
            std::map< const stream_t*, time::ptime >                m_connectionIdleSince;
            long                                                    m_maxIdleTimeoutInSeconds;
            std::string                                             m_authenticationToken;
            om::ObjPtr< data::DataBlock >                           m_authenticationDataBlock;
            os::mutex                                               m_lock;

        protected:

            SendRecvContextT(
                SAA_in          om::ObjPtr< EndpointSelector >&&                    endpointSelector,
                SAA_in_opt      const long                                          maxIdleTimeoutInSeconds = MAX_TIMEOUT_IN_SECONDS
                )
                :
                m_endpointSelector( BL_PARAM_FWD( endpointSelector ) ),
                m_dataBlocksPool( data::datablocks_pool_type::createInstance() ),
                m_connectionsPool( connections_pool_type::createInstance() ),
                m_maxIdleTimeoutInSeconds( maxIdleTimeoutInSeconds )
            {
            }

        public:

            auto getAuthenticationDataBlock() const NOEXCEPT -> const om::ObjPtr< data::DataBlock >&
            {
                return m_authenticationDataBlock;
            }

            auto getAuthenticationToken() const NOEXCEPT -> const std::string&
            {
                return m_authenticationToken;
            }

            void setAuthenticationToken( SAA_in std::string&& token )
            {
                m_authenticationToken = std::move( token );

                if( m_authenticationToken.empty() )
                {
                    m_authenticationDataBlock.reset();
                }
                else
                {
                    m_authenticationDataBlock =
                        security::AuthorizationCache::createAuthenticationToken( m_authenticationToken );
                }
            }

            auto endpointSelector() const NOEXCEPT -> const om::ObjPtr< EndpointSelector >&
            {
                return m_endpointSelector;
            }

            auto dataBlocksPool() const NOEXCEPT -> const om::ObjPtr< data::datablocks_pool_type >&
            {
                return m_dataBlocksPool;
            }

            long maxIdleTimeoutInSeconds() const NOEXCEPT
            {
                return m_maxIdleTimeoutInSeconds;
            }

            void maxIdleTimeoutInSeconds( SAA_in const long maxIdleTimeoutInSeconds ) NOEXCEPT
            {
                m_maxIdleTimeoutInSeconds = maxIdleTimeoutInSeconds;
            }

            stream_ref tryGetConnection( SAA_in const std::string& tag ) NOEXCEPT
            {
                BL_MUTEX_GUARD( m_lock );

                stream_ref connection;

                for( ;; )
                {
                    connection = m_connectionsPool -> tryGet( tag );

                    if( connection )
                    {
                        const auto pos = m_connectionIdleSince.find( connection.get() );
                        BL_ASSERT( pos != m_connectionIdleSince.end() );

                        const auto idleSince = pos -> second;
                        m_connectionIdleSince.erase( pos );

                        if( time::second_clock::universal_time() > ( idleSince + time::seconds( m_maxIdleTimeoutInSeconds ) ) )
                        {
                            /*
                             * The connection has been kept idle for too long and may hit the keep alive
                             * timeout which is normally configured around 5 minutes
                             *
                             * Let's discard it and try the next one
                             */

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Pooled connection with a tag "
                                    << tag
                                    << "' is being discarded due to it being kept idle for more than "
                                    << m_maxIdleTimeoutInSeconds
                                    << " seconds or unknown time"
                                );

                            continue;
                        }
                    }

                    break;
                }

                return connection;
            }

            void putConnection(
                SAA_in          const std::string&                  tag,
                SAA_in          stream_ref&&                        value
                )
            {
                BL_MUTEX_GUARD( m_lock );

                const auto* key = value.get();
                BL_ASSERT( ! cpp::contains( m_connectionIdleSince, key ) );

                const auto pair = m_connectionIdleSince.emplace( key, time::second_clock::universal_time() );
                auto guard = BL_SCOPE_GUARD( m_connectionIdleSince.erase( pair.first ); );

                m_connectionsPool -> put( tag, BL_PARAM_FWD( value ) );
                guard.dismiss();
            }
        };

        template
        <
            typename STREAM
        >
        using SendRecvContextImpl = om::ObjectImpl< SendRecvContextT< STREAM > >;

        typedef SendRecvContextImpl< tasks::TcpSocketAsyncBase >        SendRecvContext;
        typedef SendRecvContextImpl< tasks::TcpSslSocketAsyncBase >     SslSendRecvContext;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_SENDRECVCONTEXT_H_ */
