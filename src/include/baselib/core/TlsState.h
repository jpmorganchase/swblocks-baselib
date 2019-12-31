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

#ifndef __BL_TLSSTATE_H_
#define __BL_TLSSTATE_H_

#include <baselib/core/detail/UuidBoostImports.h>
#include <baselib/core/detail/OSBoostImports.h>
#include <baselib/core/detail/RandomBoostImports.h>
#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/CPP.h>

namespace bl
{
    /**
     * @brief Thread-Local Storage (TLS) state
     */

    template
    <
        typename E = void
    >
    class TlsStateT
    {
    public:

        typedef TlsStateT< E > this_type;

        struct Logging
        {
            Logging()
                :
                level( -1 )
            {
            }

            int         level;
        }
        logging;

        class Random
        {
        public:

            uuids::random_generator& uuidGen()
            {
                if( ! m_uuidGen )
                {
                    m_uuidGen.reset( new uuids::random_generator( urng() ) );
                }

                return *m_uuidGen;
            }

            random::mt19937& urng()
            {
                if( ! m_urng )
                {
                    m_urng.reset( new random::mt19937 );
                    random::seed( *m_urng );
                }

                return *m_urng;
            }

        private:

            cpp::SafeUniquePtr< random::mt19937 >               m_urng;
            cpp::SafeUniquePtr< uuids::random_generator >       m_uuidGen;
        }
        random;

        struct
        {
            cpp::ScalarTypeIniter< bool >                       xmlInitialized;
            cpp::ScalarTypeIniter< bool >                       xsltInitialized;
        }
        xml;

        /*
         * Note that the default interface can throw
         */

        static auto tlsData() -> this_type&
        {
            if( nullptr == g_tlsData.get() )
            {
                g_tlsData.reset( new this_type );
            }

            return *g_tlsData.get();
        }

        /*
         * This interface can only be called if we know the tlsData
         * has already been initialized
         */

        static auto tlsDataNoThrow() NOEXCEPT -> this_type&
        {
            if( nullptr == g_tlsData.get() )
            {
                BL_RIP_MSG( "TLS data cannot be null" );
            }

            return *g_tlsData.get();
        }

    private:

        static os::thread_specific_ptr< this_type > g_tlsData;
    };

    typedef TlsStateT<> TlsState;

    template
    <
        typename E
    >
    os::thread_specific_ptr< typename TlsStateT< E >::this_type >
    TlsStateT< E >::g_tlsData;

    inline TlsState& tlsData()
    {
        return TlsState::tlsData();
    }

    inline TlsState& tlsDataNoThrow() NOEXCEPT
    {
        return TlsState::tlsDataNoThrow();
    }

} // bl

#endif /* __BL_TLSSTATE_H_ */
