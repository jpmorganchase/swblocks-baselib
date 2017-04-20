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

#ifndef __BL_SECURESTRINGWRAPPER_H_
#define __BL_SECURESTRINGWRAPPER_H_

#include <baselib/core/BaseIncludes.h>
#include <baselib/core/CPP.h>

namespace bl
{
    namespace str
    {
        using cpp::secureWipe;

        /*
         * The design of this class is affected by the fact that
         * move operations for strings which implement SSO do not
         * work as expected; copying data for small strings instead of moving.
         */

        template
        <
            typename T = void
        >
        class SecureStringWrapperT
        {
        public:

            enum : std::size_t
            {
                INITIAL_CAPACITY = 16u
            };

            SecureStringWrapperT( SAA_inout std::string* externalString = nullptr )
                : m_implPtr( externalString ? externalString : &m_impl )
            {
            }

            SecureStringWrapperT( SAA_in const SecureStringWrapperT& other )
                : m_implPtr( &m_impl )
            {
                append( *other.m_implPtr );
            }

            SecureStringWrapperT( SAA_in SecureStringWrapperT&& other )
                : m_implPtr( &m_impl )
            {
                append( *other.m_implPtr );
                other.clear();
            }

            explicit SecureStringWrapperT( SAA_in std::string&& other )
                : m_implPtr( &m_impl )
            {
                append( other );
                secureClear( other );
            }

            SecureStringWrapperT& operator=( SAA_in const SecureStringWrapperT& other )
            {
                if( this == &other )
                {
                    return *this;
                }

                clear();
                append( *other.m_implPtr );

                return *this;
            }

            SecureStringWrapperT& operator=( SAA_in SecureStringWrapperT&& other )
            {
                if( this == &other )
                {
                    return *this;
                }

                return *this = std::move( *other.m_implPtr );
            }

            SecureStringWrapperT& operator=( SAA_in std::string&& other )
            {
                clear();
                append( other );
                secureClear( other );

                return *this;
            }

            ~SecureStringWrapperT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                clear();

                BL_NOEXCEPT_END()
            }

            void append( SAA_in const SecureStringWrapperT& other )
            {
                append( *other.m_implPtr );
            }

            void append( SAA_in const std::string& other )
            {
                grow( size() + other.size() );

                for( const auto ch : other )
                {
                    m_implPtr -> push_back( ch );
                }
            }

            void append( SAA_in const char ch )
            {
                grow( size() + 1 );

                m_implPtr -> push_back( ch );
            }

            const std::string& getAsNonSecureString() const NOEXCEPT
            {
                return *m_implPtr;
            }

            std::size_t size() const NOEXCEPT
            {
                return m_implPtr -> size();
            }

            bool empty() const NOEXCEPT
            {
                return m_implPtr -> empty();
            }

            void clear() NOEXCEPT
            {
                secureClear( *m_implPtr );
            }

        private:

            static void secureClear( SAA_inout std::string& other ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                secureWipe( other );
                other.clear();

                BL_NOEXCEPT_END()
            }

            std::size_t calculateNewCapacity( SAA_in std::size_t newSize )
            {
                auto requiredCapacity =
                    std::max( m_implPtr -> capacity(), static_cast< std::size_t >( INITIAL_CAPACITY ) );

                if( newSize > requiredCapacity )
                {
                    do
                    {
                        requiredCapacity <<= 1;
                    }
                    while( newSize > requiredCapacity );
                }

                return requiredCapacity;
            }

            void grow( SAA_in const std::size_t newSize )
            {
                if( m_implPtr -> empty() )
                {
                    m_implPtr -> reserve( calculateNewCapacity( newSize ) );
                }
                else if( newSize > m_implPtr -> capacity() )
                {
                    SecureStringWrapperT temp( *this );

                    clear();

                    m_implPtr -> reserve( calculateNewCapacity( newSize ) );
                    m_implPtr -> assign( *temp.m_implPtr );
                }
            }

        private:

            std::string                                     m_impl;
            std::string*                                    m_implPtr;
        };

        using SecureStringWrapper = SecureStringWrapperT<>;

    } // str

} // bl

#endif /* __BL_SECURESTRINGWRAPPER_H_ */
