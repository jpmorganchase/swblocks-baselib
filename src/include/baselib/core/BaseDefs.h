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

#ifndef __BL_BASEDEFS_H_
#define __BL_BASEDEFS_H_

/**
 * Must disable warning 4503 globally for VC compiler, before importing the
 * json spirit headers as json spirit uses boost spirit and this generates a
 * lot of extremely long symbol names due to its use of nested templates.
 *
 * Boost property tree was considered as an alternative for json parsing,
 * but it too uses boost spirit internally, and generated the same warnings.
 *
 * The inclusion of some other boost headers, like bitmask.hpp, also
 * triggers this warning hence disabling it in this common header
 */

#if defined(_MSC_VER)
#pragma warning (disable:4503)
#endif


#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/assert.hpp>
#include <boost/detail/bitmask.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/detail/OSBoostImports.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory>
#include <cstdlib>
#include <iostream>

#define BL_MUTEX_GUARD( lock ) \
    bl::os::mutex_guard BL_ANONYMOUS_VARIABLE( g ) ( lock )

/*
 * This header should not depend on any other non-external includes
 */

/*
 * Define the std assert macros to map to BOOST_ASSERT for now
 */

#define BL_ASSERT( expr )               BOOST_ASSERT( expr )
#define BL_VERIFY( expr )               BOOST_VERIFY( expr )

/**
 * @brief Declares the type (usually enum) as a bitmask
 */

#define BL_DECLARE_BITMASK( bitmaskType ) \
    BOOST_BITMASK( bitmaskType ) \

#if defined(_MSC_VER)
#define BL_LIMITTED_CPP11_SUPPORT
#define NOTHROW_REAL
#else
#define NOTHROW_REAL throw()
#endif

#if defined(BL_LIMITTED_CPP11_SUPPORT)
#define CONSTEXPR
#define NOEXCEPT    throw()
#else // // BL_LIMITTED_CPP11_SUPPORT
#define CONSTEXPR   constexpr
#define NOEXCEPT    noexcept
#endif // BL_LIMITTED_CPP11_SUPPORT

#if defined(__CDT_PARSER__)
#define OVERRIDE
#else // defined(__CDT_PARSER__)
#define OVERRIDE    override
#endif // defined(__CDT_PARSER__)

#define FINAL       final

#define BL_UNUSED( x ) \
    do \
    { \
        ( void )( x ); \
    } \
    while( false ) \

#define BL_ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( ( x )[ 0 ] ) )

/**
 * @brief BL_PARAM_FWD is intended to be used for idiomatic forwarding of local parameters
 * which are either true rvalue references (Foo&&) or universal references (T&&) which only
 * happens in the case when template deduction for the parameter type is applied
 *
 * In both cases the type which needs to be used to instantiate std::forward explicitly can
 * be derived by taking the parameter declared type (with decltype) and removing the
 * reference from it (if any)
 *
 * This macro will help us eliminate unnecessary verbosity in the code while also keep the
 * intent very clear and explicit (that parameter is being forwarded)
 */

#define BL_PARAM_FWD( x ) \
    std::forward< decltype( x ) >( x ) \

#define BL_CONCATENATE_IMPL( s1, s2 ) s1 ## s2
#define BL_CONCATENATE( s1, s2 ) BL_CONCATENATE_IMPL( s1, s2 )

#ifdef __COUNTER__

#define BL_ANONYMOUS_VARIABLE( name ) \
    BL_CONCATENATE( name, __COUNTER__ ) \

#else

#define BL_ANONYMOUS_VARIABLE( name ) \
    BL_CONCATENATE( name, __LINE__ ) \

#endif

#if defined(BL_LIMITTED_CPP11_SUPPORT)

#define BL_CTR_DEFAULT( className, visibility ) \
    visibility: \
        className() {} \
    private: \

#define BL_CTR_DEFAULT_NOTHROW( className, visibility ) \
    visibility: \
        className() NOEXCEPT {} \
    private: \

#define BL_CTR_DEFAULT_DELETE( className ) \
    private: \
        className(); \

#define BL_CTR_COPY_DELETE( className ) \
    private: \
        className( const className& ); \
        className& operator =( const className& ); \

#define BL_CTR_MOVE_DELETE( className ) \
    private: \
        className( className&& ); \
        className& operator =( className&& ); \

#else

#define BL_CTR_DEFAULT( className, visibility ) \
    visibility: \
        className() = default; \
    private: \

#define BL_CTR_DEFAULT_NOTHROW( className, visibility ) \
    visibility: \
        className() NOEXCEPT = default; \
    private: \

#define BL_CTR_DEFAULT_DELETE( className ) \
    public: \
        className() = delete; \
    private: \

#define BL_CTR_COPY_DELETE( className ) \
    public: \
        className( const className& ) = delete; \
        className& operator =( const className& ) = delete; \
    private: \

#define BL_CTR_MOVE_DELETE( className ) \
    public: \
        className( className&& ) = delete; \
        className& operator =( className&& ) = delete; \
    private: \

#endif

#define BL_CTR_COPY_DEFAULT( className ) \
    public: \
        className( const className& ) = default; \
        className& operator =( const className& ) = default; \
    private: \

#define BL_CTR_MOVE_DEFAULT( className ) \
    public: \
        className( className&& ) = default; \
        className& operator =( className&& ) = default; \
    private: \

#define BL_CTR_COPY_DEFAULT_T( className, T, base_type, visibility, qualifiers ) \
    visibility: \
        className( T other ) qualifiers \
            : \
            base_type( BL_PARAM_FWD( other ) ) \
        { \
        } \
        \
        className& operator =( T other ) qualifiers \
        { \
            base_type::operator =( BL_PARAM_FWD( other ) ); \
            return *this; \
        } \
    private: \

/**
 * @brief Makes the class not create-able
 */

#define BL_NO_CREATE( className ) \
    BL_CTR_DEFAULT( className, protected ) \

#define BL_DECLARE_STATIC( className ) \
    BL_CTR_DEFAULT( className, private ) \

#define BL_NO_COPY( className ) \
    BL_CTR_COPY_DELETE( className ) \

#define BL_NO_COPY_OR_MOVE( className ) \
    BL_CTR_COPY_DELETE( className ) \
    BL_CTR_MOVE_DELETE( className ) \

#define BL_NO_POLYMORPHIC_BASE( className ) \
    protected: \
        ~className() NOEXCEPT {} \
    private: \

#define BL_DECLARE_ABSTRACT( className ) \
    BL_CTR_DEFAULT( className, protected ) \
    BL_NO_POLYMORPHIC_BASE( className ) \

/*
 * Note: do not try to make BL_DEFINE_STATIC_MEMBER not take type
 * and use decltype( className< TCLASS >::memberName ) because
 * this breaks clang (likely a compiler bug)
 *
 * We need clang to be working, so we need this workaround for now
 */

#define BL_DEFINE_STATIC_MEMBER( className, type, memberName ) \
    template \
    < \
        typename TCLASS \
    > \
    type \
    className< TCLASS >::memberName \

#define BL_DEFINE_STATIC_CONST_STRING( className, memberName ) \
    BL_DEFINE_STATIC_MEMBER( className, const std::string, memberName )

#define BL_DEFINE_STATIC_STRING( className, memberName ) \
    BL_DEFINE_STATIC_MEMBER( className, std::string, memberName )

#define BL_DEFINE_STATIC_CONST_STRING_REF_INIT( className, memberName ) \
    BL_DEFINE_STATIC_MEMBER( className, const std::string&, g_ ## memberName ) = className< TCLASS >::memberName ## Init();

namespace bl
{
    /**
     * @brief An empty class to be used for template specializations
     */
    class EmptyClass
    {
    };

    template
    <
        typename T
    >
    inline bool isPowerOfTwo( SAA_in const T value ) NOEXCEPT
    {
        /*
         * 0 is not power of two
         * then we just check that there is only one bit set
         */

        return ( value && 0 == ( value & (value - 1 ) ) );
    }

    namespace detail
    {
        template
        <
            typename T,
            typename U
        >
        inline T alignedOfDefault(
            SAA_in              T             value,
            SAA_in              U             alignment
            ) NOEXCEPT
        {
            BL_ASSERT( alignment && isPowerOfTwo( alignment ) );

            const T result = ( alignment * ( ( value + alignment - 1 ) / alignment ) );

            BL_ASSERT( value <= result );
            BL_ASSERT( 0 == ( result % alignment ) );

            return result;
        }

        template
        <
            typename T,
            typename U,
            bool isPointer
        >
        class AlignedOfImpl
        {
            BL_DECLARE_STATIC( AlignedOfImpl )

        public:

            static T alignedOf(
                SAA_in              T             value,
                SAA_in              U             alignment
                ) NOEXCEPT
            {
                return alignedOfDefault( value, alignment );
            }
        };

        template
        <
            typename T,
            typename U
        >
        class AlignedOfImpl< T, U, true /* isPointer */ >
        {
            BL_DECLARE_STATIC( AlignedOfImpl )

        public:

            static T alignedOf(
                SAA_in              T             value,
                SAA_in              U             alignment
                ) NOEXCEPT
            {
                return reinterpret_cast< T >(
                    reinterpret_cast< void* >(
                        alignedOfDefault(
                            reinterpret_cast< std::uintptr_t >( value ),
                            alignment
                            )
                        )
                    );
            }
        };

    } // detail

    template
    <
        typename T,
        typename U
    >
    inline T alignedOf(
        SAA_in              const T             value,
        SAA_in              const U             alignment
        ) NOEXCEPT
    {
        return detail::AlignedOfImpl< T, U, std::is_pointer< T >::value >::alignedOf( value, alignment );
    }

    namespace detail
    {
        /**
         * @brief class Globals - generic global state
         */

        template
        <
            typename E = void
        >
        class GlobalsT
        {
            BL_DECLARE_STATIC( GlobalsT )

        private:

            static bool g_isInUnitTest;

        public:

            static bool isInUnitTest() NOEXCEPT
            {
                return g_isInUnitTest;
            }

            static void isInUnitTest( SAA_in const bool isInUnitTest ) NOEXCEPT
            {
                g_isInUnitTest = isInUnitTest;
            }
        };

        BL_DEFINE_STATIC_MEMBER( GlobalsT, bool, g_isInUnitTest ) = false;

        typedef GlobalsT<> Globals;

    } // detail

    namespace global
    {
        /**
         * @brief This function will return if the code is executing in unit test
         *
         * Note that this function is not to be used liberally and ideally only
         * in very special cases which are well understood and ideally only
         * for workarounds in tests where there is no other option (last resort)
         */

        inline bool isInUnitTest() NOEXCEPT
        {
            return detail::Globals::isInUnitTest();
        }

        /**
         * @brief This function will set the UT state
         *
         * It should not be used by production code, but only by UT code
         */

        inline void isInUnitTest( SAA_in const bool isInUnitTest ) NOEXCEPT
        {
            detail::Globals::isInUnitTest( isInUnitTest );
        }

    } // global

} // bl

#endif /* __BL_BASEDEFS_H_ */
