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

#ifndef __BL_UTILS_H_
#define __BL_UTILS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/lexical_cast.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/Logging.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace utils
    {
        using boost::lexical_cast;
        using boost::conversion::try_lexical_convert;
        using boost::bad_lexical_cast;

        namespace detail
        {
            /**
             * @brief class Utils
             */

            template
            <
                typename E = void
            >
            class UtilsT FINAL
            {
                BL_DECLARE_STATIC( UtilsT )

            public:

                enum
                {
                    RETRY_COUNT_DEFAULT = 5,
                };

                enum : long
                {
                    RETRY_TIMEOUT_IN_MILLISECONDS_DEFAULT = 500,
                };

                static time::time_duration timeoutDefault()
                {
                    return time::milliseconds( RETRY_TIMEOUT_IN_MILLISECONDS_DEFAULT );
                }

                static time::time_duration timeoutNoDelay()
                {
                    return time::neg_infin;
                }

                template
                <
                    typename R,
                    typename EXCEPTION
                >
                static R retryOnError(
                    SAA_in          cpp::function< R () >                       cb,
                    SAA_in_opt      const std::size_t                           retryCount = RETRY_COUNT_DEFAULT,
                    SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
                    SAA_in_opt      const bool                                  logException = false
                    )
                {
                    std::size_t retries = 0;

                    for( ;; )
                    {
                        try
                        {
                            return cb();

                            break;
                        }
                        catch( EXCEPTION& e )
                        {
                            if( retries < retryCount )
                            {
                                if( logException )
                                {
                                    BL_LOG_MULTILINE(
                                        Logging::debug(),
                                        BL_MSG()
                                            << "An exception occurred while performing retryable operation:\n"
                                            << eh::diagnostic_information( e )
                                        );
                                }

                                if( ! retryTimeout.is_special() )
                                {
                                    os::sleep( retryTimeout );
                                }

                                ++retries;
                                continue;
                            }

                            throw;
                        }
                    }
                }
            };

            typedef UtilsT<> Utils;

        } // detail

        /**
         * @brief Find the last value in range (the opposite of std::find)
         *
         * @returns An iterator to the last element in the range [first,last) that compares equal to val.
         * If no such element is found, the function returns last.
         *
         * @note find_last requires bidirectional iterators to compile (unlike std::find)
         */
        template
        <
            typename Iterator,
            typename T
        >
        inline Iterator find_last(
            SAA_in      const Iterator      first,
            SAA_in      const Iterator      last,
            SAA_in      const T&            val
            )
        {
            for( Iterator iter = last; iter != first; )
            {
                if( *--iter == val )
                {
                    return iter;
                }
            }

            return last;
        }

        inline time::time_duration timeoutDefault()
        {
            return detail::Utils::timeoutDefault();
        }

        inline time::time_duration timeoutNoDelay()
        {
            return detail::Utils::timeoutNoDelay();
        }

        template
        <
            typename EXCEPTION
        >
        inline void retryOnError(
            SAA_in          const cpp::void_callback_t&                 cb,
            SAA_in_opt      const std::size_t                           retryCount = detail::Utils::RETRY_COUNT_DEFAULT,
            SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
            SAA_in_opt      const bool                                  logException = false
            )
        {
            detail::Utils::retryOnError< void, EXCEPTION >( cb, retryCount, retryTimeout, logException );
        }

        inline void retryOnAllErrors(
            SAA_in          const cpp::void_callback_t&                 cb,
            SAA_in_opt      const std::size_t                           retryCount = detail::Utils::RETRY_COUNT_DEFAULT,
            SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
            SAA_in_opt      const bool                                  logException = false
            )
        {
            detail::Utils::retryOnError< void, std::exception >( cb, retryCount, retryTimeout, logException );
        }

        inline bool tryRetryOnAllErrors(
            SAA_in          const cpp::void_callback_t&                 cb,
            SAA_in_opt      const std::size_t                           retryCount = detail::Utils::RETRY_COUNT_DEFAULT,
            SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
            SAA_in_opt      const bool                                  logException = false
            )
        {
            try
            {
                detail::Utils::retryOnError< void, std::exception >( cb, retryCount, retryTimeout, logException );

                return true;
            }
            catch( std::exception& )
            {
                return false;
            }
        }

        template
        <
            typename R,
            typename EXCEPTION
        >
        inline R retryOnError(
            SAA_in          cpp::function< R () >                       cb,
            SAA_in_opt      const std::size_t                           retryCount = detail::Utils::RETRY_COUNT_DEFAULT,
            SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
            SAA_in_opt      const bool                                  logException = false
            )
        {
            return detail::Utils::retryOnError< R, EXCEPTION >( cb, retryCount, retryTimeout, logException );
        }

        template
        <
            typename R
        >
        inline R retryOnAllErrors(
            SAA_in          cpp::function< R () >                       cb,
            SAA_in_opt      const std::size_t                           retryCount = detail::Utils::RETRY_COUNT_DEFAULT,
            SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
            SAA_in_opt      const bool                                  logException = false
            )
        {
            return detail::Utils::retryOnError< R, std::exception >( cb, retryCount, retryTimeout, logException );
        }

        template
        <
            typename R
        >
        inline auto tryRetryOnAllErrors(
            SAA_in          cpp::function< R () >                       cb,
            SAA_in_opt      const std::size_t                           retryCount = detail::Utils::RETRY_COUNT_DEFAULT,
            SAA_in_opt      const time::time_duration&                  retryTimeout = timeoutNoDelay(),
            SAA_in_opt      const bool                                  logException = false
            ) -> std::pair< R, bool >
        {
            try
            {
                return std::make_pair(
                    detail::Utils::retryOnError< R, std::exception >( cb, retryCount, retryTimeout, logException ),
                    true
                    );
            }
            catch( std::exception& )
            {
                return std::make_pair( R(), false );
            }
        }

        template
        <
            typename E = void
        >
        class ExecutionTimerT FINAL
        {
            BL_NO_COPY( ExecutionTimerT )

        private:

            std::string                                             m_name;
            Logging::Channel*                                       m_channel;
            long                                                    m_durationThresholdInSeconds;
            decltype( time::microsec_clock::universal_time() )      m_startTime;
            cpp::ScalarTypeIniter< bool >                           m_canceled;

        public:

            enum : long
            {
                NO_DURATION_THRESHOLD = -1
            };

            ExecutionTimerT(
                SAA_in_opt      std::string&&                       name = std::string(),
                SAA_in_opt      Logging::Channel&                   channel = Logging::notify(),
                SAA_in_opt      const long                          durationThresholdInSeconds = NO_DURATION_THRESHOLD
                )
                :
                m_name( BL_PARAM_FWD( name ) ),
                m_channel( &channel ),
                m_durationThresholdInSeconds( durationThresholdInSeconds )
            {
                m_startTime = time::microsec_clock::universal_time();
            }

            ExecutionTimerT( SAA_in ExecutionTimerT&& other )
                :
                m_name( std::move( other.m_name ) ),
                m_channel( std::move( other.m_channel ) ),
                m_durationThresholdInSeconds( std::move( other.m_durationThresholdInSeconds ) ),
                m_startTime( std::move( other.m_startTime ) )
            {
            }

            ExecutionTimerT& operator =( SAA_in ExecutionTimerT&& other )
            {
                m_name = std::move( other.m_name );
                m_channel = std::move( other.m_channel );
                m_durationThresholdInSeconds = std::move( other.m_durationThresholdInSeconds );
                m_startTime = std::move( other.m_startTime );
                return *this;
            }

            void cancel()
            {
                m_canceled = true;
            }

            ~ExecutionTimerT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( ! m_canceled )
                {
                    const auto duration = time::microsec_clock::universal_time() - m_startTime;

                    const auto durationThresholdInSecondsSatisfied =
                        m_durationThresholdInSeconds == NO_DURATION_THRESHOLD ||
                        duration > time::time_duration( time::seconds( m_durationThresholdInSeconds ) );

                    if( durationThresholdInSecondsSatisfied )
                    {
                        BL_LOG(
                            *m_channel,
                            BL_MSG()
                                << ( m_name.empty() ? "Command" : m_name )
                                << " completed in "
                                << time::to_simple_string( duration )
                            );
                    }
                }

                BL_NOEXCEPT_END()
            }
        };

        typedef ExecutionTimerT<> ExecutionTimer;

         /**
         * @brief enum class LogFlags
         */

        enum class LogFlags
        {
            NONE,
            DEFAULT,
            DEBUG_ONLY
        };

        namespace detail
        {
             /**
             * @brief class TryCatchLog
             */

            template
            <
                typename E = void
            >
            class TryCatchLogT
            {
                BL_DECLARE_STATIC( TryCatchLogT )

            public:

                template
                <
                    typename RETURN,
                    typename EXCEPTION
                >
                static RETURN tryCatchLogImpl(
                    SAA_in              const cpp::function< std::string () >&          cbMessage,
                    SAA_in              const cpp::function< RETURN () >&               cbBlock,
                    SAA_in_opt          const cpp::function< RETURN () >&               cbOnError,
                    SAA_in_opt          LogFlags                                        logFlags
                    )
                {
                    /*
                     * If RETURN is not void, then the code should return from both 'try' and 'catch' blocks,
                     * and never reach the end of tryCatchLogImpl function for non-void RETURNs
                     */

                    BL_ASSERT( ( std::is_same< void, RETURN >::value || cbOnError ) );

                    try
                    {
                        return cbBlock();
                    }
                    catch( EXCEPTION& e )
                    {
                        if( logFlags == LogFlags::DEFAULT )
                        {
                            BL_LOG_MULTILINE(
                                Logging::warning(),
                                BL_MSG()
                                    << cbMessage()
                                    << " ["
                                    << e.what()
                                    << "]"
                                );
                        }

                        if( logFlags == LogFlags::DEFAULT || logFlags == LogFlags::DEBUG_ONLY )
                        {
                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << ( logFlags == LogFlags::DEBUG_ONLY ? cbMessage() : "" )
                                    << "\nFull exception details:\n"
                                    << eh::diagnostic_information( e )
                                );
                        }

                        if( cbOnError )
                        {
                            return cbOnError();
                        }
                    }

                    return returnIfVoid( static_cast< RETURN* >( nullptr ) );
                }

            private:

                template
                <
                    typename RETURN
                >
                static RETURN returnIfVoid( RETURN* )
                {
                    BL_THROW( UnexpectedException(), "Shouldn't be here, RETURN type is non-void" );
                }

                static void returnIfVoid( void* )
                {
                }
            };

            typedef TryCatchLogT<> TryCatchLog;

        } // detail

        template
        <
            typename RETURN,
            typename EXCEPTION
        >
        inline RETURN tryCatchLog(
            SAA_in              const cpp::function< std::string () >&          cbMessage,
            SAA_in              const cpp::function< RETURN () >&               cbBlock,
            SAA_in_opt          const cpp::function< RETURN () >&               cbOnError = cpp::function< RETURN () >(),
            SAA_in_opt          LogFlags                                        logFlags = LogFlags::DEFAULT
            )
        {
            return detail::TryCatchLog::tryCatchLogImpl< RETURN, EXCEPTION >( cbMessage, cbBlock, cbOnError, logFlags );
        }

        template
        <
            typename RETURN
        >
        inline RETURN tryCatchLog(
            SAA_in              const cpp::function< std::string () >&          cbMessage,
            SAA_in              const cpp::function< RETURN () >&               cbBlock,
            SAA_in_opt          const cpp::function< RETURN () >&               cbOnError = cpp::function< RETURN () >(),
            SAA_in_opt          LogFlags                                        logFlags = LogFlags::DEFAULT
            )
        {
            return detail::TryCatchLog::tryCatchLogImpl< RETURN, std::exception >( cbMessage, cbBlock, cbOnError, logFlags );
        }

        inline void tryCatchLog(
            SAA_in              const cpp::function< std::string () >&          cbMessage,
            SAA_in              const cpp::function< void () >&                 cbBlock,
            SAA_in_opt          const cpp::function< void () >&                 cbOnError = cpp::function< void () >(),
            SAA_in_opt          LogFlags                                        logFlags = LogFlags::DEFAULT
            )
        {
            detail::TryCatchLog::tryCatchLogImpl< void, std::exception >( cbMessage, cbBlock, cbOnError, logFlags );
        }

        template
        <
            typename RETURN,
            typename EXCEPTION
        >
        inline RETURN tryCatchLog(
            SAA_in              const char*                                     message,
            SAA_in              const cpp::function< RETURN () >&               cbBlock,
            SAA_in_opt          const cpp::function< RETURN () >&               cbOnError = cpp::function< RETURN () >(),
            SAA_in_opt          LogFlags                                        logFlags = LogFlags::DEFAULT
            )
        {
            return detail::TryCatchLog::tryCatchLogImpl< RETURN, EXCEPTION >(
                [ & ]() -> std::string
                {
                    return message;
                },
                cbBlock,
                cbOnError,
                logFlags
                );
        }

        template
        <
            typename RETURN
        >
        inline RETURN tryCatchLog(
            SAA_in              const char*                                     message,
            SAA_in              const cpp::function< RETURN () >&               cbBlock,
            SAA_in_opt          const cpp::function< RETURN () >&               cbOnError = cpp::function< RETURN () >(),
            SAA_in_opt          LogFlags                                        logFlags = LogFlags::DEFAULT
            )
        {
            return detail::TryCatchLog::tryCatchLogImpl< RETURN, std::exception >(
                [ & ]() -> std::string
                {
                    return message;
                },
                cbBlock,
                cbOnError,
                logFlags
                );
        }

        inline void tryCatchLog(
            SAA_in              const char*                                     message,
            SAA_in              const cpp::function< void () >&                 cbBlock,
            SAA_in_opt          const cpp::function< void () >&                 cbOnError = cpp::function< void () >(),
            SAA_in_opt          LogFlags                                        logFlags = LogFlags::DEFAULT
            )
        {
            detail::TryCatchLog::tryCatchLogImpl< void, std::exception >(
                [ & ]() -> std::string
                {
                    return message;
                },
                cbBlock,
                cbOnError,
                logFlags
                );
        }

    } // utils

} // bl

#endif /* __BL_UTILS_H_ */
