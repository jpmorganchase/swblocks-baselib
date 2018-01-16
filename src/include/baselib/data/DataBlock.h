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

#ifndef __BL_DATA_DATABLOCK_H_
#define __BL_DATA_DATABLOCK_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/Pool.h>
#include <baselib/core/BaseIncludes.h>

#include <cstddef>

namespace bl
{
    namespace data
    {
        /**
         * @brief class DataBlock - a basic data block info
         */

        template
        <
            typename E = void
        >
        class DataBlockT;

        typedef om::ObjectImpl< DataBlockT<> > DataBlock;

        /*
         * Note: The data blocks are assumed to be pooled and just moved across the pipeline via a shared
         * pool. Otherwise the whole process will be very inefficient as massive amounts of data will have
         * to be privately copied from one processing unit to another. And in addition to that we'll have
         * many heap allocations as well.
         */

        typedef om::ObjectImpl< SimplePool< om::ObjPtr< DataBlock > > > datablocks_pool_type;

        template
        <
            typename E
        >
        class DataBlockT :
            public om::Object
        {
            BL_DECLARE_OBJECT_IMPL_DEFAULT( DataBlockT )

        private:

            class DataDeleter FINAL
            {
            private:

                bool m_noDelete;

            public:

                DataDeleter( SAA_in const bool noDelete = false )
                {
                    m_noDelete = noDelete;
                }


                void operator()( SAA_in char* ptr ) const NOEXCEPT
                {
                    if( ! m_noDelete )
                    {
                        delete[] ptr;
                    }
                }
            };

            /**
             * @brief Default block capacity
             */

            static const std::size_t                                            g_BlockCapacityDefault;

            cpp::SafeUniquePtr< char[], DataDeleter >                           m_data;
            cpp::ScalarTypeIniter< std::size_t >                                m_capacity;
            cpp::ScalarTypeIniter< std::size_t >                                m_size;
            cpp::ScalarTypeIniter< bool >                                       m_freed;
            cpp::ScalarTypeIniter< std::size_t >                                m_offset1;

            static std::size_t calculateCapacity( SAA_in_opt const std::size_t capacity ) NOEXCEPT
            {
                return capacity ? capacity : g_BlockCapacityDefault;
            }

        protected:

            DataBlockT( SAA_in_opt const std::size_t capacity = 0U )
            {
                m_capacity = calculateCapacity( capacity );

                /*
                 * Note: This pragma below is to pacify the MSVC static analysis tools (/analyze)
                 *
                 * This block doesn't need to be initialized here. It will be initialized later
                 * on when / before it is used
                 */

                #ifdef _WIN32
                #pragma warning( suppress : 6001 )
                #endif // _WIN32
                m_data = cpp::SafeUniquePtr< char[], DataDeleter >::attach( new char[ m_capacity.value() ] );
                m_size = m_capacity;
            }

            DataBlockT(
                SAA_in          char*                                           data,
                SAA_in          const std::size_t                               capacity
                )
            {
                m_data = cpp::SafeUniquePtr< char[], DataDeleter >::attach( data, DataDeleter( true /* noDelete */ ) );
                m_capacity = capacity;
                m_size = m_capacity;
            }

            void readEnsureAvailable( SAA_in const std::size_t size )
            {
                BL_CHK_T(
                    false,
                    m_offset1 + size <= m_size,
                    BufferTooSmallException(),
                    BL_MSG()
                        << "Attempt to read "
                        << size
                        << " bytes with read position "
                        << m_offset1
                        << " and write position "
                        << m_size
                    );
            }

        public:

            typedef char*                                                       iterator;
            typedef const char*                                                 const_iterator;

            auto capacity() const NOEXCEPT -> std::size_t
            {
                return m_capacity;
            }

            auto capacity64() const NOEXCEPT -> std::uint64_t
            {
                return m_capacity;
            }

            auto size() const NOEXCEPT -> std::size_t
            {
                return m_size;
            }

            auto size64() const NOEXCEPT -> std::uint64_t
            {
                return m_size;
            }

            void setSize( SAA_in const std::size_t size ) NOEXCEPT
            {
                BL_ASSERT( size <= m_capacity );
                BL_ASSERT( m_offset1 <= size );
                m_size = size;
            }

            bool freed() const NOEXCEPT
            {
                return m_freed;
            }

            void freed( SAA_in const bool freed ) NOEXCEPT
            {
                m_freed = freed;
            }

            auto offset1() const NOEXCEPT -> std::size_t
            {
                return m_offset1;
            }

            void setOffset1( SAA_in const std::size_t offset1 ) NOEXCEPT
            {
                BL_ASSERT( offset1 <= m_size );
                m_offset1 = offset1;
            }

            void reset() NOEXCEPT
            {
                m_offset1 = 0U;
                m_size = 0U;
            }

            auto begin() NOEXCEPT -> iterator
            {
                return m_data.get();
            }

            auto end() NOEXCEPT -> iterator
            {
                return m_data.get() + m_size;
            }

            auto begin() const NOEXCEPT -> const_iterator
            {
                return m_data.get();
            }

            auto end() const NOEXCEPT -> const_iterator
            {
                return m_data.get() + m_size;
            }

            auto pv() const NOEXCEPT -> void*
            {
                return reinterpret_cast< void* >( m_data.get() );
            }

            void write(
                SAA_in          const void*                                     data,
                SAA_in          const std::size_t                               size
                )
            {
                BL_CHK_T(
                    false,
                    m_size + size <= m_capacity,
                    BufferTooSmallException(),
                    BL_MSG()
                        << "Attempt to write "
                        << size
                        << " bytes with write position "
                        << m_size
                        << " and capacity "
                        << m_capacity
                    );

                std::memcpy( m_data.get() + m_size, data, size );
                m_size += size;
            }

            template
            <
                typename T
            >
            void write( SAA_in const T value )
            {
                write( &value, sizeof( T ) );
            }

            void write( SAA_in const char* text )
            {
                const std::int32_t textSize = numbers::safeCoerceTo< std::int32_t >( std::strlen( text ) );

                write( textSize );
                write( text, textSize );
            }

            void write( SAA_in const std::string& text )
            {
                const std::int32_t textSize = numbers::safeCoerceTo< std::int32_t >( text.size() );

                write( textSize );
                write( text.c_str(), textSize );
            }

            template
            <
                typename T
            >
            void read( SAA_out T* value )
            {
                static_assert( std::is_pod< T >::value, "typename T must be POD" );

                BL_ASSERT( value != nullptr );

                readEnsureAvailable( sizeof( T ) );

                std::memcpy( value, m_data.get() + m_offset1, sizeof( T ) );
                m_offset1 += sizeof( T );
            }

            void read( SAA_out std::string* text )
            {
                BL_ASSERT( text != nullptr );

                std::int32_t textSize;
                read( &textSize );

                readEnsureAvailable( textSize );

                text -> assign( m_data.get() + m_offset1, textSize );
                m_offset1 += textSize;
            }

            static auto get(
                SAA_in_opt      const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool,
                SAA_in_opt      const std::size_t                               capacity = defaultCapacity()
                )
                -> om::ObjPtr< DataBlock >
            {
                auto newBlock = dataBlocksPool ? dataBlocksPool -> tryGet() : nullptr;

                if( ! newBlock )
                {
                    newBlock = DataBlock::createInstance( capacity );
                }

                /*
                 * Note that the order of setting these is important as the setSize has
                 * an assertion that the size cannot be smaller that the offset1
                 */

                newBlock -> setOffset1( 0U );
                newBlock -> setSize( 0U );

                return newBlock;
            }

            static auto copy(
                SAA_in          const om::ObjPtr< DataBlock >&                  block,
                SAA_in_opt      const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool = nullptr
                )
                -> om::ObjPtr< DataBlock >
            {
                auto newBlock = copy( block -> begin(), block -> size(), dataBlocksPool, block -> capacity() );

                newBlock -> setOffset1( block -> offset1() );

                return newBlock;
            }

            static auto copy(
                SAA_in          const void*                                     data,
                SAA_in          const std::size_t                               size,
                SAA_in_opt      const om::ObjPtr< datablocks_pool_type >&       dataBlocksPool = nullptr,
                SAA_in          const std::size_t                               capacity = defaultCapacity()
                )
                -> om::ObjPtr< DataBlock >
            {
                auto newBlock = get( dataBlocksPool, capacity );

                newBlock -> write( data, size );

                return newBlock;
            }

            static std::size_t defaultCapacity() NOEXCEPT
            {
                return g_BlockCapacityDefault;
            }
        };

        BL_DEFINE_STATIC_MEMBER( DataBlockT, const std::size_t, g_BlockCapacityDefault ) = 1024 * 1024;

    } // data

} // bl

#endif /* __BL_DATA_DATABLOCK_H_ */
