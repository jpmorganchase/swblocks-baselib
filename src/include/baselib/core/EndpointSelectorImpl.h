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

#ifndef __BL_ENDPOINTSELECTORIMPL_H_
#define __BL_ENDPOINTSELECTORIMPL_H_

#include <baselib/core/EndpointSelector.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BoxedObjects.h>
#include <baselib/core/BaseIncludes.h>

#include <vector>

namespace bl
{
    namespace detail
    {
        /**
         * @brief class EndpointCircularIteratorBaseT - the base class for the endpoint iterators
         */

        template
        <
            typename E = void
        >
        class EndpointCircularIteratorBaseT : public EndpointCircularIterator
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( EndpointCircularIteratorBaseT, EndpointCircularIterator )

        protected:

            typedef EndpointCircularIterator                                            base_type;

            time::system_time                                                           m_approxLastRetryTime;

            const cpp::ScalarTypeIniter< std::size_t >                                  m_maxRetryCount;
            const time::time_duration                                                   m_retryTimeout;

            EndpointCircularIteratorBaseT(
                SAA_in_opt                          const std::size_t                   maxRetryCount =
                    base_type::DEFAULT_MAX_RETRY_COUNT,
                SAA_in_opt                          const time::time_duration           retryTimeout =
                    time::seconds( base_type::DEFAULT_RETRY_TIMEOUT_IN_SECONDS )
                )
                :
                m_maxRetryCount( maxRetryCount ),
                m_retryTimeout( retryTimeout )
            {
            }

        public:

            virtual bool canRetryNow( SAA_inout_opt time::time_duration* timeout = nullptr ) NOEXCEPT OVERRIDE
            {
                if( ! canRetry() )
                {
                    /*
                     * Retry is not allowed to begin with
                     */

                    if( timeout )
                    {
                        *timeout = time::milliseconds( 0 );
                    }

                    return false;
                }

                const auto now = os::get_system_time();

                if( m_approxLastRetryTime.is_neg_infinity() )
                {
                    m_approxLastRetryTime = now;

                    return true;
                }

                const auto nextAttempt = m_approxLastRetryTime + m_retryTimeout;

                if( now >= nextAttempt )
                {
                    /*
                     * The time elapsed since the last retry is bigger or equal to
                     * m_retryTimeout - we can allow the retry
                     */

                    m_approxLastRetryTime = now;

                    if( timeout )
                    {
                        *timeout = time::milliseconds( 0 );
                    }

                    return true;
                }

                /*
                 * The time elapsed since the last retry is less than
                 * m_retryTimeout - i.e. not ok to retry now
                 *
                 * We always require the timeout to be 100 milliseconds extra
                 * to ensure that a wait operation on it will be satisfied
                 */

                if( timeout )
                {
                    *timeout = ( nextAttempt - now ) + time::milliseconds( 200 );
                }

                return false;
            }

            virtual std::size_t maxRetryCount() NOEXCEPT OVERRIDE
            {
                return m_maxRetryCount;
            }

            virtual const time::time_duration& retryTimeout() NOEXCEPT OVERRIDE
            {
                return m_retryTimeout;
            }
        };

        typedef EndpointCircularIteratorBaseT<> EndpointCircularIteratorBase;
    }

    /**
     * @brief class SimpleEndpointSelector - a simple endpoint selector
     * bound to a single fixed endpoint
     */

    template
    <
        typename E = void
    >
    class SimpleEndpointSelectorT : public EndpointSelector
    {
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( SimpleEndpointSelectorT, EndpointSelector )

    protected:

        /**
         * @brief class SimpleEndpointCircularIteratorImpl - a simple endpoint circular
         * iterator bound to a single fixed endpoint
         */

        template
        <
            typename E2 = void
        >
        class SimpleEndpointCircularIteratorT : public detail::EndpointCircularIteratorBase
        {
            BL_DECLARE_OBJECT_IMPL( SimpleEndpointCircularIteratorT )

        protected:

            typedef detail::EndpointCircularIteratorBase                                base_type;

            using base_type::m_maxRetryCount;
            using base_type::m_retryTimeout;

            const std::string                                                           m_host;
            const unsigned short                                                        m_port;

            cpp::ScalarTypeIniter< std::size_t >                                        m_retryCount;

            SimpleEndpointCircularIteratorT(
                SAA_in                              std::string&&                       host,
                SAA_in                              const unsigned short                port,
                SAA_in_opt                          const std::size_t                   maxRetryCount =
                    base_type::DEFAULT_MAX_RETRY_COUNT,
                SAA_in_opt                          const time::time_duration           retryTimeout =
                    time::seconds( base_type::DEFAULT_RETRY_TIMEOUT_IN_SECONDS )
                )
                :
                base_type( maxRetryCount, retryTimeout ),
                m_host( std::forward< std::string >( host ) ),
                m_port( port )
            {
                m_approxLastRetryTime = time::neg_infin;
            }

        public:

            virtual bool selectNext() OVERRIDE
            {
                /*
                 * We assume the last retry time was near the time selectNext was called on
                 * the iterator
                 */

                ++m_retryCount;
                m_approxLastRetryTime = os::get_system_time();

                return canRetry();
            }

            virtual std::size_t count() const NOEXCEPT OVERRIDE
            {
                /*
                 * The selector is bound to a single fixed endpoint
                 */

                return 1U;
            }

            virtual const std::string& host() const NOEXCEPT OVERRIDE
            {
                return m_host;
            }

            virtual unsigned short port() const NOEXCEPT OVERRIDE
            {
                return m_port;
            }

            virtual void resetRetry() NOEXCEPT OVERRIDE
            {
                m_retryCount = 0U;
                m_approxLastRetryTime = time::neg_infin;
            }

            virtual bool canRetry() NOEXCEPT OVERRIDE
            {
                return ( m_retryCount < m_maxRetryCount );
            }
        };

        typedef om::ObjectImpl< SimpleEndpointCircularIteratorT<> >                 iterator_t;

        const std::string                                                           m_host;
        const unsigned short                                                        m_port;

        const cpp::ScalarTypeIniter< std::size_t >                                  m_maxRetryCount;
        const time::time_duration                                                   m_retryTimeout;

        SimpleEndpointSelectorT(
            SAA_in                              std::string&&                       host,
            SAA_in                              const unsigned short                port,
            SAA_in_opt                          const std::size_t                   maxRetryCount =
                EndpointCircularIterator::DEFAULT_MAX_RETRY_COUNT,
            SAA_in_opt                          const time::time_duration           retryTimeout =
                time::seconds( EndpointCircularIterator::DEFAULT_RETRY_TIMEOUT_IN_SECONDS )
            )
            :
            m_host( std::forward< std::string >( host ) ),
            m_port( port ),
            m_maxRetryCount( maxRetryCount ),
            m_retryTimeout( retryTimeout )
        {
        }

    public:

        virtual om::ObjPtr< EndpointCircularIterator > createIterator() OVERRIDE
        {
            return iterator_t::template createInstance< EndpointCircularIterator >(
                cpp::copy( m_host ),
                m_port,
                m_maxRetryCount,
                m_retryTimeout
                );
        }

        virtual std::size_t count() const NOEXCEPT OVERRIDE
        {
            /*
             * The selector is bound to a single fixed endpoint
             */

            return 1U;
        }
    };

    typedef om::ObjectImpl< SimpleEndpointSelectorT<> > SimpleEndpointSelectorImpl;

    /**
     * @brief class EndpointSelectorImpl - an endpoint selector generic implementation
     */

    template
    <
        typename E = void
    >
    class EndpointSelectorImplT : public EndpointSelector
    {
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( EndpointSelectorImplT, EndpointSelector )

    protected:

        typedef std::vector< std::pair< std::string, unsigned short > >             entry_vector_t;
        typedef om::ObjectImpl< om::BoxedValueObject< entry_vector_t > >            boxed_entry_vector_t;
        const om::ObjPtr< boxed_entry_vector_t >                                    m_entries;
        const unsigned short                                                        m_port;

        template
        <
            typename E2 = void
        >
        class EndpointSelectorIteratorImplT : public detail::EndpointCircularIteratorBase
        {
            BL_DECLARE_OBJECT_IMPL( EndpointSelectorIteratorImplT )

        protected:

            typedef detail::EndpointCircularIteratorBase                                base_type;

            using base_type::m_maxRetryCount;
            using base_type::m_retryTimeout;

            typedef EndpointSelectorImplT< E2 >                                     selector_t;

            const om::ObjPtr< selector_t >                                          m_selector;
            std::size_t                                                             m_index;

            std::map< std::size_t, cpp::ScalarTypeIniter< std::size_t > >           m_retryCounts;

            EndpointSelectorIteratorImplT( SAA_in const om::ObjPtr< selector_t >& selector )
                :
                m_selector( om::copy( selector ) ),
                m_index( 0U )
            {
                chkIndex();

                m_approxLastRetryTime = time::neg_infin;
            }

            void chkIndex()
            {
                BL_CHK(
                    false,
                    m_index != m_selector -> m_entries -> value().size(),
                    BL_MSG()
                        << "Endpoint selector is empty"
                    );
            }

        public:

            virtual bool selectNext() OVERRIDE
            {
                ++m_index;

                if( m_index == m_selector -> m_entries -> value().size() )
                {
                    m_index = 0U;
                    chkIndex();
                }

                auto& retryCount = m_retryCounts[ m_index ];

                ++retryCount;
                m_approxLastRetryTime = os::get_system_time();

                return canRetry();
            }

            virtual std::size_t count() const NOEXCEPT OVERRIDE
            {
                return m_selector -> m_entries -> value().size();
            }

            virtual const std::string& host() const NOEXCEPT OVERRIDE
            {
                BL_ASSERT( m_index < m_selector -> m_entries -> value().size() );
                return m_selector -> m_entries -> value()[ m_index ].first;
            }

            virtual unsigned short port() const NOEXCEPT OVERRIDE
            {
                BL_ASSERT( m_index < m_selector -> m_entries -> value().size() );
                return m_selector -> m_entries -> value()[ m_index ].second;
            }

            virtual void resetRetry() NOEXCEPT OVERRIDE
            {
                for( auto& pair : m_retryCounts )
                {
                    pair.second = 0U;
                }

                m_approxLastRetryTime = time::neg_infin;
            }

            virtual bool canRetry() NOEXCEPT OVERRIDE
            {
                const auto& retryCount = m_retryCounts[ m_index ];

                return ( retryCount < m_maxRetryCount );
            }
        };

        typedef om::ObjectImpl< EndpointSelectorIteratorImplT<> > EndpointSelectorIteratorImpl;

        EndpointSelectorImplT(
            SAA_in                              const unsigned short                port
            )
            :
            m_entries( boxed_entry_vector_t::createInstance() ),
            m_port( port )
        {
        }

        template
        <
            typename Iterator
        >
        EndpointSelectorImplT(
            SAA_in                              const unsigned short                port,
            SAA_in                              Iterator                            first,
            SAA_in                              Iterator                            last
            )
            :
            m_entries( boxed_entry_vector_t::createInstance() ),
            m_port( port )
        {
            std::for_each(
                first,
                last,
                [ this ]( SAA_in const std::string& host ) -> void
                {
                    addHost( cpp::copy( host ) );
                }
                );
        }

    public:

        /**
         * @brief Adds host with default port
         */

        void addHost( SAA_in std::string&& host )
        {
            m_entries -> lvalue().emplace_back( std::forward< std::string >( host ), m_port );
        }

        void addHostPort(
            SAA_in                              std::string&&                       host,
            SAA_in                              const unsigned short                port
            )
        {
            m_entries -> lvalue().emplace_back( std::forward< std::string >( host ), port );
        }

        virtual om::ObjPtr< EndpointCircularIterator > createIterator() OVERRIDE
        {
            return EndpointSelectorIteratorImpl::template createInstance< EndpointCircularIterator >(
                om::copy( this )
                );
        }

        virtual std::size_t count() const NOEXCEPT OVERRIDE
        {
            return m_entries -> value().size();
        }
    };

    typedef om::ObjectImpl< EndpointSelectorImplT<> > EndpointSelectorImpl;

} // bl

#endif /* __BL_ENDPOINTSELECTORIMPL_H_ */
