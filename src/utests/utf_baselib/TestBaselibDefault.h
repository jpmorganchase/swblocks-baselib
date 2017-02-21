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

#include <utests/baselib/UtfBaseLibCommon.h>
#include <utests/baselib/TestUtils.h>
#include "examples/objmodel/MyInterfaces.h"
#include "examples/objmodel/MyObjectImpl.h"

#include <baselib/core/GroupBy.h>
#include <baselib/core/Table.h>
#include <baselib/core/Tree.h>

#include <utests/baselib/TestFsUtils.h>

UTF_AUTO_TEST_CASE( BaseLib_Default )
{
    const int result = 2 + 2;
    UTF_CHECK( 4 == result );
}

/************************************************************************
 * BaseDefs.h tests
 */

namespace
{
    enum class ParamForwardingReferenceType
    {
        Rvalue,
        Lvalue,
        ConstLvalue,
    };

    inline void paramForwardingConstIntTargetCall(
        SAA_in      const int&                                  x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "const int&"
            );

        UTF_REQUIRE_EQUAL( expectedReferenceType, ParamForwardingReferenceType::ConstLvalue );
        UTF_REQUIRE_EQUAL( x, 10 );
    }

    inline void paramForwardingConstIntTargetCall(
        SAA_inout   int&                                        x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "int&"
            );

        UTF_REQUIRE_EQUAL( expectedReferenceType, ParamForwardingReferenceType::Lvalue );
        UTF_REQUIRE_EQUAL( x, 10 );
    }

    inline void paramForwardingConstIntTargetCall(
        SAA_in      int&&                                       x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "int&&"
            );

        UTF_REQUIRE_EQUAL( expectedReferenceType, ParamForwardingReferenceType::Rvalue );
        UTF_REQUIRE_EQUAL( x, 10 );
    }

    template
    <
        typename T
    >
    inline void paramForwardingIntUniversalRefCall(
        SAA_in      T&&                                         x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        paramForwardingConstIntTargetCall( BL_PARAM_FWD( x ), expectedReferenceType );
    }

    inline void paramForwardingIntRvalueRefCall(
        SAA_in      int&&                                       x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        paramForwardingConstIntTargetCall( BL_PARAM_FWD( x ), expectedReferenceType );
    }

    inline void paramForwardingIntTests()
    {
        int                 i1 = 10;
        int&                i2 = i1;
        const int&          i3 = i1;
        int                 i4 = 10;

        paramForwardingIntUniversalRefCall( 10, ParamForwardingReferenceType::Rvalue );
        paramForwardingIntUniversalRefCall( i1, ParamForwardingReferenceType::Lvalue );
        paramForwardingIntUniversalRefCall( i2, ParamForwardingReferenceType::Lvalue );
        paramForwardingIntUniversalRefCall( i3, ParamForwardingReferenceType::ConstLvalue );
        paramForwardingIntUniversalRefCall( std::move( i4 ), ParamForwardingReferenceType::Rvalue );

        paramForwardingIntRvalueRefCall( 10, ParamForwardingReferenceType::Rvalue );
        paramForwardingIntRvalueRefCall( std::move( i1 ), ParamForwardingReferenceType::Rvalue );
    }

    class ParamForwardingTestClass
    {
        BL_NO_COPY( ParamForwardingTestClass )

    private:

        int m_value;

    public:

        ParamForwardingTestClass( SAA_in const int value )
            :
            m_value( value )
        {
        }

        ParamForwardingTestClass( SAA_in ParamForwardingTestClass&& other )
            :
            m_value( other.m_value )
        {
        }

        ParamForwardingTestClass& operator = ( SAA_in ParamForwardingTestClass&& other )
        {
            m_value = other.m_value;
            return *this;
        }

        int value() const NOEXCEPT
        {
            return m_value;
        }
    };

    inline void paramForwardingConstClassTargetCall(
        SAA_in      const ParamForwardingTestClass&             x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "const ParamForwardingTestClass&"
            );

        UTF_REQUIRE_EQUAL( expectedReferenceType, ParamForwardingReferenceType::ConstLvalue );
        UTF_REQUIRE_EQUAL( x.value(), 10 );
    }

    inline void paramForwardingConstClassTargetCall(
        SAA_inout   ParamForwardingTestClass&                   x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "ParamForwardingTestClass&"
            );

        UTF_REQUIRE_EQUAL( expectedReferenceType, ParamForwardingReferenceType::Lvalue );
        UTF_REQUIRE_EQUAL( x.value(), 10 );
    }

    inline void paramForwardingConstClassTargetCall(
        SAA_in      ParamForwardingTestClass&&                  x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "ParamForwardingTestClass&&"
            );

        UTF_REQUIRE_EQUAL( expectedReferenceType, ParamForwardingReferenceType::Rvalue );
        UTF_REQUIRE_EQUAL( x.value(), 10 );
    }

    template
    <
        typename T
    >
    inline void paramForwardingClassUniversalRefCall(
        SAA_in      T&&                                         x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        paramForwardingConstClassTargetCall( BL_PARAM_FWD( x ), expectedReferenceType );
    }

    inline void paramForwardingIntRvalueRefCall(
        SAA_in      ParamForwardingTestClass&&                  x,
        SAA_in      const ParamForwardingReferenceType          expectedReferenceType
        )
    {
        paramForwardingConstClassTargetCall( BL_PARAM_FWD( x ), expectedReferenceType );
    }

    inline ParamForwardingTestClass paramForwardingGetClass()
    {
        return ParamForwardingTestClass( 10 );
    }

    inline void paramForwardingClassTests()
    {
        typedef ParamForwardingTestClass class_t;

        class_t             c1( 10 );
        class_t&            c2 = c1;
        const class_t&      c3 = c1;
        class_t             c4( 10 );

        paramForwardingClassUniversalRefCall( paramForwardingGetClass(), ParamForwardingReferenceType::Rvalue );
        paramForwardingClassUniversalRefCall( c1, ParamForwardingReferenceType::Lvalue );
        paramForwardingClassUniversalRefCall( c2, ParamForwardingReferenceType::Lvalue );
        paramForwardingClassUniversalRefCall( c3, ParamForwardingReferenceType::ConstLvalue );
        paramForwardingClassUniversalRefCall( std::move( c4 ), ParamForwardingReferenceType::Rvalue );

        paramForwardingIntRvalueRefCall( std::move( c1 ), ParamForwardingReferenceType::Rvalue );
        paramForwardingIntRvalueRefCall( class_t( 10 ), ParamForwardingReferenceType::Rvalue );
        paramForwardingIntRvalueRefCall( paramForwardingGetClass(), ParamForwardingReferenceType::Rvalue );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( BaseLib_ParamFwding )
{
    paramForwardingIntTests();

    paramForwardingClassTests();
}

UTF_AUTO_TEST_CASE( BaseLib_BaseDefsTests )
{
    using bl::isPowerOfTwo;
    using bl::alignedOf;

    UTF_REQUIRE( ! isPowerOfTwo( 0 ) );
    UTF_REQUIRE( isPowerOfTwo( 1 ) );
    UTF_REQUIRE( isPowerOfTwo( 2 ) );
    UTF_REQUIRE( ! isPowerOfTwo( 3 ) );
    UTF_REQUIRE( isPowerOfTwo( 4 ) );
    UTF_REQUIRE( ! isPowerOfTwo( 5 ) );
    UTF_REQUIRE( ! isPowerOfTwo( 7 ) );
    UTF_REQUIRE( isPowerOfTwo( 8 ) );
    UTF_REQUIRE( ! isPowerOfTwo( 9 ) );
    UTF_REQUIRE( isPowerOfTwo( 16 ) );
    UTF_REQUIRE( isPowerOfTwo( 32 ) );
    UTF_REQUIRE( isPowerOfTwo( 256 ) );
    UTF_REQUIRE( isPowerOfTwo( 0x8000000000000000ULL ) );
    UTF_REQUIRE( ! isPowerOfTwo( 0x8000000000000001ULL ) );

    UTF_REQUIRE( ! isPowerOfTwo( -1 ) );
    UTF_REQUIRE( ! isPowerOfTwo( -2 ) );
    UTF_REQUIRE( ! isPowerOfTwo( -3 ) );
    UTF_REQUIRE( ! isPowerOfTwo( -4 ) );

    UTF_REQUIRE_EQUAL( 0, alignedOf( 0, 1 ) );
    UTF_REQUIRE_EQUAL( 0, alignedOf( 0, 2 ) );
    UTF_REQUIRE_EQUAL( 0, alignedOf( 0, 4 ) );
    UTF_REQUIRE_EQUAL( 0, alignedOf( 0, 8 ) );

    UTF_REQUIRE_EQUAL( 1, alignedOf( 1, 1 ) );
    UTF_REQUIRE_EQUAL( 2, alignedOf( 1, 2 ) );
    UTF_REQUIRE_EQUAL( 4, alignedOf( 1, 4 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 1, 8 ) );

    UTF_REQUIRE_EQUAL( 2, alignedOf( 2, 1 ) );
    UTF_REQUIRE_EQUAL( 2, alignedOf( 2, 2 ) );
    UTF_REQUIRE_EQUAL( 4, alignedOf( 2, 4 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 2, 8 ) );

    UTF_REQUIRE_EQUAL( 3, alignedOf( 3, 1 ) );
    UTF_REQUIRE_EQUAL( 4, alignedOf( 3, 2 ) );
    UTF_REQUIRE_EQUAL( 4, alignedOf( 3, 4 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 3, 8 ) );

    UTF_REQUIRE_EQUAL( 4, alignedOf( 4, 1 ) );
    UTF_REQUIRE_EQUAL( 4, alignedOf( 4, 2 ) );
    UTF_REQUIRE_EQUAL( 4, alignedOf( 4, 4 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 4, 8 ) );

    UTF_REQUIRE_EQUAL( 7, alignedOf( 7, 1 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 7, 2 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 7, 4 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 7, 8 ) );

    UTF_REQUIRE_EQUAL( 8, alignedOf( 8, 1 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 8, 2 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 8, 4 ) );
    UTF_REQUIRE_EQUAL( 8, alignedOf( 8, 8 ) );

    UTF_REQUIRE_EQUAL( 9, alignedOf( 9, 1 ) );
    UTF_REQUIRE_EQUAL( 10, alignedOf( 9, 2 ) );
    UTF_REQUIRE_EQUAL( 12, alignedOf( 9, 4 ) );
    UTF_REQUIRE_EQUAL( 16, alignedOf( 9, 8 ) );

    int* p0 = ( int* )0;
    int* p1 = ( int* )1;
    int* p2 = ( int* )2;
    int* p3 = ( int* )3;
    int* p4 = ( int* )4;
    int* p8 = ( int* )8;

    UTF_REQUIRE_EQUAL( p0, alignedOf( p0, 1 ) );
    UTF_REQUIRE_EQUAL( p0, alignedOf( p0, 2 ) );
    UTF_REQUIRE_EQUAL( p0, alignedOf( p0, 4 ) );
    UTF_REQUIRE_EQUAL( p0, alignedOf( p0, 8 ) );

    UTF_REQUIRE_EQUAL( p1, alignedOf( p1, 1 ) );
    UTF_REQUIRE_EQUAL( p2, alignedOf( p1, 2 ) );
    UTF_REQUIRE_EQUAL( p4, alignedOf( p1, 4 ) );
    UTF_REQUIRE_EQUAL( p8, alignedOf( p1, 8 ) );

    UTF_REQUIRE_EQUAL( p3, alignedOf( p3, 1 ) );
    UTF_REQUIRE_EQUAL( p4, alignedOf( p3, 2 ) );
    UTF_REQUIRE_EQUAL( p4, alignedOf( p3, 4 ) );
    UTF_REQUIRE_EQUAL( p8, alignedOf( p3, 8 ) );

    UTF_REQUIRE_EQUAL( p4, alignedOf( p4, 1 ) );
    UTF_REQUIRE_EQUAL( p4, alignedOf( p4, 2 ) );
    UTF_REQUIRE_EQUAL( p4, alignedOf( p4, 4 ) );
    UTF_REQUIRE_EQUAL( p8, alignedOf( p4, 8 ) );
}

/************************************************************************
 * ErrorHandling tests
 */

namespace
{
    inline bool getTrue()  { return true; }
    inline bool getFalse() { return false; }

    class DestructorTest
    {
        bl::cpp::ScalarTypeIniter< bool >           m_indestructible;

    public:

        ~DestructorTest()
        {
            if( m_indestructible )
            {
                std::puts( "Attempting to destroy an indestructible class" );
                std::terminate();
            }
        }

        /**
         * Report if static destructors are invoked during emergency shutdown
         */

        void makeIndestructible() NOEXCEPT
        {
            m_indestructible = true;
        }
    };

    static DestructorTest   g_destructorTest;
}

UTF_AUTO_TEST_CASE( BaseLib_EhGenerateAbort )
{
    if( test::UtfArgsParser::isClient() )
    {
        bl::os::GlobalProcessInit initer;

        g_destructorTest.makeIndestructible();

        BL_RIP_MSG( "Abort requested" );

        UTF_FAIL( "BL_RIP_MSG must terminate the process" );
    }

    UTF_CHECK( true );
}

UTF_AUTO_TEST_CASE( BaseLib_EhGenerateAssertionFailure )
{
    if( test::UtfArgsParser::isClient() )
    {
        bl::os::GlobalProcessInit initer;

        g_destructorTest.makeIndestructible();

        BL_ASSERT( true == false );

        UTF_FAIL( "BL_ASSERT must terminate the process" );
    }

    UTF_CHECK( true );
}

UTF_AUTO_TEST_CASE( BaseLib_EhGenerateCrash )
{
    if( test::UtfArgsParser::isClient() )
    {
        bl::os::GlobalProcessInit initer;

        g_destructorTest.makeIndestructible();

        UTF_MESSAGE( "Generating an Access violation" );

        int* i = nullptr;
        *i = 42;

        UTF_FAIL( "Access violation must terminate the process" );
    }

    UTF_CHECK( true );
}

UTF_AUTO_TEST_CASE( BaseLib_EhGenerateInvalidParameter )
{
    if( test::UtfArgsParser::isClient() )
    {
        /*
         * This test is Windows-specific and it produces meaningful output only
         * when linked against the debug version of the C run-time library
         */

        bl::os::GlobalProcessInit initer;

        g_destructorTest.makeIndestructible();

        UTF_MESSAGE( "Generating an Invalid Parameter error" );

        FILE* file = nullptr;
        ::fputs( "foo", file );

        UTF_FAIL( "Invalid Parameter error must terminate the process" );
    }

    UTF_CHECK( true );
}

UTF_AUTO_TEST_CASE( BaseLib_EhGenerateTermination )
{
    if( test::UtfArgsParser::isClient() )
    {
        bl::os::GlobalProcessInit initer;

        std::terminate();

        UTF_FAIL( "terminate() must terminate the process" );
    }

    UTF_CHECK( true );
}

UTF_AUTO_TEST_CASE( BaseLib_TestErrorHandling )
{
    const auto cbTest = [] ( SAA_in const bool useMessageBuffer, SAA_in const int errNo )
    {
        UTF_MESSAGE( "***************** Exception dump *****************\n" );

        try
        {
            if( useMessageBuffer )
            {
                if( -1 == errNo )
                    BL_THROW( bl::UnexpectedException(), BL_MSG() << "This is test exception " << 42 << " ; " << 1.2 );
                else
                    BL_CHK_EC( bl::eh::error_code( errNo, bl::eh::generic_category() ), BL_MSG() << "This is test exception " << 42 << " ; " << 1.2 );
            }
            else
            {
                if( -1 == errNo )
                    BL_THROW( bl::UnexpectedException(), "This is test exception 42 ; 1.2" );
                else
                    BL_CHK_EC( bl::eh::error_code( errNo, bl::eh::generic_category() ), "This is test exception 42 ; 1.2" );
            }

            UTF_CHECK( false );
        }
        catch( bl::UnexpectedException& e )
        {
            UTF_CHECK_EQUAL( "bl::UnexpectedException", e.fullTypeName() );

            const auto msg = e.what();
            UTF_MESSAGE( msg );
            UTF_CHECK_EQUAL( "This is test exception 42 ; 1.2", msg );

            const auto details = e.details();
            UTF_MESSAGE( details );

            UTF_CHECK( nullptr == e.errNo() );

            const auto* timeThrown = e.timeThrown();
            UTF_CHECK( nullptr != timeThrown && ! timeThrown -> empty() );

            bl::str::regex regex( bl::time::regexLocalTimeISO() );
            bl::str::smatch results;
            UTF_CHECK( bl::str::regex_match( *timeThrown, results, regex ) );
        }
        catch( bl::SystemException& e )
        {
            UTF_CHECK_EQUAL( "bl::SystemException", e.fullTypeName() );

            const auto msg = e.what();
            UTF_MESSAGE( msg );
            UTF_CHECK_EQUAL( "This is test exception 42 ; 1.2: Permission denied", msg );

            const auto details = e.details();
            UTF_MESSAGE( details );

            UTF_REQUIRE( nullptr != e.errNo() );
            UTF_CHECK( errNo == *e.errNo() );

            UTF_REQUIRE_EQUAL( EACCES, errNo );
            const auto* str = e.errorCodeMessage();
            UTF_REQUIRE( nullptr != str );
            UTF_REQUIRE_EQUAL( "Permission denied", *str );
        }

        UTF_MESSAGE( "***************** End exception dump *****************\n" );
    };

    cbTest( false /* useMessageBuffer */, -1 /* errNo */ );
    cbTest( false /* useMessageBuffer */, EACCES /* errNo */ );

    cbTest( true /* useMessageBuffer */, -1 /* errNo */ );
    cbTest( true /* useMessageBuffer */, EACCES /* errNo */ );

    BL_CHK( false, getTrue(), BL_MSG() << "This one should not throw ... " );
    BL_CHK( false, getTrue(), BL_MSG() << "This one should not throw: " << 42 );
    BL_CHK_T( false, getTrue(), bl::ArgumentNullException(), BL_MSG() << "This one should not throw: " << 42 );

    const auto cbTestChk = [] ( SAA_in const int errNo )
    {
        UTF_MESSAGE( "***************** Exception dump *****************\n" );

        try
        {
            if( -1 == errNo )
                BL_CHK( false, getFalse(), BL_MSG() << "This one should throw: " << 42 );
            else
                BL_CHK_EC(
                    bl::eh::error_code( errNo, bl::eh::generic_category() ),
                    BL_MSG()
                        << "This one should throw: "
                        << 42
                    );

            UTF_CHECK( false );
        }
        catch( bl::UnexpectedException& e )
        {
            const auto msg = e.what();
            UTF_MESSAGE( msg );
            UTF_CHECK_EQUAL( "This one should throw: 42", msg );

            const auto details = e.details();
            UTF_MESSAGE( details );

            UTF_CHECK( nullptr == e.errNo() );
        }
        catch( bl::SystemException& e )
        {
            const auto msg = e.what();
            UTF_MESSAGE( msg );
            UTF_CHECK_EQUAL( "This one should throw: 42: Permission denied", msg );

            const auto details = e.details();
            UTF_MESSAGE( details );

            UTF_REQUIRE( nullptr != e.errNo() );
            UTF_CHECK( errNo == *e.errNo() );

            UTF_REQUIRE_EQUAL( EACCES, errNo );
            const auto* str = bl::eh::get_error_info< bl::eh::errinfo_error_code_message >( e );
            UTF_REQUIRE( nullptr != str );
            UTF_REQUIRE_EQUAL( "Permission denied", *str );
        }

        UTF_MESSAGE( "***************** End exception dump *****************\n" );
    };

    cbTestChk( -1 /* errNo */ );
    cbTestChk( EACCES /* errNo */ );
}

UTF_AUTO_TEST_CASE( BaseLib_TestUserMessageException )
{
    const std::string message = "This is message for the user!";

    try
    {
        BL_THROW_USER( message );
        UTF_FAIL( BL_MSG() << "API must throw" );
    }
    catch( bl::UserMessageException& e )
    {
        BL_LOG_MULTILINE(
            bl::Logging::debug(),
            BL_MSG()
                << "User message exception dump:\n"
                << bl::eh::diagnostic_information( e )
            );

        UTF_CHECK_EQUAL( message, std::string( e.what() ) );
        UTF_REQUIRE( bl::eh::isUserFriendly( e ) );

        const bool* isUserFriendly =
            bl::eh::get_error_info< bl::eh::errinfo_is_user_friendly >( e );

        UTF_REQUIRE( isUserFriendly && *isUserFriendly );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_TestCheckArgumentMacro )
{
    const auto arg1 = 1U;

    class Foo
    {
    public:
    };

    Foo f;

    BL_CHK_ARG( true, arg1 );
    BL_CHK_ARG( true, f );

    try
    {
        BL_CHK_ARG( false, arg1 );
        UTF_FAIL( BL_MSG() << "API must throw" );
    }
    catch( bl::ArgumentException& e )
    {
        const auto msg = e.what();
        UTF_MESSAGE( BL_MSG() << "Expected message: " << msg );
        UTF_CHECK_EQUAL( "Invalid argument value '1' provided for argument 'arg1'", msg );
    }

    try
    {
        BL_CHK_ARG( false, f );
        UTF_FAIL( BL_MSG() << "API must throw" );
    }
    catch( bl::ArgumentException& e )
    {
        const auto msg = e.what();
        UTF_MESSAGE( BL_MSG() << "Expected message: " << msg );
        UTF_CHECK_EQUAL( "Invalid argument value provided for argument 'f'", msg );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_TestErrorCodeToStringAndHasher )
{
    using namespace bl;

    const auto ec1 = eh::errc::make_error_code( eh::errc::no_such_file_or_directory );
    const auto ec2 = eh::errc::make_error_code( eh::errc::file_exists );

    UTF_REQUIRE( ec1 != ec2 );

    const auto ec1AsString = eh::errorCodeToString( ec1 );
    const auto ec2AsString = eh::errorCodeToString( ec2 );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Error code 1 as string: "
            << ec1AsString
            << "; ("
            << ec1
            << ")"
        );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Error code 2 as string: "
            << ec2AsString
            << "; ("
            << ec2
            << ")"
        );

    std::hash< eh::error_code > hasher;
    std::hash< std::string > stringHasher;

    UTF_REQUIRE( ec1AsString != ec2AsString );

    UTF_REQUIRE( hasher( ec1 ) != hasher( ec2 ) );
    UTF_REQUIRE_EQUAL( hasher( ec1 ), stringHasher( ec1AsString ) );
    UTF_REQUIRE_EQUAL( hasher( ec2 ), stringHasher( ec2AsString ) );
}

UTF_AUTO_TEST_CASE( BaseLib_TestErrorCodeFromExceptionPtr )
{
    using namespace bl;

    const auto ec = eh::errc::make_error_code( eh::errc::errc_t::file_exists );

    try
    {
        BL_THROW_EC( ec, "Test exception" );
    }
    catch( std::exception& )
    {
        UTF_REQUIRE( ec == eh::errorCodeFromExceptionPtr( std::current_exception() ) );
    }

    try
    {
        BL_THROW(
            UnexpectedException()
                << eh::errinfo_error_code( ec ),
            "Test exception"
            );
    }
    catch( std::exception& )
    {
        UTF_REQUIRE( ec == eh::errorCodeFromExceptionPtr( std::current_exception() ) );
    }

    try
    {
        BL_THROW(
            UnexpectedException(),
            "Test exception"
            );
    }
    catch( std::exception& )
    {
        UTF_REQUIRE( eh::error_code() == eh::errorCodeFromExceptionPtr( std::current_exception() ) );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_EhExceptionHooksTests )
{
    using namespace bl;

    const auto errorCodeExpected = eh::errc::make_error_code( eh::errc::argument_out_of_domain );

    const auto throwAndCatchException = [ & ]() -> void
    {
        try
        {
            BL_THROW(
                ArgumentException()
                    << eh::errinfo_error_code( errorCodeExpected ),
                BL_MSG()
                    << "This is test exception"
                );
        }
        catch( ArgumentException& )
        {
        }
    };

    const auto throwHook = [ & ](
        SAA_in                  const BaseException&            exception,
        SAA_inout               bool&                           invokedFlag
        ) NOEXCEPT -> void
    {
        if( std::string( "bl::ArgumentException" ) == exception.fullTypeName() )
        {
            const auto* errorCode = exception.errorCode();

            if( errorCode && errorCodeExpected == *errorCode )
            {
                invokedFlag = true;
            }
        }
    };

    {
        bool throwHookInvoked1 = false;
        bool throwHookInvoked2 = false;

        {
            BL_EXCEPTION_HOOKS_THROW_GUARD( cpp::bind< void >( throwHook, _1, cpp::ref( throwHookInvoked1 ) ) );
        }

        UTF_REQUIRE_EQUAL( throwHookInvoked1, false );

        {
            BL_EXCEPTION_HOOKS_THROW_GUARD( cpp::bind< void >( throwHook, _1, cpp::ref( throwHookInvoked1 ) ) );

            {
                BL_EXCEPTION_HOOKS_THROW_GUARD( cpp::bind< void >( throwHook, _1, cpp::ref( throwHookInvoked2 ) ) );

                throwAndCatchException();
            }

            UTF_REQUIRE_EQUAL( throwHookInvoked1, false );

            #ifdef BL_ENABLE_EXCEPTION_HOOKS
            UTF_REQUIRE_EQUAL( throwHookInvoked2, true );
            #else // BL_ENABLE_EXCEPTION_HOOKS
            UTF_REQUIRE_EQUAL( throwHookInvoked2, false );
            #endif // BL_ENABLE_EXCEPTION_HOOKS

            throwAndCatchException();
        }

        #ifdef BL_ENABLE_EXCEPTION_HOOKS
        UTF_REQUIRE_EQUAL( throwHookInvoked1, true );
        UTF_REQUIRE_EQUAL( throwHookInvoked2, true );
        #else // BL_ENABLE_EXCEPTION_HOOKS
        UTF_REQUIRE_EQUAL( throwHookInvoked1, false );
        UTF_REQUIRE_EQUAL( throwHookInvoked2, false );
        #endif // BL_ENABLE_EXCEPTION_HOOKS
    }
}

/************************************************************************
 * MessageBuffer tests
 */

UTF_AUTO_TEST_CASE( BaseLib_TestMessageBuffer )
{
    UTF_CHECK_EQUAL( ( BL_MSG() << "This is test: " << 42 << ", " << 1.3 ).text(), "This is test: 42, 1.3" );

    UTF_CHECK_EQUAL(
        ( BL_MSG()
            << "This is test: "
            << ( BL_MSG()
                    << "(Nested buffer: "
                    << ( BL_MSG() << 42 )
                    << ")"
               )
            << ", "
            << 1.3
        ).text(),
        "This is test: (Nested buffer: 42), 1.3"
        );
}


/************************************************************************
 * Uuid tests
 */

UTF_AUTO_TEST_CASE( BaseLib_TestUuid )
{
    UTF_MESSAGE( "***************** uuid tests *****************\n" );

    const auto u1 = bl::uuids::create();
    const auto u2 = bl::uuids::create();
    UTF_CHECK( u1 != u2 );

    const auto s1 = bl::uuids::uuid2string( u1 );
    const auto s2 = bl::uuids::uuid2string( u2 );
    UTF_CHECK( s1 != s2 );

    UTF_MESSAGE( s1 );
    UTF_MESSAGE( s2 );

    const auto nu1 = bl::uuids::string2uuid( s1 );
    const auto nu2 = bl::uuids::string2uuid( s2 );

    UTF_CHECK_EQUAL( u1, nu1 );
    UTF_CHECK_EQUAL( u2, nu2 );

    const auto& nil = bl::uuids::nil();

    UTF_MESSAGE( BL_MSG() << nil );

    {
        UTF_REQUIRE( bl::uuids::isUuid( "4f082035-e301-4cce-94f0-68f1c99f9223" ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( "4f082035-e301-4cce-68f1c99f9223" ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( "" ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( "abcd0123" ) )

        const auto u = bl::uuids::create();
        std::string s = bl::uuids::uuid2string( u );

        UTF_REQUIRE( bl::uuids::isUuid( s ) )
        UTF_REQUIRE( bl::uuids::isUuid( bl::str::to_lower_copy(s) ) )
        UTF_REQUIRE( bl::uuids::isUuid( bl::str::to_upper_copy(s) ) )
        
        UTF_REQUIRE( bl::uuids::containsUuid( s ) )
        UTF_REQUIRE( bl::uuids::containsUuid( bl::str::to_lower_copy(s) ) )
        UTF_REQUIRE( bl::uuids::containsUuid( bl::str::to_upper_copy(s) ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( "prefix_" + s ) )
        UTF_REQUIRE( bl::uuids::containsUuid( "prefix_" + s ) )

        UTF_REQUIRE( ! bl::uuids::isUuid( s.substr( 0, s.size() - 1 ) ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( s + " " ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( s + "-" ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( s + "0" ) )
        UTF_REQUIRE( ! bl::uuids::isUuid( s + "-0" ) )
    }

    UTF_MESSAGE( "*************** end uuid tests ***************\n" );
}

UTF_AUTO_TEST_CASE( BaseLib_TestUuidPerformance )
{
    UTF_MESSAGE( "***************** uuid perf tests *****************\n" );

    const auto t1 = bl::time::microsec_clock::universal_time();

    /*
     * Just create a million uuids to ensure there is no perf issue.
     * It should be fast.
     */

    const std::size_t count = 1000000;

    for( std::size_t i = 0; i < count; ++i )
    {
        bl::uuids::create();
    }

    const auto duration = bl::time::microsec_clock::universal_time() - t1;

    UTF_CHECK( duration < bl::time::seconds( 10 ) );

    UTF_MESSAGE( BL_MSG() << "Creating " << count << " uuids took " << duration );

    UTF_MESSAGE( "*************** end uuid perf tests ***************\n" );
}

UTF_AUTO_TEST_CASE( BaseLib_TestUuidUniqueness )
{
    std::set< std::string > ids;

    const std::size_t count = 100000;

    for( std::size_t i = 0; i < count; ++i )
    {
        /*
         * insert returns pair and the second field indicates if the item
         * was actually inserted (i.e. new) vs. already in the set
         */

        const auto s = bl::str::to_lower_copy( bl::uuids::uuid2string( bl::uuids::create() ) );

        UTF_CHECK( ids.insert( s ).second );
    }

    UTF_MESSAGE( BL_MSG() << "Created " << count << " unique uuids" );
}

BL_IID_DECLARE( iid_test12345, "2a5b48f8-fc88-40f6-b69a-af53e5932603" )
BL_IID_DECLARE( iid_test12346, "1b9fb58d-1ff6-4c84-8d6e-c64ee42670e2" )

namespace
{
    const bl::uuid_t& UuidReturnAsConstRef1() NOEXCEPT
    {
        return iids::iid_test12345();
    }

    const bl::uuid_t& UuidReturnAsConstRef2() NOEXCEPT
    {
        return iids::iid_test12346();
    }
}

UTF_AUTO_TEST_CASE( BaseLib_UuidTestDeclareMacro )
{
    const auto u1 = bl::uuids::string2uuid( "2a5b48f8-fc88-40f6-b69a-af53e5932603" );
    const auto u2 = bl::uuids::string2uuid( "1b9fb58d-1ff6-4c84-8d6e-c64ee42670e2" );

    const auto& cu1 = UuidReturnAsConstRef1();
    const auto& cu2 = UuidReturnAsConstRef2();

    UTF_MESSAGE( BL_MSG() << cu1 );
    UTF_MESSAGE( BL_MSG() << cu2 );

    UTF_CHECK_EQUAL( u1, cu1 );
    UTF_CHECK_EQUAL( u2, cu2 );
}

UTF_AUTO_TEST_CASE( BaseLib_UuidGenerate50 )
{
    for( std::size_t i = 0; i < 50; ++i )
    {
        UTF_MESSAGE( bl::uuids::generateUuidDef( bl::uuids::create() ) );
    }
}

/************************************************************************
 * Logging tests
 */

namespace
{
    void loggingExpectLine(
        SAA_in    const std::string&         line,
        SAA_in    const bool                 hasTimestamp,
        SAA_in    const std::string&         prefix,
        SAA_in    const std::string&         text
        )
    {
        if( hasTimestamp )
        {
            UTF_CHECK( 0 == line.find( prefix + "[" ) );
            UTF_CHECK( std::string::npos != line.find( "] " + text ) );
        }
        else
        {
            UTF_CHECK_EQUAL( line, prefix + text );
        }
    }

    void loggingValidateOutput(
        SAA_in    const std::string&         text,
        SAA_in    const bool                 expectDebug = false,
        SAA_in    const bool                 hasTimestamp = false
        )
    {
        std::string line;
        bl::cpp::SafeInputStringStream is( text );

        std::getline( is, line );
        loggingExpectLine( line, hasTimestamp, "", "Logging level 0" );
        std::getline( is, line );
        loggingExpectLine( line, hasTimestamp, "ERROR: ", "Logging level 1" );
        std::getline( is, line );
        loggingExpectLine( line, hasTimestamp, "WARNING: ", "Logging level 2" );
        std::getline( is, line );
        loggingExpectLine( line, hasTimestamp, "INFO: ", "Logging level 3" );

        if( expectDebug )
        {
            std::getline( is, line );
            UTF_CHECK( 0 == line.find( "DEBUG: [" ) );
            UTF_CHECK( std::string::npos != line.find( "] Logging level 4" ) );
        }

        std::getline( is, line );
        UTF_CHECK_EQUAL( line, "" );

        UTF_CHECK( is.eof() );
    }

    void loggingLogAllLevels( SAA_in const int level = bl::Logging::LL_DEFAULT )
    {
        bl::cpp::SafeUniquePtr< bl::Logging::LevelPusher > pushLevel;

        if( bl::Logging::LL_DEFAULT != level )
        {
            pushLevel.reset( new bl::Logging::LevelPusher( level, false /* global */ ) );
        }

        BL_LOG( bl::Logging::notify(), BL_MSG() << "Logging level " << 0 );
        BL_LOG( bl::Logging::error(), BL_MSG() << "Logging level " << 1 );
        BL_LOG( bl::Logging::warning(), BL_MSG() << "Logging level " << 2 );
        BL_LOG( bl::Logging::info(), BL_MSG() << "Logging level " << 3 );
        BL_LOG( bl::Logging::debug(), BL_MSG() << "Logging level " << 4 );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_LoggingBasicTests )
{
    const auto cbTest = [] ( SAA_in const bool tryDebug ) -> void
    {
        using bl::Logging;

        bl::cpp::SafeOutputStringStream os;
        const Logging::line_logger_t ll(
            bl::cpp::bind( &Logging::defaultLineLoggerWithLock, _1, _2, _3, _4, true /*addNewLine */, bl::cpp::ref( os ) )
            );
        Logging::LineLoggerPusher pushLogger( ll );

        loggingLogAllLevels( Logging::LL_INFO );
        loggingValidateOutput( os.str(), false /* expectDebug */ );

        if( tryDebug )
        {
            Logging::LevelPusher pushLevel( Logging::LL_DEBUG );
            BL_LOG( Logging::debug(), BL_MSG() << "Logging level " << 4 );
            loggingValidateOutput( os.str(), true /* expectDebug */ );
        }
    };

    cbTest( true  /* tryDebug */ );
    cbTest( false /* tryDebug */ );

    {
        using bl::Logging;

        Logging::Level level;

        UTF_CHECK( Logging::tryStringToLogLevel( "NOTIFY", level ) );

        UTF_CHECK_EQUAL( level, Logging::Level::LL_NOTIFY );

        UTF_CHECK( ! Logging::tryStringToLogLevel( "ERROR2", level ) );

        UTF_CHECK_EQUAL(
            Logging::Level::LL_ERROR,
            Logging::stringToLogLevel( "ERROR" )
            );

        UTF_CHECK_EQUAL(
            Logging::Level::LL_DEBUG,
            Logging::stringToLogLevel( "debug" )
            );

        UTF_CHECK_THROW(
            Logging::stringToLogLevel( "TRACE2" ),
            bl::UnexpectedException
            );

        UTF_CHECK_EQUAL(
            Logging::debug().prefix(),
            Logging::levelToChannel( Logging::stringToLogLevel( "debug" ) ).prefix()
            );

        UTF_CHECK_THROW(
            Logging::levelToChannel( Logging::stringToLogLevel( "warning2" ) ),
            bl::UnexpectedException
            );

        UTF_CHECK_THROW(
            Logging::levelToChannel( Logging::stringToLogLevel( "none" ) ),
            bl::UnexpectedException
            );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_LoggingThreadLocalTest )
{
    using bl::Logging;

    bl::cpp::SafeOutputStringStream os;

    const Logging::line_logger_t ll(
        bl::cpp::bind( &Logging::defaultLineLoggerWithLock, _1, _2, _3, _4, true /*addNewLine */, bl::cpp::ref( os ) )
        );
    Logging::LineLoggerPusher pushLogger( ll );

    Logging::LevelPusher pushLevel( Logging::LL_DEBUG );

    bl::os::thread t( bl::cpp::bind( &loggingLogAllLevels, Logging::LL_INFO )  );
    t.join();

    /*
     * Timestamps are always forced by the UTF logger
     */

    loggingValidateOutput( os.str(), false /* expectDebug */, false /* hasTimestamp */ );
}

UTF_AUTO_TEST_CASE( BaseLib_LoggingVerboseModeTests )
{
    using bl::Logging;

    bl::cpp::SafeOutputStringStream os;

    const Logging::line_logger_t ll(
        bl::cpp::bind( &Logging::defaultLineLoggerWithLock, _1, _2, _3, _4, true /*addNewLine */, bl::cpp::ref( os ) )
        );
    Logging::LineLoggerPusher pushLogger( ll );

    loggingLogAllLevels( Logging::LL_DEBUG );

    std::string line;
    bl::cpp::SafeInputStringStream is( os.str() );

    for( std::size_t i = 0; i < 5; ++i )
    {
        std::getline( is, line );
        UTF_MESSAGE( line );

        if( 0 == i )
        {
            UTF_CHECK( 0 == line.find( "[" ) );
        }
        else
        {
            UTF_CHECK( std::string::npos != line.find( ": [" ) );
        }

        UTF_CHECK( std::string::npos != line.find( "] Logging level " ) );
    }

    UTF_CHECK( ! is.eof() );
    std::getline( is, line );
    UTF_CHECK_EQUAL( line, "" );

    UTF_CHECK( is.eof() );
}

UTF_AUTO_TEST_CASE( BaseLib_LoggingMultiLineTests )
{
    using bl::Logging;

    bl::cpp::SafeOutputStringStream os;

    const Logging::line_logger_t ll(
            bl::cpp::bind( &Logging::defaultLineLoggerWithLock, _1, _2, _3, _4, true /*addNewLine */, bl::cpp::ref( os ) )
            );

    Logging::LineLoggerPusher pushLogger( ll );

    Logging::LevelPusher pushLevel( Logging::LL_INFO );

    BL_LOG_MULTILINE( Logging::info(), BL_MSG() << "Line1 test\nLine2\nLine3" );

    std::string line;
    bl::cpp::SafeInputStringStream is( os.str() );

    std::getline( is, line );
    UTF_MESSAGE( line );
    UTF_CHECK_EQUAL( line, "INFO: Line1 test" );

    std::getline( is, line );
    UTF_MESSAGE( line );
    UTF_CHECK_EQUAL( line, "INFO: Line2" );

    std::getline( is, line );
    UTF_MESSAGE( line );
    UTF_CHECK_EQUAL( line, "INFO: Line3" );

    UTF_CHECK( ! is.eof() );
    std::getline( is, line );
    UTF_CHECK_EQUAL( line, "" );

    UTF_CHECK( is.eof() );
}

UTF_AUTO_TEST_CASE( BaseLib_LoggingConcurrencyTests )
{
    UTF_MESSAGE( BL_MSG() << "********************* Logging concurrency tests begin *********************" );

    using bl::Logging;

    bl::cpp::SafeOutputStringStream os;

    const Logging::line_logger_t ll(
            bl::cpp::bind( &Logging::defaultLineLoggerWithLock, _1, _2, _3, _4, true /*addNewLine */, bl::cpp::ref( os ) )
            );

    Logging::LineLoggerPusher pushLogger( ll );

    Logging::LevelPusher pushLevel( Logging::LL_INFO, true /* global */ );

    bl::tasks::scheduleAndExecuteInParallel(
        []( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
        {
            const auto cb = []( SAA_in const std::size_t id ) -> void
            {
                if( 0 == ( id % 2 ) )
                {
                    BL_LOG(
                        Logging::info(),
                        BL_MSG()
                            << "Task executed: "
                            << id
                        );
                }
                else
                {
                    BL_LOG_MULTILINE(
                        Logging::info(),
                        BL_MSG()
                            << "Task executed successfully\n"
                            << "The task id was: "
                            << id
                        );
                }
            };

            for( std::size_t i = 0; i < 100; ++i )
            {
                if( 0 == ( i % 2 ) )
                {
                    eq -> push_front( bl::cpp::bind< void >( cb, i ) );
                }
                else
                {
                    eq -> push_back( bl::cpp::bind< void >( cb, i ) );
                }
            }
        }
        );

    bl::cpp::SafeInputStringStream is( os.str() );

    std::string line;
    while( ! is.eof() )
    {
        std::getline( is, line );

        if( line.empty() )
        {
            /*
             * Only the last line is allowed to be empty
             */

            UTF_REQUIRE( is.eof() );
            break;
        }

        UTF_MESSAGE( BL_MSG() << "Log line is: " << line );
        UTF_REQUIRE( 0 == line.find( "INFO: " ) );
    }

    UTF_MESSAGE( BL_MSG() << "********************* Logging concurrency tests end *********************" );
}

/************************************************************************
 * ThreadPool tests
 */

UTF_AUTO_TEST_CASE( BaseLib_ThreadPoolTests )
{
    const auto tp = bl::om::lockDisposable(
        bl::ThreadPoolImpl::createInstance( bl::os::getAbstractPriorityDefault(), 8 )
        );

    UTF_CHECK_EQUAL( tp -> size(), 8U );

    /*
     * Grow the thread pool from 8 to 20 threads (must be idempotent)
     */

    UTF_CHECK_EQUAL( tp -> resize( 20 ), 20U );
    UTF_CHECK_EQUAL( tp -> size(), 20U );

    UTF_CHECK_EQUAL( tp -> resize( 20 ), 20U );
    UTF_CHECK_EQUAL( tp -> size(), 20U );

    /*
     * Try to shrink the thread pool but it must not change
     */

    UTF_CHECK_EQUAL( tp -> resize( 10 ), 20U );
    UTF_CHECK_EQUAL( tp -> size(), 20U );

    if( test::UtfArgsParser::isClient() )
    {
        /*
         * Schedule some task in the default thread pool which hangs and then try
         * to shut it down to test that it will abort after some timeout
         */

        bl::tasks::scheduleAndExecuteInParallel(
            []( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
            {
                eq -> push_back(
                    [ & ]() -> void
                    {
                        bl::os::sleep( bl::time::hours( 2 ) );
                    }
                    );

                bl::os::sleep( bl::time::seconds( 1 ) );

                const auto tpDefault = bl::ThreadPoolDefault::getDefault();
                tpDefault -> dispose();
            }
            );
    }
}

/************************************************************************
 * os::< shared library support > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSSharedLibTests )
{
    if( bl::os::onWindows() )
    {
        const auto lib = bl::os::loadLibrary( "kernel32.dll" );
        UTF_CHECK( lib );

        const auto pfn = bl::os::getProcAddress( lib, "CloseHandle" );
        UTF_CHECK( pfn );

        try
        {
            bl::os::loadLibrary( "kernel32-unknown.dll" );
            UTF_CHECK( false );
        }
        catch( bl::SystemException& e )
        {
            const std::string details = e.details();
            UTF_MESSAGE( details );

            const auto ec = bl::eh::get_error_info< bl::eh::errinfo_system_code >( e );
            UTF_REQUIRE( nullptr != ec );
            UTF_REQUIRE_EQUAL( 126, *ec );

            const auto* str = e.errorCodeMessage();
            UTF_REQUIRE( nullptr != str );
            UTF_REQUIRE_EQUAL( "The specified module could not be found", *str );
        }
    }

    if( bl::os::onUNIX() )
    {
        const auto lib = bl::os::loadLibrary( bl::os::onDarwin() ? "libc.dylib" : "libc.so.6" );
        UTF_CHECK( lib );

        const auto pfn = bl::os::getProcAddress( lib, "strlen" );
        UTF_CHECK( pfn );

        try
        {
            bl::os::loadLibrary( "libunknown.so" );
            UTF_CHECK( false );
        }
        catch( bl::SystemException& e )
        {
            const std::string details = e.details();
            UTF_MESSAGE( details );

            const auto ec = bl::eh::get_error_info< bl::eh::errinfo_errno >( e );
            UTF_REQUIRE( nullptr != ec );

            /*
             * Note: on Unix, dlopen does not necessarily set errno. In
             * OSImplUNIX.h, when createException is invoked with 0 for errno,
             * ENOTSUP is used instead. Yet the original error is the one
             * reported by dlerror and encoded in the exception message.
             */

            if( bl::os::onDarwin() )
            {
                UTF_REQUIRE( *ec == ENOENT );
            }
            else
            {
                UTF_REQUIRE( *ec == EACCES || *ec == ENOTSUP );
            }

            const auto str = e.message();

            UTF_REQUIRE( nullptr != str );

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "message: "
                    << *str
                );

            if( bl::os::onDarwin() )
            {
                UTF_REQUIRE( str -> find( "image not found" ) != std::string::npos );
            }
            else
            {
                UTF_REQUIRE( str -> find( "No such file or directory" ) != std::string::npos );
            }
        }
    }
}

/************************************************************************
 * os::< create process support > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSCreateProcessTests )
{
    if( bl::os::onWindows() )
    {
        if( test::UtfArgsParser::isAnalysisEnabled() )
        {
            /*
             * TODO: CreateProcess is triggering some weird application verifier
             * breaks in apphelp.dll which do not appear to be issue with the code
             *
             * The breaks also happen only if we turn on the unaligned flag on
             * page heap
             */

            return;
        }

        {
            const auto proc = bl::os::createProcess( "cmd.exe /c exit 3" );
            UTF_CHECK( proc );

            UTF_REQUIRE( bl::os::getPid() );
            UTF_REQUIRE( bl::os::getPid( proc ) );
            UTF_REQUIRE( bl::os::getPid() != bl::os::getPid( proc ) );

            const auto ec = bl::os::tryAwaitTermination( proc );
            UTF_CHECK_EQUAL( ec, 3 );

            std::vector< std::string > vec;
            vec.push_back( "cmd.exe" );
            vec.push_back( "/c" );
            vec.push_back( "exit 3" );

            const auto proc2 = bl::os::createProcess( vec );
            UTF_CHECK( proc2 );
            UTF_REQUIRE( bl::os::getPid( proc2 ) != bl::os::getPid( proc ) );

            const auto ec2 = bl::os::tryAwaitTermination( proc2 );
            UTF_CHECK_EQUAL( ec2, 3 );

        }

        {
            const auto proc = bl::os::createProcess( "cmd.exe /c exit 0", bl::os::ProcessCreateFlags::DetachProcess );
            UTF_CHECK( proc );

            const auto ec = bl::os::tryAwaitTermination( proc );
            UTF_CHECK_EQUAL( ec, 0 );
        }

        UTF_CHECK_THROW( bl::os::createProcess( "doesnotexistproc" ), bl::SystemException );
    }

    if( bl::os::onUNIX() )
    {
        {
            const auto proc = bl::os::createProcess( "bash -c \"exit 3\"" );
            UTF_CHECK( proc );

            const auto ec = bl::os::tryAwaitTermination( proc );
            UTF_CHECK_EQUAL( ec, 3 );
        }

        {
            const auto proc = bl::os::createProcess( "bash -c \"exit 0\"", bl::os::ProcessCreateFlags::DetachProcess );
            UTF_CHECK( proc );

            const auto ec = bl::os::tryAwaitTermination( proc );
            UTF_CHECK_EQUAL( ec, 0 );
        }

        {
            std::vector< std::string > vec;
            vec.push_back( "bash" );
            vec.push_back( "-c" );
            vec.push_back( "exit 3" );

            const auto proc = bl::os::createProcess( vec );
            UTF_CHECK( proc );

            const auto ec = bl::os::tryAwaitTermination( proc );
            UTF_CHECK_EQUAL( ec, 3 );
        }

        UTF_CHECK_THROW( bl::os::createProcess( "doesnotexistproc" ), bl::SystemException );

        UTF_CHECK_THROW( bl::os::createProcess( "A:\\Hello.exe world" ), bl::ArgumentException );
        UTF_CHECK_THROW( bl::os::createProcess( "foo bar\\" ), bl::ArgumentException );

        /*
         * These tests are intended to run when the process is running as root.
         */

        if( test::UtfArgsParser::isClient() )
        {
            if( test::UtfArgsParser::userId().empty() )
            {
                UTF_FAIL( "Test BaseLib_OSCreateProcessTests with the is-client argument must also specify userid." );
            }

            /*
             * Remove the temporary files so that we don't get a false positive.  Because process should be running as root,
             * it should be able to do this, no matter who created the file.
             */

            const auto cleanup = bl::os::createProcess( "rm -f /tmp/out /tmp/out2" );
            UTF_CHECK( cleanup );

            UTF_CHECK_EQUAL( bl::os::tryAwaitTermination( cleanup ) , 0 );

            const auto filename = "/tmp/out";
            const std::string command = std::string( "touch " ) + filename;

            const auto proc3 = bl::os::createProcess( test::UtfArgsParser::userId(), command );
            UTF_CHECK( proc3 );

            const auto ec3 = bl::os::tryAwaitTermination( proc3 );
            UTF_CHECK_EQUAL( ec3, 0 );

            const auto fileOwner = bl::os::getFileOwner( filename );
            UTF_REQUIRE_EQUAL( test::UtfArgsParser::userId(), fileOwner );

            const auto filename2 = "/tmp/out2";
            std::vector< std::string > vec2;
            vec2.push_back( "touch" );
            vec2.push_back( filename2 );

            const auto proc4 = bl::os::createProcess( test::UtfArgsParser::userId(), vec2 );
            UTF_CHECK( proc4 );

            const auto ec4 = bl::os::tryAwaitTermination( proc4 );
            UTF_CHECK_EQUAL( ec4, 0 );

            const auto fileOwner2 = bl::os::getFileOwner( filename2 );
            UTF_REQUIRE_EQUAL( test::UtfArgsParser::userId(), fileOwner2 );
        }
    }
}

UTF_AUTO_TEST_CASE( BaseLib_OSCreateProcessTests2 )
{
    if( bl::os::onUNIX() && test::UtfArgsParser::isClient() )
    {
        const auto proc = bl::os::createProcess( test::UtfArgsParser::userId(), "bash -c 'env|sort; ulimit -a'" );
        UTF_CHECK( proc );

        const auto ec = bl::os::tryAwaitTermination( proc );
        UTF_CHECK_EQUAL( ec, 0 );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_OSCreateProcessRedirectedTests )
{
    if( bl::os::onWindows() && test::UtfArgsParser::isAnalysisEnabled() )
    {
        /*
         * TODO: CreateProcess is triggering some weird application verifier
         * breaks in apphelp.dll which do not appear to be issue with the code
         *
         * The breaks also happen only if we turn on the unaligned flag on
         * page heap
         */

        return;
    }

    bl::tasks::scheduleAndExecuteInParallel(
        []( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
        {
            BL_UNUSED( eq );

            std::vector< std::string > linesOut;
            std::vector< std::string > linesErr;

            const auto cbRedirectedIos = [ & ](
                SAA_in              const bl::os::process_handle_t  process,
                SAA_in_opt          std::istream*                   out,
                SAA_in_opt          std::istream*                   err,
                SAA_in_opt          std::ostream*                   in
                ) -> void
            {
                UTF_REQUIRE( process );
                UTF_REQUIRE( out );
                UTF_REQUIRE( err );
                UTF_REQUIRE( in );

                linesOut.clear();
                linesErr.clear();

                /*
                 * Read all pipes in separate tasks
                 */

                eq -> push_back(
                    [ & ]() -> void
                    {
                        std::string line;

                        while( ! out -> eof() )
                        {
                            line.clear();
                            std::getline( *out, line );

                            const auto i = linesOut.size();

                            linesOut.push_back( std::move( line ) );

                            BL_LOG(
                                bl::Logging::debug(),
                                BL_MSG()
                                    << "STDOUT[ "
                                    << std::setw( 3 )
                                    << i
                                    << " ] is '"
                                    << linesOut[ i ]
                                    << "'"
                                );
                        }
                    }
                );

                eq -> push_back(
                    [ & ]() -> void
                    {
                        std::string line;

                        while( ! err -> eof() )
                        {
                            line.clear();
                            std::getline( *err, line );

                            if( line.empty() && err -> fail() )
                            {
                                /*
                                 * Only add empty lines if they're legitimate
                                 * -- i.e. not the last line after the stream
                                 * was closed, but there was not real new line
                                 * at the end
                                 */

                                continue;
                            }

                            const auto i = linesErr.size();

                            linesErr.push_back( std::move( line ) );

                            BL_LOG(
                                bl::Logging::debug(),
                                BL_MSG()
                                    << "STDERR[ "
                                    << std::setw( 3 )
                                    << i
                                    << " ] is '"
                                    << linesErr[ i ]
                                    << "'"
                                );
                        }
                    }
                );

                bl::os::sleep( bl::time::seconds( 1 ) );

                /*
                 * Answer the 'Do you wish to continue?' question
                 * and verify the answer was echoed
                 */

                ( *in ) << "test1234" << std::endl;

                bl::os::sleep( bl::time::seconds( 1 ) );

                /*
                 * Answer the 'Press any key to continue . . .' question
                 * and verify the text
                 */

                ( *in ) << std::endl;

                /*
                 * Flush will throw if some of the tasks have failed
                 */

                eq -> flush();
            };

            const auto cbRedirectedFile = [ & ](
                SAA_in              const bl::os::process_handle_t  process,
                SAA_in_opt          std::FILE*                      outFile,
                SAA_in_opt          std::FILE*                      errFile,
                SAA_in_opt          std::FILE*                      inFile
                ) -> void
            {
                UTF_REQUIRE( process );
                UTF_REQUIRE( outFile );
                UTF_REQUIRE( errFile );
                UTF_REQUIRE( inFile );

                const auto out = bl::os::fileptr2istream( outFile );
                const auto err = bl::os::fileptr2istream( errFile );
                const auto in = bl::os::fileptr2ostream( inFile );

                cbRedirectedIos( process, out.get(), err.get(), in.get() );
            };

            const auto cbTestDefault = [ & ](
                SAA_in_opt      const bl::os::process_redirect_callback_ios_t&      callbackIos,
                SAA_in_opt      const bl::os::process_redirect_callback_file_t&     callbackFile
                ) -> void
            {
                bl::fs::TmpDir tmpDir;

                const auto filePath =
                    tmpDir.path() / ( bl::os::onWindows() ? "batch_file.cmd" :"batch_file.sh" );

                auto filePathStr = filePath.string();

                {
                    bl::fs::SafeOutputFileStreamWrapper outputFile( filePath );
                    auto& os = outputFile.stream();

                    if( bl::os::onWindows() )
                    {
                        os << "@echo off" << std::endl;
                        os << "setlocal" << std::endl;
                        os << "set /p answer=Do you wish to continue?" << std::endl;
                        os << "echo %answer%" << std::endl;
                        os << "pause" << std::endl;
                        os << "dir c:\\" << std::endl;
                        os << "dir c:\\ 1>&2" << std::endl;
                        os << "set" << std::endl;
                        os << "endlocal" << std::endl;
                    }
                    else if( bl::os::onUNIX() )
                    {
                        os << "read -p \"Do you wish to continue?\" yn" << std::endl;
                        os << "read -s anykey" << std::endl;
                        os << "ls -la /" << std::endl;
                        os << "ls -la / 1>&2" << std::endl;
                        os << "env|sort" << std::endl;
                        os << "exit 0" << std::endl;
                    }
                    else
                    {
                        UTF_FAIL( "Unknown platform" );
                    }
                }

                if( bl::os::onWindows() )
                {
                    const std::string lfnPrefix( "\\\\?\\" );

                    if( 0 == filePathStr.find( lfnPrefix ) )
                    {
                        filePathStr.erase( 0, lfnPrefix.size() );
                    }
                }

                /*
                 * This environment variable must be inherited by the child process
                 */

                const auto envVarName = "TEST_" + bl::uuids::uuid2string( bl::uuids::create() );

                bl::os::setEnvironmentVariable( envVarName, "1234" );

                BL_SCOPE_EXIT(
                    {
                        bl::os::unsetEnvironmentVariable( envVarName );
                    }
                    );

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "****************************"
                    );

                const auto processRef = bl::os::createProcess(
                    ( bl::os::onWindows() ? "cmd.exe /c " : "bash " ) + filePathStr,
                    bl::os::ProcessCreateFlags::RedirectAll | bl::os::ProcessCreateFlags::WaitToFinish,
                    callbackIos,
                    callbackFile
                    );

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "****************************"
                    );

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "STDOUT count is "
                        << linesOut.size()
                    );

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "STDERR count is "
                        << linesErr.size()
                    );

                const int exitCode = bl::os::tryAwaitTermination( processRef );

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "exit code is "
                        << exitCode
                    );

                UTF_REQUIRE( 0 == exitCode );

                UTF_REQUIRE( 2U <= linesOut.size() );
                UTF_REQUIRE( 2U <= linesErr.size() );

                if( bl::os::onWindows() )
                {
                    UTF_REQUIRE_EQUAL( linesOut[ 0 ], "Do you wish to continue?test1234" );
                    UTF_REQUIRE_EQUAL( linesOut[ 1 ], "Press any key to continue . . . " );
                }

                UTF_REQUIRE( bl::cpp::contains( linesOut, envVarName + "=1234" ) );
            };

            cbTestDefault( cbRedirectedIos, bl::os::process_redirect_callback_file_t() );

            cbTestDefault( bl::os::process_redirect_callback_ios_t(), cbRedirectedFile );
        }
        );
}

UTF_AUTO_TEST_CASE( BaseLib_OSCreateProcessRedirectedMergedTests )
{
    if( bl::os::onWindows() && test::UtfArgsParser::isAnalysisEnabled() )
    {
        /*
         * TODO: CreateProcess is triggering some weird application verifier
         * breaks in apphelp.dll which do not appear to be issue with the code
         *
         * The breaks also happen only if we turn on the unaligned flag on
         * page heap
         */

        return;
    }

    std::vector< std::string > lines;

    const auto cbRedirectedIos = [ & ](
        SAA_in              const bl::os::process_handle_t  process,
        SAA_in              std::istream&                   out
        ) -> void
    {
        BL_UNUSED( process );
        BL_ASSERT( process );

        std::string line;

        while( ! out.eof() )
        {
            line.clear();
            std::getline( out, line );

            const auto i = lines.size();

            lines.push_back( std::move( line ) );

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "STDOUT[ "
                    << std::setw( 3 )
                    << i
                    << " ] is '"
                    << lines[ i ]
                    << "'"
                );
        }
    };

    bl::fs::TmpDir tmpDir;

    const auto filePath =
        tmpDir.path() / ( bl::os::onWindows() ? "batch_file.cmd" :"batch_file.sh" );

    auto filePathStr = filePath.string();

    {
        bl::fs::SafeOutputFileStreamWrapper outputFile( filePath );
        auto& os = outputFile.stream();

        if( bl::os::onWindows() )
        {
            os << "@echo off" << std::endl;
            os << "setlocal" << std::endl;
            os << "dir c:\\" << std::endl;
            os << "dir c:\\ 1>&2" << std::endl;
            os << "endlocal" << std::endl;
        }
        else if( bl::os::onUNIX() )
        {
            os << "ls -la /" << std::endl;
            os << "ls -la / 1>&2" << std::endl;
            os << "exit 0" << std::endl;
        }
        else
        {
            UTF_FAIL( "Unknown platform" );
        }
    }

    if( bl::os::onWindows() )
    {
        const std::string lfnPrefix( "\\\\?\\" );

        if( 0 == filePathStr.find( lfnPrefix ) )
        {
            filePathStr.erase( 0, lfnPrefix.size() );
        }
    }

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "****************************"
        );

    const auto processRef = bl::os::createRedirectedProcessMergeOutputAndWait(
        ( bl::os::onWindows() ? "cmd.exe /c " : "bash " ) + filePathStr,
        cbRedirectedIos
        );

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "****************************"
        );

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "STDOUT count is "
            << lines.size()
        );

    const int exitCode = bl::os::tryAwaitTermination( processRef );

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "exit code is "
            << exitCode
        );

    UTF_REQUIRE( 0 == exitCode );

    UTF_REQUIRE( 2U <= lines.size() );
}

/************************************************************************
 * os::< await process termination > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSTryAwaitTerminationTests )
{
    const auto cbTestWait = []( SAA_in const bl::os::process_handle_t proc )
    {
        try
        {
            bl::os::tryAwaitTermination( proc, 1000 /* in milliseconds */ );
            UTF_FAIL( "os::tryAwaitTermination must throw" );
        }
        catch( bl::eh::system_error& e )
        {
            const auto* message = bl::eh::get_error_info< bl::eh::errinfo_message >( e );
            UTF_REQUIRE( message );
            UTF_REQUIRE( 0U == message -> find( "Process did not terminate within specified timeout interval" ) );
        }

        UTF_REQUIRE( false == bl::os::tryTimedAwaitTermination( proc, nullptr, 1000 /* in milliseconds */ ) );

        bl::os::terminateProcess( proc );

        int exitCode = 0;

        UTF_REQUIRE( true == bl::os::tryTimedAwaitTermination( proc, &exitCode ) );
        UTF_REQUIRE( 0 != exitCode );

        exitCode = bl::os::tryAwaitTermination( proc );
        UTF_REQUIRE( 0 != exitCode );
    };

    const auto cbRedirectedIos = [ & ](
        SAA_in              const bl::os::process_handle_t  process,
        SAA_in_opt          std::istream*                   out,
        SAA_in_opt          std::istream*                   err,
        SAA_in_opt          std::ostream*                   in
        ) -> void
    {
        UTF_REQUIRE( process );
        UTF_REQUIRE( out );
        UTF_REQUIRE( err );
        UTF_REQUIRE( in );

        cbTestWait( process );
    };

    if( bl::os::onWindows() )
    {
        if( test::UtfArgsParser::isAnalysisEnabled() )
        {
            /*
             * TODO: CreateProcess is triggering some weird application verifier
             * breaks in apphelp.dll which do not appear to be issue with the code
             *
             * The breaks also happen only if we turn on the unaligned flag on
             * page heap
             */

            return;
        }

        const auto proc = bl::os::createProcess(
            "cmd.exe",
            bl::os::ProcessCreateFlags::RedirectAll | bl::os::ProcessCreateFlags::WaitToFinish,
            cbRedirectedIos
            );

        UTF_CHECK( proc );
    }

    if( bl::os::onUNIX() )
    {
        const auto proc = bl::os::createProcess(
            "bash",
            bl::os::ProcessCreateFlags::RedirectAll | bl::os::ProcessCreateFlags::WaitToFinish,
            cbRedirectedIos
            );

        UTF_CHECK( proc );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_OSTerminateProcessTree )
{
    const auto cbTestWait = []( SAA_in const bl::os::process_handle_t proc )
    {
        try
        {
            bl::os::tryAwaitTermination( proc, 1000 /* in milliseconds */ );
            UTF_FAIL( "os::tryAwaitTermination must throw" );
        }
        catch( bl::eh::system_error& e )
        {
            const auto* message = bl::eh::get_error_info< bl::eh::errinfo_message >( e );
            UTF_REQUIRE( message );
            UTF_REQUIRE( 0U == message -> find( "Process did not terminate within specified timeout interval" ) );
        }

        UTF_REQUIRE( false == bl::os::tryTimedAwaitTermination( proc, nullptr, 1000 /* in milliseconds */ ) );

        bl::os::terminateProcess( proc, true /* force */, true /* includeSubprocesses */ );

        int exitCode = 0;

        UTF_REQUIRE( true == bl::os::tryTimedAwaitTermination( proc, &exitCode ) );
        UTF_REQUIRE( 0 != exitCode );

        exitCode = bl::os::tryAwaitTermination( proc );
        UTF_REQUIRE( 0 != exitCode );
    };

    const auto cbRedirectedIos = [ & ](
        SAA_in              const bl::os::process_handle_t  process,
        SAA_in_opt          std::istream*                  out,
        SAA_in_opt          std::istream*                 err,
        SAA_in_opt          std::ostream*                   in
        ) -> void
    {
        UTF_REQUIRE( process );
        UTF_REQUIRE( out );
        UTF_REQUIRE( err );
        UTF_REQUIRE( in );

        cbTestWait( process );
    };

    if( bl::os::onWindows() )
    {
        if( test::UtfArgsParser::isAnalysisEnabled() )
        {
            /*
             * TODO: CreateProcess is triggering some weird application verifier
             * breaks in apphelp.dll which do not appear to be issue with the code
             *
             * The breaks also happen only if we turn on the unaligned flag on
             * page heap
             */

            return;
        }

        const auto proc = bl::os::createProcess(
            "cmd.exe /c cmd.exe /c cmd.exe",
            bl::os::ProcessCreateFlags::RedirectAll | bl::os::ProcessCreateFlags::WaitToFinish,
            cbRedirectedIos
            );

        UTF_CHECK( proc );
    }

    if( bl::os::onUNIX() )
    {
        const auto proc = bl::os::createProcess(
            "bash -c \"cat\"",
            bl::os::ProcessCreateFlags::RedirectAll | bl::os::ProcessCreateFlags::WaitToFinish,
            cbRedirectedIos
            );

        UTF_CHECK( proc );
    }
}

/************************************************************************
 * os::< get current executable path > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetCurrentExecutablePathTests )
{
    const auto exe = bl::os::getCurrentExecutablePath();

    /*
     * Verify the executable name is correct
     */

    const auto filename = bl::fs::path( exe ).filename().string();

    if( bl::os::onWindows() )
    {
        UTF_CHECK( "utf-baselib.exe" == filename || "utf-baselib-shared.exe" == filename );
    }

    if( bl::os::onUNIX() )
    {
        UTF_CHECK_EQUAL( filename, "utf-baselib" );
    }
}

/************************************************************************
 * os::< get/set/unset env var > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetSetEnvVarTests )
{
    UTF_CHECK( bl::os::tryGetEnvironmentVariable( "PATH" ) );
    UTF_CHECK( ! bl::os::tryGetEnvironmentVariable( "THISENVVARDOESNOTEXIST__" ) );
    UTF_CHECK( ! bl::os::tryGetEnvironmentVariable( "" ) );

    UTF_CHECK_NO_THROW( bl::os::getEnvironmentVariable( "PATH" ) );
    UTF_CHECK_THROW( bl::os::getEnvironmentVariable( "THISENVVARDOESNOTEXIST__" ), bl::UnexpectedException );
    UTF_CHECK_THROW( bl::os::getEnvironmentVariable( "" ), bl::UnexpectedException );

    const auto varName = "TEST_" + bl::uuids::uuid2string( bl::uuids::create() );

    UTF_REQUIRE( ! bl::os::tryGetEnvironmentVariable( varName ) );

    bl::os::setEnvironmentVariable( varName, "1234" );
    UTF_CHECK_EQUAL( bl::os::getEnvironmentVariable( varName ), "1234" );

    bl::os::setEnvironmentVariable( varName, "56789" );
    UTF_CHECK_EQUAL( bl::os::getEnvironmentVariable( varName ), "56789" );

    bl::os::unsetEnvironmentVariable( varName );
    UTF_CHECK( ! bl::os::tryGetEnvironmentVariable( varName ) );

    bl::os::unsetEnvironmentVariable( varName );
    UTF_CHECK( ! bl::os::tryGetEnvironmentVariable( varName ) );

    bl::os::unsetEnvironmentVariable( "THISENVVARDOESNOTEXIST__" );
    UTF_CHECK( ! bl::os::tryGetEnvironmentVariable( "THISENVVARDOESNOTEXIST__" ) );

    UTF_CHECK_THROW( bl::os::setEnvironmentVariable( "", "" ), bl::SystemException );
    UTF_CHECK_THROW( bl::os::setEnvironmentVariable( "", "foo" ), bl::SystemException );
    UTF_CHECK_THROW( bl::os::setEnvironmentVariable( "FOO", "" ), bl::SystemException );
    UTF_CHECK_THROW( bl::os::setEnvironmentVariable( "FOO=BAR", "baz" ), bl::SystemException );

    UTF_CHECK_THROW( bl::os::unsetEnvironmentVariable( "" ), bl::SystemException );
    UTF_CHECK_THROW( bl::os::unsetEnvironmentVariable( "FOO=BAR" ), bl::SystemException );
}

/************************************************************************
 * os::< get local application data directory > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetLocalAppDataDirTests )
{
    /*
     * Simple test to ensure no exception is raised
     */

    UTF_CHECK_NO_THROW( bl::os::getLocalAppDataDir() );
}

/************************************************************************
 * os::getPid() tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetPidTests )
{
    /*
     * Simple test to ensure no exception is raised
     */

    UTF_REQUIRE( bl::os::getPid() );
}

/************************************************************************
 * os::< long path names support on Windows > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSLongFileNamesWindowsTests )
{
    if( ! bl::os::onWindows() )
    {
        /*
         * These tests apply to Windows only
         */

        return;
    }

    UTF_MESSAGE( BL_MSG() << "\n****************************** Paths testing ****************************\n\n" );

    bl::fs::TmpDir tmpDir;
    const auto& tmpPath = tmpDir.path();

    {
        bl::fs::path path( "c:\\" );

        UTF_MESSAGE(
            BL_MSG()
                << path
                << "; "
                << "is_absolute: "
                << path.is_absolute()
                << "; "
                << "is_complete: "
                << path.is_complete()
                << "; "
                << "has_root_path: "
                << path.has_root_path()
                << "; "
                << "has_root_name: "
                << path.has_root_name()
                << "; "
                << "has_root_directory: "
                << path.has_root_directory()
                << "; "
                << "root_path: "
                << path.root_path()
                << "; "
                << "root_name: "
                << path.root_name()
                << "; "
                << "root_directory: "
                << path.root_directory()
            );
    }

    {
        bl::fs::path path;

        UTF_MESSAGE(
            BL_MSG()
                << path
                << "; "
                << "is_absolute: "
                << path.is_absolute()
                << "; "
                << "is_complete: "
                << path.is_complete()
                << "; "
                << "has_root_path: "
                << path.has_root_path()
                << "; "
                << "has_root_name: "
                << path.has_root_name()
                << "; "
                << "has_root_directory: "
                << path.has_root_directory()
                << "; "
                << "root_path: "
                << path.root_path()
                << "; "
                << "root_name: "
                << path.root_name()
                << "; "
                << "root_directory: "
                << path.root_directory()
            );
    }

    {
        bl::fs::path path( tmpPath );

        UTF_MESSAGE(
            BL_MSG()
                << path
                << "; "
                << "is_absolute: "
                << path.is_absolute()
                << "; "
                << "is_complete: "
                << path.is_complete()
                << "; "
                << "has_root_path: "
                << path.has_root_path()
                << "; "
                << "has_root_name: "
                << path.has_root_name()
                << "; "
                << "has_root_directory: "
                << path.has_root_directory()
                << "; "
                << "root_path: "
                << path.root_path()
                << "; "
                << "root_name: "
                << path.root_name()
                << "; "
                << "root_directory: "
                << path.root_directory()
            );
    }

    {
        bl::fs::path path( tmpPath );

        const auto dirName = bl::uuids::uuid2string( bl::uuids::create() );

        /*
         * Create a long path to be deleted at the end
         */

        for( std::size_t i = 0; i < 100; ++i )
        {
            path /= dirName;
            bl::fs::safeCreateDirectory( path );
        }
    }

    UTF_MESSAGE( BL_MSG() << "\n\n**************************** Paths testing end ****************************\n" );
}

UTF_AUTO_TEST_CASE( BaseLib_OSLongFileNamesAppVerifCrashTests )
{
    if( ! bl::os::onWindows() )
    {
        /*
         * These tests apply to Windows only
         */

        return;
    }

    UTF_MESSAGE( BL_MSG() << "\n****************************** AppVerif Crash test ****************************\n\n" );

    const auto* lfnTest1 =
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName/LongN"
    "ame/LongName/LongName/LongName/L"
    "ongName/LongName/LongName/LongNa"
    "me/LongName/LongName/LongName/Lo"
    "ngName/LongName/LongName/LongNam"
    "e/LongName/LongName/LongName/Lon"
    "gName/LongName/LongName/LongName"
    "/LongName/LongName/LongName/Long"
    "Name/LongName/LongName/LongName/"
    "LongName/LongName/LongName";

    const std::string lfnTest2( lfnTest1 );

    const bl::fs::path path1( lfnTest1 );
    UTF_REQUIRE( ! path1.empty() );

    const bl::fs::path path2( lfnTest2 );
    UTF_REQUIRE_EQUAL( path1, path2 );

    bl::fs::path path3;
    path3 = lfnTest1;
    UTF_REQUIRE_EQUAL( path1, path3 );

    bl::fs::path path4;
    path4 = lfnTest2;
    UTF_REQUIRE_EQUAL( path1, path4 );
}

UTF_AUTO_TEST_CASE( BaseLib_OSDeletePathTests )
{
    const auto& path = test::UtfArgsParser::path();

    if( path.empty() )
    {
        /*
         * Path parameter is required
         */

        return;
    }

    UTF_MESSAGE(
        BL_MSG()
            << "Deleting path "
            << path
            );

    bl::fs::safeRemoveAll( path );
}

/************************************************************************
 * os::< stdio std::FILE* operations and large file support > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSLargeFileSupportTests )
{
    bl::fs::TmpDir tmpDir;

    const auto& tmpPath = tmpDir.path();

    const auto largeFilePath = tmpPath / "large_file.bin";

    const unsigned char pattern[] = { 1, 2, 3, 4, 5 };
    unsigned char buffer[ BL_ARRAY_SIZE( pattern ) ];

    ::memset( buffer, 0, BL_ARRAY_SIZE( buffer ) );

    std::uint64_t pos = 0;

    /*
     * Seek at 5 GB and write something then read it later
     */

    pos += 1024U * 1024U;
    pos *= 1024U;
    pos *= 5U;

    {
        const auto fileptr = bl::os::fopen( largeFilePath, "wb" );
        bl::os::fseek( fileptr, pos, SEEK_SET );
        bl::os::fwrite( fileptr, pattern, BL_ARRAY_SIZE( pattern ) );

        const auto newPos = bl::os::ftell( fileptr );
        UTF_REQUIRE( ( pos + BL_ARRAY_SIZE( pattern ) ) == newPos );
    }

    {
        const auto fileptr = bl::os::fopen( largeFilePath, "rb" );
        bl::os::fseek( fileptr, pos, SEEK_SET );
        bl::os::fread( fileptr, buffer, BL_ARRAY_SIZE( buffer ) );

        static_assert(
            BL_ARRAY_SIZE( buffer ) == BL_ARRAY_SIZE( pattern ),
            "Buffer and pattern must have the same size"
            );
        UTF_REQUIRE( 0 == ::memcmp( buffer, pattern, BL_ARRAY_SIZE( buffer ) ) );

        const auto newPos = bl::os::ftell( fileptr );
        UTF_REQUIRE( ( pos + BL_ARRAY_SIZE( buffer ) ) == newPos );
    }
}

/************************************************************************
 * os::< getFileCreateTime() / setFileCreateTime() > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetSetFileCreateTimeTests )
{
    bl::fs::path tmpPath;

    const auto now = std::time( nullptr );
    BL_CHK_ERRNO_NM( ( std::time_t )( -1 ), now );

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        UTF_REQUIRE( bl::fs::exists( tmpPath ) );

        if( bl::os::onUNIX() )
        {
            /*
             * Create time is not supported on UNIX
             */

            UTF_REQUIRE_THROW( bl::fs::safeGetFileCreateTime( tmpPath ), bl::NotSupportedException );
            UTF_REQUIRE_THROW( bl::fs::safeSetFileCreateTime( tmpPath, now ), bl::NotSupportedException );
        }
        else
        {
            const auto time = bl::fs::safeGetFileCreateTime( tmpPath );
            UTF_REQUIRE( time );
            UTF_REQUIRE_EQUAL( time, bl::fs::safeGetFileCreateTime( tmpPath ) );

            const auto newTime = time + 2000;
            bl::fs::safeSetFileCreateTime( tmpPath, newTime );
            UTF_REQUIRE_EQUAL( newTime, bl::fs::safeGetFileCreateTime( tmpPath ) );
        }
    }

    UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
}

/************************************************************************
 * os::getUserName() tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetUserNameTests )
{
    /*
     * Simple test to ensure no exception is raised
     */

    const auto userName = bl::os::getUserName();

    UTF_REQUIRE( ! userName.empty() );

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "User name = "
            << userName
        );
}

UTF_AUTO_TEST_CASE( BaseLib_OSLoggedInUserNamesTests )
{
    if( bl::os::onWindows() )
    {
        const auto names = bl::os::getLoggedInUserNames();

        UTF_REQUIRE( ! names.empty() );

        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "Logged-in user names: "
                << bl::str::joinQuoteFormatted( names )
            );

        const auto currentUser = bl::os::getUserName();

        const auto pos = std::find_if(
            names.begin(),
            names.end(),
            [ &currentUser ]( SAA_in const std::string& value ) -> bool
            {
                return bl::str::iequals( currentUser, value );
            }
            );

        UTF_REQUIRE( pos != names.end() );

        /*
         * Ensure Window Manager account isn't returned
         */

        for( const auto& name : names )
        {
            UTF_REQUIRE( ! bl::str::istarts_with( name, "DWM-" ) );
        }
    }
    else
    {
        UTF_REQUIRE_THROW( bl::os::getLoggedInUserNames(), NotSupportedException );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_OSClientCheckTests )
{
    if( bl::os::onWindows() )
    {
        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "isClientOS: "
                << bl::os::isClientOS()
            );
    }
    else
    {
        UTF_REQUIRE( ! bl::os::isClientOS() );
    }
}

/************************************************************************
 * os::getConsoleSize() tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSGetConsoleSizeTests )
{
    /*
     * Simple test to ensure no exception is raised
     */

    int columns, rows;

    const auto success = bl::os::getConsoleSize( columns, rows );

    if( success )
    {
        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "Console size = "
                << columns
                << " x "
                << rows
        );

        UTF_REQUIRE( columns > 0 );
        UTF_REQUIRE( rows > 0 );
    }
    else
    {
        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "Unable to determine the console size"
        );
    }
}

/************************************************************************
 * os::readFromInput() tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSReadFromInputTests )
{
    if( test::UtfArgsParser::isClient() )
    {
        UTF_MESSAGE( "***************** readFromInput interactive tests *****************\n" );

        std::cout << "Enter name (must be visible): ";
        const auto name = bl::os::readFromInput();

        UTF_MESSAGE( BL_MSG() << "Entered name '" << name << "'" );
        UTF_CHECK( ! name.empty() );

        std::cout << "Enter password (must be hidden): ";
        const auto pass = bl::os::readFromInput( true );
        std::cout << std::endl;

        UTF_MESSAGE( BL_MSG() << "Entered password '" << pass << "'" );
        UTF_CHECK( ! pass.empty() );

        UTF_MESSAGE( "\n***************** end readFromInput interactive tests *****************\n" );
    }
}

/************************************************************************
 * os::< junctions support > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSJunctionsTests )
{
    if( ! bl::os::onWindows() )
    {
        /*
         * Junctions are only supported on Windows
         *
         * On Linux we use symlinks
         */

        return;
    }

    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        const auto subDir = tmpPath / "sub-dir";
        const auto junctionDir = tmpPath / "junction-dir";
        const auto filePath = subDir / "file.txt";

        bl::fs::safeMkdirs( subDir );
        bl::fs::safeMkdirs( junctionDir );

        UTF_REQUIRE( ! bl::os::isJunction( subDir ) );
        UTF_REQUIRE( ! bl::os::isJunction( junctionDir ) );

        bl::os::createJunction( subDir, junctionDir );

        UTF_REQUIRE( bl::os::isJunction( junctionDir ) );
        UTF_REQUIRE_EQUAL( subDir, bl::os::getJunctionTarget( junctionDir ) );

        {
            const auto file = bl::os::fopen( filePath, "wb" );
            UTF_REQUIRE( bl::fs::exists( filePath ) );
        }

        const auto filePathViaJunction = junctionDir / "file.txt";
        UTF_REQUIRE( bl::fs::exists( filePathViaJunction ) );

        bl::os::deleteJunction( junctionDir );
        UTF_REQUIRE( ! bl::os::isJunction( junctionDir ) );

        UTF_REQUIRE( ! bl::fs::exists( filePathViaJunction ) );
        UTF_REQUIRE( bl::fs::exists( filePath ) );

        UTF_REQUIRE( bl::fs::exists( junctionDir ) );
    }

    UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
}

/************************************************************************
 * os::< hardlink support > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_OSHardlinkTests )
{
    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        /*
         * On Windows, directories always have just 1 link
         * On *nix file systems, each directory has at least 2 links (itself and its parent), plus one
         * for each sub-directory
         */

        UTF_REQUIRE_EQUAL( bl::os::onWindows() ? 1U : 2U, bl::os::getHardlinkCount( tmpPath ) );

        const auto filePath = tmpPath / "file.txt";
        const auto hardlinkPath = tmpPath / "hardlink.txt";

        {
            const auto file = bl::os::fopen( filePath, "wb" );
            bl::os::fwrite( file, "text", 4 );
            UTF_REQUIRE( bl::fs::exists( filePath ) );
            UTF_REQUIRE_EQUAL( 1U, bl::os::getHardlinkCount( filePath ) );
        }

        UTF_REQUIRE( ! bl::fs::exists( hardlinkPath ) );

        bl::os::createHardlink( filePath, hardlinkPath );

        UTF_REQUIRE( bl::fs::exists( filePath ) );
        UTF_REQUIRE( bl::fs::exists( hardlinkPath ) );
        UTF_REQUIRE_EQUAL( 2U, bl::os::getHardlinkCount( filePath ) );
        UTF_REQUIRE_EQUAL( 2U, bl::os::getHardlinkCount( hardlinkPath ) );

        /*
         * Cannot hard-link a file to itself
         */

        UTF_REQUIRE_THROW( bl::os::createHardlink( filePath, filePath ), bl::SystemException );

        bl::fs::safeRemove( filePath );

        UTF_REQUIRE( ! bl::fs::exists( filePath ) );
        UTF_REQUIRE( bl::fs::exists( hardlinkPath ) );
        UTF_REQUIRE_THROW( bl::os::getHardlinkCount( filePath ), bl::SystemException );
        UTF_REQUIRE_EQUAL( 1U, bl::os::getHardlinkCount( hardlinkPath ) );

        /*
         * Cannot hard-link a non-existing file
         */

        UTF_REQUIRE_THROW( bl::os::createHardlink( filePath, hardlinkPath ), bl::SystemException );
    }

    UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
}

/************************************************************************
 * os::< ipc > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_NamedMutexTests )
{
    const auto lockName = "BL-Test-Mutex-b04c79af-463f-418d-bac7-536a4056e8cf";

    bl::os::RobustNamedMutex lock( lockName );

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "Trying to acquire a named lock '"
            << lockName
            << "'"
        );

    {
        bl::os::ipc::scoped_lock< decltype( lock ) > guard( lock );

        const long timeoutInSeconds = test::UtfArgsParser::isClient() ? 30 : 1;

        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "A named lock '"
                << lockName
                << "' has been acquired; waiting for "
                << timeoutInSeconds
                << " seconds ..."
            );

        bl::os::sleep( bl::time::seconds( timeoutInSeconds ) );
    }

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "A named lock '"
            << lockName
            << "' has been released"
        );
}

/************************************************************************
 * Convert tabs to spaces utility code
 */

namespace
{
    void fileConvertTabs2Spaces( SAA_in const fs::path& path )
    {
        UTF_MESSAGE( BL_MSG() << "Converting file: " << path );

        /*
         * Just read all lines and, replace tabs with 4 spaces and
         * write the lines back into the same file
         */

        std::vector< std::string > lines;

        {
            bl::fs::SafeInputFileStreamWrapper file( path );
            auto& is = file.stream();

            std::string line;
            while( ! is.eof() )
            {
                std::getline( is, line );

                if( ! is.eof() || ! line.empty() )
                {
                    lines.push_back( line );
                }
            }
        }

        {
            bl::fs::SafeOutputFileStreamWrapper outputFile( path );
            auto& os = outputFile.stream();

            for( auto& line : lines )
            {
                bl::str::replace_all( line, "\t", "    " );
                os << line << "\n";
            }
        }
    }

    void convertTabs2Spaces(
        SAA_in          const bl::fs::path&                             rootDir,
        SAA_in_opt      const std::set< std::string >&                  extensions = std::set< std::string >()
        )
    {
        for( bl::fs::recursive_directory_iterator end, it( rootDir ) ; it != end; ++it  )
        {
            if( ! bl::fs::is_regular_file( it -> status() ) )
            {
                /*
                 * Skip non-file entries
                 */

                continue;
            }

            const auto pathw = it -> path().native();
            std::string path( pathw.begin(), pathw.end() );

            const auto pos = path.rfind( '.' );
            if( std::string::npos == pos )
            {
                /*
                 * Skip files without extensions
                 */

                continue;
            }

            const auto ext = bl::str::to_lower_copy( path.substr( pos ) );
            if( ! extensions.empty() && extensions.find( ext ) == extensions.end() )
            {
                /*
                 * Skip all extensions we don't care about
                 */

                continue;
            }

            fileConvertTabs2Spaces( path );
        }
    }
}

UTF_AUTO_TEST_CASE( BaseLib_ConvertTabs2SpacesTests )
{
    if( test::UtfArgsParser::path().empty() )
    {
        return;
    }

    const fs::path inputPath = test::UtfArgsParser::path();

    if( fs::path_exists( inputPath ) && fs::is_directory( inputPath ) )
    {
        convertTabs2Spaces( inputPath );
    }
    else
    {
        fileConvertTabs2Spaces( inputPath );
    }
}

/************************************************************************
 * Update file headers utility code
 */

namespace
{
    void updateOneFileHader(
        SAA_in          const std::string&                              licenseText,
        SAA_in          const bl::fs::path&                             path
        )
    {
        UTF_MESSAGE(
            BL_MSG()
                << "Updating header of file: "
                << path
            );

        /*
         * Just read all lines and, replace tabs with 4 spaces and
         * write the lines back into the same file
         */

        std::vector< std::string > lines;

        {
            bl::fs::SafeInputFileStreamWrapper file( path );
            auto& is = file.stream();

            std::string line;
            while( ! is.eof() )
            {
                std::getline( is, line );

                if( ! is.eof() || ! line.empty() )
                {
                    lines.push_back( line );
                }
            }
        }

        {
            bl::fs::SafeOutputFileStreamWrapper outputFile( path );
            auto& os = outputFile.stream();

            bool inFileHader = false;
            bool inFileBody = false;
            bool licenseWritten = false;
            bool headerParsed = false;

            for( const auto& line : lines )
            {
                if( ! inFileHader && ! inFileBody )
                {
                    const auto trimmed = bl::str::trim_copy( line );

                    if( trimmed.empty() )
                    {
                        /*
                         * Skip over all empty lines in the beginning of the file
                         */

                        continue;
                    }

                    if( ! headerParsed && bl::str::starts_with( trimmed, "/*" ) )
                    {
                        inFileHader = true;
                    }
                    else
                    {
                        inFileBody = true;
                    }
                }

                if( inFileBody )
                {
                    if( ! licenseWritten )
                    {
                        if( ! licenseText.empty() )
                        {
                            os << licenseText;
                        }

                        licenseWritten = true;
                    }

                    /*
                     * If we are in the file body we simply copy the line and continue
                     */

                    os << line << "\n";

                    continue;
                }

                /*
                 * If we are here that means we are in he current header, but not in
                 * the file body yet - check if the current header is about to close
                 */

                if( ! headerParsed && inFileHader && bl::cpp::contains( line, "*/" ) )
                {
                    inFileHader = false;
                    headerParsed = true;
                }
            }
        }
    }

    void updateFileHaders(
        SAA_in          const std::string&                              licenseText,
        SAA_in          const bl::fs::path&                             rootDir,
        SAA_in_opt      const std::set< std::string >&                  extensions = std::set< std::string >()
        )
    {
        for( bl::fs::recursive_directory_iterator end, it( rootDir ) ; it != end; ++it  )
        {
            if( ! bl::fs::is_regular_file( it -> status() ) )
            {
                /*
                 * Skip non-file entries
                 */

                continue;
            }

            const auto pathw = it -> path().native();
            std::string path( pathw.begin(), pathw.end() );

            const auto pos = path.rfind( '.' );
            if( std::string::npos == pos )
            {
                /*
                 * Skip files without extensions
                 */

                continue;
            }

            const auto ext = bl::str::to_lower_copy( path.substr( pos ) );
            if( ! extensions.empty() && extensions.find( ext ) == extensions.end() )
            {
                /*
                 * Skip all extensions we don't care about
                 */

                continue;
            }

            updateOneFileHader( licenseText, path );
        }
    }
}

UTF_AUTO_TEST_CASE( BaseLib_UpdateFileHeaders )
{
    if( test::UtfArgsParser::path().empty() )
    {
        return;
    }

    const auto licenseText = test::UtfArgsParser::licenseKey().empty() ?
        std::string() : bl::encoding::readTextFile( test::UtfArgsParser::licenseKey() );

    const fs::path inputPath = test::UtfArgsParser::path();

    if( fs::path_exists( inputPath ) && fs::is_directory( inputPath ) )
    {
        std::set< std::string > extensions;

        extensions.insert( ".h" );
        extensions.insert( ".cpp" );

        updateFileHaders( licenseText, inputPath, extensions );
    }
    else
    {
        updateOneFileHader( licenseText, inputPath );
    }
}

/************************************************************************
 * DataBlock interface tests
 */

UTF_AUTO_TEST_CASE( BaseLib_DataBlockTests )
{
    {
        const auto block = bl::data::DataBlock::createInstance();
        UTF_REQUIRE( block );

        const std::size_t expectedDefaultSize = bl::data::DataBlock::defaultCapacity();

        UTF_CHECK( block -> size() );
        UTF_CHECK( expectedDefaultSize == block -> size() );
        UTF_CHECK( expectedDefaultSize == block -> size64() );
        UTF_CHECK( expectedDefaultSize == block -> capacity() );
        UTF_CHECK( expectedDefaultSize == block -> capacity64() );

        {
            auto& b = *block;

            UTF_CHECK( b.begin() );
            UTF_CHECK( b.end() );

            UTF_CHECK( b.begin() < b.end() );
            UTF_CHECK( expectedDefaultSize == ( std::size_t )( b.end() - b.begin() ) );
        }

        {
            const auto& b = *block;

            UTF_CHECK( b.begin() );
            UTF_CHECK( b.end() );

            UTF_CHECK( b.begin() < b.end() );
            UTF_CHECK( expectedDefaultSize == ( std::size_t )( b.end() - b.begin() ) );
        }

        block -> setSize( 1234 );
        UTF_CHECK( 1234U == block -> size() );
        UTF_CHECK( 1234U == block -> size64() );
        UTF_CHECK( expectedDefaultSize == block -> capacity() );
        UTF_CHECK( expectedDefaultSize == block -> capacity64() );

        {
            const auto& b = *block;

            UTF_CHECK( b.begin() );
            UTF_CHECK( b.end() );

            UTF_CHECK( b.begin() < b.end() );
            UTF_CHECK( 1234 == ( b.end() - b.begin() ) );
        }
    }

    {
        const auto block = bl::data::DataBlock::createInstance( 1234 );
        UTF_REQUIRE( block );

        UTF_CHECK( 1234U == block -> size() );
        UTF_CHECK( 1234U == block -> size64() );
        UTF_CHECK( 1234U == block -> capacity() );
        UTF_CHECK( 1234U == block -> capacity64() );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_NetworkByteOrderFunctionsTests )
{
    /*
     * Tests network byte order functions
     */

    {
        const decltype( bl::os::host2NetworkLong( 0 ) ) value = 12345678;

        const auto h2n = bl::os::host2NetworkLong( value );
        UTF_CHECK( h2n );

        const auto n2h = bl::os::network2HostLong( h2n );
        UTF_CHECK( n2h );
        UTF_CHECK_EQUAL( n2h, value );
    }

    {
        const decltype( bl::os::host2NetworkShort( 0 ) ) value = 12345;

        const auto h2n = bl::os::host2NetworkShort( value );
        UTF_CHECK( h2n );

        const auto n2h = bl::os::network2HostShort( h2n );
        UTF_CHECK( n2h );
        UTF_CHECK_EQUAL( n2h, value );
    }

    {
        /*
         * Initialize input value that can't fit in 32bit integer
         * so that lo and hi parts are actually used.
         */
        std::uint64_t big = ( static_cast< std::uint64_t >( 123 ) << 32 ) + 456;

        std::uint32_t lo;
        std::uint32_t hi;

        bl::os::host2NetworkLongLong( big, lo, hi );

        std::uint64_t big2 = bl::os::network2HostLongLong( lo, hi );

        UTF_CHECK_EQUAL( big, big2 );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_NetworkHelperFunctionsTests )
{
    if( bl::os::onWindows() && test::UtfArgsParser::isAnalysisEnabled() )
    {
        /*
         * TODO: gethostname() for some reason is causing app verifier
         * leak on WSACleanup which isn't an issue with the application
         * and also non-critical
         *
         * We will disable for now and investigate later on
         */

        return;
    }

    const auto hostName = bl::net::getShortHostName();

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "Host name: "
            << hostName
        );

    UTF_CHECK( ! bl::cpp::contains( hostName, '.' ) );

    UTF_CHECK_EQUAL( bl::net::getShortHostName( "host1.com" ), "host1" );
    UTF_CHECK_EQUAL( bl::net::getShortHostName( "host2" ), "host2" );

    UTF_CHECK_THROW( bl::net::getShortHostName( "" ), bl::ArgumentException );

    /*
     * Fully-qualified domain name (FQDN)
     *
     * Hosts without a domain (e.g. virtual machines) return just their host name
     */

    const auto fullHostName = bl::net::getCanonicalHostName();

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "Fully qualified host name: "
            << fullHostName
        );

    if( bl::os::onDarwin() )
    {
        /*
         * On Darwin the host name can be different capitalization than what is in the
         * fullHostName, so we need to normalize it
         */

        UTF_CHECK(
            bl::str::starts_with(
                bl::str::to_lower_copy( fullHostName ),
                bl::str::to_lower_copy( hostName )
                )
            );
    }
    else
    {
        UTF_CHECK( bl::str::starts_with( fullHostName, hostName ) );
    }

    /*
     * Localhost alias and IP addresses return different results depending on the OS
     */

    UTF_CHECK( ! bl::net::getCanonicalHostName( "localhost" ).empty() );
    UTF_CHECK( ! bl::net::getCanonicalHostName( "127.0.0.1" ).empty() );

    UTF_CHECK_THROW( bl::net::getCanonicalHostName( "" ), bl::ArgumentException );
    UTF_CHECK_THROW( bl::net::getCanonicalHostName( "invalidhostname" ), bl::SystemException );
    UTF_CHECK_THROW( bl::net::getCanonicalHostName( "host.domain.invalid" ), bl::SystemException );

    /*
     * Measure the performance of getCanonicalHostName()
     *
     * The current host name should be cached by the OS so the API call should be quite cheap
     * (under 1 ms per call)
     */

    if( test::UtfArgsParser::isClient() )
    {
        utest::TestUtils::measureRuntime(
            "Querying CanonicalHostName 10000 times",
            [ & ]() -> void
            {
                for( auto i = 0; i < 10000; ++i )
                {
                    ( void ) bl::net::getCanonicalHostName();
                }
            }
            );
    }
}

/************************************************************************
 * SafeUniquePtr< T > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_SafeUniquePtrTests )
{
    static long g_refs = 0L;

    class Foo :
        public bl::cpp::noncopyable,
        public bl::om::Object
    {
        BL_QITBL_DECLARE( bl::om::Object )

    protected:

        Foo()
        {
            ++g_refs;
        }

        ~Foo() NOEXCEPT
        {
            --g_refs;
        }
    };

    typedef bl::om::ObjectImpl< Foo > FooImpl;
    typedef bl::cpp::SafeUniquePtr< FooImpl, bl::om::detail::Deleter > FooImplPtr;

    UTF_REQUIRE_EQUAL( 0L, g_refs );

    {
        auto p1 = FooImplPtr::attach( FooImpl::createInstance().release() );
        auto p2 = FooImplPtr::attach( FooImpl::createInstance().release() );

        UTF_REQUIRE( nullptr != p1 );
        UTF_REQUIRE( nullptr != p2 );

        p1 = std::move( p2 );
        UTF_REQUIRE( nullptr != p1 );
        UTF_REQUIRE( nullptr == p2 );
    }

    /*
     * Ensure no memory leaks
     */

    UTF_REQUIRE_EQUAL( 0L, g_refs );
}

UTF_AUTO_TEST_CASE( BaseLib_SafeUniquePtrAndContainersTests )
{
    using namespace utest;

    typedef bl::cpp::SafeUniquePtr< MyObjectImpl, bl::om::detail::Deleter > MyObjectImplPtr;

    std::vector< MyObjectImplPtr > v1;
    std::vector< bl::om::ObjPtr< MyObjectImpl > > v2;

    v1.push_back( MyObjectImplPtr::attach( MyObjectImpl::createInstance().release() ) );
    v2.push_back( MyObjectImpl::createInstance() );

    /*
     * Add a fake UTF check, so the UTF library doesn't complain there are no checks
     */

    UTF_REQUIRE( v1.size() && v2.size() );
}

/************************************************************************
 * ScalarTypeIniter< T > tests
 */

namespace
{
    class ScalarTypeIniterTester
    {
    public:

        enum TestEnum
        {
            NonZero = 1,
            Zero = 0,
        };

        template
        <
            typename T
        >
        static void runTests( SAA_in const T realValue, SAA_in_opt const T defaultValue = T() )
        {
            T localValue;

            bl::cpp::ScalarTypeIniter< T > i1;
            bl::cpp::ScalarTypeIniter< T > i2( realValue );
            bl::cpp::ScalarTypeIniter< T > i3 = realValue;

            UTF_REQUIRE_EQUAL( i1, defaultValue );
            UTF_REQUIRE_EQUAL( i2, realValue );
            UTF_REQUIRE_EQUAL( i3, realValue );

            i1 = realValue;
            UTF_REQUIRE_EQUAL( i1, realValue );

            localValue = i1;
            UTF_REQUIRE_EQUAL( localValue, realValue );

            i1 = defaultValue;
            UTF_REQUIRE_EQUAL( i1, defaultValue );

            UTF_REQUIRE_EQUAL( i2, i3 );
        }
    };
}

UTF_AUTO_TEST_CASE( BaseLib_ScalarTypeIniterTests )
{
    int dummy = 0;

    ScalarTypeIniterTester::runTests< int >( 42, 0 );
    ScalarTypeIniterTester::runTests< int* >( &dummy, nullptr );
    ScalarTypeIniterTester::runTests< bool >( true, false );

    ScalarTypeIniterTester::runTests< ScalarTypeIniterTester::TestEnum >(
        ScalarTypeIniterTester::NonZero,
        ScalarTypeIniterTester::Zero
        );
}

/************************************************************************
 * PathUtils tests
 */

namespace
{
    bl::fs::path pathNormalizeForOS( SAA_in std::string&& path )
    {
        if( bl::os::onWindows() )
        {
            /*
             * The passed in paths are Windows, so in this case we
             * just return it as is
             */

            return path;
        }

        /*
         * Just delete the drive letter prefix and then convert the slashes
         */

        if( path.length() >= 2 && path[ 1 ] == ':' )
        {
            path.erase( 0, 2 );
        }

        std::replace( path.begin(), path.end(), '\\', '/' );

        return path;
    }
}

UTF_AUTO_TEST_CASE( BaseLib_PathUtilsTests )
{
    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo" ), pathNormalizeForOS( "c:\\" ), relPath ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "foo" );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo" ), pathNormalizeForOS( "c:\\foo" ), relPath ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "" );
    }

    {
        bl::fs::path relPath( "baz" );

        UTF_REQUIRE( ! bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo" ), pathNormalizeForOS( "c:\\bar" ), relPath ) );

        /*
         * Verify that relPath should be left unmodified if fs::getRelativePath
         * returns false
         */

        UTF_REQUIRE_EQUAL( relPath.string(), "baz" );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo\\bar\\baz" ), pathNormalizeForOS( "c:\\foo\\bar" ), relPath ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "baz" );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo\\bar\\baz" ), pathNormalizeForOS( "c:\\foo\\bar\\" ), relPath ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "baz" );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo\\bar\\baz" ), pathNormalizeForOS( "c:\\foo\\bar\\\\" ), relPath ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "baz" );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo\\bar\\baz" ), pathNormalizeForOS( "c:/foo/bar/" ), relPath ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "baz" );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( ! bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo" ), pathNormalizeForOS( "c:\\bar" ), relPath, true /* allowNonStrictRoot */ ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), pathNormalizeForOS( "..\\foo" ) );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( ! bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo\\version2.0" ), pathNormalizeForOS( "c:\\bar" ), relPath, true /* allowNonStrictRoot */ ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), pathNormalizeForOS( "..\\foo\\version2.0" ) );
    }

    {
        bl::fs::path relPath;

        UTF_REQUIRE( bl::fs::getRelativePath( pathNormalizeForOS( "c:\\foo\\bar\\baz" ), pathNormalizeForOS( "c:/foo/bar/" ), relPath, true /* allowNonStrictRoot */ ) );
        UTF_REQUIRE( relPath.is_relative() );
        UTF_REQUIRE_EQUAL( relPath.string(), "baz" );
    }

    {
        /*
         * All this is needed because std::make_tuple() supports only 5 arguments in MSVC and we need 7 values...
         */

        typedef enum : std::uint8_t
        {
            NONE            = 0,
            UNIX            = 1,
            WINDOWS         = 2,
            ALL             = UNIX | WINDOWS,
        } systems;

        const auto system = bl::os::onUNIX() ? UNIX : WINDOWS;

        static const std::tuple
        <
            systems /* valid on which OSes */,
            bool /* contains ./.. */,
            const char* /* path */
        > testDirs[] =
        {
            std::make_tuple(    NONE, false, "" ),
            std::make_tuple(     ALL,  true, "." ),
            std::make_tuple(     ALL,  true, ".." ),
            std::make_tuple(     ALL, false, ".dir" ),
            std::make_tuple(     ALL, false, "..dir" ),
            std::make_tuple(     ALL, false, "dir." ),
            std::make_tuple(     ALL, false, "dir.." ),
            std::make_tuple(     ALL, false, "my.dir" ),
            std::make_tuple(     ALL, false, "my..dir" ),
            std::make_tuple(     ALL, false, "my_dir" ),
            std::make_tuple(     ALL, false, "my-dir" ),
            std::make_tuple(     ALL, false, "my+dir" ),
            std::make_tuple(     ALL, false, "my dir" ),
            std::make_tuple(     ALL, false, "UTF8 characters δυο, три, čtyři, पाँच.dat" ),
            std::make_tuple(    UNIX, false, "my>dir" ),
            std::make_tuple(    UNIX, false, "my<dir" ),
            std::make_tuple(    UNIX, false, "my|dir" ),
            std::make_tuple(    UNIX, false, "my:dir" ),
            std::make_tuple(    UNIX, false, "my\"dir" ),
            std::make_tuple(    UNIX, false, "my?dir" ),
            std::make_tuple(    UNIX, false, "my*dir" ),
            std::make_tuple(    UNIX, false, "my\\dir" ),
            std::make_tuple(    NONE, false, "my/dir" ),
            std::make_tuple(    UNIX, false, "my" "\x01" "dir" ),
            std::make_tuple(    UNIX, false, "my" "\x1F" "dir" ),
            std::make_tuple(    UNIX, false, "my\r\ndir" ),
            std::make_tuple(    NONE, false, "/" ),
            std::make_tuple(    UNIX, false, "\\" ),
            std::make_tuple(    UNIX, false, "*.*" ),
            std::make_tuple(    UNIX, false, "?" ),
        };

        for( const auto& test : testDirs )
        {
            const bool isPortable = std::get< 0 >( test ) == ALL;
            const bool isValid = ( std::get< 0 >( test ) & system ) == system;
            const bool isDotDir = std::get< 1 >( test );
            const auto dir = std::get< 2 >( test );

            UTF_CHECK_EQUAL( isPortable && ! isDotDir, bl::fs::isPortableDirectoryName( dir, false /* don't allow current/parent dir */ ) );
            UTF_CHECK_EQUAL( isValid && ! isDotDir, bl::fs::isValidDirectoryName( dir, false /* don't allow current/parent dir */ ) );

            UTF_CHECK_EQUAL( isPortable, bl::fs::isPortableDirectoryName( dir, true /* allow current/parent dir */ ) );
            UTF_CHECK_EQUAL( isValid, bl::fs::isValidDirectoryName( dir, true /* allow current/parent dir */ ) );
        }

        static const std::tuple
        <
            bool /* portable */,
            systems /* valid on which OSes */,
            systems /* is absolute on which OSes */,
            bool /* contains ./.. */,
            const char* /* path */
        > testPaths[] =
        {
            std::make_tuple( false,    NONE,    NONE, false, "" ),
            std::make_tuple( false,     ALL,     ALL, false, "/" ),
            std::make_tuple( false,     ALL, WINDOWS, false, "\\" ),
            std::make_tuple( false,     ALL, WINDOWS, false, "C:" ),
            std::make_tuple( false,     ALL, WINDOWS, false, "C:\\" ),
            std::make_tuple(  true,     ALL,    NONE, false, "file name.txt" ),
            std::make_tuple(  true,     ALL,    NONE, false, "dir/file_name.txt" ),
            std::make_tuple( false,     ALL,     ALL, false, "/dir/file_name.txt" ),
            std::make_tuple(  true,     ALL,    NONE,  true, "./dir/file_name.txt" ),
            std::make_tuple(  true,     ALL,    NONE,  true, "../dir/file_name.txt" ),
            std::make_tuple(  true,     ALL,    NONE,  true, "dir/./file_name.txt" ),
            std::make_tuple(  true,     ALL,    NONE,  true, "dir/../file_name.txt" ),
            std::make_tuple(  true,     ALL,    NONE, false, "UTF8 characters/1/δυο/три/čtyři/पाँच.dat" ),
            std::make_tuple( false,     ALL,    NONE, false, "UTF8 characters\\1\\δυο\\три\\čtyři\\पाँच.dat" ),
            std::make_tuple( false,    UNIX,    NONE, false, "file/with:some::colons" ),
            std::make_tuple(  true,     ALL,    NONE, false, "dir name/ with / spaces.TXT " ),
            std::make_tuple( false,     ALL, WINDOWS, false, "C:\\Program Files\\myapp\\bin\\myapp-binary" ),
            std::make_tuple( false,     ALL,    NONE, false, "Program Files\\myapp\\bin\\myapp-binary" ),
            std::make_tuple( false,     ALL, WINDOWS, false, "\\\\HOST-123\\share$\\some file.dat" ),
            std::make_tuple( false,     ALL,     ALL, false, "//HOST-123/share$/another file.dat" ),
        };

        for( const auto& test : testPaths )
        {
            const bool isPortable = std::get< 0 >( test );
            const bool isValid = ( std::get< 1 >( test ) & system ) == system;
            const bool isAbsolute = ( std::get< 2 >( test ) & system ) == system;
            const bool isDotDir = std::get< 3 >( test );
            const std::string path = std::get< 4 >( test );

            UTF_MESSAGE( path );

            UTF_CHECK_EQUAL(
                isPortable && ! isDotDir && ! isAbsolute,
                bl::fs::isPortablePath( path, false /* don't allow current/parent dir */ )
                );

            UTF_CHECK_EQUAL(
                isValid && ! isDotDir && ! isAbsolute,
                bl::fs::isValidPath( path, false /* don't allow current/parent dir */ )
                );

            UTF_CHECK_EQUAL(
                isPortable && ! isAbsolute,
                bl::fs::isPortablePath( path, true /* allow current/parent dir */ )
                );

            UTF_CHECK_EQUAL(
                isValid && ! isAbsolute,
                bl::fs::isValidPath( path, true /* allow current/parent dir */ )
                );

            UTF_CHECK_EQUAL(
                isValid,
                bl::fs::isValidPath( path, true /* allow current/parent dir */, true /* allowAbsolutePath */ )
                );
        }
    }
}

/************************************************************************
 * BoxedValueObject tests
 */

UTF_AUTO_TEST_CASE( BaseLib_BoxedValueObjectTests )
{
    {
        typedef bl::om::ObjectImpl< bl::om::BoxedValueObject< bl::fs::file_status > > status_obj_t;

        bl::fs::file_status s1( bl::fs::symlink_file );
        bl::fs::file_status s2( bl::fs::socket_file );

        UTF_REQUIRE_EQUAL( s1.type(), bl::fs::symlink_file );
        UTF_REQUIRE_EQUAL( s2.type(), bl::fs::socket_file  );

        auto obj1 = status_obj_t::createInstance();
        obj1 -> moveOrCopyNothrow( std::move( s1 ) );

        auto obj2 = status_obj_t::createInstance();
        obj2 -> moveOrCopyNothrow( std::move( s2 ) );

        UTF_REQUIRE_EQUAL( s1.type(), bl::fs::symlink_file );
        UTF_REQUIRE_EQUAL( s2.type(), bl::fs::socket_file  );

        UTF_REQUIRE_EQUAL( obj1 -> value().type(), bl::fs::symlink_file );
        UTF_REQUIRE_EQUAL( obj2 -> value().type(), bl::fs::socket_file );
    }

    {
        typedef bl::om::ObjectImpl< bl::om::BoxedValueObject< bl::fs::path > > entry_obj_t;

        bl::fs::path p1( "bar" );
        bl::fs::path p2( "baz" );

        UTF_REQUIRE_EQUAL( p1, bl::fs::path( "bar" ) );
        UTF_REQUIRE_EQUAL( p2, bl::fs::path( "baz" ) );

        const auto obj1 = entry_obj_t::createInstance();
        const auto obj2 = entry_obj_t::createInstance();

        obj1 -> swapValue( std::move( p1 ) );
        obj2 -> swapValue( std::move( p2 ) );

        UTF_REQUIRE_EQUAL( p1, bl::fs::path( "" ) );
        UTF_REQUIRE_EQUAL( p2, bl::fs::path( "" ) );

        UTF_REQUIRE_EQUAL( obj1 -> value(), bl::fs::path( "bar" ) );
        UTF_REQUIRE_EQUAL( obj2 -> value(), bl::fs::path( "baz" ) );
    }

}

/************************************************************************
 * EndpointSelector tests
 */

UTF_AUTO_TEST_CASE( BaseLib_SimpleEndpointSelectorImplTests )
{
    {
        const auto maxRetryCount = bl::EndpointCircularIterator::DEFAULT_MAX_RETRY_COUNT / 2;
        const auto retryTimeout =
            time::seconds( bl::EndpointCircularIterator::DEFAULT_RETRY_TIMEOUT_IN_SECONDS / 2 );

        const auto selector =
            bl::SimpleEndpointSelectorImpl::createInstance< bl::EndpointSelector >(
            "cassandra.jpmchase.com",
            1234,
            maxRetryCount,
            retryTimeout
            );

        UTF_REQUIRE_EQUAL( selector -> count(), 1U );

        const auto iterator = selector -> createIterator();

        UTF_REQUIRE_EQUAL( iterator -> maxRetryCount(), maxRetryCount );
        UTF_REQUIRE_EQUAL( iterator -> retryTimeout(), retryTimeout );
    }

    const auto selector =
        bl::SimpleEndpointSelectorImpl::createInstance< bl::EndpointSelector >( "cassandra.jpmchase.com", 1234 );
    UTF_REQUIRE_EQUAL( selector -> count(), 1U );

    {
        const auto iterator = selector -> createIterator();

        UTF_REQUIRE_EQUAL(
            iterator -> maxRetryCount(),
            bl::EndpointCircularIterator::DEFAULT_MAX_RETRY_COUNT
            );

        UTF_REQUIRE_EQUAL(
            iterator -> retryTimeout(),
            time::seconds( bl::EndpointCircularIterator::DEFAULT_RETRY_TIMEOUT_IN_SECONDS )
            );

        UTF_REQUIRE_EQUAL( iterator -> host(), "cassandra.jpmchase.com" );
        UTF_REQUIRE_EQUAL( iterator -> port(), 1234 );
        UTF_REQUIRE_EQUAL( iterator -> count(), 1U );

        UTF_REQUIRE( iterator -> canRetry() );
        UTF_REQUIRE( iterator -> canRetryNow() );

        for( std::size_t i = 0; i < 10; ++i )
        {
            if( i < ( iterator -> maxRetryCount() - 1 ) )
            {
                UTF_REQUIRE( iterator -> selectNext() );
                UTF_REQUIRE( iterator -> canRetry() );
            }
            else
            {
                UTF_REQUIRE( ! iterator -> selectNext() );
                UTF_REQUIRE( ! iterator -> canRetry() );
                UTF_REQUIRE( ! iterator -> canRetryNow() );
            }

            UTF_REQUIRE_EQUAL( iterator -> host(), "cassandra.jpmchase.com" );
            UTF_REQUIRE_EQUAL( iterator -> port(), 1234 );
        }
    }

    {
        const auto iterator = selector -> createIterator();

        bl::time::time_duration timeout;

        UTF_REQUIRE( iterator -> canRetry() );
        UTF_REQUIRE( iterator -> canRetryNow( &timeout ) );
        UTF_REQUIRE_EQUAL( timeout, bl::time::milliseconds( 0 ) );

        UTF_REQUIRE( ! iterator -> canRetryNow( &timeout ) );
        UTF_REQUIRE( timeout != bl::time::milliseconds( 0 ) );
        UTF_REQUIRE( ! timeout.is_special() );

        /*
         * TODO: turn off part of the test which is fragile
         *
         * It will be re-enabled once we figure a better way
         * to do this
         */

        /*
        os::sleep( timeout );

        UTF_REQUIRE( iterator -> canRetryNow( &timeout ) );
        UTF_REQUIRE_EQUAL( timeout, time::milliseconds( 0 ) );
        UTF_REQUIRE( ! iterator -> canRetryNow( &timeout ) );
        */
    }
}

/************************************************************************
 * EndpointSelectorImpl tests
 */

UTF_AUTO_TEST_CASE( BaseLib_EndpointSelectorImplTests )
{
    /*
     * Just use our real cassandra nodes dev cluster
     */

    const char* hosts[] =
    {
        "host1.domain.net",
        "host2.domain.net",
        "host3.domain.net",
        "host4.domain.net",
    };

    const auto cb = [ &hosts ]( SAA_in const bl::om::ObjPtr< bl::EndpointSelectorImpl >& selector ) -> void
    {
        {
            bl::time::time_duration timeout;

            UTF_REQUIRE_EQUAL( selector -> count(), 4U );

            const auto iterator = selector -> createIterator();
            UTF_REQUIRE_EQUAL( iterator -> count(), 4U );


            const std::size_t iterationsCount = 10U * BL_ARRAY_SIZE( hosts );

            for( std::size_t i = 0; i < iterationsCount; ++i )
            {
                UTF_REQUIRE_EQUAL( iterator -> host(), hosts[ i % BL_ARRAY_SIZE( hosts ) ] );
                UTF_REQUIRE_EQUAL( iterator -> port(), 1234 );

                const auto maxIterations =
                    BL_ARRAY_SIZE( hosts ) * ( iterator -> maxRetryCount() - 1 );

                if( i < maxIterations )
                {
                    UTF_REQUIRE( iterator -> selectNext() );
                    UTF_REQUIRE( iterator -> canRetry() );
                }
                else
                {
                    UTF_REQUIRE( ! iterator -> selectNext() );
                    UTF_REQUIRE( ! iterator -> canRetry() );
                    UTF_REQUIRE( ! iterator -> canRetryNow() );
                }
            }
        }

        {
            const auto iterator = selector -> createIterator();

            bl::time::time_duration timeout;

            UTF_REQUIRE( iterator -> canRetry() );
            UTF_REQUIRE( iterator -> canRetryNow( &timeout ) );
            UTF_REQUIRE_EQUAL( timeout, bl::time::milliseconds( 0 ) );

            UTF_REQUIRE( ! iterator -> canRetryNow( &timeout ) );
            UTF_REQUIRE( timeout != bl::time::milliseconds( 0 ) );
            UTF_REQUIRE( ! timeout.is_special() );

            /*
             * TODO: turn off part of the test which is fragile
             *
             * It will be re-enabled once we figure a better way
             * to do this
             */

            /*
            os::sleep( timeout );

            UTF_REQUIRE( iterator -> canRetryNow( &timeout ) );
            UTF_REQUIRE_EQUAL( timeout, time::milliseconds( 0 ) );
            UTF_REQUIRE( ! iterator -> canRetryNow( &timeout ) );
            */
        }
    };

    {
        const auto selector = bl::EndpointSelectorImpl::createInstance( 1234 );
        UTF_REQUIRE_EQUAL( selector -> count(), 0U );

        for( std::size_t i = 0; i < BL_ARRAY_SIZE( hosts ); ++i )
        {
            selector -> addHost( hosts[ i ] );
        }

        cb( selector );
    }

    {
        const auto selector = bl::EndpointSelectorImpl::createInstance(
            1234,
            hosts,
            hosts + BL_ARRAY_SIZE( hosts )
            );
        UTF_REQUIRE_EQUAL( selector -> count(), 4U );

        cb( selector );
    }

    {
        std::vector< std::string > v( hosts, hosts + BL_ARRAY_SIZE( hosts ) );

        const auto selector = bl::EndpointSelectorImpl::createInstance(
            1234,
            v.begin(),
            v.end()
            );
        UTF_REQUIRE_EQUAL( selector -> count(), 4U );

        cb( selector );
    }
}

/************************************************************************
 * UuidIteratorImpl tests
 */

UTF_AUTO_TEST_CASE( BaseLib_TestUuidIteratorImpl )
{
    std::set< bl::uuid_t > unique;

    auto pv = bl::bo::uuid_vector::createInstance();

    pv -> lvalue().push_back( bl::uuids::create() );
    pv -> lvalue().push_back( bl::uuids::create() );
    pv -> lvalue().push_back( bl::uuids::create() );

    const auto single = bl::uuids::create();
    const auto beginSingle = &single;

    const auto i1 = bl::UuidIteratorImpl::createInstance< bl::UuidIterator >( beginSingle, beginSingle + 1 );

    const auto i2 = bl::UuidIteratorImpl::createInstance< bl::UuidIterator >(
        pv -> value(),
        bl::om::qi< bl::om::Object >( pv )
        );

    /*
     * Reset vector to ensure the iterator holds a reference to it
     */

    pv.reset();

    UTF_REQUIRE( i1 -> hasCurrent() );
    UTF_REQUIRE( unique.insert( i1 -> current() ).second );
    i1 -> loadNext();
    UTF_REQUIRE( ! i1 -> hasCurrent() );

    UTF_REQUIRE( i2 -> hasCurrent() );
    i2 -> loadNext();
    UTF_REQUIRE( i2 -> hasCurrent() );
    i2 -> loadNext();
    UTF_REQUIRE( i2 -> hasCurrent() );
    i2 -> loadNext();
    UTF_REQUIRE( ! i2 -> hasCurrent() );

    i2 -> reset();
    UTF_REQUIRE( i2 -> hasCurrent() );

    for( ; i2 -> hasCurrent(); i2 -> loadNext() )
    {
        UTF_REQUIRE( unique.insert( i2 -> current() ).second );
    }

    UTF_REQUIRE_EQUAL( ( std::size_t ) 4, unique.size() );
}

/************************************************************************
 * ScopeGuard tests
 */

UTF_AUTO_TEST_CASE( BaseLib_TestScopeGuard )
{
    {
        bool called1 = false;
        bool called2 = false;

        {
            auto g1 = BL_SCOPE_GUARD( { called1 = true; } );
            auto g2 = BL_SCOPE_GUARD( { called2 = true; } );

            UTF_REQUIRE( ! called1 );
            UTF_REQUIRE( ! called2 );

            g2.dismiss();
        }

        UTF_REQUIRE( called1 );
        UTF_REQUIRE( ! called2 );
    }

    {
        bool called1 = false;
        bool called2 = false;

        {
            BL_SCOPE_EXIT( { called1 = true; } );
            BL_SCOPE_EXIT( { called2 = true; } );

            UTF_REQUIRE( ! called1 );
            UTF_REQUIRE( ! called2 );
        }

        UTF_REQUIRE( called1 );
        UTF_REQUIRE( called2 );
    }
}

/************************************************************************
 * FsUtils tests
 */

UTF_AUTO_TEST_CASE( FsUtils_TestMakeHidden )
{
    bl::fs::path tmpPath;
    BL_SCOPE_EXIT( { bl::fs::safeDeletePathNothrow( tmpPath ); } );

    bl::fs::path pathSave;

    bl::fs::createTempDir( tmpPath );
    pathSave = tmpPath;

    tmpPath = bl::fs::makeHidden( tmpPath );
    UTF_REQUIRE( pathSave != tmpPath );
    UTF_REQUIRE( 0U == tmpPath.filename().string().find( "." ) );
    pathSave = tmpPath;

    tmpPath = bl::fs::makeHidden( tmpPath );
    UTF_REQUIRE( pathSave == tmpPath );

    if( bl::os::onWindows() )
    {
        UTF_REQUIRE( bl::fs::safeGetFileAttributes( tmpPath ) & bl::os::FileAttributeHidden );
    }

    UTF_REQUIRE_THROW( bl::fs::makeHidden( tmpPath / ".non-existent" ), bl::UnexpectedException );
    UTF_REQUIRE_THROW( bl::fs::makeHidden( "." ), bl::UnexpectedException );
    UTF_REQUIRE_THROW( bl::fs::makeHidden( ".." ), bl::UnexpectedException );

    bl::fs::safeUpdateFileAttributes( tmpPath, bl::os::FileAttributeHidden, true /* remove */ );

    if( bl::os::onWindows() )
    {
        UTF_REQUIRE( ! ( bl::fs::safeGetFileAttributes( tmpPath ) & bl::os::FileAttributeHidden ) );
    }

    bl::fs::safeUpdateFileAttributes( tmpPath, bl::os::FileAttributeHidden, true /* remove */ );

    bl::fs::safeUpdateFileAttributes( tmpPath, bl::os::FileAttributeHidden, false /* remove */ );

    if( bl::os::onWindows() )
    {
        UTF_REQUIRE( bl::fs::safeGetFileAttributes( tmpPath ) & bl::os::FileAttributeHidden );
    }

    bl::fs::safeUpdateFileAttributes( tmpPath, bl::os::FileAttributeHidden, false /* remove */ );
}

UTF_AUTO_TEST_CASE( FsUtils_TestCreateTempDirAndCreateDirectory )
{
    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        UTF_REQUIRE( bl::fs::exists( tmpDir.path() ) );

        for( std::size_t i = 0; i < 2000; i++ )
        {
            bl::fs::safeCreateDirectory( tmpDir.path() );
        }
    }

    UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
}

UTF_AUTO_TEST_CASE( FsUtils_TestMkdirs )
{
    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        UTF_REQUIRE( bl::fs::exists( tmpPath ) );

        /*
         * Just attempt to create a lot of directories in parallel
         */

        bl::tasks::scheduleAndExecuteInParallel(
            [ &tmpPath ]( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
            {
                const auto cb = []( SAA_in const bl::om::ObjPtrCopyable< bl::bo::path >& nestedDirs ) -> void
                {
                    bl::fs::safeMkdirs( nestedDirs -> value() );
                    UTF_REQUIRE( bl::fs::exists( nestedDirs -> value() ) );

                    bl::fs::safeMkdirs( nestedDirs -> value() );
                    UTF_REQUIRE( bl::fs::exists( nestedDirs -> value() ) );
                };

                std::set< std::string > allPaths;

                for( std::size_t i = 0; i < 500; i++ )
                {
                    auto nestedDirs =
                        tmpPath /
                        ( "foo" + bl::utils::lexical_cast< std::string >( i % 5 ) ) /
                        ( "bar" + bl::utils::lexical_cast< std::string >( i % 5 ) ) /
                        ( "baz" + bl::utils::lexical_cast< std::string >( i % 20 ) );

                    const auto pair = allPaths.insert( nestedDirs.string() );

                    if( pair.second )
                    {
                        /*
                         * This is a new path; it should not exists
                         */

                        UTF_REQUIRE( ! bl::fs::exists( nestedDirs ) );
                    }

                    bl::om::ObjPtrCopyable< bl::bo::path > pathObj( bl::bo::path::createInstance() );

                    pathObj -> lvalue().swap( nestedDirs );

                    eq -> push_back( bl::cpp::bind< void >( cb, pathObj ) );
                    eq -> push_back( bl::cpp::bind< void >( cb, pathObj ) );
                    eq -> push_back( bl::cpp::bind< void >( cb, pathObj ) );
                }

                eq -> flush();

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "Number of unique paths created: "
                        << allPaths.size()
                    );
            }
            );
    }
}

UTF_AUTO_TEST_CASE( FsUtils_JunctionsTests )
{
    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        const auto subDir = tmpPath / "sub-dir";
        const auto junctionDir = tmpPath / "junction-dir";
        const auto junctionDir2 = tmpPath / "junction-dir2";
        const auto filePath = subDir / "file.txt";
        const auto filePathViaJunction = junctionDir / "file.txt";

        bl::fs::safeMkdirs( subDir );

        UTF_REQUIRE( ! bl::fs::isDirectoryJunction( subDir ) );
        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir ), bl::UnexpectedException )
        bl::fs::createDirectoryJunction( subDir, junctionDir );
        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir ) );

        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir2 ), bl::UnexpectedException )
        bl::fs::createDirectoryJunction( subDir, junctionDir2 );
        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir2 ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir2 ) );

        {
            const auto file = bl::os::fopen( filePath, "wb" );
            UTF_REQUIRE( bl::fs::exists( filePath ) );
        }

        UTF_REQUIRE( bl::fs::exists( filePathViaJunction ) );

        bl::fs::deleteDirectoryJunction( junctionDir );
        UTF_REQUIRE( ! bl::fs::exists( junctionDir ) );
        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir ), bl::UnexpectedException )

        UTF_REQUIRE( ! bl::fs::exists( filePathViaJunction ) );
        UTF_REQUIRE( bl::fs::exists( filePath ) );

        /*
         * Now delete the directory to test the broken junction case
         */

        bl::eh::error_code ec;

        UTF_REQUIRE( bl::fs::path_exists( junctionDir2 ) );
        ec.clear();
        UTF_REQUIRE( bl::fs::path_exists( junctionDir2, ec ) );
        UTF_REQUIRE( ! ec );

        bl::fs::safeDeletePathNothrow( subDir );
        UTF_REQUIRE( ! bl::fs::exists( filePath ) );

        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir2 ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir2 ) );

        UTF_REQUIRE( bl::fs::path_exists( junctionDir2 ) );
        ec.clear();
        UTF_REQUIRE( bl::fs::path_exists( junctionDir2, ec ) );
        UTF_REQUIRE( ! ec );

        bl::fs::deleteDirectoryJunction( junctionDir2 );
        UTF_REQUIRE( ! bl::fs::exists( junctionDir2 ) );
        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir2 ), bl::UnexpectedException )

        UTF_REQUIRE( ! bl::fs::path_exists( junctionDir2 ) );
        ec.clear();
        UTF_REQUIRE( ! bl::fs::path_exists( junctionDir2, ec ) );
        UTF_REQUIRE( ec );
    }

    UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
}

UTF_AUTO_TEST_CASE( FsUtils_TestSafeRemove )
{
    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        const auto subDir = tmpPath / "sub-dir";
        const auto junctionDir = tmpPath / "junction-dir";
        const auto junctionDir2 = tmpPath / "junction-dir2";
        const auto filePath = subDir / "file.txt";
        const auto filePathViaJunction = junctionDir / "file.txt";

        bl::fs::safeRemoveIfExists( subDir );
        bl::fs::safeRemoveIfExists( filePathViaJunction );
        BL_CHK_EC_NM( bl::fs::trySafeRemoveIfExists( subDir ) );
        BL_CHK_EC_NM( bl::fs::trySafeRemoveIfExists( filePathViaJunction ) );

        bl::fs::safeMkdirs( subDir );

        UTF_REQUIRE( ! bl::fs::isDirectoryJunction( subDir ) );
        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir ), bl::UnexpectedException )
        bl::fs::createDirectoryJunction( subDir, junctionDir );
        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir ) );

        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir2 ), bl::UnexpectedException )
        bl::fs::createDirectoryJunction( subDir, junctionDir2 );
        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir2 ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir2 ) );

        {
            const auto file = bl::os::fopen( filePath, "wb" );
            UTF_REQUIRE( bl::fs::exists( filePath ) );
            UTF_REQUIRE( bl::fs::exists( filePathViaJunction ) );
        }

        bl::fs::safeRemove( filePathViaJunction );
        UTF_REQUIRE( ! bl::fs::exists( filePath ) );
        UTF_REQUIRE( ! bl::fs::exists( filePathViaJunction ) );

        bl::fs::safeRemove( subDir );
        UTF_REQUIRE( ! bl::fs::exists( subDir ) );
        UTF_REQUIRE( ! bl::fs::exists( junctionDir ) );
        UTF_REQUIRE( ! bl::fs::exists( junctionDir2 ) );
        UTF_REQUIRE( bl::fs::path_exists( junctionDir ) );
        UTF_REQUIRE( bl::fs::path_exists( junctionDir2 ) );

        bl::fs::safeRemove( junctionDir );
        UTF_REQUIRE( ! bl::fs::path_exists( junctionDir ) );
        UTF_REQUIRE( bl::fs::path_exists( junctionDir2 ) );
        UTF_REQUIRE( bl::fs::exists( tmpPath ) );

        bl::fs::safeRemove( junctionDir2 );
        UTF_REQUIRE( ! bl::fs::path_exists( junctionDir ) );
        UTF_REQUIRE( ! bl::fs::path_exists( junctionDir2 ) );
        UTF_REQUIRE( bl::fs::exists( tmpPath ) );
    }

    UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
}

UTF_AUTO_TEST_CASE( FsUtils_TestSafeRemoveAll )
{
    bl::fs::path tmpPath;

    {
        bl::fs::TmpDir tmpDir;
        tmpPath = tmpDir.path();

        const auto subDir = tmpPath / "sub-dir";
        const auto subDir2 = subDir / "sub-dir2";
        const auto junctionDir = tmpPath / "junction-dir";
        const auto junctionDir2 = tmpPath / "junction-dir2";
        const auto filePath = subDir / "file.txt";
        const auto filePathViaJunction = junctionDir / "file.txt";

        bl::fs::safeRemoveAllIfExists( subDir );
        bl::fs::safeRemoveAllIfExists( filePathViaJunction );
        BL_CHK_EC_NM( bl::fs::trySafeRemoveAllIfExists( subDir ) );
        BL_CHK_EC_NM( bl::fs::trySafeRemoveAllIfExists( filePathViaJunction ) );

        bl::fs::safeMkdirs( subDir );
        bl::fs::safeMkdirs( subDir2 );

        UTF_REQUIRE( ! bl::fs::isDirectoryJunction( subDir ) );
        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir ), bl::UnexpectedException )
        bl::fs::createDirectoryJunction( subDir, junctionDir );
        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir ) );

        UTF_CHECK_THROW( bl::fs::isDirectoryJunction( junctionDir2 ), bl::UnexpectedException )
        bl::fs::createDirectoryJunction( subDir, junctionDir2 );
        UTF_REQUIRE( bl::fs::isDirectoryJunction( junctionDir2 ) );
        UTF_REQUIRE_EQUAL( subDir, bl::fs::getDirectoryJunctionTarget( junctionDir2 ) );

        {
            const auto file = bl::os::fopen( filePath, "wb" );
            UTF_REQUIRE( bl::fs::exists( filePath ) );
            UTF_REQUIRE( bl::fs::exists( filePathViaJunction ) );
        }

        bl::fs::safeRemoveAll( tmpPath );
        UTF_REQUIRE( ! bl::fs::exists( tmpPath ) );
    }
}

#define UTF_TEST_NORMALIZE( input, expected )                   \
    {                                                           \
        const auto normalized( bl::fs::normalize( input ) );    \
                                                                \
        BL_LOG(                                                 \
            bl::Logging::debug(),                               \
            BL_MSG()                                            \
                << "input: "                                    \
                << input                                        \
                << "\nnormalized: "                             \
                << normalized                                   \
                << "\nexpected: "                               \
                << expected                                     \
            );                                                  \
                                                                \
        UTF_CHECK( bl::fs::path( expected ).compare( normalized ) == 0 ); \
    }

UTF_AUTO_TEST_CASE( FsUtils_TestNormalize )
{
    /*
     * absolute path
     */

    UTF_TEST_NORMALIZE(
        bl::fs::absolute( test::UtfArgsParser::argv0() ),
        bl::fs::normalize( test::UtfArgsParser::argv0() )
        );

    /*
     * absolute non-existent path
     */

    UTF_TEST_NORMALIZE(
        bl::fs::absolute( bl::fs::path( test::UtfArgsParser::argv0() ) / "foobaztestfoofaz" ),
        bl::fs::normalize( bl::fs::path( test::UtfArgsParser::argv0() ) / "foobaztestfoofaz" )
        );

    /*
     * relative path
     */

    UTF_TEST_NORMALIZE(
        bl::fs::path( test::UtfArgsParser::argv0() ).filename(),
        bl::fs::path( bl::fs::current_path() / ( bl::fs::path( test::UtfArgsParser::argv0() ).filename() ) )
        );

    /*
     * relative non-existent path
     */

    UTF_TEST_NORMALIZE(
        "foobaztestfoofaz/foobaztestfoofaz",
        bl::fs::normalize( bl::fs::path( "foobaztestfoofaz" ) / "foobaztestfoofaz" )
        );

    /*
     * single dotted non-existent path
     */

    UTF_TEST_NORMALIZE(
        "./foobaztestfoofaz",
        bl::fs::normalize( bl::fs::path( "." ) ) / "foobaztestfoofaz"
        );

    /*
     * twice dotted non-existent path
     */

    UTF_TEST_NORMALIZE(
        "../foobaztestfoofaz",
        bl::fs::normalize( bl::fs::path( ".." ) ) / "foobaztestfoofaz"
        );

    /*
     * twice dotted non-existent path
     */

    UTF_TEST_NORMALIZE(
        "./foobaztestfoofaz/../foobaztestfoofaz2",
        bl::fs::normalize( bl::fs::path( "foobaztestfoofaz2" ) )
        );

    if( bl::os::onUNIX() )
    {
        /*
         * escaped paths
         */

        UTF_TEST_NORMALIZE(
            bl::fs::normalize( "\\foobaztestfoofaz1\\foobaztestfoofaz2" ),
            bl::fs::normalize( bl::fs::path( "\\foobaztestfoofaz1\\foobaztestfoofaz2" ) )
            );

        /*
         * Windows-style UNC/LFN paths are not supported on Unix
         */

        UTF_CHECK_THROW(
            bl::fs::normalize( "\\\\host\\directoryname" ),
            bl::ArgumentException
            );
    }

    if( bl::os::onWindows() )
    {
        /*
         * using regular slashes
         */

        UTF_TEST_NORMALIZE(
            std::string( "foobaztestfoofaz1/foobaztestfoofaz2/foobaztestfoofaz3" ),
            bl::fs::absolute( bl::fs::path( "foobaztestfoofaz1\\foobaztestfoofaz2\\foobaztestfoofaz3" ) )
            );

        /*
         * UNC path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\host\\directoryname" ),
            bl::fs::path( "\\\\?\\UNC\\host\\directoryname" )
            );

        /*
         * UNC dotted path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\host\\directoryname\\." ),
            bl::fs::path( "\\\\?\\UNC\\host\\directoryname" )
            );

        /*
         * UNC twice dotted path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\host\\directoryname\\.." ),
            bl::fs::path( "\\\\?\\UNC\\host" )
            );

        /*
         * LFN path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\?\\D:\\very long path" ),
            bl::fs::path( "\\\\?\\D:\\very long path" )
            );

        /*
         * LFN path dotted path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\?\\D:\\very long path\\.\\another level" ),
            bl::fs::path( "\\\\?\\D:\\very long path\\another level" )
            );

        /*
         * LFN path twice dotted path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\?\\D:\\very long path\\..\\another level" ),
            bl::fs::path( "\\\\?\\D:\\another level" )
            );

        /*
         * LFN path multi-dotted path
         */

        UTF_TEST_NORMALIZE(
            std::string( "\\\\?\\D:\\very long path\\.\\another level\\..\\yet another level" ),
            bl::fs::path( "\\\\?\\D:\\very long path\\yet another level" )
            );
    }
}

UTF_AUTO_TEST_CASE( FsUtils_TestCreateLockFile )
{
    bl::fs::TmpDir tmpDir;
    const auto tmpPath = tmpDir.path();

    const auto lockFile = tmpPath / "lockfile.lock";

    std::atomic< int > numCreated( 0 );

    std::vector< bl::os::thread > threads;

    for ( int i = 0; i < 10; ++i ) {
        threads.push_back(
            bl::os::thread([ & ]()
                {
                    if( bl::fs::createLockFile( lockFile.string() ) )
                    {
                        ++numCreated;
                    }
                })
            );
    }

    for( auto& thread : threads )
    {
        thread.join();
    }

    UTF_CHECK_EQUAL( 1, numCreated );

    UTF_REQUIRE( bl::fs::exists( lockFile ) );

    auto expectedFileContent = std::to_string( bl::os::getPid() );
    expectedFileContent += '\n';

    UTF_CHECK_EQUAL(
        expectedFileContent,
        bl::encoding::readTextFile( lockFile )
        );
}

/************************************************************************
 * StringUtils tests
 */

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsJoinFormattedTests )
{
    std::vector< std::string > empty;
    std::vector< std::string > vec;

    vec.push_back( "one" );
    vec.push_back( "two" );
    vec.push_back( "three" );
    vec.push_back( "four" );
    vec.push_back( "five" );

    UTF_CHECK_EQUAL( "", bl::str::joinFormatted( empty, " ", " " ) );

    UTF_CHECK_EQUAL( "onetwothreefourfive", bl::str::joinFormatted( vec, "", "" ) );
    UTF_CHECK_EQUAL( "one, two, three, four and five", bl::str::joinFormatted( vec, ", ", " and " ) );
    UTF_CHECK_EQUAL( "one\ntwo\nthree\nfour\nfive", bl::str::joinNewLineFormatted( vec ) );
    UTF_CHECK_EQUAL( "'one', 'two', 'three', 'four' and 'five'", bl::str::joinQuoteFormatted( vec ) );

    const auto formatter1 =
        [](
            SAA_in      std::ostream&           stream,
            SAA_in      const std::string&      value
        )
        {
            stream << '(' << bl::str::to_upper_copy( value ) << ')';
        };

    UTF_CHECK_EQUAL( "", bl::str::joinFormatted< std::string >( empty, ", ", ", ", formatter1 ) );

    UTF_CHECK_EQUAL( "(ONE)|(TWO)|(THREE)|(FOUR)=>(FIVE)", bl::str::joinFormatted< std::string >( vec, "|", "=>", formatter1 ) );

    int n = 0;
    const auto formatter2 =
        [ &n ](
            SAA_in      std::ostream&           stream,
            SAA_in      const std::string&      value
        )
        {
            stream << "[" << ++n << "]=" << value;
        };

    UTF_CHECK_EQUAL( "[1]=one;[2]=two;[3]=three;[4]=four;[5]=five", bl::str::joinFormatted< std::string >( vec, ";", ";", formatter2 ) );
    UTF_CHECK_EQUAL( "[6]=one+[7]=two+[8]=three+[9]=four>[10]=five", bl::str::joinFormatted< std::string >( vec, "+", ">", formatter2 ) );
}

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsSplitTests )
{
    std::vector< std::string > vec;

    /*
     * Beware, an empty string will be split into a vector of ONE empty element!
     */

    bl::str::split( vec, "", bl::str::is_any_of( ",;" ) );
    UTF_REQUIRE_EQUAL( 1U, vec.size() );
    UTF_CHECK_EQUAL( "", vec[ 0 ] );

    /*
     * This works as expected
     */

    bl::str::split( vec, "abc", bl::str::is_any_of( ",;" ) );
    UTF_REQUIRE_EQUAL( 1U, vec.size() );
    UTF_CHECK_EQUAL( "abc", vec[ 0 ] );

    bl::str::split( vec, "abc;def,;ghij", bl::str::is_any_of( ",;" ) );
    UTF_REQUIRE_EQUAL( 4U, vec.size() );
    UTF_CHECK_EQUAL( "abc", vec[ 0 ] );
    UTF_CHECK_EQUAL( "def", vec[ 1 ] );
    UTF_CHECK_EQUAL( "", vec[ 2 ] );
    UTF_CHECK_EQUAL( "ghij", vec[ 3 ] );

    bl::str::split( vec, "   ", bl::str::is_space() );
    UTF_REQUIRE_EQUAL( 4U, vec.size() );
    UTF_CHECK_EQUAL( "", vec[ 0 ] );
    UTF_CHECK_EQUAL( "", vec[ 1 ] );
    UTF_CHECK_EQUAL( "", vec[ 2 ] );
    UTF_CHECK_EQUAL( "", vec[ 3 ] );

    bl::str::split( vec, " abc ", bl::str::is_space() );
    UTF_REQUIRE_EQUAL( 3U, vec.size() );
    UTF_CHECK_EQUAL( "", vec[ 0 ] );
    UTF_CHECK_EQUAL( "abc", vec[ 1 ] );
    UTF_CHECK_EQUAL( "", vec[ 2 ] );

    bl::str::split( vec, "abc/def//ghij/", bl::str::is_from_range( '/', '/' ) );
    UTF_REQUIRE_EQUAL( 5U, vec.size() );
    UTF_CHECK_EQUAL( "abc", vec[ 0 ] );
    UTF_CHECK_EQUAL( "def", vec[ 1 ] );
    UTF_CHECK_EQUAL( "", vec[ 2 ] );
    UTF_CHECK_EQUAL( "ghij", vec[ 3 ] );
    UTF_CHECK_EQUAL( "", vec[ 4 ] );

    bl::str::split( vec, "12345-6789-0", bl::str::is_equal_to( '-' ) );
    UTF_REQUIRE_EQUAL( 3U, vec.size() );
    UTF_CHECK_EQUAL( "12345", vec[ 0 ] );
    UTF_CHECK_EQUAL( "6789", vec[ 1 ] );
    UTF_CHECK_EQUAL( "0", vec[ 2 ] );

    /*
     * Empty string is tokenized into an empty sequence (as expected)
     */

    typedef bl::str::escaped_list_separator< char > separator_t;

    separator_t sep( "\\", ":", "\"\'");

    std::string empty;
    bl::str::tokenizer< separator_t > tokens1( empty, sep );

    UTF_CHECK_NO_THROW( vec.assign( tokens1.begin(), tokens1.end() ) );
    UTF_REQUIRE_EQUAL( 0U, vec.size() );

    /*
     * Check various escape characters (quoted text cannot be properly nested)
     */

    std::string input = "/bin:/opt/a\\:b/c:'http://abc:7890':\"xyz:'ne\"st\"ed':/abc:def\\\\ghi\":end";
    bl::str::tokenizer< separator_t > tokens2( input, sep );

    UTF_CHECK_NO_THROW( vec.assign( tokens2.begin(), tokens2.end() ) );
    UTF_REQUIRE_EQUAL( 5U, vec.size() );
    UTF_CHECK_EQUAL( "/bin", vec[ 0 ] );
    UTF_CHECK_EQUAL( "/opt/a:b/c", vec[ 1 ] );
    UTF_CHECK_EQUAL( "http://abc:7890", vec[ 2 ] );
    UTF_CHECK_EQUAL( "xyz:nested:/abc:def\\ghi", vec[ 3 ] );
    UTF_CHECK_EQUAL( "end", vec[ 4 ] );

    input = "missing closing quote 'is ok: doh";
    bl::str::tokenizer< separator_t > tokens3( input, sep );

    UTF_CHECK_NO_THROW( vec.assign( tokens3.begin(), tokens3.end() ) );
    UTF_REQUIRE_EQUAL( 1U, vec.size() );
    UTF_CHECK_EQUAL( "missing closing quote is ok: doh", vec[ 0 ] );

    input = "dangling escape\\";
    bl::str::tokenizer< separator_t > tokens4( input, sep );

    UTF_CHECK_THROW( vec.assign( tokens4.begin(), tokens4.end() ), boost::escaped_list_error );

    {
        const std::string str1 = "abcd";
        const std::string str2 = "";
        const std::string str3 = "abcdabcd";
        const std::string str4 = "abcd_middle_abcd";
        const std::string str5 = "left_abcdabcd_right";
        const std::string str6 = "left_abcd_middle_abcd_right";

        const std::string sep = "abcd";
        const std::string sep1 = "abcdef";
        const std::string sep2 = "";

        {
            UTF_REQUIRE_THROW( bl::str::splitString( str1, sep1, str1.length(), 0U ), bl::ArgumentException );
            UTF_REQUIRE_THROW( bl::str::splitString( str2, sep1, 0U, str1.length() ), bl::ArgumentException );
            UTF_REQUIRE_THROW( bl::str::splitString( str3, sep2, 0U, str1.length() ), bl::ArgumentException );
        }

        {
            const auto result = bl::str::splitString( str1, sep, 0U, str1.length() );
            UTF_REQUIRE_EQUAL( result.size(), 2U );
        }

        {
            const auto result = bl::str::splitString( str1, sep );
            UTF_REQUIRE_EQUAL( result.size(), 2U );
        }

        {
            const auto result = bl::str::splitString( str1, sep1, 0U, str1.length() );
            UTF_REQUIRE_EQUAL( result.size(), 1U );
        }

        {
            const auto result = bl::str::splitString( str3, sep, 0U, str3.length() );
            UTF_REQUIRE_EQUAL( result.size(), 3U );
        }

        {
            const auto result = bl::str::splitString( str3, sep, 4U, str3.length() );
            UTF_REQUIRE_EQUAL( result.size(), 2U );
        }

        {
            const auto result = bl::str::splitString( str4, sep, 0U, str4.length() );
            UTF_REQUIRE_EQUAL( result.size(), 3U );
        }

        {
            const auto result = bl::str::splitString( str4, sep1, 0U, str4.length() );
            UTF_REQUIRE_EQUAL( result.size(), 1U );
        }

        {
            const auto result = bl::str::splitString( str5, sep, 0U, str5.length() );
            UTF_REQUIRE_EQUAL( result.size(), 3U );
        }

        {
            const auto result = bl::str::splitString( str5, sep, 0U, str5.find("_right") );
            UTF_REQUIRE_EQUAL( result.size(), 3U );
        }

        {
            const auto result = bl::str::splitString( str6, sep, 0U, str6.length() );
            UTF_REQUIRE_EQUAL( result.size(), 3U );
        }
    }
}

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsParsePropertiesListTests )
{
    {
        const auto properties = str::parsePropertiesList( "" );

        UTF_REQUIRE( properties.empty() );
    }

    {
        const auto properties = str::parsePropertiesList( ";;" );

        UTF_REQUIRE( properties.empty() );
    }

    {
        const auto properties = str::parsePropertiesList( "name1=value1" );

        UTF_REQUIRE_EQUAL( 1U, properties.size() );
        UTF_REQUIRE_EQUAL( std::string( "value1" ), properties.at( "name1" ) );
    }

    {
        const auto properties = str::parsePropertiesList( ";name1=value1; empty= " );

        UTF_REQUIRE_EQUAL( 2U, properties.size() );
        UTF_REQUIRE_EQUAL( std::string( "value1" ), properties.at( "name1" ) );
        UTF_REQUIRE_EQUAL( std::string( "" ), properties.at( "empty" ) );
    }

    {
        const auto properties =
            str::parsePropertiesList( "name1=value1; name2=value2;name3=value3;" );

        UTF_REQUIRE_EQUAL( 3U, properties.size() );
        UTF_REQUIRE_EQUAL( std::string( "value1" ), properties.at( "name1" ) );
        UTF_REQUIRE_EQUAL( std::string( "value2" ), properties.at( "name2" ) );
        UTF_REQUIRE_EQUAL( std::string( "value3" ), properties.at( "name3" ) );
    }

    UTF_REQUIRE_THROW_MESSAGE(
        str::parsePropertiesList( ";value" ),
        bl::InvalidDataFormatException,
        "Cannot parse property in 'name=value' format"
        );

    UTF_REQUIRE_THROW_MESSAGE(
        str::parsePropertiesList( ";=value" ),
        bl::InvalidDataFormatException,
        "Cannot parse property in 'name=value' format"
        );

    UTF_REQUIRE_THROW_MESSAGE(
        str::parsePropertiesList( ";name1=value1; name1=value2" ),
        bl::InvalidDataFormatException,
        bl::resolveMessage(
            BL_MSG()
                << "Duplicate property encountered name while parsing "
                << "concatenated properties in 'name=value' format"
        )
        );
}

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsGetBeginning )
{
    const std::string text = "global file system";

    UTF_CHECK_EQUAL( ""                  , bl::str::getBeginning( text, 0 ) );
    UTF_CHECK_EQUAL( "g"                 , bl::str::getBeginning( text, 1 ) );
    UTF_CHECK_EQUAL( "global"            , bl::str::getBeginning( text, 6 ) );
    UTF_CHECK_EQUAL( "global file"       , bl::str::getBeginning( text, 11 ) );
    UTF_CHECK_EQUAL( "global file syste" , bl::str::getBeginning( text, 17 ) );
    UTF_CHECK_EQUAL( "global file system", bl::str::getBeginning( text, 18 ) );

    UTF_CHECK_EQUAL( "global file system", bl::str::getBeginning( text, 19 ) );
    UTF_CHECK_EQUAL( "global file system", bl::str::getBeginning( text, 20 ) );
}

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsGetEnding )
{
    const std::string text = "global file system";

    UTF_CHECK_EQUAL( ""                  , bl::str::getEnding( text, 0 ) );
    UTF_CHECK_EQUAL( "m"                 , bl::str::getEnding( text, 1 ) );
    UTF_CHECK_EQUAL( "system"            , bl::str::getEnding( text, 6 ) );
    UTF_CHECK_EQUAL( "file system"       , bl::str::getEnding( text, 11 ) );
    UTF_CHECK_EQUAL( "lobal file system" , bl::str::getEnding( text, 17 ) );
    UTF_CHECK_EQUAL( "global file system", bl::str::getEnding( text, 18 ) );

    UTF_CHECK_EQUAL( "global file system", bl::str::getEnding( text, 19 ) );
    UTF_CHECK_EQUAL( "global file system", bl::str::getEnding( text, 20 ) );
}

namespace
{
    std::string testFormatMessage(
        SAA_in      const char*                                 format,
        SAA_in                                                  ...
        )
    {
        va_list va;
        va_start( va, format );

        auto result = bl::str::formatMessage( format, va );

        va_end( va );

        return result;
    }
}

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsFormatMessageTests )
{
    UTF_CHECK_EQUAL( "", testFormatMessage( "" ) );
    UTF_CHECK_EQUAL( "test", testFormatMessage( "test" ) );

    UTF_CHECK_EQUAL( "test: ok", testFormatMessage( "test: %s", "ok" ) );
    UTF_CHECK_EQUAL( "test: (null)", testFormatMessage( "test: %s", nullptr ) );

    UTF_CHECK_EQUAL( "test:       data, -42, 3.1415\n", testFormatMessage( "test: %10s, %d, %g\n", "data", -42, 3.1415, "extra", 999 ) );

    UTF_CHECK_THROW( testFormatMessage( nullptr ), bl::ArgumentNullException );
    UTF_CHECK_THROW( testFormatMessage( nullptr, 1, 2, 3 ), bl::ArgumentNullException );
}

UTF_AUTO_TEST_CASE( BaseLib_StringUtilsFormatPercentTests )
{
    UTF_CHECK_EQUAL( "25.2%", bl::str::formatPercent( 25.23, 100 ) );
    UTF_CHECK_EQUAL( "0.1%", bl::str::formatPercent( 1, 1000 ) );
    UTF_CHECK_EQUAL( "0%", bl::str::formatPercent( 1, 1000, 0 /* precision */ ) );
    UTF_CHECK_EQUAL( "0.00%", bl::str::formatPercent( 0, 1000, 2 /* precision */ ) );
}

/************************************************************************
 * Utils tests
 */

UTF_AUTO_TEST_CASE( BaseLib_UtilsFindLastTests )
{
    const int array[] = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 2, 1 };

    UTF_CHECK( bl::utils::find_last( std::begin( array ), std::end( array ), 999 ) == std::end( array ) );
    UTF_CHECK( bl::utils::find_last( std::begin( array ), std::end( array ), 0 ) == &array[ 0 ] );
    UTF_CHECK( bl::utils::find_last( std::begin( array ), std::end( array ), 5 ) == &array[ 5 ] );
    UTF_CHECK( bl::utils::find_last( std::begin( array ), std::end( array ), 2 ) == &array[ 9 ] );
    UTF_CHECK( bl::utils::find_last( std::begin( array ), std::end( array ), 1 ) == &array[ 10 ] );
    UTF_CHECK( bl::utils::find_last( std::begin( array ), std::end( array ), INT_MIN ) == std::end( array ) );

    std::vector< std::string > vec;
    vec.push_back( "one" );
    vec.push_back( "two" );
    vec.push_back( "three" );
    vec.push_back( "four" );
    vec.push_back( "five" );
    vec.push_back( "two" );

    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.end(), "forty two" ) == vec.end() );
    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.end(), "one" )  == vec.begin() + 0 );
    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.end(), "four" ) == vec.begin() + 3 );
    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.end(), "five" ) == vec.begin() + 4 );
    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.end(), "two" )  == vec.begin() + 5 );
    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.begin() + 4, "two" ) == vec.begin() + 1 );
    UTF_CHECK( bl::utils::find_last( vec.begin(), vec.end(), "zero" ) == vec.end() );

    std::list< short > empty;

    UTF_REQUIRE( empty.begin() == empty.end() );
    UTF_CHECK( bl::utils::find_last( empty.begin(), empty.end(), 123 ) == empty.end() );
}

/************************************************************************
 * Tagged pool tests
 */

UTF_AUTO_TEST_CASE( BaseLib_TaggedPoolTests )
{
    typedef bl::om::ObjectImpl
        <
            bl::TaggedPool< std::string, bl::om::ObjPtr< bl::data::DataBlock > >
        >
        datablocks_tagged_pool_t;

    const auto pool = datablocks_tagged_pool_t::createInstance();

    auto block = bl::data::DataBlock::createInstance( 16 );

    {
        char* data = reinterpret_cast< char* >( block -> pv() );

        data[ 0 ] = 1;
        data[ 1 ] = 2;
        data[ 2 ] = 3;
        data[ 3 ] = 4;
    }

    const auto key1 = "key1";

    {
        {
            const auto value = pool -> tryGet( key1 );
            UTF_REQUIRE( ! value );
            pool -> put( key1, std::move( block ) );
        }

        {
            const auto value = pool -> tryGet( "key2" );
            UTF_REQUIRE( ! value );
        }

        {
            auto block = pool -> tryGet( key1 );
            UTF_REQUIRE( block );

            const char* data = reinterpret_cast< const char* >( block -> pv() );

            UTF_REQUIRE( data[ 0 ] == 1 );
            UTF_REQUIRE( data[ 1 ] == 2 );
            UTF_REQUIRE( data[ 2 ] == 3 );
            UTF_REQUIRE( data[ 3 ] == 4 );

            const auto value = pool -> tryGet( key1 );
            UTF_REQUIRE( ! value );

            pool -> put( key1, std::move( block ) );
        }

        {
            auto block = bl::data::DataBlock::createInstance( 16 );

            {
                char* data = reinterpret_cast< char* >( block -> pv() );

                data[ 0 ] = 5;
                data[ 1 ] = 6;
                data[ 2 ] = 7;
                data[ 3 ] = 8;
            }

            pool -> put( "key1", std::move( block ) );
        }

        {
            const auto value = pool -> tryGet( "key2" );
            UTF_REQUIRE( ! value );
        }

        {
            for( auto i = 0; i < 2; ++i )
            {
                const auto block = pool -> tryGet( key1 );
                UTF_REQUIRE( block );

                const char* data = reinterpret_cast< const char* >( block -> pv() );

                UTF_REQUIRE( data[ 0 ] == 1 || data[ 0 ] == 5 );

                if( data[ 0 ] == 1 )
                {
                    UTF_REQUIRE( data[ 1 ] == 2 );
                    UTF_REQUIRE( data[ 2 ] == 3 );
                    UTF_REQUIRE( data[ 3 ] == 4 );
                }
                else
                {
                    UTF_REQUIRE( data[ 1 ] == 6 );
                    UTF_REQUIRE( data[ 2 ] == 7 );
                    UTF_REQUIRE( data[ 3 ] == 8 );
                }
            }

            const auto value = pool -> tryGet( key1 );
            UTF_REQUIRE( ! value );

            {
                auto block = bl::data::DataBlock::createInstance( 16 );
                pool -> put( key1, std::move( block ) );
            }

            {
                auto block = bl::data::DataBlock::createInstance( 16 );
                pool -> put( "key2", std::move( block ) );
            }
        }

        {
            UTF_REQUIRE( pool -> tryGet( key1 ) );
            UTF_REQUIRE( pool -> tryGet( "key2" ) );
        }
    }
}

/************************************************************************
 * Utils tests
 */

UTF_AUTO_TEST_CASE( BaseLib_RetryOnErrorTests )
{
    std::size_t successCalled = 0;
    std::size_t failLessCalled = 0;
    std::size_t failMoreCalled = 0;

    const std::size_t retryCount = 4;

    const auto cbTestSuccess = [ & ]() -> void
    {
        ++successCalled;
    };

    const auto cbTestAlwaysFail = [ & ]() -> void
    {
        BL_THROW(
            bl::UnexpectedException(),
            BL_MSG()
                << "This is fail always exception"
            );
    };

    const auto cbTestFailLess = [ & ]() -> void
    {
        ++failLessCalled;

        if( failLessCalled < retryCount )
        {
            BL_THROW(
                bl::UnexpectedException(),
                BL_MSG()
                    << "This is fail less exception"
                );
        }
    };

    const auto cbTestFailMore = [ & ]() -> void
    {
        ++failMoreCalled;

        BL_THROW(
            bl::UnexpectedException(),
            BL_MSG()
                << "This is fail more exception"
            );
    };

    auto startTime = bl::time::microsec_clock::universal_time();

    UTF_REQUIRE_EQUAL( successCalled, 0U );
    bl::utils::retryOnError< bl::UnexpectedException >( cbTestSuccess, retryCount );
    UTF_REQUIRE_EQUAL( successCalled, 1U );

    auto elapsedTime = bl::time::microsec_clock::universal_time() - startTime;
    UTF_REQUIRE( elapsedTime < bl::time::seconds( 3 ) );

    startTime = bl::time::microsec_clock::universal_time();

    try
    {
        bl::utils::retryOnError< bl::UnexpectedException >(
            cbTestAlwaysFail,
            5U /* retryCount */,
            bl::time::milliseconds( 500 ) /* retryTimeout */
            );

        UTF_FAIL( "Exception must be thrown" );
    }
    catch( bl::UnexpectedException& )
    {
    }

    elapsedTime = bl::time::microsec_clock::universal_time() - startTime;
    UTF_REQUIRE( elapsedTime > bl::time::seconds( 2 ) );

    UTF_REQUIRE_EQUAL( successCalled, 1U );
    bl::utils::retryOnAllErrors( cbTestSuccess, retryCount, bl::time::seconds( 2 ) );
    UTF_REQUIRE_EQUAL( successCalled, 2U );

    UTF_REQUIRE_EQUAL( failLessCalled, 0U );
    bl::utils::retryOnError< bl::UnexpectedException >( cbTestFailLess, retryCount );
    UTF_REQUIRE_EQUAL( failLessCalled, retryCount );

    try
    {
        failLessCalled = 0U;
        bl::utils::retryOnError< bl::SystemException >( cbTestFailLess, retryCount );
        UTF_FAIL( "Exception must be thrown" );
    }
    catch( bl::UnexpectedException& e )
    {
        UTF_REQUIRE_EQUAL( e.what(), "This is fail less exception" );
        UTF_REQUIRE_EQUAL( failLessCalled, 1U );
    }

    try
    {
        UTF_REQUIRE_EQUAL( failMoreCalled, 0U );
        bl::utils::retryOnError< bl::UnexpectedException >( cbTestFailMore, retryCount );
        UTF_FAIL( "Exception must be thrown" );
    }
    catch( bl::UnexpectedException& e )
    {
        UTF_REQUIRE_EQUAL( e.what(), "This is fail more exception" );

        /*
         * The expected count is the normal call + the retry count
         */

        const std::size_t expectedCalls = retryCount + 1;

        UTF_REQUIRE_EQUAL( failMoreCalled, expectedCalls );
    }

    try
    {
        failMoreCalled = 0U;
        bl::utils::retryOnError< bl::SystemException >( cbTestFailMore, retryCount );
        UTF_FAIL( "Exception must be thrown" );
    }
    catch( bl::UnexpectedException& e )
    {
        UTF_REQUIRE_EQUAL( e.what(), "This is fail more exception" );
        UTF_REQUIRE_EQUAL( failMoreCalled, 1U );
    }
}

/************************************************************************
 * UniqueHandle tests
 */

UTF_AUTO_TEST_CASE( BaseLib_UniqueHandleTests )
{
    static int g_counter = 0U;
    static int g_outstanding = 0U;
    static std::map< int, bl::cpp::ScalarTypeIniter< std::size_t > > g_handleTable;

    class MyFakeResourceImpl
    {
    public:

        /*
         * Helpers to create and destroy fake resources
         */

        static int create()
        {
            ++g_counter;

            auto& refs = g_handleTable[ g_counter ];
            ++refs.lvalue();
            ++g_outstanding;
            return g_counter;
        }

        static int dup( SAA_in const int handle )
        {
            auto& refs = g_handleTable[ handle ];
            UTF_REQUIRE( refs );
            ++refs.lvalue();
            ++g_outstanding;
            return handle;
        }

        static int createFailed()
        {
            return get_null();
        }

        static void destroy( SAA_in const int handle )
        {
            auto& refs = g_handleTable[ handle ];
            UTF_REQUIRE( refs );
            --refs.lvalue();
            --g_outstanding;
        }

        static int get_null() NOEXCEPT
        {
            return -1;
        }
    };

    class MyDeleter
    {
    public:

        void operator()( SAA_in const int handle ) NOEXCEPT
        {
            MyFakeResourceImpl::destroy( handle );
        }

        int get_null() const NOEXCEPT
        {
            return MyFakeResourceImpl::get_null();
        }
    };

    typedef bl::cpp::UniqueHandle< int, MyDeleter > my_resource_ref;

    my_resource_ref nil;
    UTF_REQUIRE( nil == MyFakeResourceImpl::get_null() );

    if( nil )
    {
        UTF_FAIL( "Must be false" );
    }

    {
        my_resource_ref v1( MyFakeResourceImpl::create() );
        UTF_REQUIRE( v1 != MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 1U == g_outstanding );

        if( ! v1 )
        {
            UTF_FAIL( "Must be true" );
        }

        int i1 = v1.get();
        UTF_REQUIRE( i1 );
        UTF_REQUIRE( i1 == v1.get() );

        my_resource_ref v2( MyFakeResourceImpl::create() );
        UTF_REQUIRE( v2 != MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 2U == g_outstanding );

        auto v3 = my_resource_ref::attach( MyFakeResourceImpl::dup( v1.get() ) );
        UTF_REQUIRE( v3 != MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 3U == g_outstanding );

        const my_resource_ref v4( MyFakeResourceImpl::createFailed() );
        UTF_REQUIRE( v4 == MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 3U == g_outstanding );

        UTF_REQUIRE( v2.get() );
        const auto rawHandle = v2.release();
        UTF_REQUIRE( v2 == MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( rawHandle != MyFakeResourceImpl::get_null() );
        MyFakeResourceImpl::destroy( rawHandle );
        UTF_REQUIRE( 2U == g_outstanding );

        UTF_REQUIRE( v3.get() );
        v3.reset();
        UTF_REQUIRE( v3 == MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 1U == g_outstanding );
    }

    UTF_REQUIRE( 0U == g_outstanding );

    {
        my_resource_ref v1;
        UTF_REQUIRE( 0U == g_outstanding );
        v1 = MyFakeResourceImpl::create();
        UTF_REQUIRE( 1U == g_outstanding );
        UTF_REQUIRE( v1 != MyFakeResourceImpl::get_null() );

        my_resource_ref v2;
        v2 = MyFakeResourceImpl::create();
        UTF_REQUIRE( v2.get() != MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 2U == g_outstanding );

        my_resource_ref v3( std::move( v1 ) );
        UTF_REQUIRE( v3 != MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( v1 == MyFakeResourceImpl::get_null() );
        UTF_REQUIRE( 2U == g_outstanding );

        my_resource_ref v4;
        v4 = std::move( v3 );
        UTF_REQUIRE( MyFakeResourceImpl::get_null() != v4 );
        UTF_REQUIRE( MyFakeResourceImpl::get_null() == v3 );
        UTF_REQUIRE( v3 == v1 );
        UTF_REQUIRE( v4 != v1 );
        UTF_REQUIRE( 2U == g_outstanding );

        my_resource_ref v5( MyFakeResourceImpl::create() );
        v4 = std::move( v5 );

        v4.reset();
        UTF_REQUIRE( 1U == g_outstanding );

        v4.reset( MyFakeResourceImpl::create() );
        UTF_REQUIRE( 2U == g_outstanding );
    }

    UTF_REQUIRE( 0U == g_outstanding );

    for( const auto& pair : g_handleTable )
    {
        UTF_REQUIRE( 0U == pair.second.value() );
    }
}

/************************************************************************
 * Long file names LFN UNC path test
 */

UTF_AUTO_TEST_CASE( BaseLib_LfnPrefixesTests )
{
    if( ! bl::os::onWindows() )
    {
        /*
         * This is Windows only test
         */

        return;
    }

    std::string pathStrNormal( "c:\\" );
    std::string pathStrUnc( "\\\\foo\\bar" );

    bl::fs::path pathNormal( pathStrNormal );
    bl::fs::path pathUnc( pathStrUnc );

    UTF_REQUIRE_EQUAL( bl::fs::path( pathNormal ), pathNormal );
    UTF_REQUIRE_EQUAL( bl::fs::path( pathUnc ), pathUnc );

    const auto pathStrNormalPrefixed = pathNormal.string();
    const auto pathStrUncPrefixed = pathUnc.string();

    const std::string lfnRoot = bl::fs::detail::WinLfnUtils::g_lfnRootPath.string();
    const std::string lfnUncRoot = bl::fs::detail::WinLfnUtils::g_lfnUncRootPath.string();

    UTF_REQUIRE( 0 == pathStrNormalPrefixed.find( lfnRoot ) );
    UTF_REQUIRE( 0 == pathStrUncPrefixed.find( lfnUncRoot ) );

    {
        const auto pathStrNormalReconstructed = pathStrNormalPrefixed.substr( lfnRoot.size() );
        UTF_REQUIRE_EQUAL( pathStrNormal, pathStrNormalReconstructed );

        const auto pathStrUncReconstructed = "\\\\" + pathStrUncPrefixed.substr( lfnUncRoot.size() );
        UTF_REQUIRE_EQUAL( pathStrUnc, pathStrUncReconstructed );
    }

    {
        const auto pathStrNormalReconstructed =
            bl::fs::detail::WinLfnUtils::chk2RemovePrefix( bl::cpp::copy( pathNormal ) ).string();
        UTF_REQUIRE_EQUAL( pathStrNormal, pathStrNormalReconstructed );

        const auto pathStrUncReconstructed =
            bl::fs::detail::WinLfnUtils::chk2RemovePrefix( bl::cpp::copy( pathUnc ) ).string();
        UTF_REQUIRE_EQUAL( pathStrUnc, pathStrUncReconstructed );
    }

    {
        const auto pathStrNormalReconstructed =
            bl::fs::detail::WinLfnUtils::chk2RemovePrefix(
                bl::fs::detail::WinLfnUtils::chk2RemovePrefix( bl::cpp::copy( pathNormal ) )
                ).string();
        UTF_REQUIRE_EQUAL( pathStrNormal, pathStrNormalReconstructed );

        const auto pathStrUncReconstructed =
            bl::fs::detail::WinLfnUtils::chk2RemovePrefix(
                bl::fs::detail::WinLfnUtils::chk2RemovePrefix( bl::cpp::copy( pathUnc ) )
                ).string();
        UTF_REQUIRE_EQUAL( pathStrUnc, pathStrUncReconstructed );
    }
}

UTF_AUTO_TEST_CASE( BaseLib_LfnUNCPathTests )
{
    if( ! bl::os::onWindows() )
    {
        /*
         * This is Windows only test
         */

        return;
    }

    if( test::UtfArgsParser::path().empty() )
    {
        /*
         * This test requires command line parameter to
         * verify manually UNC paths work
         */

        return;
    }

    bl::fs::path rootDir( test::UtfArgsParser::path() );

    for( bl::fs::directory_iterator end, it( rootDir ) ; it != end; ++it  )
    {
        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "Path "
                << it -> path()
            );
    }
}

/************************************************************************
 * Container helper tests
 */

UTF_AUTO_TEST_CASE( BaseLib_ContainerHelperTests )
{
    const std::string value = "str-1";

    {
        std::unordered_set< std::string > set;

        UTF_REQUIRE( ! bl::cpp::contains( set, value ) );

        set.insert( value );

        UTF_REQUIRE( bl::cpp::contains( set, value ) );
    }

    {
        std::set< std::string > set;

        UTF_REQUIRE( ! bl::cpp::contains( set, value ) );

        set.insert( value );

        UTF_REQUIRE( bl::cpp::contains( set, value ) );
    }

    {
        std::unordered_map< std::string, std::string > map;

        UTF_REQUIRE( ! bl::cpp::contains( map, value ) );

        map[ value ] = value;

        UTF_REQUIRE( bl::cpp::contains( map, value ) );
    }

    {
        std::map< std::string, std::string > map;

        UTF_REQUIRE( ! bl::cpp::contains( map, value ) );

        map[ value ] = value;

        UTF_REQUIRE( bl::cpp::contains( map, value ) );
    }
}

/************************************************************************
 * Base64 encoding / decoding tests
 */

UTF_AUTO_TEST_CASE( BaseLib_Base64Tests )
{
    UTF_MESSAGE( "***************** BaseLib_Base64Tests tests *****************\n" );

    {
        /*
         * Encoding and decoding with padding basic tests
         */

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64EncodeString(
                "Lorem ipsum dolor sit amet, consectetur adipisicing elit,"
                ),
            "TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2ljaW5nIGVsaXQs"
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64DecodeString(
                "TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2ljaW5nIGVsaXQs"
                ),
            "Lorem ipsum dolor sit amet, consectetur adipisicing elit,"
            );

        /*
         * encoding/decoding - ground truth for padding cases
         */

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64EncodeString( "" ),
            ""
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64DecodeString( "" ),
            ""
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64EncodeString( "a" ),
            "YQ=="
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64DecodeString( "YQ==" ),
            "a"
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64EncodeString( "aa" ),
            "YWE="
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64DecodeString( "YWE=" ),
            "aa"
            );

        UTF_CHECK_EQUAL(
            bl::SerializationUtils::base64DecodeString( "TG9y" ),
            "Lor"
            );

        {
            /*
             * Test to ensure we can deal with encoding zeros in std::string
             */

            std::string original( "Lor" );

            original.append( 2U, '\0' );
            UTF_REQUIRE_EQUAL( original.size(), 5U );

            const auto encoded = bl::SerializationUtils::base64EncodeString( original );
            const auto decoded = bl::SerializationUtils::base64DecodeString( encoded );

            UTF_REQUIRE_EQUAL( decoded.size(), original.size() );
            UTF_REQUIRE_EQUAL( decoded[ 3 ], '\0' );
            UTF_REQUIRE_EQUAL( decoded[ 4 ], '\0' );
            UTF_REQUIRE_EQUAL( decoded, original );
        }

        {
            /*
             * Test the encoding and decoding of vector of bytes which also includes zero
             */

            std::vector< unsigned char > original;

            for( std::size_t i = 0U; i < 13; ++i )
            {
                original.push_back( ( unsigned char ) i );
            }

            const auto encoded = bl::SerializationUtils::base64Encode( original.data(), original.size() );
            const auto decoded = bl::SerializationUtils::base64DecodeVector( encoded );

            UTF_REQUIRE_EQUAL( decoded.size(), original.size() );

            for( std::size_t i = 0U, count = original.size(); i < count; ++i )
            {
                UTF_REQUIRE_EQUAL( decoded[ i ], original[ i ] );
            }
        }

        {
            /*
             * Encoding and decoding with varying lengths
             */

            std::string input;

            for( std::size_t i = 0; i < 1025; ++i )
            {
                UTF_CHECK_EQUAL(
                    bl::SerializationUtils::base64DecodeString(
                        bl::SerializationUtils::base64EncodeString( input )
                        ),
                        input
                    );

                input += static_cast< char >( i );
            }
        }
    }
}

UTF_AUTO_TEST_CASE( BaseLib_Base64EncodingTests )
{
    const auto currentExecutablePath =
        bl::fs::path( bl::os::getCurrentExecutablePath() );

    bl::fs::TmpDir tmpDir;
    const auto tmpPath = tmpDir.path();
    const auto outputPath = tmpPath / "parentDir" / currentExecutablePath.filename().string();

    std::size_t fileSize;

    const auto encodedString =
        bl::SerializationUtils::encodeFromFileToBase64String( currentExecutablePath, &fileSize );

    UTF_REQUIRE( fileSize > 0U );

    const auto encodedString2 =
        bl::SerializationUtils::encodeFromFileToBase64String( currentExecutablePath );

    UTF_REQUIRE_EQUAL( encodedString2, encodedString );

    UTF_REQUIRE_THROW(
        bl::SerializationUtils::encodeFromFileToBase64String(
            currentExecutablePath,
            &fileSize,
            1L /* maxSize */
            ),
        bl::UnexpectedException
        );

    UTF_REQUIRE( ! encodedString.empty() );

    UTF_REQUIRE_THROW_MESSAGE(
        bl::SerializationUtils::decodeFromBase64StringToFile(
            "invalidBase64String%",
            outputPath
            ),
        std::exception,
        "attempt to decode a value not in base64 char set"
        );

    UTF_REQUIRE( ! fs::path_exists( outputPath ) );
    UTF_REQUIRE( ! fs::path_exists( outputPath.parent_path() ) );

    bl::SerializationUtils::decodeFromBase64StringToFile( encodedString, outputPath );

    UTF_REQUIRE(
        utest::TestFsUtils::compareFileContents(
            currentExecutablePath,
            outputPath,
            true /* ignoreTimestamp */,
            true /* ignoreName */
            )
        );
}

/************************************************************************
 * Base64Url encoding / decoding tests
 */

UTF_AUTO_TEST_CASE( BaseLib_Base64UrlTests )
{
    /*
     * The base64url encoding and decoding APIs are really just small wrappers on top
     * of the base64 encoding APIs which eliminate some characters that are not safe
     * to appear in fine names and URLs such as the padding chars ('=') and the '+'
     * and '/' chars
     *
     * So the tests for these APIs will not duplicate the logic of the base64 core
     * tests above, but we will simply encode / verify and decode / verify a bunch
     * of random buffers
     */

    const std::size_t noOfTestRuns = 1024U;
    const std::size_t maxBufferSize = 1025U;

    std::vector< unsigned char > buffer;
    buffer.resize( maxBufferSize );

    for( std::size_t i = 0U; i < noOfTestRuns; ++i )
    {
        bl::random::uniform_int_distribution< std::size_t > dist( 0U, maxBufferSize );

        const auto bufferSize = dist( bl::tlsData().random.urng() );

        if( bufferSize )
        {
            random::getRandomBytes( buffer.data(), bufferSize );
        }

        /*
         * Test both the string and the vector versions of the APIs
         */

        const auto cbVerifyEncoded = [ & ]( SAA_in const std::string& encoded ) -> void
        {
            if( bufferSize )
            {
                UTF_REQUIRE( encoded.size() );
            }

            UTF_REQUIRE( ! cpp::contains( encoded, '=' ) );
            UTF_REQUIRE( ! cpp::contains( encoded, '+' ) );
            UTF_REQUIRE( ! cpp::contains( encoded, '/' ) );

            {
                const auto decoded = bl::SerializationUtils::base64UrlDecodeVector( encoded );

                UTF_REQUIRE_EQUAL( decoded.size(), bufferSize );

                if( bufferSize )
                {
                    UTF_REQUIRE_EQUAL( 0, ::memcmp( decoded.data(), buffer.data(), bufferSize ) );
                }
            }

            {
                const auto decoded = bl::SerializationUtils::base64UrlDecodeString( encoded );

                UTF_REQUIRE_EQUAL( decoded.size(), bufferSize );

                if( bufferSize )
                {
                    UTF_REQUIRE_EQUAL( 0, ::memcmp( decoded.data(), buffer.data(), bufferSize ) );
                }
            }
        };

        {
            const auto encoded = bl::SerializationUtils::base64UrlEncode( buffer.data(), bufferSize );
            cbVerifyEncoded( encoded );
        }

        {
            const std::string text( buffer.begin(), buffer.begin() + bufferSize );
            const auto encoded = bl::SerializationUtils::base64UrlEncodeString( text );
            cbVerifyEncoded( encoded );
        }
    }
}

/************************************************************************
 * URI encoding / decoding tests
 */

UTF_AUTO_TEST_CASE( BaseLib_URIEncodeDecodeTests )
{
    using namespace bl::str;

    UTF_MESSAGE( "***************** BaseLib_URIEncodeDecodeTests tests *****************\n" );

    {

        /*
         * just an example of the encoding/decoding usage, where no percent
         * encoding is used
         */

        std::string input( "\"Aardvarks lurk, OK? And they lurk in /dev/null!\"" );
        UTF_CHECK_EQUAL( uriDecode( uriEncode( input ) ), input );
    }

    {

        /*
         * safe characters, which need not be encoded
         */

        std::string input( "%41%42%43%44%45%46%47%48%49%4A%4B%4C%4D%4E%4F" );
        std::string output( "ABCDEFGHIJKLMNO" );

        UTF_CHECK_EQUAL( uriDecode( input ), output );

        UTF_CHECK( uriEncode( uriDecode( input ) ) == output || uriEncode( uriDecode( input ) ) == input );
    }

    {

        /*
         * unsafe characters, which must be encoded
         */

        std::string input( "%3A%3B%3C%3D%3E%3F%40" );
        std::string output( ":;<=>?@" );

        UTF_CHECK_EQUAL( uriDecode( input ), output );

        UTF_CHECK_EQUAL( uriEncode( uriDecode( input ) ), input );
    }
}

/************************************************************************
 * Text file encoding / decoding tests
 */

namespace
{
    void TestFileEncoding(
        SAA_in      const bl::fs::path&                     fileName,
        SAA_in      const std::string&                      content,
        SAA_in      const bl::encoding::TextFileEncoding    encoding,
        SAA_in      const std::uint64_t                     fileSize
        )
    {
        using namespace bl::encoding;

        if( bl::os::onUNIX() && ( encoding == TextFileEncoding::Utf16LE || encoding == TextFileEncoding::Utf16LE_NoPreamble ) )
        {
            UTF_REQUIRE_THROW( writeTextFile( fileName, content, encoding ), bl::NotSupportedException );

            return;
        }

        writeTextFile( fileName, content, encoding );

        UTF_CHECK_EQUAL( fileSize, bl::fs::file_size( fileName ) );

        TextFileEncoding resultEncoding;

        const auto resultContent = readTextFile( fileName, &resultEncoding );

        UTF_REQUIRE_EQUAL( resultEncoding, encoding );

        UTF_REQUIRE_EQUAL( resultContent, content );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( BaseLib_TextFilesEncodingTests )
{
    using namespace bl::encoding;

    UTF_MESSAGE( "***************** BaseLib_TextFilesEncodingTests tests *****************\n" );

    bl::fs::TmpDir tmpDir;

    const auto textFile = tmpDir.path() / "test.txt";

    const std::string asciiContent = "Test\n";
    const std::string utf8Content = "\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82\n";
    const std::string emptyContent;

    TestFileEncoding( textFile, asciiContent, TextFileEncoding::Ascii, asciiContent.length() );
    TestFileEncoding( textFile, utf8Content, TextFileEncoding::Ascii, utf8Content.length() );
    TestFileEncoding( textFile, emptyContent, TextFileEncoding::Ascii, emptyContent.length() );

    TestFileEncoding( textFile, asciiContent, TextFileEncoding::Utf8, asciiContent.length() + 3 );
    TestFileEncoding( textFile, utf8Content, TextFileEncoding::Utf8, utf8Content.length() + 3 );
    TestFileEncoding( textFile, emptyContent, TextFileEncoding::Utf8, emptyContent.length() + 3 );

    TestFileEncoding( textFile, asciiContent, TextFileEncoding::Utf16LE, asciiContent.length() * 2 + 2 );
    TestFileEncoding( textFile, utf8Content, TextFileEncoding::Utf16LE, 12 );
    TestFileEncoding( textFile, emptyContent, TextFileEncoding::Utf16LE, 2 );
}

/************************************************************************
 * Some perf tests for unordered_map rehash function
 */

UTF_AUTO_TEST_CASE( BaseLib_UnorderedMapTests )
{
    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    const std::size_t noOfItems = 5000U * 1000U;

    typedef std::unordered_map< bl::uuid_t, std::size_t > map_t;

    map_t map;

    BL_LOG(
        bl::Logging::info(),
        BL_MSG()
            << "by default map has max load factor of "
            << map.max_load_factor()
        );

    map.max_load_factor( 0.5 );

    {
        bl::utils::ExecutionTimer timer( "unordered_map test no pre-allocate" );

        for( std::size_t i = 0; i < noOfItems; ++i )
        {
            map[ bl::uuids::create() ] = i;
        }

        BL_LOG(
            bl::Logging::info(),
            BL_MSG()
                << "map has "
                << map.bucket_count()
                << " buckets and load factor "
                << map.load_factor()
            );
    }

    {
        bl::utils::ExecutionTimer timer( "unordered_map test pre-allocated (0.5 load factor)" );

        map.rehash( noOfItems * 2U );

        for( std::size_t i = 0; i < noOfItems; ++i )
        {
            map[ bl::uuids::create() ] = i;
        }

        BL_LOG(
            bl::Logging::info(),
            BL_MSG()
                << "map has "
                << map.bucket_count()
                << " buckets and load factor "
                << map.load_factor()
            );
    }

    {
        bl::utils::ExecutionTimer timer( "unordered_map rehash only test" );

        map.rehash( 0 );

        BL_LOG(
            bl::Logging::info(),
            BL_MSG()
                << "map has "
                << map.bucket_count()
                << " buckets and load factor "
                << map.load_factor()
            );
    }
}

/************************************************************************
 * NumberUtils Tests
 */

UTF_AUTO_TEST_CASE( BaseLib_FloatingPointEqualTests )
{
    UTF_REQUIRE( bl::numbers::floatingPointEqual( 1.0, 1.0 ) );

    UTF_REQUIRE( ! bl::numbers::floatingPointEqual( 1.0, 1.0 + 2 * bl::numbers::getDefaultEpsilon< double >() ) );
    UTF_REQUIRE( ! bl::numbers::floatingPointEqual( 1.0f, 1.0f + 2 * bl::numbers::getDefaultEpsilon< float >() ) );

    UTF_REQUIRE( ! bl::numbers::floatingPointEqual( 1.0 + 2 * bl::numbers::getDefaultEpsilon< double >(), 1.0 ) );
    UTF_REQUIRE( ! bl::numbers::floatingPointEqual( 1.0f + 2 * bl::numbers::getDefaultEpsilon< float >(), 1.0f ) );
}

/************************************************************************
 * os::< get user domain > tests
 */

namespace
{
    bool isLocalUserOnWindows()
    {
        const auto dnsDomain = bl::os::tryGetEnvironmentVariable( "USERDNSDOMAIN" );

        if( ! dnsDomain )
        {
            const auto computerName = bl::os::tryGetEnvironmentVariable( "COMPUTERNAME" );

            const auto userDomain = bl::os::tryGetEnvironmentVariable( "USERDNSDOMAIN" );

            if(
                ! userDomain || userDomain -> empty() ||
                ( computerName && ( *computerName == *userDomain ) )
                )
            {
                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "Federated login not possible for local users."
                    );

                return true;
            }
        }

        return false;
    }
}

UTF_AUTO_TEST_CASE( BaseLib_GetUserDomainTests )
{
    const auto onWindows = bl::os::onWindows();

    if( onWindows )
    {
        const auto domain = bl::os::tryGetUserDomain();

        if( isLocalUserOnWindows() )
        {
            UTF_REQUIRE( domain.empty() );

            UTF_REQUIRE_THROW_MESSAGE(
                bl::os::getUserDomain(),
                bl::NotSupportedException,
                "User domain name is not available"
                );
        }
        else
        {
            UTF_REQUIRE( ! domain.empty() );

            const auto domainCopy = bl::os::getUserDomain();

            UTF_REQUIRE_EQUAL( domain, domainCopy );
        }
    }
    else
    {
        UTF_REQUIRE_THROW_MESSAGE(
            bl::os::tryGetUserDomain(),
            bl::NotSupportedException,
            "tryGetUserDomain() is not implemented on Linux"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            bl::os::getUserDomain(),
            bl::NotSupportedException,
            "tryGetUserDomain() is not implemented on Linux"
            );
    }
}

/************************************************************************
 * os::< get SPNEGO token > tests
 */

UTF_AUTO_TEST_CASE( BaseLib_GetSPNEGOTokenTests )
{
    const auto server = "testserver.com";

    if( bl::os::onWindows() )
    {
        if( isLocalUserOnWindows() )
        {
            /*
             * This test cannot run if the user is a local user on Windows
             */

            return;
        }

        const auto domain = bl::os::getUserDomain();

        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "SPNEGO token will be requested for server '"
                << server
                << "' and domain '"
                << domain
                << "'"
            );

        const auto tokenBuffer = bl::os::getSPNEGOtoken( server, domain );

        UTF_REQUIRE( 0U != tokenBuffer.size() );

        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "SPNEGO token was created with size "
                << tokenBuffer.size()
            );

        BL_LOG_MULTILINE(
            bl::Logging::debug(),
            BL_MSG()
                << "SPNEGO token base64 encoded value is:\n"
                << bl::SerializationUtils::base64EncodeString( tokenBuffer )
            );
    }
    else
    {
        /*
         * Pass "fakedomain" so the exception is thrown by getSPNEGOtoken
         * but not by getUserDomain
         */

        UTF_REQUIRE_THROW_MESSAGE(
            bl::os::getSPNEGOtoken( server, "fakedomain" ),
            bl::NotSupportedException,
            "getSPNEGOtoken() is not implemented on Linux"
            );
    }
}

/************************************************************************
 * Random utils tests
 */

UTF_AUTO_TEST_CASE( BaseLib_RandomTests )
{
    const std::size_t maxValue = 1000U;

    const auto r1 = bl::random::getUniformRandomUnsignedValue< std::size_t >( maxValue );
    const auto r2 = bl::random::getUniformRandomUnsignedValue< std::size_t >( maxValue );
    const auto r3 = bl::random::getUniformRandomUnsignedValue< std::size_t >( maxValue );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Random values: "
            << r1
            << " "
            << r2
            << " "
            << r3
        );

    std::size_t count[ 2 ] = { 0 };

    for( std::size_t i = 0; i < 1000; ++i )
    {
        const auto r = bl::random::getUniformRandomUnsignedValue< std::size_t >( 1 );

        switch( r )
        {
            case 0:
            case 1:
                ++count[ r ];
                break;

            default:
                UTF_FAIL( std::to_string( r ) + " value different then 0 or 1 generated" );
        }
    }

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "1000 generations for [0, 1] interval hits for 0: "
            << count[ 0 ]
            << "; hits for 1: "
            << count[ 1 ]
        );

    /*
     * Expected after 1000 iterations the two possble values to be generated at least once
     */

    UTF_REQUIRE( count[ 0 ] );
    UTF_REQUIRE( count[ 1 ] );

    unsigned char buffer[ 13 ];
    ::memset( buffer, 0, BL_ARRAY_SIZE( buffer ) );

    bl::random::getRandomBytes( buffer, BL_ARRAY_SIZE( buffer ) );

    auto message = BL_MSG();
    message << "Random buffer values:";

    std::size_t sum = 0U;

    for( std::size_t i = 0; i < BL_ARRAY_SIZE( buffer ); ++i )
    {
        sum += buffer[ i ];
        message << " " << ( unsigned int ) buffer[ i ];
    }

    UTF_REQUIRE( sum != 0U );

    BL_LOG(
        Logging::debug(),
        message
        );
}

/************************************************************************
 * Test HTTP globals
 */

UTF_AUTO_TEST_CASE( BaseLib_HttpGlobalsTests )
{
    {
        const auto& statuses = bl::http::Parameters::emptyStatuses();
        UTF_REQUIRE( statuses.empty() );
    }

    {
        const auto& statuses = bl::http::Parameters::conflictStatuses();
        UTF_REQUIRE( ! statuses.empty() );
        UTF_REQUIRE_EQUAL( 1U, statuses.size() );
        UTF_REQUIRE( bl::cpp::contains( statuses, bl::http::Parameters::HTTP_CLIENT_ERROR_CONFLICT ) );
    }

    {
        const auto& statuses = bl::http::Parameters::unauthorizedStatuses();
        UTF_REQUIRE( ! statuses.empty() );
        UTF_REQUIRE_EQUAL( 1U, statuses.size() );
        UTF_REQUIRE( bl::cpp::contains( statuses, bl::http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED ) );
    }

    {
        const auto& statuses = bl::http::Parameters::redirectStatuses();
        UTF_REQUIRE( ! statuses.empty() );
        UTF_REQUIRE_EQUAL( 1U, statuses.size() );
        UTF_REQUIRE( bl::cpp::contains( statuses, bl::http::Parameters::HTTP_REDIRECT_TEMPORARILY ) );
    }

    {
        const auto& statuses = bl::http::Parameters::securityDefaultStatuses();
        UTF_REQUIRE( ! statuses.empty() );
        UTF_REQUIRE_EQUAL( 2U, statuses.size() );
        UTF_REQUIRE( bl::cpp::contains( statuses, bl::http::Parameters::HTTP_CLIENT_ERROR_UNAUTHORIZED ) );
        UTF_REQUIRE( bl::cpp::contains( statuses, bl::http::Parameters::HTTP_REDIRECT_TEMPORARILY ) );
    }
}

/************************************************************************
 * SafeStringStream tests
 */

namespace
{
    template
    <
        typename T
    >
    class BadAllocator : public std::allocator< T >
    {
        typedef std::allocator< T >             base;
        typedef typename base::pointer          pointer;
        typedef typename base::size_type        size_type;

    public:

        pointer allocate( size_type )
        {
            throw std::bad_alloc();
        }

        pointer allocate(
            size_type,
            const void*
            )
        {
            throw std::bad_alloc();
        }
    };

    typedef std::basic_istringstream< char, std::char_traits< char >, BadAllocator< char > > BadInputStringStream;
    typedef std::basic_ostringstream< char, std::char_traits< char >, BadAllocator< char > > BadOutputStringStream;
}

UTF_AUTO_TEST_CASE( BaseLib_SafeStringStreamTests )
{
    if( bl::os::onWindows() )
    {
        UTF_REQUIRE_EXCEPTION(
            BadInputStringStream stream( "abc" ),
            std::bad_alloc,
            utest::TestUtils::logExceptionDetails
          );
        UTF_REQUIRE_EXCEPTION(
            BadOutputStringStream stream( "abc" ),
            std::bad_alloc,
            utest::TestUtils::logExceptionDetails
            );
    }

    /*
     * TODO: due to a regression bug in GCC [5/6] std::ios_base::failure can't be caught
     * when thrown from the standard library as it is thrown with the old ABI signature
     *
     * For more details see the following links:
     *
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66145
     * http://stackoverflow.com/questions/38471518/how-to-get-io-error-messages-when-creating-a-file-in-c
     *
     * Apparently this bug will be fixed in GCC 7, but for now as a workaround we must
     * catch it as std::exception
     */

    {
        bl::cpp::SafeInputStringStream stream;
        std::ios& istream = stream;
        UTF_REQUIRE_EXCEPTION(
            istream.rdbuf( nullptr ),
            std::exception /* TODO: must be std::ios_base::failure - see comment above */,
            utest::TestUtils::logExceptionDetails
            );
        UTF_CHECK( stream.rdstate() == std::ios_base::badbit );
    }

    {
        bl::cpp::SafeOutputStringStream stream;
        UTF_REQUIRE_EXCEPTION(
            stream << static_cast< std::streambuf* >( nullptr ),
            std::exception /* TODO: must be std::ios_base::failure - see comment above */,
            utest::TestUtils::logExceptionDetails
            );
        UTF_CHECK( stream.rdstate() == std::ios_base::badbit );
    }
}

/************************************************************************
 * FsUtils_SafeFileStreamWrapper tests
 */

UTF_AUTO_TEST_CASE( FsUtils_SafeFileStreamWrapperTests )
{
    bl::fs::TmpDir tmpDir;
    const auto& tmpPath = tmpDir.path();

    {
        const auto noSuchFilePath = tmpPath / "no-such-file";
        UTF_REQUIRE_EXCEPTION(
            bl::fs::SafeInputFileStreamWrapper inputFile( noSuchFilePath ),
            bl::SystemException,
            utest::TestUtils::logExceptionDetails
            );
    }

    /*
     * TODO: due to a regression bug in GCC [5/6] std::ios_base::failure can't be caught
     * when thrown from the standard library as it is thrown with the old ABI signature
     *
     * For more details see the following links:
     *
     * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66145
     * http://stackoverflow.com/questions/38471518/how-to-get-io-error-messages-when-creating-a-file-in-c
     *
     * Apparently this bug will be fixed in GCC 7, but for now as a workaround we must
     * catch it as std::exception
     */

    {
        const auto filePath = tmpPath / "file-for-input-exception-test";
        bl::fs::SafeOutputFileStreamWrapper outputFile( filePath );

        bl::fs::SafeInputFileStreamWrapper inputFile( filePath );
        auto& is = inputFile.stream();
        UTF_REQUIRE_EXCEPTION(
            is.rdbuf( nullptr ),
            std::exception /* TODO: must be std::ios_base::failure - see comment above */,
            utest::TestUtils::logExceptionDetails
            );
        UTF_CHECK( is.rdstate() == std::ios_base::badbit );
    }

    {
        const auto filePath = tmpPath / "file-for-output-exception-test";
        bl::fs::SafeOutputFileStreamWrapper outputFile( filePath );
        auto& os = outputFile.stream();
        UTF_REQUIRE_EXCEPTION(
            os << static_cast< std::streambuf* >( nullptr ),
            std::exception /* TODO: must be std::ios_base::failure - see comment above */,
            utest::TestUtils::logExceptionDetails
            );
        UTF_CHECK( os.rdstate() == std::ios_base::badbit );
    }

    {
        const int N = 1000;
        const auto filePath = tmpPath / "read-write.txt";
        {
            bl::fs::SafeOutputFileStreamWrapper outputFile( filePath );
            auto& os = outputFile.stream();
            for( int i = 0; i < N; ++i )
            {
                os << "Line " << i << '\n';
            }
        }

        {
            bl::fs::SafeInputFileStreamWrapper inputFile( filePath );
            auto& is = inputFile.stream();
            int i = 0;
            std::string line;
            while( std::getline( is, line) )
            {
                UTF_REQUIRE( line == "Line " + std::to_string( i ) );
                ++i;
            }
            UTF_REQUIRE ( i == N );
        }

        {
            bl::fs::SafeOutputFileStreamWrapper outputFile( filePath, bl::fs::SafeOutputFileStreamWrapper::OpenMode::APPEND );
            auto& os = outputFile.stream();
            for( int i = N; i < 2*N; ++i )
            {
                os << "Line " << i << '\n';
            }
        }

        {
            bl::fs::SafeInputFileStreamWrapper inputFile( filePath );
            auto& is = inputFile.stream();
            int i = 0;
            std::string line;
            while( std::getline( is, line) )
            {
                UTF_REQUIRE( line == "Line " + std::to_string( i ) );
                ++i;
            }
            UTF_REQUIRE ( i == 2*N );
        }
    }
}

/************************************************************************
 * BaseLib_copyDirectoryPermissions tests
 */

UTF_AUTO_TEST_CASE( BaseLib_copyDirectoryPermissions_Tests )
{
    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    bl::fs::path srcDirPath = test::UtfArgsParser::path();

    bl::fs::path tgtDirPath = test::UtfArgsParser::outputPath();

    bl::fs::safeMkdirs( tgtDirPath );

    bl::os::copyDirectoryPermissions( srcDirPath, tgtDirPath );
}

/************************************************************************
 * groupBy tests
 */

UTF_AUTO_TEST_CASE( BaseLib_GroupByTest )
{
    std::vector< int > v;
    v.push_back( 1 );
    v.push_back( 2 );
    v.push_back( 2 );
    v.push_back( 3 );
    v.push_back( 3 );
    v.push_back( 3 );
    v.push_back( 4 );
    v.push_back( 4 );
    v.push_back( 4 );
    v.push_back( 4 );

    std::map< double, std::vector< int > > expected;
    expected[ 1.0 ].push_back( 1 );
    expected[ 4.0 ].push_back( 2 );
    expected[ 4.0 ].push_back( 2 );
    expected[ 9.0 ].push_back( 3 );
    expected[ 9.0 ].push_back( 3 );
    expected[ 9.0 ].push_back( 3 );
    expected[ 16.0 ].push_back( 4 );
    expected[ 16.0 ].push_back( 4 );
    expected[ 16.0 ].push_back( 4 );
    expected[ 16.0 ].push_back( 4 );

    std::function< double ( SAA_in int ) > square = []( SAA_in int const i )
    {
        return 1.0 * i * i;
    };

    const auto actual = groupBy( v.begin(), v.end(), square );

    UTF_CHECK_EQUAL( actual, expected );
}

/************************************************************************
 * Tree tests
 */

UTF_AUTO_TEST_CASE( BaseLib_TreeTests )
{
    typedef bl::Tree< std::string > StringTree;

    StringTree stringTree( "root" );

    UTF_CHECK_EQUAL( stringTree.value(), "root" );
    UTF_CHECK_EQUAL( stringTree.hasChildren(), false );

    auto& ch1 = stringTree.addChild( "child 1" );
    auto& ch2 = stringTree.addChild( "child 2" );

    auto& ch1_1 = ch1.addChild( "grandchild 1-1" );
    auto& ch1_2 = ch1.addChild( "grandchild 1-2" );

    auto& ch1_2_1 = ch1_2.addChild( "grandgrandchild 1-2-1" );

    auto& ch2_1 = ch2.addChild( "grandchild 2-1" );

    UTF_CHECK_EQUAL( stringTree.value(), "root" );
    UTF_CHECK_EQUAL( stringTree.hasChildren(), true );
    UTF_CHECK_EQUAL( stringTree.children().size(), 2U );

    UTF_CHECK_EQUAL( ch1.value(), "child 1" );
    UTF_CHECK_EQUAL( ch1.hasChildren(), true );
    UTF_CHECK_EQUAL( ch1.children().size(), 2U );
    UTF_CHECK_EQUAL( ch2.value(), "child 2" );
    UTF_CHECK_EQUAL( ch2.hasChildren(), true );
    UTF_CHECK_EQUAL( ch2.children().size(), 1U );

    UTF_CHECK_EQUAL( ch1_1.value(), "grandchild 1-1" );
    UTF_CHECK_EQUAL( ch1_1.hasChildren(), false );
    UTF_CHECK_EQUAL( ch1_1.children().size(), 0U );
    UTF_CHECK_EQUAL( ch1_2.value(), "grandchild 1-2" );
    UTF_CHECK_EQUAL( ch1_2.hasChildren(), true );
    UTF_CHECK_EQUAL( ch1_2.children().size(), 1U );
    UTF_CHECK_EQUAL( ch1_2_1.value(), "grandgrandchild 1-2-1" );
    UTF_CHECK_EQUAL( ch1_2_1.hasChildren(), false );
    UTF_CHECK_EQUAL( ch1_2_1.children().size(), 0U );

    UTF_CHECK_EQUAL( ch2_1.value(), "grandchild 2-1" );
    UTF_CHECK_EQUAL( ch2_1.hasChildren(), false );
    UTF_CHECK_EQUAL( ch2_1.children().size(), 0U );

    UTF_CHECK_EQUAL( stringTree.child( 0 ).value(), "child 1" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).children().size(), 2U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 0 ).value(), "grandchild 1-1" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 0 ).children().size(), 0U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 1 ).value(), "grandchild 1-2" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 1 ).children().size(), 1U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 1 ).child( 0 ).value(), "grandgrandchild 1-2-1" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 1 ).child( 0 ).children().size(), 0U );

    UTF_CHECK_EQUAL( stringTree.child( 1 ).value(), "child 2" );
    UTF_CHECK_EQUAL( stringTree.child( 1 ).children().size(), 1U );
    UTF_CHECK_EQUAL( stringTree.child( 1 ).child( 0 ).value(), "grandchild 2-1" );
    UTF_CHECK_EQUAL( stringTree.child( 1 ).child( 0 ).children().size(), 0U );

    UTF_REQUIRE_THROW_MESSAGE( stringTree.child( 2 ).value(), bl::ArgumentException, "index 2 is out of range, max=1" );

    stringTree.childLvalue( 0 ).removeChild( 0 );

    UTF_CHECK_EQUAL( stringTree.child( 0 ).value(), "child 1" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).children().size(), 1U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 0 ).value(), "grandchild 1-2" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 0 ).children().size(), 1U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 0 ).child( 0 ).value(), "grandgrandchild 1-2-1" );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).child( 0 ).child( 0 ).children().size(), 0U );

    UTF_REQUIRE_THROW_MESSAGE( stringTree.removeChild( 3 ), bl::ArgumentException, "index 3 is out of range, max=1" );
    UTF_REQUIRE_THROW_MESSAGE( stringTree.childLvalue( 0 ).childLvalue( 0 ).childLvalue( 0 ).removeChild( 4 ), bl::ArgumentException, "index 4 is out of range, max=-1" );

    stringTree.childLvalue( 1 ).lvalue() = "another child";

    UTF_CHECK_EQUAL( stringTree.children().size(), 2U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).value(), "child 1" );
    UTF_CHECK_EQUAL( stringTree.child( 1 ).value(), "another child" );

    stringTree.removeChild( 0 );

    UTF_CHECK_EQUAL( stringTree.children().size(), 1U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).value(), "another child" );

    auto& ch3 = stringTree.addChild( "yet another child" );

    UTF_CHECK_EQUAL( ch3.value(), "yet another child" );
    UTF_CHECK_EQUAL( ch3.hasChildren(), false );
    UTF_CHECK_EQUAL( ch3.children().size(), 0U );

    UTF_CHECK_EQUAL( stringTree.children().size(), 2U );
    UTF_CHECK_EQUAL( stringTree.child( 0 ).value(), "another child" );
    UTF_CHECK_EQUAL( stringTree.child( 1 ).value(), "yet another child" );

    auto stringTree2 = std::move( stringTree );

    UTF_CHECK( stringTree.value() != "root" );
    UTF_CHECK_EQUAL( stringTree.hasChildren(), false );
    UTF_CHECK_EQUAL( stringTree2.value(), "root" );
    UTF_CHECK_EQUAL( stringTree2.hasChildren(), true );

    StringTree stringTree3( "test" );
    stringTree3 = std::move( stringTree2 );

    UTF_CHECK( stringTree2.value() != "root" );
    UTF_CHECK_EQUAL( stringTree2.hasChildren(), false );
    UTF_CHECK_EQUAL( stringTree3.value(), "root" );
    UTF_CHECK_EQUAL( stringTree3.hasChildren(), true );

    StringTree stringSubTree( "subtree" );
    stringSubTree.addChild( "subtree child 1" );
    stringSubTree.addChild( "subtree child 2" );

    UTF_CHECK_EQUAL( stringSubTree.hasChildren(), true );

    stringTree3.addTree( std::move( stringSubTree ) );

    UTF_CHECK( stringSubTree.value() != "subtree" );
    UTF_CHECK_EQUAL( stringSubTree.hasChildren(), false );

    UTF_CHECK_EQUAL( stringTree3.children().size(), 3U );
    UTF_CHECK_EQUAL( stringTree3.child( 0 ).value(), "another child" );
    UTF_CHECK_EQUAL( stringTree3.child( 1 ).value(), "yet another child" );
    UTF_CHECK_EQUAL( stringTree3.child( 2 ).value(), "subtree" );
    UTF_CHECK_EQUAL( stringTree3.child( 2 ).children().size(), 2U );
    UTF_CHECK_EQUAL( stringTree3.child( 2 ).child( 0 ).value(), "subtree child 1" );
    UTF_CHECK_EQUAL( stringTree3.child( 2 ).child( 1 ).value(), "subtree child 2" );
}

UTF_AUTO_TEST_CASE( BaseLib_TableTests )
{
    /*
     * ------------------------
     * Name  | Age | Title
     * -----------------------
     * Tom   | 25  | Analyst
     * Harry | 30  | Associate
     * -----------------------
     */

    typedef struct
    {
        std::string name;
        std::size_t size;
    } ColumnHeader;

    typedef bl::Table< std::string, ColumnHeader > Table;

    std::vector< ColumnHeader > columnHeaders;

    ColumnHeader column1 = { "Name", 10 };
    ColumnHeader column2 = { "Age", 10 };
    ColumnHeader column3 = { "Title", 10 };

    columnHeaders.emplace_back( column1 );
    columnHeaders.emplace_back( column2 );
    columnHeaders.emplace_back( column3 );

    Table table( columnHeaders );

    UTF_CHECK_EQUAL( table.getColumnCount(), columnHeaders.size() );

    table.addRow( "Tom", "25", "Analyst" );
    table.addRow( "Harry", "30", "Associate" );

    UTF_CHECK_THROW(
        table.addRow( "James", "35", "Associate", "London" ),
        bl::UnexpectedException
        );

    UTF_CHECK( table.getColumnHeaders().at( 0 ).name == "Name" );
    UTF_CHECK( table.getColumnHeaders().at( 1 ).name == "Age" );

    UTF_CHECK_EQUAL( table.getCell( 0, 0 ), "Tom" );
    UTF_CHECK_EQUAL( table.getCell( 1, 1 ), "30" );

    UTF_CHECK_EQUAL( table.getRowCount(), 2U );
    UTF_CHECK_EQUAL( table.getColumnCount(), 3U );
}

UTF_AUTO_TEST_CASE( BaseLib_SortedVectorHelperTests )
{
    using namespace bl;

    typedef cpp::SortedVectorHelper< std::size_t > helper_t;

    const std::size_t maxCount = 20000;

    {
        std::set< std::size_t > s;
        helper_t::vector_t v;
        helper_t helper;

        v.reserve( maxCount );

        {
            utils::ExecutionTimer timer(
                bl::resolveMessage(
                    BL_MSG()
                        << "Inserting "
                        << maxCount
                        << " elements in sorted vector"
                    )
                );

            while( v.size() < maxCount )
            {
                const auto value = random::getUniformRandomUnsignedValue< std::size_t >(
                    std::numeric_limits< std::size_t >::max() /* maxValue */
                    );

                helper.insert( v, value );
            }
        }

        {
            utils::ExecutionTimer timer(
                bl::resolveMessage(
                    BL_MSG()
                        << "Searching "
                        << maxCount
                        << " elements in sorted vector twice"
                    )
                );

            bool ok = true;

            for( std::size_t i = 0; i < maxCount; ++i )
            {
                if( v.cend() == helper.cfind( v, v[ i ] ) )
                {
                    ok = false;
                    break;
                }

                if( v.end() == helper.find( v, v[ i ] ) )
                {
                    ok = false;
                    break;
                }
            }

            UTF_REQUIRE( ok );
        }

        {
            utils::ExecutionTimer timer(
                bl::resolveMessage(
                    BL_MSG()
                        << "Inserting "
                        << maxCount
                        << " elements in a set"
                    )
                );

            while( s.size() < maxCount )
            {
                const auto value = random::getUniformRandomUnsignedValue< std::size_t >(
                    std::numeric_limits< std::size_t >::max() /* maxValue */
                    );

                s.insert( value );
            }
        }

        {
            utils::ExecutionTimer timer(
                bl::resolveMessage(
                    BL_MSG()
                        << "Searching "
                        << maxCount
                        << " elements in a set twice"
                    )
                );

            bool ok = true;

            for( const auto& element : s )
            {
                const auto& constSet = s;

                if( s.cend() == constSet.find( element ) )
                {
                    ok = false;
                    break;
                }

                if( s.end() == s.find( element ) )
                {
                    ok = false;
                    break;
                }
            }

            UTF_REQUIRE( ok );
        }
    }

    {
        std::set< std::size_t > s;
        helper_t::vector_t v;
        helper_t helper;

        v.reserve( maxCount );

        {
            utils::ExecutionTimer timer(
                bl::resolveMessage(
                    BL_MSG()
                        << "Inserting and searching "
                        << maxCount
                        << " elements in both sorted vector & set"
                    )
                );

            while( s.size() < maxCount )
            {
                const auto value = random::getUniformRandomUnsignedValue< std::size_t >(
                    std::numeric_limits< std::size_t >::max() /* maxValue */
                    );

                const auto pair1 = s.insert( value );

                if( pair1.second )
                {
                    /*
                     * It was inserted, so the value was unique
                     *
                     * Verify that it is not in the vector, then verify that insert returns as expected
                     * and then finally verify that the find functions work as expected
                     */

                    UTF_REQUIRE( v.cend() == helper.cfind( v, value ) );

                    UTF_REQUIRE( v.end() == helper.find( v, value ) );

                    UTF_REQUIRE( ! helper.erase( v, value ) );

                    const auto pair2 = helper.insert( v, value );

                    UTF_REQUIRE( pair2.second );
                    UTF_REQUIRE( v.end() != pair2.first );
                    UTF_REQUIRE( value == *pair2.first );

                    {
                        const auto pos = helper.cfind( v, value );

                        UTF_REQUIRE( v.cend() != pos && value == *pos );
                    }

                    {
                        const auto pos = helper.find( v, value );

                        UTF_REQUIRE( v.end() != pos && value == *pos );
                    }
                }
                else
                {
                    /*
                     * It is already inserted - just verify that it is present in the vector too
                     * and then insert returns as expected
                     */

                    {
                        const auto pos = helper.cfind( v, value );

                        UTF_REQUIRE( v.cend() != pos || value == *pos );
                    }

                    {
                        const auto pos = helper.find( v, value );

                        UTF_REQUIRE( v.end() != pos || value == *pos );
                    }

                    const auto pair2 = helper.insert( v, value );

                    UTF_REQUIRE( ! pair2.second && v.end() != pair2.first && value == *pair2.first );
                }
            }

            for( std::size_t i = 0; i < 100; ++i )
            {
                const auto index = random::getUniformRandomUnsignedValue< std::size_t >(
                    v.size() - 1 /* maxValue */
                    );

                const auto value = v[ index ];

                {
                    const auto pos = helper.cfind( v, value );

                    UTF_REQUIRE( v.cend() != pos || value == *pos );
                }

                {
                    const auto pos = helper.find( v, value );

                    UTF_REQUIRE( v.end() != pos || value == *pos );
                }

                UTF_REQUIRE( helper.erase( v, value ) );

                UTF_REQUIRE( v.cend() == helper.cfind( v, value ) );

                UTF_REQUIRE( v.end() == helper.find( v, value ) );

                UTF_REQUIRE( ! helper.erase( v, value ) );
            }
        }
    }
}

UTF_AUTO_TEST_CASE( BaseLib_OSRegistryValueTest )
{
    if( ! bl::os::onWindows() )
    {
        return;
    }

    UTF_CHECK(
        bl::os::tryGetRegistryValue( "Software\\foo", "bar" ) == nullptr
        );

    UTF_CHECK_THROW(
        bl::os::getRegistryValue( "Software\\foo", "bar" ),
        bl::UnexpectedException
        );

    /*
     * Don't run for 32bit process as the below registry entry won't exist
     * in WOW registry hive for 32bit process running on 64bit OS.
     */

    if( sizeof(void*) == 4U )
    {
        return;
    }

    UTF_CHECK(
        bl::os::getRegistryValue(
            "Environment",
            "TEMP",
            true /* currentUser */
            ) != bl::str::empty()
        );
}

/************************************************************************
 * LoggableCounter class tests
 */

UTF_AUTO_TEST_CASE( BaseLib_LoggableCounterTests )
{
    using namespace bl;

    typedef LoggableCounterDefaultImpl::EventId EventId;

    const std::string counterName = "my counter";

    std::size_t updateCount = 0U;
    std::vector< std::size_t > updates;

    cpp::ScalarTypeIniter< bool > created;
    cpp::ScalarTypeIniter< bool > destroyed;

    const auto callback = [ & ](
        SAA_in          const EventId                           eventId,
        SAA_in          const std::size_t                       counterValue
        ) NOEXCEPT
    {
        BL_NOEXCEPT_BEGIN()

        switch( eventId )
        {
            default:
                BL_RIP_MSG( "Unexpected event id value" );
                break;

            case EventId::Create:
                {
                    created = true;

                    UTF_REQUIRE_EQUAL( counterValue, 0U );
                }
                break;

            case EventId::Update:
                {
                    ++updateCount;

                    updates.push_back( counterValue );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Loggable counter '"
                            << counterName
                            << "' was updated; new counter value is "
                            << counterValue
                        );
                }
                break;

            case EventId::Destroy:
                {
                    destroyed = true;

                    UTF_REQUIRE_EQUAL( counterValue, 0U );
                }
                break;
        }

        BL_NOEXCEPT_END()
    };

    {
        const auto counter = LoggableCounterDefaultImpl::createInstance(
            cpp::copy( counterName ),
            10U /* minDeltaToLog */,
            cpp::copy( callback )
            );
    }

    UTF_REQUIRE( created );
    UTF_REQUIRE( destroyed );
    UTF_REQUIRE_EQUAL( updateCount, 0U );
    UTF_REQUIRE_EQUAL( updates.size(), 0U );

    created = false;
    destroyed = false;

    {
        const auto counter = LoggableCounterDefaultImpl::createInstance(
            cpp::copy( counterName ),
            10U /* minDeltaToLog */,
            cpp::copy( callback )
            );

        for( std::size_t i = 0U; i < 25U; ++i )
        {
            counter -> increment();
        }

        for( std::size_t i = 0U; i < 25U; ++i )
        {
            counter -> decrement();
        }
    }

    UTF_REQUIRE( created );
    UTF_REQUIRE( destroyed );
    UTF_REQUIRE_EQUAL( updateCount, 5U );
    UTF_REQUIRE_EQUAL( updates.size(), 5U );

    UTF_REQUIRE_EQUAL( updates[ 0 ], 1U );
    UTF_REQUIRE_EQUAL( updates[ 1 ], 11U );
    UTF_REQUIRE_EQUAL( updates[ 2 ], 21U );
    UTF_REQUIRE_EQUAL( updates[ 3 ], 11U );
    UTF_REQUIRE_EQUAL( updates[ 4 ], 1U );
}

UTF_AUTO_TEST_CASE( BaseLib_CopyDirectoryWithContentTests )
{
    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    bl::fs::path sourcePath = test::UtfArgsParser::path();
    bl::fs::path targetPath = test::UtfArgsParser::outputPath();

    BL_CHK(
        true,
        sourcePath.empty(),
        BL_MSG()
            << "The --path parameter cannot be empty"
        );

    BL_CHK(
        true,
        targetPath.empty(),
        BL_MSG()
            << "The --output-path parameter cannot be empty"
        );

    const auto getDirectoryContents = [ & ]( SAA_in const bl::fs::path& path ) -> std::vector< std::string >
    {
        std::vector< std::string > paths;

        for( bl::fs::recursive_directory_iterator i( path ), end; i != end; ++i )
        {
            paths.emplace_back( i -> path().string() );
        }

        return paths;
    };

    const auto sourceContents = getDirectoryContents( sourcePath );

    UTF_REQUIRE( ! bl::fs::path_exists( targetPath ) );

    bl::fs::copyDirectoryWithContents( sourcePath, targetPath );

    const auto targetContents = getDirectoryContents( targetPath );

    UTF_REQUIRE_EQUAL( sourceContents.size(), targetContents.size() );
}

UTF_AUTO_TEST_CASE( BaseLib_ProfilesDirectoryTests )
{
    const auto profilesDirectory = bl::os::getUsersDirectory();

    UTF_REQUIRE( bl::fs::path_exists( profilesDirectory ) );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Users directory: "
            << profilesDirectory.string()
        );
}
