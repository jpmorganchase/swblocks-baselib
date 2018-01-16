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

#ifndef __BL_POOL_H_
#define __BL_POOL_H_

#include <baselib/core/BaseIncludes.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/pool/object_pool.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/OS.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/Logging.h>
#include <baselib/core/PoolAllocatorDefault.h>

#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace bl
{
    using boost::object_pool;

    /**
     * @brief SimplePoolCheckerAdaptor - an adaptor class to convert a T to a unique cookie
     *
     * This is necessary to facilitate the implementation (below) of SimplePoolCheckerNaiveImpl
     *
     * We can't use a std::hash here because the cookie is expected to be always unique and
     * collisions are not acceptable
     */

    template
    <
        typename T
    >
    class SimplePoolCheckerAdaptor FINAL
    {
    public:

        static std::uintptr_t getCookie( SAA_in const T& value ) NOEXCEPT
        {
            BL_UNUSED( value );
            BL_RT_ASSERT( false, "cookie adaptor is not defined for this type" );
            return 0U;
        }
    };

    /**
     * @brief Specialization of SimplePoolCheckerAdaptor for cpp::SafeUniquePtr
     *
     * TODO: The FINAL here is causing internal compiler error on MSVC:
     *
     * baselib/core/Pool.h(72) : fatal error C1001:
     * An internal error has occurred in the compiler.
     * (compiler file 'msc1.cpp', line 1443)
     *
     * We can remove the comment below when this issue is fixed
     */

    template
    <
        typename T
    >
    class SimplePoolCheckerAdaptor< cpp::SafeUniquePtr< T > > /* FINAL */
    {
    public:

        static std::uintptr_t getCookie( SAA_in const cpp::SafeUniquePtr< T >& value ) NOEXCEPT
        {
            return reinterpret_cast< std::uintptr_t >( value.get() );
        }
    };

    /**
     * @brief Specialization of SimplePoolCheckerAdaptor for cpp::ObjPtr
     *
     * TODO: The FINAL here is causing internal compiler error on MSVC:
     *
     * baselib/core/Pool.h(72) : fatal error C1001:
     * An internal error has occurred in the compiler.
     * (compiler file 'msc1.cpp', line 1443)
     *
     * We can remove the comment below when this issue is fixed
     */

    template
    <
        typename T
    >
    class SimplePoolCheckerAdaptor< om::ObjPtr< T > > /* FINAL */
    {
    public:

        static std::uintptr_t getCookie( SAA_in const om::ObjPtr< T >& value ) NOEXCEPT
        {
            return reinterpret_cast< std::uintptr_t >( value.get() );
        }
    };

    /**
     * @brief class SimplePoolCheckerNaiveImpl< T > - a naive
     * implementation of the simple pool checker
     *
     * The naive implementation of the pool checker is to simply
     * remember all freed objects and assert if a double free is
     * attempted
     *
     * This implementation is appropriate if we have a small pool
     * which isn't frequently churned; in that case the main goal
     * of pooling is to avoid re-creating objects which are very
     * expensive to create
     */

    template
    <
        typename T
    >
    class SimplePoolCheckerNaiveImpl
    {
    protected:

        typedef SimplePoolCheckerAdaptor< T >               adaptor_t;

        std::unordered_set< std::uintptr_t >                m_freedSet;

    public:

        void markFreed( SAA_in const T& value )
        {
            const auto cookie = adaptor_t::getCookie( value );
            BL_RT_ASSERT( m_freedSet.insert( cookie ).second, "double free of pooled object" );
        }

        void markAllocated( SAA_in const T& value )
        {
            const auto pos = m_freedSet.find( adaptor_t::getCookie( value ) );
            BL_RT_ASSERT( pos != m_freedSet.end(), "allocated pooled object which isn't marked as freed" );
            m_freedSet.erase( pos );
        }
    };

    /**
     * @brief class SimplePoolCheckerIntrusiveImplPtr< T > - an intrusive
     * implementation of the simple pool checker for pointers
     *
     * The intrusive implementation of the pool checker is to
     * expect that the object has freed() getter and setter to help track
     * this state internally
     */

    template
    <
        typename T
    >
    class SimplePoolCheckerIntrusiveImplPtr
    {
    public:

        void markFreed( SAA_in const T& value )
        {
            BL_RT_ASSERT(
                ! value -> freed(),
                "double free of a pooled block which is already freed"
                );

            value -> freed( true );
        }

        void markAllocated( SAA_in const T& value )
        {
            BL_RT_ASSERT(
                value -> freed(),
                "pooled block in the free pool which isn't marked as freed"
                );

            value -> freed( false );
        }
    };

    /**
     * @brief class SimplePool - a simple pool implementation; it pools objects
     * of type T which are expensive to create and destroy, but easy to move
     *
     * Note: T must be std::nothrow_move_constructible or std::nothrow_copy_constructible
     * Note: T must also be std::is_nothrow_default_constructible
     *
     * Note: This is somewhat naive implementation which uses a vector and a
     * lock. A more scalable implementation could be done in the future which
     * is lock free (e.g. slist based)
     *
     * The default checker is SimplePoolCheckerIntrusiveImplPtr as we expect in
     * most cases these objects to be smart pointers (om::ObjPtr or cpp::SafeUniquePtr)
     * and also this is the most efficient implementation which can be used safely
     */

    template
    <
        typename T,
        template < typename > class CHECKER = SimplePoolCheckerIntrusiveImplPtr
    >
    class SimplePool :
        public om::ObjectDefaultBase,
        private CHECKER< T >
    {
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( SimplePool )

    private:

        typedef CHECKER< T >                                checker_t;

        const std::string                                   m_name;
        std::vector< T >                                    m_impl;
        os::mutex                                           m_lock;

    protected:

        SimplePool( SAA_in std::string&& name = std::string() )
            :
            m_name( BL_PARAM_FWD( name ) )
        {
            static_assert(
                std::is_nothrow_move_constructible< T >::value ||
                std::is_nothrow_copy_constructible< T >::value,
                "T must be nothrow_move_constructible or nothrow_copy_constructible"
                );

            static_assert(
                std::is_nothrow_default_constructible< T >::value,
                "T must be is_nothrow_default_constructible"
                );
        }

        ~SimplePool() NOEXCEPT
        {
            BL_LOG(
                Logging::trace(),
                BL_MSG()
                    << "SimplePool: destroying simple pool "
                    << m_name
                    << ( m_name.empty() ? "" : " " )
                    << this
                    << "; # of cached objects: "
                    << m_impl.size()
                );
        }

    public:

        T tryGet() NOEXCEPT
        {
            BL_MUTEX_GUARD( m_lock );

            if( m_impl.size() )
            {
                auto value = std::move( m_impl.back() );
                checker_t::markAllocated( value );

                m_impl.erase( m_impl.end() - 1 );
                return value;
            }

            return T();
        }

        void put( SAA_inout T&& value )
        {
            BL_MUTEX_GUARD( m_lock );

            checker_t::markFreed( value );
            m_impl.push_back( std::forward< T >( value ) );
        }
    };

    /**
     * @brief class TaggedPool - a tagged pool implementation; it pools objects
     * of type T which grouped by tags of type K and the caller can request objects of
     * type T for a particular tag
     *
     * Note: T must be std::nothrow_move_constructible or std::nothrow_copy_constructible
     * Note: T must also be std::is_nothrow_default_constructible
     */

    template
    <
        typename K,
        typename T,
        template < typename > class CHECKER = SimplePoolCheckerIntrusiveImplPtr
    >
    class TaggedPool :
        public om::ObjectDefaultBase
    {
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( TaggedPool )

    private:

        typedef om::ObjectImpl< SimplePool< T, CHECKER > >                      simple_pool_t;
        typedef std::unordered_map< K, om::ObjPtr< simple_pool_t > >            map_t;

        map_t                                                                   m_impl;
        os::mutex                                                               m_lock;

    protected:

        TaggedPool()
        {
            static_assert(
                std::is_nothrow_move_constructible< T >::value ||
                std::is_nothrow_copy_constructible< T >::value,
                "T must be nothrow_move_constructible or nothrow_copy_constructible"
                );

            static_assert(
                std::is_nothrow_default_constructible< T >::value,
                "T must be is_nothrow_default_constructible"
                );
        }

        ~TaggedPool() NOEXCEPT
        {

            BL_LOG(
                Logging::trace(),
                BL_MSG()
                    << "TaggedPool: destroying tagged pool "
                    << this
                    << "; # of cached tags: "
                    << m_impl.size()
                );
        }

    public:

        T tryGet( SAA_in const K& tag ) NOEXCEPT
        {
            BL_MUTEX_GUARD( m_lock );

            const auto pos = m_impl.find( tag );

            if( pos != m_impl.end() )
            {
                return pos -> second -> tryGet();
            }

            return T();
        }

        void put( SAA_in const K& tag, SAA_inout T&& value )
        {
            BL_MUTEX_GUARD( m_lock );

            const auto pos = m_impl.find( tag );

            simple_pool_t* pool;

            if( pos == m_impl.end() )
            {
                auto newPool = simple_pool_t::template createInstance< simple_pool_t >();
                pool = newPool.get();
                m_impl[ tag ] = std::move( newPool );
            }
            else
            {
                pool = pos -> second.get();
            }

            pool -> put( std::forward< T >( value ) );
        }
    };

} // bl

#endif /* __BL_POOL_H_ */
