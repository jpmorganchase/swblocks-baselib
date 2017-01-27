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

#ifndef __BL_ERRORHANDLING_H_
#define __BL_ERRORHANDLING_H_

#include <baselib/core/MessageBuffer.h>
#include <baselib/core/CPP.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/TimeUtils.h>

#include <baselib/core/detail/UuidBoostImports.h>
#include <baselib/core/detail/TimeBoostImports.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/exception/all.hpp>
#include <boost/current_function.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <exception>
#include <string>
#include <utility>
#include <type_traits>
#include <sstream>

/*
 * Throw macros
 */

#define BL_EXCEPTION_IMPL( x, msg ) \
    ( \
        bl::eh::enable_current_exception( bl::eh::enable_error_info( x ) ) \
            << bl::eh::throw_function( BOOST_CURRENT_FUNCTION ) \
            << bl::eh::throw_file( __FILE__ ) \
            << bl::eh::throw_line( __LINE__ ) \
            << bl::eh::errinfo_message( msg ) \
            << bl::eh::errinfo_time_thrown( bl::time::getCurrentLocalTimeISO() ) \
    ) \

#ifdef BL_ENABLE_EXCEPTION_HOOKS

#define BL_EXCEPTION( x, msg ) bl::eh::detail::enableThrowHook( BL_EXCEPTION_IMPL( x, msg ) )

#define BL_EXCEPTION_HOOKS_THROW_GUARD( hook ) \
    const bl::eh::detail::ExceptionHooksThrowGuard BL_ANONYMOUS_VARIABLE( __ehThrowGuard ) ( hook );

#else // BL_ENABLE_EXCEPTION_HOOKS

#define BL_EXCEPTION( x, msg ) BL_EXCEPTION_IMPL( x, msg )

#define BL_EXCEPTION_HOOKS_THROW_GUARD( hook )

#endif // BL_ENABLE_EXCEPTION_HOOKS

#define BL_MAKE_USER_FRIENDLY( x ) ( ( x ) << bl::eh::errinfo_is_user_friendly( true ) ) \

#define BL_THROW( x, msg ) \
    do \
    { \
        throw BL_EXCEPTION( x, bl::resolveMessage( msg ) ); \
    } \
    while( false ) \

#define BL_THROW_USER_FRIENDLY( x, msg ) \
    do \
    { \
        throw BL_MAKE_USER_FRIENDLY( BL_EXCEPTION( x, bl::resolveMessage( msg ) ) ); \
    } \
    while( false ) \

#define BL_THROW_USER( msg ) \
    do \
    { \
        throw BL_EXCEPTION( bl::UserMessageException(), bl::resolveMessage( msg ) ); \
    } \
    while( false ) \

#define BL_THROW_EC( ec, msg ) \
    do \
    { \
        const auto __msg42 = bl::resolveMessage( msg ); \
        throw BL_EXCEPTION( bl::SystemException::create( ec, __msg42 ), __msg42 ); \
    } \
    while( false ) \

#define BL_THROW_EC_USER_FRIENDLY( ec, msg ) \
    do \
    { \
        const auto __msg42 = bl::resolveMessage( msg ); \
        throw BL_MAKE_USER_FRIENDLY( \
            BL_EXCEPTION( bl::SystemException::create( ec, __msg42 ), __msg42 ) \
            ); \
    } \
    while( false ) \

#define BL_MAKE_EXCEPTION_PTR( x, msg ) \
    std::make_exception_ptr( BL_EXCEPTION( x, bl::resolveMessage( msg ) ) ) \


/*
 * CHK utility macros
 */

#define BL_SYSTEM_ERROR_DEFAULT_MSG "System error has occurred"

/*
 * TODO: remove the double negation in the if statement below once we upgrade the compiler
 *
 * The double negation is to avoid the following MSVC compiler bug:
 * https://svn.boost.org/trac/boost/ticket/7964
 *
 * Which emits the following incorrect error:
 * C4800: 'boost::system::error_code::unspecified_bool_type' : forcing value to bool 'true' or 'false' (performance warning)
 */

#define BL_CHK_EC( ec, msg ) \
    do \
    { \
        if( !! ( ec ) ) \
        { \
            BL_THROW_EC( ec, msg ); \
        } \
    } \
    while( false ) \

#define BL_CHK_EC_USER_FRIENDLY( ec, msg ) \
    do \
    { \
        if( !! ( ec ) ) \
        { \
            BL_THROW_EC_USER_FRIENDLY( ec, msg ); \
        } \
    } \
    while( false ) \

#define BL_CHK_EC_NM( ec ) \
    BL_CHK_EC( ec, BL_SYSTEM_ERROR_DEFAULT_MSG ) \

#define BL_CHK_T( ev, rc, x, msg ) \
    do \
    { \
        if( ( ev ) == ( rc ) ) \
        { \
            BL_THROW( x, msg ); \
        } \
    } \
    while( false ) \

#define BL_CHK_T_USER_FRIENDLY( ev, rc, x, msg ) \
    BL_CHK_T( ev, rc, BL_MAKE_USER_FRIENDLY( x ), msg ) \

#define BL_CHK_USER( ev, rc, msg ) \
    BL_CHK_T( ev, rc, bl::UserMessageException(), msg ) \

#define BL_CHK_ARG( cond, argName ) \
    BL_CHK_T( \
        false, \
        ( cond ), \
        bl::ArgumentException(), \
        bl::detail::EhUtils::resolveInvalidArgumentMsg( argName, #argName ) \
        ); \

#define BL_CHK( ev, rc, msg ) \
    BL_CHK_T( ev, rc, bl::UnexpectedException(), msg ) \

#define BL_CHK_USER_FRIENDLY( ev, rc, msg ) \
    BL_CHK_T( ev, rc, BL_MAKE_USER_FRIENDLY( bl::UnexpectedException() ), msg ) \

#define BL_CHK_ERRNO( ev, rc, msg ) \
    do \
    { \
        errno = 0; \
        if( ( ev ) == ( rc ) ) \
        { \
            bl::eh::error_code __ec( errno, bl::eh::generic_category() ); \
            BL_THROW_EC( __ec, msg ); \
        } \
    } \
    while( false ) \

#define BL_CHK_ERRNO_USER_FRIENDLY( ev, rc, msg ) \
    do \
    { \
        errno = 0; \
        if( ( ev ) == ( rc ) ) \
        { \
            bl::eh::error_code __ec( errno, bl::eh::generic_category() ); \
            BL_THROW_EC_USER_FRIENDLY( __ec, msg ); \
        } \
    } \
    while( false ) \

#define BL_CHK_ERRNO_NM( ev, rc ) \
    BL_CHK_ERRNO( ev, rc, BL_SYSTEM_ERROR_DEFAULT_MSG  ) \

/*
 * Exception definition macros
 */

#define BL_DECLARE_EXCEPTION_FULL( ns, className, baseClass ) \
    namespace ns \
    { \
        template \
        < \
            typename E = void \
        > \
        class className ## T : virtual public baseClass \
        { \
        public: \
            \
            virtual const char* fullTypeName() const NOEXCEPT OVERRIDE \
            { \
                return #ns "::" #className; \
            } \
        \
        }; \
        typedef className ## T<> className; \
        \
    } \

#define BL_DECLARE_EXCEPTION_NS( ns, className ) \
    BL_DECLARE_EXCEPTION_FULL( ns, className, bl::BaseExceptionDefault ) \

#define BL_DECLARE_EXCEPTION( className ) \
    BL_DECLARE_EXCEPTION_FULL( bl, className, bl::BaseExceptionDefault ) \

#define BL_THROW_SERVER_ERROR( errcondition, msg ) \
    BL_THROW( \
        bl::ServerErrorException() \
            << bl::eh::errinfo_error_code( \
                bl::eh::errc::make_error_code( errcondition ) \
                ), \
        msg \
        ) \

#define BL_CHK_SERVER_ERROR( ev, rc, errcondition, msg ) \
    BL_CHK_T( \
        ev, \
        rc, \
        bl::ServerErrorException() \
            << bl::eh::errinfo_error_code( \
                bl::eh::errc::make_error_code( errcondition ) \
                ), \
        msg ) \

namespace bl
{
    /**
     * @brief class BaseException, the base exception for all exceptions (forward declaration)
     */

    template
    <
        typename E = void
    >
    class BaseExceptionT;

    typedef BaseExceptionT<> BaseException;

    namespace eh
    {
        namespace detail
        {
            using boost::diagnostic_information;

            /*******************************************************************
             * Exception hooks
             */

            typedef cpp::function< void ( SAA_in const BaseException& ) NOEXCEPT > ThrowExceptionHook;

            template
            <
                typename E = void
            >
            class ExceptionHooksGlobalStateT
            {
                BL_DECLARE_STATIC( ExceptionHooksGlobalStateT )

            public:

                static ThrowExceptionHook                                               g_throwHook;
                static os::mutex                                                        g_lock;

                template
                <
                    typename T
                >
                static const T&
                enableThrowHook( SAA_in const T& exception ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( g_lock );

                    if( g_throwHook )
                    {
                        g_throwHook( static_cast< const BaseException& >( exception ) );
                    }

                    BL_NOEXCEPT_END()

                    return exception;
                }
            };

            template
            <
                typename E
            >
            ThrowExceptionHook
            ExceptionHooksGlobalStateT< E >::g_throwHook;

            template
            <
                typename E
            >
            os::mutex
            ExceptionHooksGlobalStateT< E >::g_lock;

            typedef ExceptionHooksGlobalStateT<> ExceptionHooksGlobalState;

            template
            <
                typename T
            >
            inline const T&
            enableThrowHook( SAA_in const T& exception ) NOEXCEPT
            {
                return ExceptionHooksGlobalState::enableThrowHook( exception );
            }

            template
            <
                typename E = void
            >
            class ExceptionHooksThrowGuardT
            {
                BL_NO_COPY_OR_MOVE( ExceptionHooksThrowGuardT )

            protected:

                ThrowExceptionHook                                                      m_throwHook;

            public:

                ExceptionHooksThrowGuardT( SAA_in ThrowExceptionHook&& throwHook ) NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( ExceptionHooksGlobalState::g_lock );

                    m_throwHook.swap( ExceptionHooksGlobalState::g_throwHook );
                    ExceptionHooksGlobalState::g_throwHook.swap( throwHook );

                    BL_NOEXCEPT_END()
                }

                ~ExceptionHooksThrowGuardT() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    BL_MUTEX_GUARD( ExceptionHooksGlobalState::g_lock );

                    ExceptionHooksGlobalState::g_throwHook.swap( m_throwHook );

                    BL_NOEXCEPT_END()
                }
            };

            typedef ExceptionHooksThrowGuardT<> ExceptionHooksThrowGuard;

            /*
             * Exception hooks
             *******************************************************************/
        }

        /*
         * Import error_code from boost::system library
         */

        using boost::system::error_code;
        using boost::system::error_category;
        using boost::system::generic_category;
        using boost::system::system_category;
        using boost::system::system_error;

        namespace errc = boost::system::errc;

        /*
         * Expose error_info plus related APIs and the various std errinfo objects
         */

        using boost::exception;
        using boost::error_info;
        using boost::get_error_info;
        using boost::to_string;

        using boost::enable_current_exception;
        using boost::enable_error_info;

        using boost::throw_function;
        using boost::throw_file;
        using boost::throw_line;

        using boost::errinfo_api_function;
        using boost::errinfo_at_line;
        using boost::errinfo_errno;
        using boost::errinfo_file_handle;
        using boost::errinfo_file_name;
        using boost::errinfo_file_open_mode;
        using boost::errinfo_type_info_name;

        /**
         * @brief Error info for the user message (displayed in what())
         */

        typedef error_info< struct errinfo_message_, std::string >                              errinfo_message;
        typedef error_info< struct errinfo_nested_exception_ptr_, std::exception_ptr >          errinfo_nested_exception_ptr;
        typedef error_info< struct errinfo_time_thrown_, std::string >                          errinfo_time_thrown;
        typedef error_info< struct errinfo_function_name_, std::string >                        errinfo_function_name;
        typedef error_info< struct errinfo_system_code_, int >                                  errinfo_system_code;
        typedef error_info< struct errinfo_category_name_, std::string >                        errinfo_category_name;
        typedef error_info< struct errinfo_error_code_, error_code >                            errinfo_error_code;
        typedef error_info< struct errinfo_error_code_message_, std::string >                   errinfo_error_code_message;
        typedef error_info< struct errinfo_error_uuid_, uuid_t >                                errinfo_error_uuid;
        typedef error_info< struct errinfo_is_expected_, bool >                                 errinfo_is_expected;
        typedef error_info< struct errinfo_task_info_, std::string >                            errinfo_task_info;
        typedef error_info< struct errinfo_host_name_, std::string >                            errinfo_host_name;
        typedef error_info< struct errinfo_service_name_, std::string >                         errinfo_service_name;
        typedef error_info< struct errinfo_endpoint_address_, std::string >                     errinfo_endpoint_address;
        typedef error_info< struct errinfo_endpoint_port_, unsigned short >                     errinfo_endpoint_port;
        typedef error_info< struct errinfo_http_url_, std::string >                             errinfo_http_url;
        typedef error_info< struct errinfo_http_redirect_url_, std::string >                    errinfo_http_redirect_url;
        typedef error_info< struct errinfo_http_status_code_, int >                             errinfo_http_status_code;
        typedef error_info< struct errinfo_http_response_headers_, std::string >                errinfo_http_response_headers;
        typedef error_info< struct errinfo_http_request_details_, std::string >                 errinfo_http_request_details;
        typedef error_info< struct errinfo_parser_file_, std::string >                          errinfo_parser_file;
        typedef error_info< struct errinfo_parser_line_, unsigned int >                         errinfo_parser_line;
        typedef error_info< struct errinfo_parser_column_, unsigned int >                       errinfo_parser_column;
        typedef error_info< struct errinfo_parser_reason_, std::string >                        errinfo_parser_reason;
        typedef error_info< struct errinfo_external_command_output_, std::string >              errinfo_external_command_output;
        typedef error_info< struct errinfo_external_command_exit_code_, int >                   errinfo_external_command_exit_code;
        typedef error_info< struct errinfo_string_value_, std::string >                         errinfo_string_value;
        typedef error_info< struct errinfo_is_user_friendly_, bool >                            errinfo_is_user_friendly;
        typedef error_info< struct errinfo_ssl_is_verify_failed_, bool >                        errinfo_ssl_is_verify_failed;
        typedef error_info< struct errinfo_ssl_is_verify_error_, int >                          errinfo_ssl_is_verify_error;
        typedef error_info< struct errinfo_ssl_is_verify_error_message_, std::string >          errinfo_ssl_is_verify_error_message;
        typedef error_info< struct errinfo_ssl_is_verify_error_string_, std::string >           errinfo_ssl_is_verify_error_string;
        typedef error_info< struct errinfo_ssl_is_verify_subject_name_, std::string >           errinfo_ssl_is_verify_subject_name;
        typedef error_info< struct errinfo_service_status_, int >                               errinfo_service_status;
        typedef error_info< struct errinfo_service_status_category_, int >                      errinfo_service_status_category;
        typedef error_info< struct errinfo_service_status_message_, std::string >               errinfo_service_status_message;

        template
        <
            typename T
        >
        std::string diagnostic_information( SAA_in const T& exception )
        {
            cpp::SafeOutputStringStream os;

            os << detail::diagnostic_information( exception );
            auto info = get_error_info< errinfo_nested_exception_ptr >( exception );

            while( info )
            {
                try
                {
                    cpp::safeRethrowException( *info );
                }
                catch( std::exception& e )
                {
                    os << "Nested exception:\n";
                    os << detail::diagnostic_information( e );
                    info = get_error_info< errinfo_nested_exception_ptr >( e );
                }
            }

            return os.str();
        }

        inline std::string diagnostic_information( SAA_in const std::exception_ptr& eptr )
        {
            try
            {
                cpp::safeRethrowException( eptr );
            }
            catch( std::exception& e )
            {
                return diagnostic_information( e );
            }

            return std::string();
        }

        inline bool isUserFriendly( SAA_in const std::exception& exception ) NOEXCEPT
        {
            const bool* isUserFriendlyPtr =
                eh::get_error_info< eh::errinfo_is_user_friendly >( exception );

            return ( isUserFriendlyPtr && *isUserFriendlyPtr );
        }

        inline bool isErrorCondition(
            SAA_in          const eh::errc::errc_t              condition,
            SAA_in          const eh::error_code&               ec
            ) NOEXCEPT
        {
            /*
             * Note that on Linux we have to check for both generic and system categories
             * as on Linux these errors sometimes may be reported system while on Windows
             * they will always come as generic category
             *
             * eh::errc::make_error_code() always returns eh::error_code
             * with the generic category
             */

            if( eh::errc::make_error_code( condition ) == ec )
            {
                return true;
            }

            #if ! defined( _WIN32 )

            if( eh::error_code( condition, eh::system_category() ) == ec )
            {
                return true;
            }

            #endif

            return false;
        }

        inline std::string errorCodeToString( SAA_in const error_code& ec )
        {
            cpp::SafeOutputStringStream os;
            os << ec;
            return os.str();
        }

        inline eh::error_code errorCodeFromExceptionPtr( SAA_in const std::exception_ptr& eptr ) NOEXCEPT
        {
            try
            {
                cpp::safeRethrowException( eptr );
            }
            catch( eh::system_error& e )
            {
                return e.code();
            }
            catch( std::exception& e )
            {
                const auto* ec = eh::get_error_info< eh::errinfo_error_code >( e );

                if( ec )
                {
                    return *ec;
                }
            }

            return eh::error_code();
        }

    } // eh

    /**
     * @brief class BaseException, the base exception for all exceptions
     */

    template
    <
        typename E
    >
    class BaseExceptionT :
        public virtual eh::exception
    {
        BL_NO_CREATE( BaseExceptionT )

    public:

        virtual const char* fullTypeName() const NOEXCEPT = 0;

        const std::string* message() const
        {
            return eh::get_error_info< eh::errinfo_message >( *this );
        }

        std::string details() const
        {
            return eh::diagnostic_information( *this );
        }

        const int* errNo() const
        {
            return eh::get_error_info< eh::errinfo_errno >( *this );
        }

        const eh::error_code* errorCode() const
        {
            return eh::get_error_info< eh::errinfo_error_code >( *this );
        }

        const std::string* errorCodeMessage() const
        {
            return eh::get_error_info< eh::errinfo_error_code_message >( *this );
        }

        const int* httpStatusCode() const
        {
            return eh::get_error_info< eh::errinfo_http_status_code >( *this );
        }

        const std::string* httpRedirectUrl() const
        {
            return eh::get_error_info< eh::errinfo_http_redirect_url >( *this );
        }

        const std::string* externalCommandOutput() const
        {
            return eh::get_error_info< eh::errinfo_external_command_output >( *this );
        }

        const std::string* timeThrown() const
        {
            return eh::get_error_info< eh::errinfo_time_thrown >( *this );
        }
    };

    /*
     * The default base for our own exceptions which
     * should derive from std::exception
     */

    template
    <
        typename T = std::exception
    >
    class BaseExceptionDefaultT :
        public virtual T,
        public virtual BaseException
    {
    public:

        virtual const char* what() const NOTHROW_REAL
        {
            static_assert( std::is_base_of< std::exception, T >::value, "T must be derived from std::exception" );

            const std::string* msg = message();

            if( msg )
            {
                return msg -> c_str();
            }

            return T::what();
        }
    };

    typedef BaseExceptionDefaultT<> BaseExceptionDefault;

    /*
     * Declare the user message exception separately to tag it
     * as 'user friendly' explicitly
     */

    template
    <
        typename E = void
    >
    class UserMessageExceptionT :
        public virtual BaseExceptionDefault
    {
    public:

        virtual const char* fullTypeName() const NOEXCEPT OVERRIDE
        {
            return "bl::UserMessageException";
        }

        UserMessageExceptionT()
        {
            BL_MAKE_USER_FRIENDLY( *this );
        }
    };

    typedef UserMessageExceptionT<> UserMessageException;

} // bl

/*
 * Common well known exceptions
 *
 * When adding a new exception below remember to add a relevant code
 * in createExceptionFromObject() in baselib/data/eh/ServerErrorHelpers.h
 */

BL_DECLARE_EXCEPTION( ArgumentException )
BL_DECLARE_EXCEPTION( ArgumentNullException )
BL_DECLARE_EXCEPTION( CacheException )
BL_DECLARE_EXCEPTION( ExternalCommandException )
BL_DECLARE_EXCEPTION( HttpException )
BL_DECLARE_EXCEPTION( HttpServerException )
BL_DECLARE_EXCEPTION( InvalidDataFormatException )
BL_DECLARE_EXCEPTION( JavaException )
BL_DECLARE_EXCEPTION( JsonException )
BL_DECLARE_EXCEPTION( NotFoundException )
BL_DECLARE_EXCEPTION( NotSupportedException )
BL_DECLARE_EXCEPTION( ObjectDisconnectedException )
BL_DECLARE_EXCEPTION( SecurityException )
BL_DECLARE_EXCEPTION( ServerErrorException )
BL_DECLARE_EXCEPTION( ServerNoConnectionException )
BL_DECLARE_EXCEPTION( TimeoutException )
BL_DECLARE_EXCEPTION( UnexpectedException )
BL_DECLARE_EXCEPTION_FULL( bl, UserAuthenticationException, UserMessageException )
BL_DECLARE_EXCEPTION( XmlException )

namespace bl
{
    /*
     * A system exception
     */

    class SystemException :
        /**
         * *Note*: normally exceptions should use virtual inheritance (for details why see here:
         * http://www.boost.org/community/error_handling.html)
         *
         * However since system_error which derives from runtime_error doesn't have default
         * constructor 'no appropriate default constructor available' compile error because
         * boost::exception needs the default constructor for cloning, but because of the
         * virtual inheritance the compiler insists all virtual bases must have default
         * constructor.
         *
         * For more details about the issue see the following link:
         *
         * http://boost.2283326.n4.nabble.com/exception-virtual-inheritance-troubles-td2671026.html
         *
         */
        public eh::system_error,
        public virtual BaseException
    {
    protected:

        typedef eh::system_error base_type;

        SystemException( SAA_in const eh::error_code& code, SAA_in const std::string& msg )
            :
            eh::system_error( code, msg )
        {
        }

    public:

        virtual const char* fullTypeName() const NOEXCEPT OVERRIDE
        {
            return "bl::SystemException";
        }

        virtual const char* what() const NOTHROW_REAL OVERRIDE
        {
            return base_type::what();
        }

        static SystemException create( SAA_in const eh::error_code& code, SAA_in const std::string& msg )
        {
            /*
             * Create a SystemException and decorate it accordingly
             */

            SystemException exception( code, msg );

            if( code.category() == eh::system_category() )
            {
                /*
                 * This is a system / OS level code
                 */

                exception << eh::errinfo_system_code( code.value() );
            }
            else if( code.category() == eh::generic_category() )
            {
                /*
                 * This is a system / OS level code
                 */

                exception << eh::errinfo_errno( code.value() );
            }

            exception << eh::errinfo_error_code( code );

            exception << eh::errinfo_category_name( code.category().name() );

            auto errorCodeMsg = code.message();

            if( ! errorCodeMsg.empty() )
            {
                exception << eh::errinfo_error_code_message( std::move( errorCodeMsg ) );
            }

            return exception;
        }
    };

    namespace detail
    {
        template
        <
            bool resolveArgValue
        >
        class ArgumentValueResolver;

        template
        <
        >
        class ArgumentValueResolver< true /* resolveArgValue */ >
        {
        public:

            template
            <
                typename T
            >
            static MessageBuffer resolve( SAA_in const T& arg, SAA_in const std::string& argName )
            {
                MessageBuffer msg;

                msg
                    << "Invalid argument value '"
                    << arg
                    << "' provided for argument '"
                    << argName
                    << "'"
                    ;

                return msg;
            }
        };

        template
        <
        >
        class ArgumentValueResolver< false /* resolveArgValue */ >
        {
        public:

            template
            <
                typename T
            >
            static MessageBuffer resolve(
                SAA_in    const T&              /* arg */,
                SAA_in    const std::string&    argName )
            {
                MessageBuffer msg;

                msg
                    << "Invalid argument value provided for argument '"
                    << argName
                    << "'"
                    ;

                return msg;
            }
        };

        /**
         * @brief class EhUtils. Contains error handling utility code.
         */
        template
        <
            typename E = void
        >
        class EhUtilsT FINAL
        {
            BL_DECLARE_STATIC( EhUtilsT )

        public:

            template
            <
                typename T
            >
            static MessageBuffer resolveInvalidArgumentMsg( SAA_in const T& arg, SAA_in const std::string& argName )
            {
                typedef ArgumentValueResolver< std::is_scalar< T >::value > resolver_t;
                return resolver_t::resolve( arg, argName );
            }
        };

        typedef EhUtilsT<> EhUtils;

    } // detail

    namespace eh
    {
        /**
         * @brief The error handling callback interface.
         * Returns true if the exception was handled and false if it was not
         */

        typedef cpp::function< bool ( SAA_in const std::exception_ptr& eptr ) > eh_callback_t;

    } // eh

} // bl

namespace std
{
    /**
     * @brief Provide specializations of hash function in the std namespace
     * This will allow us to use eh::error_code as a key in the new C++11
     * std containers
     */

    template
    <
    >
    struct hash< bl::eh::error_code >
    {
        std::size_t operator()( const bl::eh::error_code& ec ) const
        {
            /*
             * Just convert the error code to a string and then hash it
             */

            std::hash< std::string > hasher;
            return hasher( bl::eh::errorCodeToString( ec ) );
        }
    };

} // std

#endif /* __BL_ERRORHANDLING_H_ */
