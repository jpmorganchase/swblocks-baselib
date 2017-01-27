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

#ifndef __BL_LOGGABLECOUNTER_H_
#define __BL_LOGGABLECOUNTER_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/Logging.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    /**
     * @brief A loggable counter class that allows us to track the counter movement with some
     * smoothness factor - i.e. it is only logged when the delta / change is bigger than some
     * predefined threshold
     */

    template
    <
        typename T = std::size_t
    >
    class LoggableCounterT : public om::Object
    {
        BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( LoggableCounterT, om::Object )

    public:

        enum : T
        {
            MIN_DELTA_DEFAULT = 100,
        };

        enum class EventId
        {
            Create,
            Update,
            Destroy,
        };

        typedef cpp::function
        <
            void (
                SAA_in          const EventId                           eventId,
                SAA_in          const T                                 counterValue
                ) NOEXCEPT
        >
        callback_t;

    protected:

        typedef LoggableCounterT< T >                                   this_type;

        const std::string                                               m_name;
        const T                                                         m_minDeltaToLog;
        callback_t                                                      m_callback;
        Logging::Channel&                                               m_loggingChannel;
        std::atomic< T >                                                m_counter;
        std::atomic< T >                                                m_lastLoggedCounter;

        LoggableCounterT(
            SAA_in              std::string&&                           name,
            SAA_in_opt          const T                                 minDeltaToLog = MIN_DELTA_DEFAULT,
            SAA_in_opt          callback_t&&                            callback = callback_t(),
            SAA_in_opt          Logging::Channel&                       loggingChannel = Logging::debug()
            ) NOEXCEPT
            :
            m_name( BL_PARAM_FWD( name ) ),
            m_minDeltaToLog( minDeltaToLog ),
            m_loggingChannel( loggingChannel ),
            m_counter( 0 )
        {
            m_callback = callback ?
                BL_PARAM_FWD( callback )
                :
                cpp::bind( this_type::callbackDefault, _1, _2, m_name, cpp::ref( m_loggingChannel ) );

            m_callback( EventId::Create, 0U /* counterValue */ );
        }

        ~LoggableCounterT() NOEXCEPT
        {
            m_callback( EventId::Destroy, m_counter.load() /* counterValue */ );
        }

        T incrementOrDecrement( SAA_in const bool isIncrement ) NOEXCEPT
        {
            /*
             * The compare_exchange_strong( ... ) call below requires the expected parameter to be a
             * non-const reference and this is why we don't define lastLoggedCounter as const here
             */

            T lastLoggedCounter = m_lastLoggedCounter.load();

            const T counter = isIncrement ? ++m_counter : --m_counter;

            const auto newDelta =
                counter > lastLoggedCounter ? counter - lastLoggedCounter : lastLoggedCounter - counter;

            /*
             * The check for 0U == lastLoggedCounter is to ensure that at least one update is
             * done when the counter changes for the first time
             */

            if( 0U == lastLoggedCounter || newDelta >= m_minDeltaToLog )
            {
                if(
                    m_lastLoggedCounter.compare_exchange_strong(
                        lastLoggedCounter           /* expected */,
                        counter                     /* desired */
                        )
                    )
                {
                    m_callback( EventId::Update, counter );
                }
            }

            return counter;
        }

    public:

        static void callbackDefault(
            SAA_in          const EventId                           eventId,
            SAA_in          const T                                 counterValue,
            SAA_in          const std::string&                      name,
            SAA_in          Logging::Channel&                       loggingChannel
            ) NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            switch( eventId )
            {
                default:
                    BL_RIP_MSG( "Unexpected event id value" );
                    break;

                case EventId::Create:
                    {
                        BL_LOG(
                            loggingChannel,
                            BL_MSG()
                                << "Loggable counter '"
                                << name
                                << "' was created"
                            );
                    }
                    break;

                case EventId::Update:
                    {
                        BL_LOG(
                            loggingChannel,
                            BL_MSG()
                                << "Loggable counter '"
                                << name
                                << "' was updated; new counter value is "
                                << counterValue
                            );
                    }
                    break;

                case EventId::Destroy:
                    {
                        BL_LOG(
                            loggingChannel,
                            BL_MSG()
                                << "Loggable counter '"
                                << name
                                << "' was destroyed; last counter value was "
                                << counterValue
                            );
                    }
                    break;
            }

            BL_NOEXCEPT_END()
        }

        T minDeltaToLog() const NOEXCEPT
        {
            return m_minDeltaToLog;
        }

        T increment() NOEXCEPT
        {
            return incrementOrDecrement( true /* increment */ );
        }

        T decrement() NOEXCEPT
        {
            return incrementOrDecrement( false /* increment */ );
        }
    };

    typedef LoggableCounterT<> LoggableCounterDefault;
    typedef om::ObjectImpl< LoggableCounterDefault > LoggableCounterDefaultImpl;

} // bl

#endif /* __BL_LOGGABLECOUNTER_H_ */
