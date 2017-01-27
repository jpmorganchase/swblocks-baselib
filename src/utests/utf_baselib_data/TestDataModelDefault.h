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

namespace utest
{
    namespace dm
    {
        /*
         * ContainedTestObjectBase
         */

        BL_DM_DEFINE_CLASS_BEGIN( ContainedTestObjectBase )

            BL_DM_DECLARE_STRING_PROPERTY               ( strValue )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( strValue )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( ContainedTestObjectBase )

        BL_DM_DEFINE_PROPERTY( ContainedTestObjectBase, strValue )

        /*
         * ContainedTestObject
         */

        BL_DM_DEFINE_CLASS_BEGIN( ContainedTestObject )

            BL_DM_DECLARE_STRING_PROPERTY               ( strValue )
            BL_DM_DECLARE_BOOL_PROPERTY                 ( boolValue )
            BL_DM_DECLARE_INT_PROPERTY                  ( intValue )
            BL_DM_DECLARE_UINT64_PROPERTY               ( uint64Value )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( strValue )
                BL_DM_IMPL_PROPERTY( boolValue )
                BL_DM_IMPL_PROPERTY( intValue )
                BL_DM_IMPL_PROPERTY( uint64Value )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( ContainedTestObject )

        BL_DM_DEFINE_PROPERTY( ContainedTestObject, strValue )
        BL_DM_DEFINE_PROPERTY( ContainedTestObject, boolValue )
        BL_DM_DEFINE_PROPERTY( ContainedTestObject, intValue )
        BL_DM_DEFINE_PROPERTY( ContainedTestObject, uint64Value )

        /*
         * TestObjectBase
         */

        BL_DM_DEFINE_CLASS_BEGIN( TestObjectBase )

            BL_DM_DECLARE_UINT64_PROPERTY              ( id )
            BL_DM_DECLARE_COMPLEX_PROPERTY             ( complex, ContainedTestObjectBase )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( id )
                BL_DM_IMPL_PROPERTY( complex )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( TestObjectBase )

        BL_DM_DEFINE_PROPERTY( TestObjectBase, id )
        BL_DM_DEFINE_PROPERTY( TestObjectBase, complex )

        /*
         * TestObject
         */

        BL_DM_DEFINE_CLASS_BEGIN( TestObject )

            BL_DM_DECLARE_UINT64_PROPERTY              ( id )
            BL_DM_DECLARE_MAP_PROPERTY                 ( userData, std::string )
            BL_DM_DECLARE_SIMPLE_VECTOR_PROPERTY       ( numbers, int, get_int )
            BL_DM_DECLARE_SIMPLE_VECTOR_PROPERTY       ( strings, std::string, get_str )
            BL_DM_DECLARE_INT_ALTERNATE_PROPERTY       ( number, "json_number" )
            BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY    ( str, "json_str" )
            BL_DM_DECLARE_CUSTOM_PROPERTY              ( custom )
            BL_DM_DECLARE_COMPLEX_PROPERTY             ( complex, ContainedTestObject )
            BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY      ( complexVector, ContainedTestObject )
            BL_DM_DECLARE_COMPLEX_MAP_PROPERTY         ( complexMap, ContainedTestObject )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( id )
                BL_DM_IMPL_PROPERTY( userData )
                BL_DM_IMPL_PROPERTY( numbers )
                BL_DM_IMPL_PROPERTY( strings )
                BL_DM_IMPL_PROPERTY( number )
                BL_DM_IMPL_PROPERTY( str )
                BL_DM_IMPL_PROPERTY( custom )
                BL_DM_IMPL_PROPERTY( complex )
                BL_DM_IMPL_PROPERTY( complexVector )
                BL_DM_IMPL_PROPERTY( complexMap )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( TestObject )

        BL_DM_DEFINE_PROPERTY( TestObject, id )
        BL_DM_DEFINE_PROPERTY( TestObject, userData )
        BL_DM_DEFINE_PROPERTY( TestObject, numbers )
        BL_DM_DEFINE_PROPERTY( TestObject, strings )
        BL_DM_DEFINE_PROPERTY( TestObject, number )
        BL_DM_DEFINE_PROPERTY( TestObject, str )
        BL_DM_DEFINE_PROPERTY( TestObject, custom )
        BL_DM_DEFINE_PROPERTY( TestObject, complex )
        BL_DM_DEFINE_PROPERTY( TestObject, complexVector )
        BL_DM_DEFINE_PROPERTY( TestObject, complexMap )

    } // dm

} // utest

UTF_AUTO_TEST_CASE( CoreDataModelTests )
{
    using namespace bl;
    using namespace bl::dm;
    using namespace utest::dm;

    typedef DataModelUtils dmu;

    const auto printTestObject = []( SAA_in const om::ObjPtr< TestObject >& testObj ) -> void
    {
        BL_LOG_MULTILINE(
            Logging::debug(),
            BL_MSG()
                << "\nTest object JSON:\n"
                << dmu::getDocAsPrettyJsonString( testObj )
            );
    };

    const auto printTestObjectBase = []( SAA_in const om::ObjPtr< TestObjectBase >& baseObj ) -> void
    {
        BL_LOG_MULTILINE(
            Logging::debug(),
            BL_MSG()
                << "\nTest base object JSON:\n"
                << dmu::getDocAsPrettyJsonString( baseObj )
            );
    };

    const auto printPayloadObject = []( SAA_in const om::ObjPtr< dm::Payload >& payloadObj ) -> void
    {
        BL_LOG_MULTILINE(
            Logging::debug(),
            BL_MSG()
                << "\nTest payload object JSON:\n"
                << dmu::getDocAsPrettyJsonString( payloadObj )
            );
    };

    const std::string author( "A test author" );
    const std::string description( "A test description" );
    const std::uint64_t uint64Value = 1000000000ULL * 42;

    const auto verifyTestObject = [ & ]( SAA_in const om::ObjPtr< TestObject >& testObj ) -> void
    {
        UTF_REQUIRE_EQUAL( testObj -> id(), 1234U );

        UTF_REQUIRE_EQUAL( testObj -> userData().size(), 2U );
        UTF_REQUIRE_EQUAL( testObj -> userData().at( "author" ), author );
        UTF_REQUIRE_EQUAL( testObj -> userData().at( "description" ), description );

        UTF_REQUIRE_EQUAL( testObj -> number(), 42 );

        UTF_REQUIRE_EQUAL( testObj -> numbers().size(), 1U );
        UTF_REQUIRE_EQUAL( testObj -> numbers()[ 0 ], 42 );

        UTF_REQUIRE_EQUAL( testObj -> strings().size(), 2U );
        UTF_REQUIRE_EQUAL( testObj -> strings()[ 0 ], "foo" );
        UTF_REQUIRE_EQUAL( testObj -> strings()[ 1 ], "bar" );

        UTF_REQUIRE( ! testObj -> custom().is_null() );
        UTF_REQUIRE_EQUAL( testObj -> custom().get_obj().at( "name" ).get_str(), "value" );

        const auto& complexObj = testObj -> complex();

        UTF_REQUIRE_EQUAL( complexObj -> strValue(), "strValue3" );
        UTF_REQUIRE_EQUAL( complexObj -> boolValue(), true );
        UTF_REQUIRE_EQUAL( complexObj -> intValue(), 420 );
        UTF_REQUIRE_EQUAL( complexObj -> uint64Value(), uint64Value / 2 );

        UTF_REQUIRE_EQUAL( testObj -> complexVector().size(), 2U );
        UTF_REQUIRE_EQUAL( testObj -> complexMap().size(), 2U );

        {
            {
                const auto& obj1 = testObj -> complexVector()[ 0U ];

                UTF_REQUIRE_EQUAL( obj1 -> strValue(), "strValue1" );
                UTF_REQUIRE_EQUAL( obj1 -> boolValue(), true );
                UTF_REQUIRE_EQUAL( obj1 -> intValue(), 42 );
                UTF_REQUIRE_EQUAL( obj1 -> uint64Value(), uint64Value );
            }

            {
                const auto& obj2 = testObj -> complexVector()[ 1U ];

                UTF_REQUIRE_EQUAL( obj2 -> strValue(), "strValue2" );
                UTF_REQUIRE_EQUAL( obj2 -> boolValue(), true );
                UTF_REQUIRE_EQUAL( obj2 -> intValue(), 42 );
                UTF_REQUIRE_EQUAL( obj2 -> uint64Value(), uint64Value );
            }
        }

        {
            {
                const auto& obj1 = testObj -> complexMap().at( "obj1" );

                UTF_REQUIRE_EQUAL( obj1 -> strValue(), "strValue1" );
                UTF_REQUIRE_EQUAL( obj1 -> boolValue(), true );
                UTF_REQUIRE_EQUAL( obj1 -> intValue(), 42 );
                UTF_REQUIRE_EQUAL( obj1 -> uint64Value(), uint64Value );
            }

            {
                const auto& obj2 = testObj -> complexMap().at( "obj2" );

                UTF_REQUIRE_EQUAL( obj2 -> strValue(), "strValue2" );
                UTF_REQUIRE_EQUAL( obj2 -> boolValue(), true );
                UTF_REQUIRE_EQUAL( obj2 -> intValue(), 42 );
                UTF_REQUIRE_EQUAL( obj2 -> uint64Value(), uint64Value );
            }
        }
    };

    const auto populateTestObject = [ & ]( SAA_in const om::ObjPtr< TestObject >& testObj ) -> void
    {
        testObj -> id( 1234U );

        testObj -> userDataLvalue()[ "author" ] = author;
        testObj -> userDataLvalue()[ "description" ] = description;

        testObj -> number( 42 );

        testObj -> numbersLvalue().push_back( 42 );

        testObj -> stringsLvalue().push_back( "foo" );
        testObj -> stringsLvalue().push_back( "bar" );

        testObj -> number( 42 );
        testObj -> str( "string property" );

        UTF_REQUIRE( testObj -> custom().is_null() );
        testObj -> custom( json::readFromString( "{ \"name\": \"value\" }" ) );
        UTF_REQUIRE( ! testObj -> custom().is_null() );

        const auto complexObj = ContainedTestObject::createInstance();

        complexObj -> strValue( "strValue3" );
        complexObj -> boolValue( true );
        complexObj -> intValue( 420 );
        complexObj -> uint64Value( uint64Value / 2 );

        testObj -> complexLvalue() = om::copy( complexObj );

        const auto obj1 = ContainedTestObject::createInstance();
        obj1 -> strValue( "strValue1" );
        obj1 -> boolValue( true );
        obj1 -> intValue( 42 );
        obj1 -> uint64Value( uint64Value );

        const auto obj2 = ContainedTestObject::createInstance();
        obj2 -> strValue( "strValue2" );
        obj2 -> boolValue( true );
        obj2 -> intValue( 42 );
        obj2 -> uint64Value( uint64Value );

        testObj -> complexVectorLvalue().push_back( om::copy( obj1 ) );
        testObj -> complexVectorLvalue().push_back( om::copy( obj2 ) );

        testObj -> complexMapLvalue().emplace( "obj1", om::copy( obj1 ) );
        testObj -> complexMapLvalue().emplace( "obj2", om::copy( obj2 ) );
    };

    auto testObj = TestObject::createInstance();

    printTestObject( testObj );

    populateTestObject( testObj );

    printTestObject( testObj );

    const auto testObjFromFile = DataModelUtils::loadFromFile< TestObject >(
        utest::TestUtils::resolveDataFilePath( "serialized_object.json" )
        );

    printTestObject( testObjFromFile );

    UTF_REQUIRE_EQUAL(
        DataModelUtils::getObjectHashCanonical( testObj ),
        DataModelUtils::getObjectHashCanonical( testObjFromFile )
        );

    UTF_REQUIRE_EQUAL(
        DataModelUtils::getObjectHash( testObj, str::empty() /* salt */, false /* canonicalize */ ),
        DataModelUtils::getObjectHash( testObjFromFile, str::empty() /* salt */, false /* canonicalize */ )
        );

    {
        const auto jsonText = dmu::getDocAsPackedJsonString( testObj );

        testObj = dmu::loadFromJsonText< TestObject >( jsonText );

        verifyTestObject( testObj );

        printTestObject( testObj );
    }

    {
        const auto jsonText = dmu::getDocAsPrettyJsonString( testObj );

        testObj = dmu::loadFromJsonText< TestObject >( jsonText );

        verifyTestObject( testObj );

        printTestObject( testObj );

        /*
         * Verify base object and casting behavior
         */

        const auto baseObj = dmu::loadFromJsonText< TestObjectBase >( jsonText );

        printTestObjectBase( baseObj );

        UTF_REQUIRE_EQUAL( baseObj -> id(), 1234U );
        UTF_REQUIRE( baseObj -> complex() );
        UTF_REQUIRE_EQUAL( baseObj -> complex() -> strValue(), "strValue3" );

        testObj = dmu::castTo< TestObject >( baseObj );

        verifyTestObject( testObj );

        printTestObject( testObj );

        /*
         * Verify base object and casting behavior with the placeholder object
         */

        const auto payloadObj = dmu::loadFromJsonText< Payload >( jsonText );

        printPayloadObject( payloadObj );

        testObj = dmu::castTo< TestObject >( payloadObj );

        verifyTestObject( testObj );

        printTestObject( testObj );
    }

    {
        /*
         * Test the read-only behavior for each property type
         *
            BL_DM_DECLARE_UINT64_PROPERTY              ( id )
            BL_DM_DECLARE_MAP_PROPERTY                 ( userData, std::string )
            BL_DM_DECLARE_SIMPLE_VECTOR_PROPERTY       ( numbers, int, get_int )
            BL_DM_DECLARE_SIMPLE_VECTOR_PROPERTY       ( strings, std::string, get_str )
            BL_DM_DECLARE_INT_ALTERNATE_PROPERTY       ( number, "json_number" )
            BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY    ( str, "json_str" )
            BL_DM_DECLARE_CUSTOM_PROPERTY              ( custom )
            BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY      ( complexVector, ContainedTestObject )
            BL_DM_DECLARE_COMPLEX_MAP_PROPERTY         ( complexMap, ContainedTestObject )
        */

        auto testObj = TestObject::createInstance();

        UTF_REQUIRE( ! testObj -> readOnly() );
        testObj -> readOnly( true );
        UTF_REQUIRE( testObj -> readOnly() );

        /*
         * Test *Lvalue() setters
         */

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> userDataLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> numbersLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> stringsLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> strLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> customLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> complexVectorLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        UTF_REQUIRE_THROW_MESSAGE(
            testObj -> complexMapLvalue(),
            UnexpectedException,
            "Trying to modify a read only object"
            );

        {
            /*
             * Test const (reference) setters
             */

            const auto stringValue = std::string( "foo" );

            UTF_REQUIRE_THROW_MESSAGE(
                testObj -> number( 42 ),
                UnexpectedException,
                "Trying to modify a read only object"
                );

            UTF_REQUIRE_THROW_MESSAGE(
                testObj -> str( stringValue ),
                UnexpectedException,
                "Trying to modify a read only object"
                );
        }

        {
            /*
             * Test move semantics setters
             */

            auto stringValue = std::string( "foo" );
            auto customValue = json::readFromString( "{ \"name\": \"value\" }" );

            UTF_REQUIRE_THROW_MESSAGE(
                testObj -> number( 42 ),
                UnexpectedException,
                "Trying to modify a read only object"
                );

            UTF_REQUIRE_THROW_MESSAGE(
                testObj -> str( std::move( stringValue ) ),
                UnexpectedException,
                "Trying to modify a read only object"
                );

            UTF_REQUIRE_THROW_MESSAGE(
                testObj -> custom( std::move( customValue ) ),
                UnexpectedException,
                "Trying to modify a read only object"
                );
        }

        {
            /*
             * unmapped() behavior tests
             */

            testObj -> readOnly( false );
            UTF_REQUIRE( ! testObj -> readOnly() );

            UTF_REQUIRE_EQUAL( 0U, testObj -> unmapped().size() );

            populateTestObject( testObj );

            printTestObject( testObj );

            UTF_REQUIRE_EQUAL( 0U, testObj -> unmapped().size() );

            {
                const auto jsonText = dmu::getDocAsPrettyJsonString( testObj );

                testObj = dmu::loadFromJsonText< TestObject >( jsonText );

                UTF_REQUIRE_EQUAL( 0U, testObj -> unmapped().size() );
            }

            {
                auto jsonObj = dmu::getJsonObject( testObj );

                jsonObj[ "unmappedName" ] = "unmappedValue";

                testObj = dmu::loadFromJsonObject< TestObject >( std::move( jsonObj ) );

                UTF_REQUIRE_EQUAL( 1U, testObj -> unmapped().size() );

                verifyTestObject( testObj );

                printTestObject( testObj );
            }

            {
                const auto jsonText = dmu::getDocAsPrettyJsonString( testObj );

                testObj = dmu::loadFromJsonText< TestObject >( jsonText );

                UTF_REQUIRE_EQUAL( 1U, testObj -> unmapped().size() );
            }

            {
                const auto jsonObj = dmu::getJsonObject( testObj );

                UTF_REQUIRE_EQUAL( jsonObj.at( "unmappedName" ), std::string( "unmappedValue" ) );

                testObj = dmu::loadFromJsonObject< TestObject >( std::move( jsonObj ) );

                UTF_REQUIRE_EQUAL( 1U, testObj -> unmapped().size() );
            }
        }
    }
}

