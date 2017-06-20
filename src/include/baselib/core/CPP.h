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

#ifndef __BL_CPP_H_
#define __BL_CPP_H_

#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/detail/AbortImpl.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/any.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <cstdio>
#include <memory>
#include <mutex>
#include <type_traits>
#include <set>
#include <unordered_map>

#define BL_STDIO_TEXT( lambdaBody ) \
    bl::stdioText( [ & ]() -> void lambdaBody ) \

/**
 * @brief This macro will be used to implement / enforce APIs and methods
 * which need to provide no-throw semantics.
 */

#define BL_RIP_MSG( msg ) \
    do \
    { \
        BL_STDIO_TEXT( \
            { \
                std::cerr \
                    << "ERROR: RIP: Unrecoverable error has occurred at " \
                    << __FILE__ \
                    << "(" \
                    << __LINE__ \
                    << "): " \
                    << msg \
                    << std::endl; \
                \
                std::fflush( stderr ); \
            } \
            ); \
        \
        bl::os::fastAbort(); \
    } \
    while( false ) \

#define BL_RT_ASSERT( expr, msg ) \
    do \
    { \
        if( ! ( expr ) ) \
        { \
            BL_RIP_MSG( msg ); \
        } \
    } \
    while( false ) \

#define BL_NOEXCEPT_BEGIN() \
    try \
    { \

#define BL_NOEXCEPT_END() \
    } \
    catch( std::exception& e ) \
    { \
        BL_RIP_MSG( e.what() ); \
    } \

#define BL_WARN_NOEXCEPT_BEGIN() \
    try \
    { \

#define BL_WARN_NOEXCEPT_END( location ) \
    } \
    catch( std::exception& e ) \
    { \
        BL_LOG_MULTILINE( \
            bl::Logging::warning(), \
            BL_MSG() \
                << location \
                << ": NOEXCEPT block threw an exception, details:\n" \
                << bl::eh::diagnostic_information( e ) \
            ); \
    } \

#define BL_SCOPE_EXIT_IMPL( lambdaBody ) \
    const auto BL_ANONYMOUS_VARIABLE( __scopeExit ) = \
        bl::cpp::ScopeGuard::create( [ & ]() -> void lambdaBody ); \

#define BL_SCOPE_EXIT( lambdaBody ) \
    BL_SCOPE_EXIT_IMPL( \
        { \
            BL_NOEXCEPT_BEGIN() \
            \
            lambdaBody \
            \
            BL_NOEXCEPT_END() \
        } \
        ) \

#define BL_SCOPE_EXIT_WARN_ON_FAILURE( lambdaBody, location ) \
    BL_SCOPE_EXIT_IMPL( \
        { \
            BL_WARN_NOEXCEPT_BEGIN() \
            \
            lambdaBody \
            \
            BL_WARN_NOEXCEPT_END( location ) \
        } \
        ) \

#define BL_SCOPE_GUARD_IMPL( lambdaBody ) \
    bl::cpp::ScopeGuard::create( [ & ]() -> void lambdaBody ) \

#define BL_SCOPE_GUARD( lambdaBody ) \
    BL_SCOPE_GUARD_IMPL( \
        { \
            BL_NOEXCEPT_BEGIN() \
            \
            lambdaBody \
            \
            BL_NOEXCEPT_END() \
        } \
        ) \

#define BL_SCOPE_GUARD_WARN_ON_FAILURE( lambdaBody, location ) \
    BL_SCOPE_GUARD_IMPL( \
        { \
            BL_WARN_NOEXCEPT_BEGIN() \
            \
            lambdaBody \
            \
            BL_WARN_NOEXCEPT_END( location ) \
        } \
        ) \

#define BL_TYPEDEF_BOOL_TYPE \
    typedef int bl::cpp::BoolStruct< void >::* BoolType

#define BL_OPERATOR_BOOL \
    operator BoolType

#define BL_OPERATOR_BOOL_CONVERTIBLE_TO_TRUE \
    ( &bl::cpp::BoolStruct< void >::member )

/**
 * @brief Macro to define safe string streams classes
 * When a bit (failbit, badbit,...) matching the exception mask is set the stream throws std::ios_base::failure exception
 */

#define BL_DECLARE_SAFE_STRING_STREAM( className, baseClassName, openMode, exceptionFlags ) \
    template \
    < \
        typename E = void \
    > \
    class className ## T : public baseClassName \
    { \
        BL_CTR_COPY_DELETE( className ## T ) \
        BL_CTR_MOVE_DELETE( className ## T ) \
        \
    public: \
        \
        typedef baseClassName                                       base_type; \
        \
        explicit className ## T ( \
            SAA_in_opt          std::ios_base::openmode             which = ( openMode ) \
            ) \
            : \
            base_type( which ) \
        { \
            if( rdstate() & std::ios_base::badbit ) \
            { \
                throw std::ios_base::failure( #className ": std::ios_base::badbit set" ); \
            } \
            if( rdstate() & std::ios_base::failbit ) \
            { \
                throw std::ios_base::failure( #className ": std::ios_base::failbit set" ); \
            } \
            exceptions( exceptionFlags ); \
        } \
        \
        explicit className ## T ( \
            SAA_in              const std::string&                  str, \
            SAA_in_opt          std::ios_base::openmode             which = ( openMode ) \
            ) \
            : \
            base_type( str, which ) \
        { \
            if( rdstate() & std::ios_base::badbit ) \
            { \
                throw std::ios_base::failure( #className ": std::ios_base::badbit set" ); \
            } \
            if( rdstate() & std::ios_base::failbit ) \
            { \
                throw std::ios_base::failure( #className ": std::ios_base::failbit set" ); \
            } \
            exceptions( exceptionFlags ); \
        } \
    }; \
    typedef className ## T<> className; \

namespace bl
{
    namespace cpp
    {
        using boost::noncopyable;

        using boost::function;
        using boost::bind;
        using boost::mem_fn;

        using boost::ref;
        using boost::cref;

        using boost::any;
        using boost::any_cast;

        using boost::counting_iterator;

        using boost::circular_buffer;

        typedef function< void () > void_callback_t;
        typedef function< bool () > bool_callback_t;

        typedef function< void () NOEXCEPT > void_callback_noexcept_t;
        typedef function< bool () NOEXCEPT > bool_callback_noexcept_t;

    } // cpp

    namespace detail
    {
        /**
         * @brief class GlobalHooks
         */

        template
        <
            typename E = void
        >
        class GlobalHooksT
        {
            static std::recursive_mutex g_lock;

        public:

            static void stdioText( SAA_in const cpp::void_callback_t& cb )
            {
                std::lock_guard< std::recursive_mutex > g( g_lock );

                cb();
            }
        };

        template
        <
            typename E
        >
        std::recursive_mutex
        GlobalHooksT< E >::g_lock;

        typedef GlobalHooksT<> GlobalHooks;

    } // detail

    inline void stdioText( SAA_in const cpp::void_callback_t& cb )
    {
        detail::GlobalHooks::stdioText( cb );
    }

    namespace cpp
    {
        /*
         * To be able to define safe bool operator by implementing the
         * safe bool idiom:
         *
         * http://www.artima.com/cppsource/safebool.html
         *
         * The one we have here is a simplified version adapted from
         * the MSVC implementation in STD
         */

        template
        <
            typename E
        >
        struct BoolStruct
        {
            int member;
        };

        /**
         * @brief A safer version of unique_ptr that doesn't allow direct construction from T*
         * (a static attach method is provided for that purpose)
         *
         * Note: It will also re-define the copy semantic for raw pointer and xvalue to make
         * it safe and correct wrt to pointers which are references to the same reference
         * counted object. You may have the same exact raw pointer value, but it is a
         * different reference. The default behavior of unique_ptr does not support this.
         */

        template
        <
            typename T,
            typename D = std::default_delete< T >
        >
        class SafeUniquePtr : public std::unique_ptr< T, D >
        {
            BL_CTR_DEFAULT_NOTHROW( SafeUniquePtr, public )
            BL_CTR_COPY_DELETE( SafeUniquePtr )

        private:

            static SafeUniquePtr< T, D > attach( SAA_in_opt std::nullptr_t ) NOEXCEPT;

        public:

            typedef std::unique_ptr< T, D >                             base_type;
            typedef typename std::unique_ptr< T, D >::pointer           pointer;

            BL_CTR_COPY_DEFAULT_T( SafeUniquePtr, SAA_in_opt std::nullptr_t, base_type, public, NOEXCEPT )

        protected:

            SafeUniquePtr( SAA_in_opt pointer other ) NOEXCEPT
                :
                base_type( BL_PARAM_FWD( other ) )
            {
            }

            SafeUniquePtr(
                SAA_in_opt      pointer     other,
                SAA_inout       D&&         deleter
                ) NOEXCEPT
                :
                base_type( BL_PARAM_FWD( other ), BL_PARAM_FWD( deleter ) )
            {
            }

            SafeUniquePtr& operator =( SAA_in_opt pointer other ) NOEXCEPT
            {
                base_type::reset();

                base_type::operator =( BL_PARAM_FWD( other ) );
                return *this;
            }

        public:

            SafeUniquePtr( SAA_inout_opt SafeUniquePtr&& other ) NOEXCEPT
                :
                base_type( BL_PARAM_FWD( other ) )
            {
            }

            SafeUniquePtr& operator =( SAA_inout_opt SafeUniquePtr&& other ) NOEXCEPT
            {
                base_type::reset();

                base_type::operator =( BL_PARAM_FWD( other ) );
                return *this;
            }

            static SafeUniquePtr< T, D > attach( SAA_in_opt pointer ptr ) NOEXCEPT
            {
                return SafeUniquePtr< T, D >( ptr );
            }

            static SafeUniquePtr< T, D > attach(
                SAA_in_opt      pointer     ptr,
                SAA_inout       D&&         deleter
                ) NOEXCEPT
            {
                return SafeUniquePtr< T, D >( ptr, BL_PARAM_FWD( deleter ) );
            }
        };

        /**
         * @brief helper to safely create a shared pointer which has initer/deleter
         *
         * Note that the initer and deleter are passed by value because:
         *
         * 1) We generally expect them to be trivially constructible and
         * 2) The compiler will apply copy elision anyway, so there won't
         *    be extra copies (see http://en.wikipedia.org/wiki/Copy_elision)
         */

        template
        <
            typename T,
            typename I,
            typename D
        >
        std::shared_ptr< T > makeSharedWithIniterDeleter( SAA_in I initer, SAA_in D deleter )
        {
            /*
             * We must ensure that the deleter is nothrow copy-constructible
             */

            static_assert(
                std::is_nothrow_constructible< D, const D& >::value,
                "deleter must be nothrow copy-constructible"
                );

            /*
             * First initialize the object as owned by local unique_ptr and then we
             * call the initer. If the initer throws then the deleter will not be
             * called.
             */

            std::unique_ptr< T > up( new T );
            initer( up.get() );

            /*
             * At this point we have a fully initialized pointer. Now we can
             * create a shared pointer with deleter.
             *
             * Note that this constructor can throw (std::bad_alloc) and if an
             * exception is thrown the passed in pointer will be deleted
             * (according to the standard). For more information see this link:
             * http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
             */

            return std::shared_ptr< T >( up.release(), deleter );
        }

        /**
         * @brief A simple wrapper to initialize scalar type
         */

        template
        <
            typename T
        >
        class ScalarTypeIniter
        {
        private:

            T m_value;

        public:

            ScalarTypeIniter() NOEXCEPT
                :
                m_value()
            {
                static_assert( std::is_scalar< T >::value, "T must be scalar type" );
            }

            ScalarTypeIniter( SAA_in const T value ) NOEXCEPT
                :
                m_value( value )
            {
                static_assert( std::is_scalar< T >::value, "T must be scalar type" );
            }

            operator T () const NOEXCEPT
            {
                return m_value;
            }

            operator T& () NOEXCEPT
            {
                return m_value;
            }

            T value() const NOEXCEPT
            {
                return m_value;
            }

            T& lvalue() NOEXCEPT
            {
                return m_value;
            }
        };

        template
        <
            typename E = void
        >
        class ScopeGuardT
        {
        protected:

            cpp::void_callback_noexcept_t       m_cb;
            bool                                m_disabled;

        public:

            typedef ScopeGuardT< E >            this_type;

            ScopeGuardT( SAA_in void_callback_noexcept_t&& cb )
                :
                m_cb( std::forward< void_callback_noexcept_t >( cb ) ),
                m_disabled( false )
            {
            }

            ScopeGuardT( SAA_in ScopeGuardT&& rhs )
                :
                m_cb( std::move( rhs.m_cb ) ),
                m_disabled( rhs.m_disabled )
            {
                rhs.dismiss();
            }

            ScopeGuardT& operator=( SAA_in ScopeGuardT&& rhs )
            {
                m_cb = std::move( rhs.m_cb );
                m_disabled = rhs.m_disabled;
                rhs.dismiss();

                return *this;
            }

            ~ScopeGuardT() NOEXCEPT
            {
                runNow();
            }

            void dismiss() NOEXCEPT
            {
                m_disabled = true;
            }

            void runNow() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( ! m_disabled )
                {
                    m_disabled = true;

                    m_cb();
                }

                BL_NOEXCEPT_END()
            }

            static auto create( SAA_in void_callback_noexcept_t&& cb ) -> this_type
            {
                return this_type( std::forward< void_callback_noexcept_t >( cb ) );
            }
        };

        typedef ScopeGuardT<> ScopeGuard;

        /**
         * @brief A wrapper to std::rethrow_exception which will try to catch std::bad_exception
         * to account for the case when the exception being rethrown is bad (i.e. when the
         * exception was captured with std::current_exception it failed to copy it)
         */

        SAA_noreturn
        inline void safeRethrowException( SAA_in const std::exception_ptr& eptr )
        {
            if( ! eptr )
            {
                BL_RIP_MSG( "Attempting to rethrow a nullptr exception_ptr" );
            }

            try
            {
                std::rethrow_exception( eptr );
            }
            catch( std::bad_exception& )
            {
                BL_RIP_MSG( "A bad exception was encountered (e.g. copy of the exception has failed)" );
            }
        }

        /**
         * @brief Simple wrapper to convert prvalue ('pure rvalue') reference to
         * an xvalue ('expiring value') reference - i.e. create a temporary explicitly
         */

        template
        <
            typename T
        >
        T copy( SAA_in const T& value )
        {
            return T( value );
        }

        /**
         * @brief Simple wrapper around find() method for containers
         * used to check if specific value exists in the container
         */

        template
        <
            typename T,
            typename U
        >
        bool contains(
            SAA_in      const T&                                container,
            SAA_in      const U&                                value
            )
        {
            return container.find( value ) != container.end();
        }

        template
        <
            typename T,
            typename U
        >
        bool contains(
            SAA_in      const std::vector< T >&                 container,
            SAA_in      const U&                                value
            )
        {
            return std::find( container.begin(), container.end(), value ) != container.end();
        }

        template
        <
            typename T,
            typename U
        >
        bool contains(
            SAA_in      const std::basic_string< T >&          text,
            SAA_in      const U&                               value
            )
        {
            return std::string::npos != text.find( value );
        }

        template
        <
            typename T
        >
        bool contains(
            SAA_in      const std::basic_string< T >&          text,
            SAA_in      const T                                value
            )
        {
            return std::string::npos != text.find( value );
        }

        /**
         * @brief For a given map/unordered_map returns its keys
         *        map< key, value > -> set< key >
         */

        template
        <
            typename MapT
        >
        std::set< typename MapT::key_type > getKeys( SAA_in const MapT& map )
        {
            std::set< typename MapT::key_type > result;

            for( const auto& current : map )
            {
                result.insert( current.first );
            }

            return result;
        }

        /**
         * @brief Generic function to all possible casts, even stronger than reinterpret_cast
         * In particular to resolve compilation warning/error
         * "ISO C++ forbids casting between pointer-to-function and pointer-to-object"
         */
         template
         <
            typename ToType,
            typename FromType
         >
         ToType union_cast( SAA_in const FromType from )
         {
            static_assert( sizeof( ToType ) == sizeof( FromType ), "ToType size has to be equal FromType" );

            union
            {
                ToType   m_to;
                FromType m_from;
            } u;

            u.m_from = from;
            return u.m_to;
         }

         /**
         * @brief Extensions of std::type_traits.
         */

        template
        <
            typename T
        >
        struct TypeTraits
        {
            /**
             * @brief Resolves type to prvalue (i.e. pure rvalue)
             */

            typedef typename std::conditional
                <
                    std::is_scalar< T >::value,
                    T,
                    const T&
                >::type
                prvalue;

            /**
             * @brief Resolves type to xvalue (i.e. 'expiring' rvalue)
             */

            typedef typename std::conditional
                <
                    std::is_scalar< T >::value,
                    T,
                    T&&
                >::type
                xvalue;
        };

        /**
         * @brief class DeleterBaseImpl< D, isEmptyDeleter > - a deleter base class
         *
         * This class is specialized in two flavors - one which is meant for empty
         * deleter class in which case it is inherited and another for non-empty
         * deleter class in which the deleter is aggregated
         *
         * This is the standard trick to avoid the deleter storage when it is empty
         * which is the case normally
         */

        template
        <
            typename D,
            bool isEmptyDeleter
        >
        class DeleterBaseImpl;

        template
        <
            typename D
        >
        class DeleterBaseImpl< D, true /* isEmptyDeleter */ > :
            private D
        {
            BL_NO_POLYMORPHIC_BASE( DeleterBaseImpl )

        protected:

            typedef D base_type;

            DeleterBaseImpl() NOEXCEPT
            {
                static_assert(
                    std::is_nothrow_default_constructible< D >::value,
                    "The object deleter must be nothrow default-constructible"
                    );
            }

            DeleterBaseImpl( SAA_in D deleter ) NOEXCEPT
                :
                base_type( deleter )
            {
                static_assert(
                    std::is_nothrow_default_constructible< D >::value,
                    "The object deleter must be nothrow default-constructible"
                    );

                static_assert(
                    std::is_nothrow_constructible< D, D >::value,
                    "The object deleter must be nothrow copy-constructible"
                    );
            }

        public:

            typedef typename std::remove_reference< D >::type DeleterType;

            auto get_deleter() NOEXCEPT -> DeleterType&
            {
                return *this;
            }

            auto get_deleter() const NOEXCEPT -> const DeleterType&
            {
                return *this;
            }
        };

        template
        <
            typename D
        >
        class DeleterBaseImpl< D, false /* isEmptyDeleter */ >
        {
            BL_NO_POLYMORPHIC_BASE( DeleterBaseImpl )

        private:

            D m_deleter;

            DeleterBaseImpl() NOEXCEPT
            {
                static_assert(
                    std::is_nothrow_default_constructible< D >::value,
                    "The object deleter must be nothrow default-constructible"
                    );
            }

            DeleterBaseImpl( SAA_in D deleter ) NOEXCEPT
                :
                m_deleter( deleter )
            {
                static_assert(
                    std::is_nothrow_default_constructible< D >::value,
                    "The object deleter must be nothrow default-constructible"
                    );

                static_assert(
                    std::is_nothrow_constructible< D, D >::value,
                    "The object deleter must be nothrow copy-constructible"
                    );
            }

        public:

            typedef typename std::remove_reference< D >::type DeleterType;

            auto get_deleter() NOEXCEPT -> DeleterType&
            {
                return m_deleter;
            }

            auto get_deleter() const NOEXCEPT -> const DeleterType&
            {
                return m_deleter;
            }
        };

        template
        <
            typename T,
            typename D
        >
        class DeleterBase :
            public DeleterBaseImpl< D, std::is_empty< D >::value >
        {
            BL_NO_POLYMORPHIC_BASE( DeleterBase )

        protected:

            typedef DeleterBaseImpl< D, std::is_empty< D >::value > base_type;

            DeleterBase() NOEXCEPT
            {
                static_assert(
                    std::is_nothrow_default_constructible< T >::value,
                    "The handle type must be nothrow default-constructible"
                    );
            }

            DeleterBase( SAA_in D deleter ) NOEXCEPT
                :
                base_type( deleter )
            {
                static_assert(
                    std::is_nothrow_default_constructible< T >::value,
                    "The handle type must be nothrow default-constructible"
                    );
            }

        public:

            T get_null() const NOEXCEPT
            {
                return base_type::get_deleter().get_null();
            }
        };

        /**
         * @brief class UniqueHandle - unique_ptr like template capable of holding opaque handles
         * and / or scalar types which are not pointers and can't be dereferenced (e.g. int) and
         * also have a custom nullptr (e.g. -1 if the type is int)
         */

        template
        <
            typename T,
            typename D
        >
        class UniqueHandle FINAL :
            private DeleterBase< T, D >
        {
            BL_NO_COPY( UniqueHandle )

        private:

            typedef DeleterBase< T, D > base_type;

            T m_handle;

        public:

            static UniqueHandle attach( SAA_in const T handle, SAA_in D deleter = D() ) NOEXCEPT
            {
                return UniqueHandle( handle, deleter );
            }

            UniqueHandle attach( SAA_in std::nullptr_t ) NOEXCEPT
            {
                return attach( get_null() );
            }

            explicit UniqueHandle( SAA_in D deleter = D() )
                :
                base_type( deleter ),
                m_handle( base_type::get_null() )
            {
            }

            explicit UniqueHandle( SAA_in const T handle, SAA_in D deleter = D() )
                :
                base_type( deleter ),
                m_handle( handle )
            {
            }

            ~UniqueHandle() NOEXCEPT
            {
                reset();
            }

            UniqueHandle( SAA_inout UniqueHandle&& other ) NOEXCEPT
                :
                m_handle( other.m_handle )
            {
                other.m_handle = get_null();
            }

            UniqueHandle& operator=( SAA_inout UniqueHandle&& other ) NOEXCEPT
            {
                reset( other.m_handle );
                other.m_handle = get_null();

                return *this;
            }

            UniqueHandle& operator=( SAA_in const T handle ) NOEXCEPT
            {
                reset( handle );
                return *this;
            }

            T get_null() const NOEXCEPT
            {
                return base_type::get_null();
            }

            void reset() NOEXCEPT
            {
                reset( get_null() );
            }

            void reset( SAA_in const T handle ) NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( get_null() != m_handle )
                {
                    base_type::get_deleter()( m_handle );
                    m_handle = get_null();
                }

                m_handle = handle;

                BL_NOEXCEPT_END()
            }

            T release() NOEXCEPT
            {
                if( get_null() != m_handle )
                {
                    const T handle = m_handle;
                    m_handle = get_null();
                    return handle;
                }

                return get_null();
            }

            void swap( UniqueHandle& other ) NOEXCEPT
            {
                std::swap( m_handle, other.m_handle );
            }

            T get() const NOEXCEPT
            {
                return m_handle;
            }

            BL_TYPEDEF_BOOL_TYPE;

            BL_OPERATOR_BOOL() const NOEXCEPT
            {
                return m_handle != get_null() ? BL_OPERATOR_BOOL_CONVERTIBLE_TO_TRUE : nullptr;
            }
        };

        template
        <
            typename T,
            typename D
        >
        bool operator ==( SAA_in const UniqueHandle< T, D >& lhs, SAA_in const T rhs )
        {
            return lhs.get() == rhs;
        }

        template
        <
            typename T,
            typename D
        >
        bool operator ==( SAA_in const T lhs, SAA_in const UniqueHandle< T, D >& rhs )
        {
            return lhs == rhs.get();
        }

        template
        <
            typename T,
            typename D
        >
        bool operator ==( SAA_in const UniqueHandle< T, D >& lhs, SAA_in const UniqueHandle< T, D >& rhs )
        {
            return lhs.get() == rhs.get();
        }

        template
        <
            typename T,
            typename D
        >
        bool operator !=( SAA_in const UniqueHandle< T, D >& lhs, SAA_in const T rhs )
        {
            return lhs.get() != rhs;
        }

        template
        <
            typename T,
            typename D
        >
        bool operator !=( SAA_in const T lhs, SAA_in const UniqueHandle< T, D >& rhs )
        {
            return lhs != rhs.get();
        }

        template
        <
            typename T,
            typename D
        >
        bool operator !=( SAA_in const UniqueHandle< T, D >& lhs, SAA_in const UniqueHandle< T, D >& rhs )
        {
            return lhs.get() != rhs.get();
        }

        /*
         * @brief Define SafeInputStringStream and SafeOutputStringStream classes and their supporting constants
         */

        BL_DECLARE_SAFE_STRING_STREAM(
            SafeInputStringStream,
            std::istringstream,
            std::ios_base::in,
            std::ios_base::badbit
            )

        BL_DECLARE_SAFE_STRING_STREAM(
            SafeOutputStringStream,
            std::ostringstream,
            std::ios_base::out,
            std::ios_base::failbit | std::ios_base::badbit
            )

        inline void secureWipe( SAA_inout cpp::SafeOutputStringStream& oss ) NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            const auto currentPos = oss.tellp();

            if( currentPos > 0 )
            {
                oss.seekp( 0 );

                for( auto i = 0; i < currentPos; ++i )
                {
                    oss << '0';
                }
            }

            BL_NOEXCEPT_END()
        }

#if defined(_MSC_VER)
/*
 * Disable VC compiler warning C4250: 'class1' : inherits 'class2::member' via dominance
 * The VC <sstream> header file uses the same pragma to disable the warning
 */
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

        BL_DECLARE_SAFE_STRING_STREAM(
            SafeStringStream,
            std::stringstream,
            std::ios_base::in | std::ios_base::out,
            std::ios_base::badbit
            )

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

        /**
         * @brief class SortedVectorHelper - a helper class to maintain vector in sorted state
         *
         * Using sorted std::vector instead of e.g. std::set in most cases is actually better
         * and faster because of locality of data (for access) plus it gives you random access
         *
         * Using sorted std::vector is not appropriate only when the vector size is expected
         * to be relatively large and non-trivial amount of inserts will be happening often or
         * when very large number of inserts and deletes will be done very frequently and the
         * size of the vector is non-trivial
         *
         * Note also that Boost.Container too has implementation of a vector based sorted
         * containers (flat_* - flat_set / flat_map, etc), but these implementations are non
         * standard and generally not necessary as in most cases one can simply use the std
         * vector implementation with the 3 helpers below and taking a dependency on huge
         * chunk of new non-std implementation when one can use the standard implementation
         * is not necessary (the only thing that it provides extra, which is slightly better
         * encapsulation, is also typically not necessary because these classes are not meant
         * to be abstractions, but implementation blocks - e.g. if a vector is expected to be
         * sorted you are going to only use sorted inserts on it)
         */

        template
        <
            typename T,
            typename P = std::less< T >
        >
        class SortedVectorHelper
        {
            BL_CTR_DEFAULT( SortedVectorHelper, public )
            BL_NO_COPY_OR_MOVE( SortedVectorHelper )

        public:

            typedef std::vector< T >                                vector_t;
            typedef T                                               value_t;
            typedef P                                               predicate_t;

            typedef typename vector_t::iterator                     iterator;
            typedef typename vector_t::const_iterator               const_iterator;

        protected:

            predicate_t                                             m_lessThan;

        public:

            auto insert(
                SAA_inout           vector_t&                       vector,
                SAA_in              T                               value
                )
                -> std::pair< iterator, bool >
            {
                auto pos = std::lower_bound( vector.begin(), vector.end(), value, m_lessThan );

                if( vector.end() == pos || m_lessThan( value, *pos ) )
                {
                    return std::make_pair(
                        vector.insert( std::move( pos ), std::move( value ) ),
                        true /* inserted */
                        );
                }

                return std::make_pair( std::move( pos ), false /* inserted */ );
            }

            auto erase(
                SAA_inout           vector_t&                       vector,
                SAA_in              const T&                        value
                )
                -> bool
            {
                const auto pos = find( vector, value );

                if( pos != vector.end() )
                {
                    vector.erase( pos );

                    return true;
                }

                return false;
            }

            auto cfind(
                SAA_inout           vector_t&                       vector,
                SAA_in              const T&                        value
                )
                -> const_iterator
            {
                auto pos = std::lower_bound( vector.cbegin(), vector.cend(), value, m_lessThan );

                return vector.cend() == pos || m_lessThan( value, *pos ) ? vector.cend() : pos;
            }

            auto find(
                SAA_inout           vector_t&                       vector,
                SAA_in              const T&                        value
                )
                -> iterator
            {
                auto pos = std::lower_bound( vector.begin(), vector.end(), value, m_lessThan );

                return vector.end() == pos || m_lessThan( value, *pos ) ? vector.end() : pos;
            }
        };

        /**
         * @brief An enum hasher helper to allow using enum as key in std containers; e.g.:
         *
         * std::map< EnumType, T, cpp::EnumHasher > map_t;
         */

        struct EnumHasher
        {
            template
            <
                typename T
            >
            std::size_t operator()( SAA_in const T value ) const
            {
                return static_cast< std::size_t >( value );
            }
        };

        inline void secureWipe( SAA_inout std::string& text ) NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            text.assign( text.size(), '0' );

            BL_NOEXCEPT_END()
        }

        inline void secureWipe( SAA_inout std::vector< std::string >& strings ) NOEXCEPT
        {
            for( auto& item : strings )
            {
                secureWipe( item );
            }
        }

        template
        <
            typename K
        >
        inline void secureWipe( SAA_inout std::map< K, std::string >& strings ) NOEXCEPT
        {
            for( auto& pair : strings )
            {
                secureWipe( pair.second );
            }
        }

        template
        <
            typename K
        >
        inline void secureWipe( SAA_inout std::unordered_map< K, std::string >& strings ) NOEXCEPT
        {
            for( auto& pair : strings )
            {
                secureWipe( pair.second );
            }
        }

#define BL_WIPE_ON_EXIT( target ) \
    BL_SCOPE_EXIT( \
        bl::str::secureWipe( target ); \
        )

    } // cpp

} // bl

namespace std
{
    /**
     * @brief Provide specializations of hash function in the std namespace
     * This will allow us to use std::unique_ptr and SafeUniquePtr as a key
     * in the new C++11 std containers
     */

    template
    <
        typename T
    >
    struct hash< unique_ptr< T > >
    {
        std::size_t operator()( const unique_ptr< T >& ptr ) const
        {
            std::hash< T* > hasher;
            return hasher( ptr.get() );
        }
    };

    template
    <
        typename T
    >
    struct hash< bl::cpp::SafeUniquePtr< T > >
    {
        std::size_t operator()( const bl::cpp::SafeUniquePtr< T >& ptr ) const
        {
            std::hash< T* > hasher;
            return hasher( ptr.get() );
        }
    };

} // std

#endif /* __BL_CPP_H_ */
