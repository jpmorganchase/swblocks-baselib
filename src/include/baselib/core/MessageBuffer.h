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

#ifndef __BL_MESSAGEBUFFER_H_
#define __BL_MESSAGEBUFFER_H_

#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/CPP.h>

#include <sstream>
#include <memory>
#include <iostream>
#include <type_traits>

#define BL_MSG() bl::MessageBuffer()

#define BL_MSG_CB( message ) \
    [ & ]() -> std::string \
    { \
        return bl::resolveMessage( message ); \
    } \

namespace bl
{
    /**
     * @brief class MessageBuffer
     */
    template
    <
        typename E = void
    >
    class MessageBufferT
    {
        BL_NO_COPY( MessageBufferT )
        BL_CTR_DEFAULT( MessageBufferT, public )

    private:

        mutable cpp::SafeUniquePtr< cpp::SafeOutputStringStream > m_buffer;

    public:

        typedef MessageBufferT< E > this_type;

        /*
         * Since we disable copy we must implement move semantics explicitly
         */

        MessageBufferT( SAA_in MessageBufferT&& other )
            :
            m_buffer( std::move( other.m_buffer ) )
        {
        }

        MessageBufferT& operator =( SAA_in MessageBufferT&& other )
        {
            m_buffer = std::move( other.m_buffer );
            return *this;
        }

        template
        <
            typename T
        >
        const this_type& operator <<( SAA_in T&& value ) const
        {
            if( ! m_buffer )
            {
                m_buffer.reset( new cpp::SafeOutputStringStream );
            }

            ( * m_buffer ) << std::forward< T >( value );

            return *this;
        }

        const this_type& operator <<( SAA_in const this_type& value ) const
        {
            if( value.m_buffer )
            {
                *this << value.m_buffer -> str();
            }

            return *this;
        }

        std::string text() const
        {
            if( m_buffer )
            {
                return m_buffer -> str();
            }

            return "";
        }
    };

    typedef MessageBufferT<> MessageBuffer;

    namespace detail
    {
        /**
         * @brief class MessageResolver
         */
        template
        <
            bool isMessageBuffer
        >
        class MessageResolver
        {
            BL_DECLARE_STATIC( MessageResolver )

        public:

            template
            <
                typename T
            >
            static std::string resolveMsg( SAA_in T&& msg )
            {
                return std::forward< T >( msg );
            }
        };

        template
        <
        >
        class MessageResolver< true >
        {
            BL_DECLARE_STATIC( MessageResolver )

        public:

            static std::string resolveMsg( SAA_in const MessageBuffer& msg )
            {
                return msg.text();
            }
        };
    }

    template
    <
        typename T
    >
    inline std::string resolveMessage( SAA_in T&& msg )
    {
        typedef detail::MessageResolver< std::is_same< MessageBuffer, typename std::decay< decltype( msg ) >::type >::value >
            resolver_t;

        return resolver_t::resolveMsg( std::forward< T >( msg ) );
    }

} // bl

#endif /* __BL_MESSAGEBUFFER_H_ */

