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

#ifndef __BL_WATCHDOG_H_
#define __BL_WATCHDOG_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    /**
     * @brief class Watchdog
     *
     * Implements a "watchdog" task that monitors timestamps updated by
     * other tasks and kills the application after a configurable period
     * of inactivity.
     */

    template
    <
        typename E = void
    >
    class WatchdogT FINAL
    {
        BL_NO_COPY_OR_MOVE( WatchdogT )

    public:

        enum : std::size_t
        {
            MAX_MONITORS = 4096U
        };

    private:

        /*
         * The reason why atomics are used instead of mutex + integers
         * is not for performance, but to avoid any possibility of
         * exceptions or dead locks (via operations on mutexes, or I/O)
         * in the main loop of the watchdog.
         *
         * To avoid resizing and associated concurrency difficulties,
         * this implementation fixes the maximum number of monitors.
         */

        std::array< std::atomic< std::uint64_t >, MAX_MONITORS >            m_expirations;

        std::atomic< std::size_t >                                          m_monitorCount;

        /*
         * Individual monitors are accessed by names - thus the map from
         * monitor name to the monitor index (in the m_expirations array).
         * The reason why this map does not directly contain the expirations
         * as values is the avoidance of any locking inside the map loop
         * of the watchdog. If the map contained the expirations, it would
         * have to be locked for reads because writes would potentially
         * be happening concurrently.
         */

        mutable os::mutex                                                   m_namesMutex;
        std::unordered_map< std::string, std::size_t >                      m_names;

        const time::time_duration                                           m_checkingInterval;
        const std::uint64_t                                                 m_checkingIntervalInMicros;

        std::uint64_t timeDurationInCheckingIntervals( SAA_in const time::time_duration& duration ) const NOEXCEPT
        {
            const auto durationInMicroseconds = static_cast< std::uint64_t >( duration.total_microseconds() );
            const auto numberOfIntervals = ( durationInMicroseconds + m_checkingIntervalInMicros - 1 ) / m_checkingIntervalInMicros;
            return static_cast< std::uint64_t >( numberOfIntervals );
        }

    public:

        /**
         * @brief This constructor is public and allows creating independent
         * instances of this class for unit testing.
         */

        WatchdogT( SAA_in const time::time_duration& checkingInterval )
            :
            m_monitorCount( 0U ),
            m_checkingInterval( checkingInterval ),
            m_checkingIntervalInMicros( static_cast< std::uint64_t >( m_checkingInterval.total_microseconds() ) )
        {
            /*
             * Atomics require explicit initialization
             * Setting them all to std::numeric_limits< std::unit64_t >::max()
             * simplifies the extendExpiration method, because the main loop
             * of the watchdog will effectively ignore very large expirations.
             */

            for( std::size_t i = 0; i < MAX_MONITORS; ++i )
            {
                m_expirations[ i ] = std::numeric_limits< std::uint64_t >::max();
            }
        }

        std::size_t setupMonitor( SAA_in const std::string& name )
        {
            std::size_t monitorIndex;

            {
                BL_MUTEX_GUARD( m_namesMutex );

                const auto pos = m_names.find( name );

                if( pos == m_names.end() )
                {
                    /*
                     * We are holding the lock on m_namesMutex here.
                     * Therefore, we can read and modify m_monitorCount
                     * without "compare-and-swap" logic.
                     */

                    monitorIndex = m_monitorCount;

                    if( monitorIndex >= MAX_MONITORS )
                    {
                        BL_THROW(
                            UnexpectedException(),
                            "Maximum number of monitors reached"
                            );
                    }

                    m_monitorCount = monitorIndex + 1;
                    m_names.emplace( name, monitorIndex );
                }
                else
                {
                    monitorIndex = pos -> second;
                }
            }

            return monitorIndex;
        }

        /**
         * @brief Set up a monitor with a given name and timeout.
         * If the monitor with such name already exists,
         * extends the expiration of the monitor by specified
         * timeout value.
         * If a monitor cannot be setup due to monitor array
         * size restriction, this function throws UnexpectedException.
         */

        void extendExpiration(
            SAA_in      const std::string&                name,
            SAA_in      const time::time_duration&        extension
            )
        {
            const auto monitorIndex = setupMonitor( name );
            m_expirations[ monitorIndex ] = timeDurationInCheckingIntervals( extension ) + 1;
        }

        /**
         * @brief Produces a list of monitors that would expire (and trigger unless
         * extended) within the specified time horizon.
         */

        std::vector< std::string > expiringMonitors( SAA_in const time::time_duration& horizon )
        {
            const auto horizonInIntervals = timeDurationInCheckingIntervals( horizon );

            std::vector< std::string > result;

            {
                /*
                 * Code below uses m_names, therefore needs to
                 * be under the lock
                 */

                BL_MUTEX_GUARD( m_namesMutex );

                for( const auto& entry : m_names )
                {
                    const auto& name = entry.first;
                    const auto& monitorIndex = entry.second;
                    const auto expiration = m_expirations[ monitorIndex ].load();

                    if( expiration <= horizonInIntervals )
                    {
                        result.push_back( name );
                    }
                }
            }

            return result;
        }

        /**
         * @brief Needs to be called by the thread that "powers" the watchdog.
         * When this method returns, it means that the watchdog has
         * noticed condition which should lead to termination.
         * This abstracts the implementation from the instantiation of
         * the watcher thread, as well as from the specifics of the
         * termination logic.
         * maxIterations parameters allows negative unit-testing.
         * In the real system, this would be set to a very large value.
         *
         * Returns number of iterations performed (useful primarily
         * for the unit tests)
         */

        std::uint64_t run( SAA_in const std::uint64_t maxIterations ) NOEXCEPT
        {
            for( std::uint64_t iteration = 0U; iteration < maxIterations; ++iteration )
            {
                const auto monitorCount = m_monitorCount.load();

                for( std::size_t i = 0U; i < monitorCount; ++i )
                {
                    if( ! --m_expirations[ i ] )
                    {
                        return iteration;
                    }
                }

                os::sleep( m_checkingInterval );
            }

            return maxIterations;
        }

    };

    typedef WatchdogT<> Watchdog;

} // bl

#endif /* __BL_WATCHDOG_H_ */
