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

            SecureStringWrapperT(
                SAA_inout_opt       std::string*            externalString = nullptr,
                SAA_in_opt          const std::size_t       initialCapacity = 0U
                )
                : m_implPtr( externalString ? externalString : &m_impl ),
                  m_dataPtr( m_implPtr -> data() )
            {
                /*
                 * This class can be used in three ways:
                 *  1. as a standalone object, which uses internal string as a storage
                 *  2. as a wrapper of an external string, which will be managed only by object of this class
                 *  3. as a wrapper of an external string, which will be modified outside of this class
                 * In order to assure a secure memory cleanup in the last case, user has to specify
                 * an initial capacity up to which the string is expected to grow. The string has to
                 * be empty, otherwise the allocation hint will be ignored. If the string is reallocated
                 * due to external changes, the wrapper is no longer able to wipe it securely.In such case,
                 * the application will be terminated. Due to this, the only safe operations are probably
                 * 'push_back', 'append' and some forms of 'assign' with the first being recommended way of
                 * updating the string.
                 */

                if( initialCapacity > 0 )
                {
                    BL_CHK(
                        false,
                        externalString && externalString -> empty(),
                        "Initial capacity can only be specified for empty external string "
                        );

                    reserve( initialCapacity );
                }
            }

            SecureStringWrapperT( SAA_in const SecureStringWrapperT& other )
                : m_implPtr( &m_impl ),
                  m_dataPtr( m_implPtr -> data() )
            {
                append( other );
            }

            SecureStringWrapperT( SAA_in SecureStringWrapperT&& other )
                : m_implPtr( &m_impl ),
                  m_dataPtr( m_implPtr -> data() )
            {
                append( other );
                other.clear();
            }

            explicit SecureStringWrapperT( SAA_in std::string&& other )
                : m_implPtr( &m_impl ),
                  m_dataPtr( m_implPtr -> data() )
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
                append( other );

                return *this;
            }

            SecureStringWrapperT& operator=( SAA_in SecureStringWrapperT&& other )
            {
                if( this == &other )
                {
                    return *this;
                }

                clear();
                append( other );
                other.clear();

                return *this;
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

                m_size = size();
            }

            void append( SAA_in const char ch )
            {
                grow( size() + 1 );

                m_implPtr -> push_back( ch );

                m_size = size();
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
                checkWrappedStringIntegrity();

                secureClear( *m_implPtr );

                m_size = size();
            }

        private:

            void reserve( SAA_in const std::size_t newCapacity )
            {
                m_implPtr -> reserve( newCapacity );
                m_dataPtr = m_implPtr -> data();
            }

            void checkWrappedStringIntegrity()
            {
                BL_RT_ASSERT(
                    m_dataPtr == m_implPtr -> data(),
                    "Unable to ensure integrity of the wrapped string, as it was reallocated by external operation"
                    );

                /*
                 * The wrapped external string can grow as a result of external operation but cannot shrink.
                 */

                BL_RT_ASSERT(
                    m_size <= size(),
                    "Unable to ensure integrity of the wrapped string, as it was shrank by external operation"
                    );
            }

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
                    reserve( calculateNewCapacity( newSize ) );
                }
                else if( newSize > m_implPtr -> capacity() )
                {
                    SecureStringWrapperT temp( *this );

                    clear();
                    reserve( calculateNewCapacity( newSize ) );

                    for( const auto ch : *temp.m_implPtr )
                    {
                        m_implPtr -> push_back( ch );
                    }
                }
            }

        private:

            std::string                                     m_impl;
            std::string*                                    m_implPtr;

            const char*                                     m_dataPtr;
            cpp::ScalarTypeIniter< std::size_t >            m_size;
        };

        using SecureStringWrapper = SecureStringWrapperT<>;

    } // str

} // bl

#endif /* __BL_SECURESTRINGWRAPPER_H_ */
