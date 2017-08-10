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

#ifndef __BL_JNI_DIRECTBYTEBUFFER_H_
#define __BL_JNI_DIRECTBYTEBUFFER_H_

#include <baselib/jni/JniEnvironment.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        template
        <
            typename E = void
        >
        class DirectByteBufferT
        {
            BL_NO_COPY_OR_MOVE( DirectByteBufferT )

            private:

                bl::cpp::SafeUniquePtr< char[] >                    m_buffer;
                GlobalReference< jobject >                          m_javaBuffer;
                std::size_t                                         m_capacity;
                std::size_t                                         m_writePos;
                std::size_t                                         m_readPos;

            public:

                DirectByteBufferT( SAA_in const std::size_t capacity )
                    :
                    m_capacity( capacity ),
                    m_writePos( 0 ),
                    m_readPos( 0 )
                {
                    m_buffer = cpp::SafeUniquePtr< char[] >::attach( new char[ capacity ] );

                    const auto& environment = JniEnvironment::instance();

                    m_javaBuffer = environment.createGlobalReference< jobject >(
                        environment.createDirectByteBuffer( m_buffer.get(), capacity )
                        );
                }

                std::size_t getCapacity() const
                {
                    return m_capacity;
                }

                std::size_t getSize() const
                {
                    return m_writePos;
                }

                void prepareForWrite()
                {
                    m_writePos = 0;
                }

                void prepareForRead()
                {
                    const auto& environment = JniEnvironment::instance();

                    environment.flipByteBuffer( m_javaBuffer.get() );
                    m_writePos = environment.getByteBufferLimit( m_javaBuffer.get() );

                    m_readPos = 0;
                }

                void prepareForJavaRead() const
                {
                    JniEnvironment::instance().setByteBufferPosition(
                        m_javaBuffer.get(),
                        static_cast< jint >( 0 )
                        );

                    JniEnvironment::instance().setByteBufferLimit(
                        m_javaBuffer.get(),
                        static_cast< jint >( m_writePos )
                        );
                }

                void prepareForJavaWrite() const
                {
                    JniEnvironment::instance().clearByteBuffer( m_javaBuffer.get() );
                }

                const GlobalReference< jobject >& getJavaBuffer() const
                {
                    return m_javaBuffer;
                }

                template
                <
                    typename T
                >
                void write( SAA_in const T value )
                {
                    static_assert( std::is_pod< T >::value, "typename T must be POD" );

                    BL_CHK_T(
                        false,
                        m_writePos + sizeof( T ) <= m_capacity,
                        JavaException(),
                        BL_MSG()
                            << "Attempt to write "
                            << sizeof( T )
                            << " bytes with write position "
                            << m_writePos
                            << " and capacity "
                            << m_capacity
                        );

                    std::memcpy( m_buffer.get() + m_writePos, &value, sizeof( T ) );
                    m_writePos += sizeof( T );
                }

                void write( SAA_in const std::string& text )
                {
                    int32_t textSize = static_cast< int32_t >( text.size() );
                    write( textSize );

                    BL_CHK_T(
                        false,
                        m_writePos + textSize <= m_capacity,
                        JavaException(),
                        BL_MSG()
                            << "Attempt to write "
                            << textSize
                            << " bytes with write position "
                            << m_writePos
                            << " and capacity "
                            << m_capacity
                        );

                    std::memcpy( m_buffer.get() + m_writePos, text.c_str(), textSize );
                    m_writePos += textSize;
                }

                template
                <
                    typename T
                >
                void read( SAA_out T* value)
                {
                    static_assert( std::is_pod< T >::value, "typename T must be POD" );

                    BL_ASSERT( value != nullptr );

                    BL_CHK_T(
                        false,
                        m_readPos + sizeof( T ) <= m_writePos,
                        JavaException(),
                        BL_MSG()
                            << "Attempt to read "
                            << sizeof( T )
                            << " bytes with read position "
                            << m_readPos
                            << " and write position "
                            << m_writePos
                        );

                    std::memcpy( value, m_buffer.get() + m_readPos, sizeof( T ) );
                    m_readPos += sizeof( T );
                }

                void read( SAA_out std::string* text )
                {
                    BL_ASSERT( text != nullptr );

                    int32_t textSize;
                    read( &textSize );

                    BL_CHK_T(
                        false,
                        m_readPos + textSize <= m_writePos,
                        JavaException(),
                        BL_MSG()
                            << "Attempt to read "
                            << textSize
                            << " bytes with read position "
                            << m_readPos
                            << " and write position "
                            << m_writePos
                        );

                    text -> assign( m_buffer.get() + m_readPos, textSize );
                    m_readPos += textSize;
                }
        };

        typedef DirectByteBufferT<> DirectByteBuffer;

    } // jni

} // bl

#endif /* __BL_JNI_DIRECTBYTEBUFFER_H_ */
