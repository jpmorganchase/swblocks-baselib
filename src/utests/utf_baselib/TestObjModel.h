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

#include "examples/objmodel/MyInterfaces.h"
#include "examples/objmodel/MyObjectImpl.h"

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotify.h>
#include <baselib/tasks/TaskBase.h>

#include <baselib/reactive/Observable.h>
#include <baselib/reactive/Observer.h>

#include <baselib/data/FilesystemMetadata.h>

#include <baselib/core/UuidIteratorImpl.h>
#include <baselib/core/UuidIterator.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/OS.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <set>

#include <utests/baselib/Utf.h>

BL_IID_DECLARE( TestInterface1234, "f8c57376-0d16-4161-976c-b97d30874c53" )
BL_IID_DECLARE( TestInterface1235, "43ca7a70-c55e-4b3d-b333-21fbb71be7fd" )

class TestInterface1234 : public bl::om::Object
{
    BL_DECLARE_INTERFACE( TestInterface1234 )

public:

    virtual int    getValue() = 0;
    virtual void   incValue() = 0;
};

class TestInterface1235 : public bl::om::Object
{
    BL_DECLARE_INTERFACE( TestInterface1235 )

public:

    virtual int    getValue2() = 0;
};

template
<
    typename T = void
>
class MyObjSimpleT : public bl::om::Object
{
    BL_DECLARE_OBJECT_IMPL_DEFAULT( MyObjSimpleT )

private:

    int m_value;

protected:

    MyObjSimpleT( SAA_in const int value )
        :
        m_value( value )
    {
    }

public:

    int getTheValue() const NOEXCEPT
    {
        return m_value;
    }
};

typedef bl::om::ObjectImpl< MyObjSimpleT<> > MyObjSimple;

template
<
    typename T = void
>
class MyObjEncapsulatedT : public TestInterface1234
{
    BL_DECLARE_OBJECT_IMPL_ONEIFACE( MyObjEncapsulatedT, TestInterface1234 )

private:

    int m_value1;
    int m_value2;

protected:

    MyObjEncapsulatedT( SAA_in const int value1, SAA_in const int value2 )
        :
        m_value1( value1 ),
        m_value2( value2 )
    {
    }

public:

    virtual int getValue() OVERRIDE
    {
        return m_value1;
    }

    virtual void incValue() OVERRIDE
    {
        m_value1 += m_value2;
    }
};

typedef bl::om::ObjectImpl< MyObjEncapsulatedT<> > MyObjEncapsulated;

UTF_AUTO_TEST_CASE( ObjModel_InterfaceDefinitionsTests )
{
    using namespace bl;
    using namespace bl::data;

    std::set< std::string > ids;

    static_assert(
        sizeof( void * ) == sizeof( om::Object ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::Object::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( om::Disposable ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::Disposable::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( om::SharedPtr ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::SharedPtr::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( om::Factory ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::Factory::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( om::Loader ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::Loader::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( om::Resolver ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::Resolver::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( om::Proxy ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( om::Proxy::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( reactive::Observer ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( reactive::Observer::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( reactive::Observable ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( reactive::Observable::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( tasks::Task ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( tasks::Task::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( tasks::ExecutionQueue ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( tasks::ExecutionQueue::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( tasks::ExecutionQueueNotify ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( tasks::ExecutionQueueNotify::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( UuidIterator ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( UuidIterator::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( FilesystemMetadataRO ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( FilesystemMetadataRO::iid() ) ).second );

    static_assert(
        sizeof( void * ) == sizeof( FilesystemMetadataWO ),
        "An interface definition should be just a vtable"
        );
    UTF_CHECK( ids.insert( uuids::uuid2string( FilesystemMetadataWO::iid() ) ).second );
}

UTF_AUTO_TEST_CASE( ObjModel_BasicTests )
{
    using namespace bl;
    using namespace bl::data;

    bool calledOnZeroRefs = false;

    /*
     * Depending on what the order of execution of tests is there
     * might be some objects pending destruction in the thread pool
     *
     * Wait a little to give them a chance to go
     */

    std::size_t retries = 0U;
    const std::size_t maxRetries = 120U;

    for( ;; )
    {
        if( 0U == bl::om::outstandingObjectRefs() || retries >= maxRetries )
        {
            UTF_REQUIRE_EQUAL( 0L, bl::om::outstandingObjectRefs() );

            break;
        }

        os::sleep( time::seconds( 1L ) );

        ++retries;
    }

    const auto cbOnZeroRefs = [ &calledOnZeroRefs ]() -> void
    {
        calledOnZeroRefs = true;
    };

    om::setOnZeroRefsCallback( cbOnZeroRefs );

    try
    {
        {
            auto o1 = MyObjSimple::createInstance( 42 );
            UTF_CHECK( o1 );
            UTF_CHECK_EQUAL( o1 -> getTheValue(), 42 );

            const auto o2 = MyObjEncapsulated::createInstance( 13, 2 );
            UTF_CHECK( o2 );

            UTF_CHECK_EQUAL( o2 -> getValue(), 13 );
            UTF_CHECK_EQUAL( o2 -> getValue(), 13 );

            o2 -> incValue();
            UTF_CHECK_EQUAL( o2 -> getValue(), 15 );
            UTF_CHECK_EQUAL( o2 -> getValue(), 15 );

            auto o3 = std::move( o1 );
            UTF_CHECK( nullptr != o3 );
            UTF_CHECK( nullptr == o1 );

            om::ObjPtr< MyObjSimple > o4( om::ObjPtr< MyObjSimple >::attach( o3.release() ) );
            UTF_CHECK( nullptr == o3 );
            UTF_CHECK( nullptr != o4 );

            {
                const auto i1 = om::tryQI< TestInterface1234 >( o2 );
                UTF_CHECK( i1 );
                UTF_CHECK_EQUAL( i1 -> getValue(), 15 );
                i1 -> incValue();
                UTF_CHECK_EQUAL( i1 -> getValue(), 17 );
                UTF_CHECK_EQUAL( i1 -> getValue(), 17 );

                auto i2 = om::copy( i1 );
                UTF_CHECK_EQUAL( i1, i2 );

                i2.reset();

                const auto i3 = om::copy( i2 );
                UTF_CHECK( nullptr == i3 );
            }

            {
                const auto i1 = om::tryQI< TestInterface1234 >( o2.get() );
                UTF_CHECK( i1 );
                UTF_CHECK_EQUAL( i1 -> getValue(), 17 );
                i1 -> incValue();
                UTF_CHECK_EQUAL( i1 -> getValue(), 19 );
                UTF_CHECK_EQUAL( i1 -> getValue(), 19 );

                auto i2 = om::copy( i1.get() );
                UTF_CHECK_EQUAL( i1, i2 );

                i2.reset();

                const auto i3 = om::copy( i2.get() );
                UTF_CHECK( nullptr == i3 );
            }

            {
                const auto i1 = om::tryQI< TestInterface1235 >( o2 );
                UTF_CHECK( nullptr == i1.get() );
            }

            {
                try
                {
                    const auto i1 = om::qi< TestInterface1235 >( o2 );
                    UTF_FAIL( "Exception should be thrown" );
                }
                catch( InterfaceNotSupportedException& )
                {
                }
            }

            /*
             * Test areEqual( ... ) APIs
             */

            {
                auto i1 = om::qi< TestInterface1234 >( o2 );
                auto c1 = om::copy( o2 );

                /* both are non-null */
                UTF_CHECK( om::areEqual( o2, i1 ) );
                UTF_CHECK( om::areEqual( c1, i1 ) );

                /* one nullptr the other non-null */
                c1.reset();
                UTF_CHECK( ! om::areEqual( c1, i1 ) );

                /* both are nullptr */
                i1.reset();
                UTF_CHECK( om::areEqual( c1, i1 ) );
            }

            {
                const auto i1 = om::qi< TestInterface1234 >( o2 );
                const auto c1 = om::copy( o2 );

                /*
                 * Test various combinations to invoke the overloads
                 */

                UTF_CHECK( om::areEqual( o2, i1 ) );
                UTF_CHECK( om::areEqual( o2, o2 ) );

                UTF_CHECK( om::areEqual( o2.get(), i1 ) );
                UTF_CHECK( om::areEqual( o2, i1.get() ) );
                UTF_CHECK( om::areEqual( o2.get(), i1.get() ) );
            }
        }

        om::setOnZeroRefsCallback();
    }
    catch( std::exception& )
    {
        om::setOnZeroRefsCallback( cbOnZeroRefs );
        throw;
    }

    /*
     * Make sure we end up with zero object refs (i.e. where we started)
     */

    UTF_CHECK( calledOnZeroRefs );
}

UTF_AUTO_TEST_CASE( ObjModel_FactoryTests )
{
    using namespace bl;
    using namespace bl::data;

    using bl::om::detail::FactoryImpl;

    const auto factoryImpl = FactoryImpl::createInstance();
    const auto factory = om::qi< om::Factory >( factoryImpl );

    const om::clsid_t clsid = uuids::create();

    try
    {
        /*
         * Verify the class not yet registered case
         */

        factory -> createInstance( clsid, TestInterface1234::iid() );
    }
    catch( ClassNotFoundException& )
    {
    }

    const auto cbCreate = []
    (
        SAA_in       const om::iid_t&  iid,
        SAA_inout    om::Object*       identity
    ) -> om::objref_t
    {
        BL_UNUSED( identity );
        return MyObjEncapsulated::createInstance( 13, 2 ) -> queryInterface( iid );
    };

    factoryImpl -> registerClass( clsid, cbCreate );

    {
        const auto o2 = om::wrap< TestInterface1234 >( factory -> createInstance( clsid, TestInterface1234::iid() ) );
        UTF_CHECK( o2 );
        UTF_CHECK_EQUAL( o2 -> getValue(), 13 );
        UTF_CHECK_EQUAL( o2 -> getValue(), 13 );

        o2 -> incValue();
        UTF_CHECK_EQUAL( o2 -> getValue(), 15 );
        UTF_CHECK_EQUAL( o2 -> getValue(), 15 );
    }

    try
    {
        /*
         * Verify the class already registered case
         */

        factoryImpl -> registerClass( clsid, cbCreate );
    }
    catch( UnexpectedException& )
    {
    }

    try
    {
        /*
         * Verify the invalid callback case
         */

        factoryImpl -> registerClass( uuids::create(), om::register_class_callback_t() );
    }
    catch( UnexpectedException& )
    {
    }

    factoryImpl -> unregisterClass( clsid );

    try
    {
        /*
         * Verify the class unregistered case
         */

        factory -> createInstance( clsid, TestInterface1234::iid() );
    }
    catch( ClassNotFoundException& )
    {
    }
}

UTF_AUTO_TEST_CASE( ObjModel_GlobalApiTests )
{
    using namespace bl;
    using namespace bl::data;

    const om::clsid_t clsid = uuids::create();

    try
    {
        /*
         * Verify the class not yet registered case
         */

        om::createInstance< TestInterface1234 >( clsid );
    }
    catch( ClassNotFoundException& )
    {
    }

    const auto cbCreate = []
    (
        SAA_in       const om::iid_t&  iid,
        SAA_inout    om::Object*       identity
    ) -> om::objref_t
    {
        BL_UNUSED( identity );
        return MyObjEncapsulated::createInstance( 13, 2 ) -> queryInterface( iid );
    };

    om::registerClass( clsid, cbCreate );

    {
        const auto o2 = om::createInstance< TestInterface1234 >( clsid );
        UTF_CHECK( o2 );
        UTF_CHECK_EQUAL( o2 -> getValue(), 13 );
        UTF_CHECK_EQUAL( o2 -> getValue(), 13 );

        o2 -> incValue();
        UTF_CHECK_EQUAL( o2 -> getValue(), 15 );
        UTF_CHECK_EQUAL( o2 -> getValue(), 15 );
    }

    try
    {
        /*
         * Verify the class already registered case
         */

        om::registerClass( clsid, cbCreate );
    }
    catch( UnexpectedException& )
    {
    }

    try
    {
        /*
         * Verify the invalid callback case
         */

        om::registerClass( uuids::create(), om::register_class_callback_t() );
    }
    catch( UnexpectedException& )
    {
    }

    om::unregisterClass( clsid );

    try
    {
        /*
         * Verify the class unregistered case
         */

        om::createInstance< TestInterface1234 >( clsid );
    }
    catch( ClassNotFoundException& )
    {
    }
}

UTF_AUTO_TEST_CASE( ObjModel_MyObjectImplTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace utest;

    const auto i1 = om::createInstance< MyInterface1 >( clsids::MyObjectImpl() );
    UTF_REQUIRE( i1 );
    UTF_CHECK_EQUAL( 13, i1 -> getValue() );

    UTF_CHECK( 0L != om::outstandingObjectRefs() );

    const auto i2 = om::qi< MyInterface2 >( i1 );
    const auto i3 = om::qi< MyInterface3 >( i1 );

    UTF_REQUIRE( i2 );
    i2 -> incValue( 5 );
    UTF_CHECK_EQUAL( 18, i1 -> getValue() );

    UTF_REQUIRE( i3 );
    i3 -> setValue( 3 );
    UTF_CHECK_EQUAL( 3, i1 -> getValue() );

    UTF_CHECK( om::areEqual( i1, i2 ) );
    UTF_CHECK( om::areEqual( i2, i3 ) );
}

/************************************************************************
 * Tests for std::shared_ptr< T > in the object model wrappers
 */

namespace
{
    template
    <
        typename T,
        typename U
    >
    bool ownerEqual( SAA_in const std::shared_ptr< T >& ptr1, SAA_in const std::shared_ptr< U >& ptr2 )
    {
        return ( false == ptr1.owner_before( ptr2 ) && false == ptr2.owner_before( ptr1 ) );
    }
}

UTF_AUTO_TEST_CASE( ObjModel_SharedPtrTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace utest;

    typedef om::ObjectImpl< MyObjectImplT<>, true /* enableSharedPtr */ > MyObjectSharedImpl;
    typedef om::ObjectImpl< MyObjectImplT<>, false /* enableSharedPtr */ > MyObjectNonSharedImpl;

    {
        const auto i1 = om::makeShared( MyObjectSharedImpl::createInstance< MyInterface1 >() );
        UTF_REQUIRE( i1 );
        const auto i2 = om::qi< MyInterface2 >( i1 );
        UTF_REQUIRE( i2 );

        UTF_REQUIRE( om::areEqual( i1.get(), i2.get() ) );
        UTF_REQUIRE( ownerEqual( i1, i2 ) );

        const auto i3 = om::makeShared( i1.get() );
        UTF_REQUIRE( om::areEqual( i1.get(), i3.get() ) );
        UTF_REQUIRE( ownerEqual( i1, i3 ) );
    }

    {
        const auto i1 = om::makeShared( MyObjectNonSharedImpl::createInstance< MyInterface1 >() );
        UTF_REQUIRE( i1 );
        const auto i2 = om::qi< MyInterface2 >( i1 );
        UTF_REQUIRE( i2 );

        UTF_REQUIRE( om::areEqual( i1.get(), i2.get() ) );
        UTF_REQUIRE( ownerEqual( i1, i2 ) );

        const auto i3 = om::makeShared( i1.get() );
        UTF_REQUIRE( om::areEqual( i1.get(), i3.get() ) );
        UTF_REQUIRE( ! ownerEqual( i1, i3 ) );
    }

    {
        const auto o = MyObjectSharedImpl::createInstance< MyInterface1 >();

        UTF_REQUIRE( om::tryGetSharedPtr( o ) );
        UTF_REQUIRE( om::tryGetSharedPtr( o.get() ) );
        UTF_REQUIRE( om::getSharedPtr( o ) );
        UTF_REQUIRE( om::getSharedPtr( o.get() ) );
    }

    {
        const auto o = MyObjectNonSharedImpl::createInstance< MyInterface1 >();

        UTF_CHECK( nullptr == om::tryGetSharedPtr( o ).get() );
        UTF_CHECK( nullptr == om::tryGetSharedPtr( o.get() ).get() );

        try
        {
            om::getSharedPtr( o );
            UTF_FAIL( BL_MSG() << "Must thrown an exception" );
        }
        catch( InterfaceNotSupportedException& )
        {
        }

        try
        {
            om::getSharedPtr( o.get() );
            UTF_FAIL( BL_MSG() << "Must thrown an exception" );
        }
        catch( InterfaceNotSupportedException& )
        {
        }
    }
}

UTF_AUTO_TEST_CASE( ObjModel_MakeSharedTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace utest;

    const auto i1 = om::createInstance< MyInterface1 >( clsids::MyObjectImpl() );
    UTF_REQUIRE( i1 );

    const auto si1 = om::makeShared( i1 );
    UTF_REQUIRE( si1 );
    UTF_CHECK_EQUAL( 13, si1 -> getValue() );

    const auto si2 = om::makeShared( i1.get() );
    UTF_REQUIRE( si2 );
    UTF_CHECK_EQUAL( 13, si2 -> getValue() );

    std::shared_ptr< MyInterface1 > si3 = si1;
    UTF_REQUIRE( si3 );
    UTF_CHECK_EQUAL( 13, si3 -> getValue() );

    const auto si4 = om::makeShared( om::qi< MyInterface2 >( i1 ) );
    UTF_REQUIRE( si4 );
    si4 -> incValue( 5 );
    UTF_CHECK_EQUAL( 18, si1 -> getValue() );
}

/************************************************************************
 * Tests for ProxyImpl
 */

UTF_AUTO_TEST_CASE( ObjModel_ProxyImplTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace utest;

    const auto i1 = om::createInstance< MyInterface1 >( clsids::MyObjectImpl() );

    const auto proxy = om::ProxyImpl::createInstance< om::Proxy >();

    const MyInterface1* const nullValue = nullptr;

    UTF_CHECK_EQUAL( nullValue, proxy -> tryAcquireRef< MyInterface1 >().get() );
    proxy -> disconnect();
    UTF_CHECK_EQUAL( nullValue, proxy -> tryAcquireRef< MyInterface1 >().get() );
    proxy -> disconnect();
    UTF_CHECK_EQUAL( nullValue, proxy -> tryAcquireRef< MyInterface1 >().get() );

    proxy -> connect( i1.get() );
    const auto i2 = proxy -> tryAcquireRef< MyInterface1 >();
    UTF_REQUIRE( i2 );

    UTF_REQUIRE( om::areEqual( i1, i2 ) );
    UTF_REQUIRE( proxy -> tryAcquireRef< MyInterface1 >() );

    proxy -> disconnect();
    UTF_CHECK_EQUAL( nullValue, proxy -> tryAcquireRef< MyInterface1 >().get() );
}

/************************************************************************
 * Tests for ObjPtrDisposable
 */

namespace
{
    class FooDisposable : public bl::om::Disposable
    {
        BL_CTR_DEFAULT( FooDisposable, protected )
        BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( FooDisposable, bl::om::Disposable )

    private:

        bl::cpp::ScalarTypeIniter< bool > m_disposed;

    protected:

        ~FooDisposable() NOEXCEPT
        {
            BL_RT_ASSERT( m_disposed, "Object must be disposed" );
        }

    public:

        bool isDisposed() const NOEXCEPT
        {
            return m_disposed;
        }

        virtual void dispose() OVERRIDE
        {
            m_disposed = true;
        }
    };

    typedef bl::om::ObjectImpl< FooDisposable > FooDisposableImpl;

} // __unnamed

UTF_AUTO_TEST_CASE( ObjModel_ObjPtrDisposableTests )
{
    using namespace bl;

    auto obj = om::lockDisposable( FooDisposableImpl::createInstance() );

    obj = om::lockDisposable( FooDisposableImpl::createInstance() );
    obj -> dispose();

    UTF_REQUIRE( obj -> isDisposed() );
}

