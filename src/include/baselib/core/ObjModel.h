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

#ifndef __BL_OBJMODEL_H_
#define __BL_OBJMODEL_H_

#include <baselib/core/EcUtils.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/CPP.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/OS.h>
#include <baselib/core/Logging.h>
#include <baselib/core/Compiler.h>
#include <baselib/core/RefCountedBase.h>
#include <baselib/core/BaseIncludes.h>

#include <type_traits>
#include <atomic>
#include <utility>
#include <unordered_map>
#include <set>

/*
 * Declare some common iids
 */

BL_IID_DECLARE( Object, "2a5b48f8-fc88-40f6-b69a-af53e5932603" )
BL_IID_DECLARE( Disposable, "72b462a1-0c11-438a-b9f0-a93c839302c0" )
BL_IID_DECLARE( SharedPtr, "5e2c9642-9a94-4f55-9d3e-eb92d135536c" )
BL_IID_DECLARE( Factory, "9e1db93b-2b3f-4e4d-8ef2-303a0daa450f" )
BL_IID_DECLARE( Loader, "8655d8d4-a096-42cc-93c5-585b32686d34" )
BL_IID_DECLARE( Resolver, "fc3e748b-929d-4792-a2ed-b29b3c7ac583" )
BL_IID_DECLARE( Proxy, "081643f2-18f7-4d8f-b2cc-367fc5a735a6" )
BL_IID_DECLARE( Plugin, "32bb5c6f-876d-43f2-9756-94729a31cae0" )

BL_DECLARE_EXCEPTION( InterfaceNotSupportedException )
BL_DECLARE_EXCEPTION( ClassNotFoundException )

/*
 * The object model implementation
 */

namespace bl
{
    namespace om
    {
        class Object;
        class Disposable;
        class SharedPtr;
        class Factory;
        class Resolver;
        class Loader;
        class Proxy;
        class Plugin;

        namespace detail
        {
            template
            <
                typename I = void
            >
            class DeleterT
            {
            public:

                /*
                 * Just release the object reference.
                 * This is a nothrow operation.
                 */

                template
                <
                    typename T
                >
                void operator ()( SAA_in T* ptr ) const NOEXCEPT
                {
                    typedef typename std::conditional< std::is_same< I, void >::value, T, I >::type interface_t;

                    static_cast< interface_t* >( ptr ) -> release();
                }
            };

            typedef DeleterT<> Deleter;

        } // detail

        /**
         * @brief A safe smart object template for object reference
         */

        template
        <
            typename T,
            typename I = void
        >
        class ObjPtr : public cpp::SafeUniquePtr< T, detail::DeleterT< I > >
        {
            BL_CTR_COPY_DELETE( ObjPtr )
            BL_CTR_DEFAULT_NOTHROW( ObjPtr, public )

        public:

            typedef cpp::SafeUniquePtr< T, detail::DeleterT< I > > base_type;
            typedef ObjPtr< T, I >                                 this_type;

        private:

            static this_type attach( SAA_in_opt std::nullptr_t ) NOEXCEPT;

            BL_CTR_COPY_DEFAULT_T( ObjPtr, SAA_in_opt T*, base_type, protected, NOEXCEPT )
            BL_CTR_COPY_DEFAULT_T( ObjPtr, SAA_in_opt ObjPtr&&, base_type, public, NOEXCEPT )
            BL_CTR_COPY_DEFAULT_T( ObjPtr, SAA_in_opt std::nullptr_t, base_type, public, NOEXCEPT )

        public:

            static this_type attach( SAA_in_opt T* ptr ) NOEXCEPT
            {
                return this_type( ptr );
            }
        };

        /********************************************************
         * Implement copy( ... ) wrappers
         */

        template
        <
            typename T,
            typename I = void
        >
        inline ObjPtr< T, I > copyAs( SAA_inout_opt T* ptr ) NOEXCEPT
        {
            typedef typename std::conditional< std::is_same< I, void >::value, T, I >::type interface_t;

            if( ptr )
            {
                static_cast< interface_t* >( ptr ) -> addRef();
                return ObjPtr< T, I >::attach( ptr );
            }

            return nullptr;
        }

        template
        <
            typename T
        >
        inline ObjPtr< T > copy( SAA_inout_opt T* ptr ) NOEXCEPT
        {
            return copyAs< T, void >( ptr );
        }

        template
        <
            typename T,
            typename I
        >
        inline ObjPtr< T, I > copy( SAA_inout_opt const ObjPtr< T, I >& ptr ) NOEXCEPT
        {
            return copyAs< T, I >( ptr.get() );
        }

        template
        <
            typename T1,
            typename T2,
            typename I
        >
        inline ObjPtr< T1, I > copy( SAA_inout_opt const ObjPtr< T2, I >& ptr ) NOEXCEPT
        {
            return copy< T1 >( ptr.get() );
        }

        template
        <
            typename T1,
            typename T2,
            typename I
        >
        inline ObjPtr< T1, I > moveAs( SAA_inout_opt ObjPtr< T2, I >& ptr ) NOEXCEPT
        {
            typedef typename std::conditional< std::is_same< I, void >::value, T1, I >::type interface_t;

            if( ptr )
            {
                return ObjPtr< T1, I >::attach( static_cast< interface_t* >( ptr.release() ) );
            }

            return nullptr;
        }

        /**
         * @brief A safe smart object template for object reference to a copyable object
         */

        template
        <
            typename T,
            typename I = void
        >
        class ObjPtrCopyable : public ObjPtr< T, I >
        {
            BL_CTR_DEFAULT_NOTHROW( ObjPtrCopyable, public )

        public:

            typedef ObjPtr< T, I >                                      base_type;
            typedef ObjPtrCopyable< T, I >                              this_type;
            typedef typename base_type::pointer                         pointer;

            BL_CTR_COPY_DEFAULT_T( ObjPtrCopyable, SAA_in_opt pointer, base_type, protected, NOEXCEPT )
            BL_CTR_COPY_DEFAULT_T( ObjPtrCopyable, SAA_in_opt ObjPtrCopyable&&, base_type, public, NOEXCEPT )
            BL_CTR_COPY_DEFAULT_T( ObjPtrCopyable, SAA_in_opt std::nullptr_t, base_type, public, NOEXCEPT )

        public:

            ObjPtrCopyable( SAA_in const ObjPtrCopyable& other )
                :
                base_type( om::copyAs< T, I >( other.get() ).release() )
            {
            }

            ObjPtrCopyable& operator =( SAA_in const ObjPtrCopyable& other )
            {
                base_type::reset( om::copyAs< T, I >( other.get() ).release() );
                return *this;
            }

            template
            <
                typename I2
            >
            ObjPtrCopyable( SAA_in const ObjPtr< T, I2 >& other )
                :
                base_type( om::copyAs< T, I >( other.get() ).release() )
            {
            }

            template
            <
                typename I2
            >
            ObjPtrCopyable& operator =( SAA_in const ObjPtr< T, I2 >& other )
            {
                base_type::reset( om::copyAs< T, I >( other.get() ).release() );
                return *this;
            }

            ObjPtr< T, I > detachAsUnique() NOEXCEPT
            {
                return ObjPtr< T, I >::attach( base_type::release() );
            }

            static this_type attach( SAA_in_opt pointer ptr ) NOEXCEPT
            {
                return this_type( ptr );
            }

            static this_type acquireRef( SAA_in_opt pointer ptr ) NOEXCEPT
            {
                typedef typename std::conditional< std::is_same< I, void >::value, T, I >::type interface_t;

                if( ptr )
                {
                    static_cast< interface_t* >( ptr ) -> addRef();
                }

                return attach( ptr );
            }
        };

        /**
         * @brief interface Object. This will be the base interface which all
         * objects will implement.
         */

        class Object
        {
            BL_DECLARE_INTERFACE( Object )

        public:

            virtual objref_t queryInterface( SAA_in const iid_t& iid ) NOEXCEPT = 0;
            virtual void  addRef() NOEXCEPT = 0;
            virtual void  release() NOEXCEPT = 0;
        };

        /**
         * @brief interface Disposable. This will be the base interface for
         * all disposable objects.
         */

        class Disposable : public Object
        {
            BL_DECLARE_INTERFACE( Disposable )

        public:

            virtual void dispose() = 0;
        };

        /**
         * @brief interface SharedPtr. This interface will be used to add support
         * for std::enable_shared_from_this< T > like functionality for objects
         */

        class SharedPtr : public Object
        {
            BL_DECLARE_INTERFACE( SharedPtr )

        public:

            virtual std::shared_ptr< Object > getSharedPtr() = 0;
        };

        /**
         * @brief interface Factory. This will be the interface for
         * the class factory returned by the loader.
         */

        class Factory : public Object
        {
            BL_DECLARE_INTERFACE( Factory )

        public:

            /**
             * @brief Creates an object of class clsid and returns the
             * requested interface
             */

            virtual objref_t createInstance(
                SAA_in          const clsid_t&    clsid,
                SAA_in          const iid_t&      iid,
                SAA_inout_opt   Object*           identity = nullptr
                ) = 0;
        };

        /**
         * @brief interface Resolver. This will be the interface for
         * the server (plug-in) resolver.
         */

        class Resolver : public Object
        {
            BL_DECLARE_INTERFACE( Resolver )

        public:

            virtual int resolveServer(
                SAA_in          const clsid_t&                  clsid,
                SAA_out         serverid_t&                     serverid
                ) NOEXCEPT = 0;

            virtual int getFactory(
                SAA_in          const serverid_t&               serverid,
                SAA_out         objref_t&                       factory,
                SAA_in          const bool                      onlyIfLoaded = false
                ) NOEXCEPT = 0;

            virtual int registerHost() NOEXCEPT = 0;

            virtual int getCoreServices( SAA_out_opt objref_t& coreServices ) NOEXCEPT = 0;

            virtual int setCoreServices( SAA_in_opt objref_t coreServices ) NOEXCEPT = 0;
        };

        /**
         * @brief interface Loader. This will be the interface for
         * the object loader.
         */

        class Loader : public Object
        {
            BL_DECLARE_INTERFACE( Loader )

        public:

            virtual objref_t getInstance(
                SAA_in          const clsid_t&                  clsid,
                SAA_in          const iid_t&                    iid,
                SAA_inout_opt   Object*                         identity = nullptr
                ) = 0;
        };

        /*
         * @brief interface Plugin. This must be a C-style interface to prevent
         * possible binary incompatibility between the agent and plug-ins.
         * Error codes must be used for error handling to ensure exceptions are
         * not propagated across the binary interface.
         */

        class Plugin : public Disposable
        {
            BL_DECLARE_INTERFACE( Plugin )

        public:

            virtual int init() NOEXCEPT = 0;

            virtual int start() NOEXCEPT = 0;
        };

        /**
         * @brief interface Proxy. This will be the interface to allow
         * observer objects to be created which can be connected
         * and disconnected easily
         */

        class Proxy : public Object
        {
            BL_DECLARE_INTERFACE( Proxy )

        public:

            /**
             * @brief This will acquire reference to the object if it is
             * connected or return nullptr if the object has been disconnected
             *
             * If the object was acquired successfully and the guard parameter
             * is not nullptr then the guard will be holding the internal proxy
             * lock, so no other call can acquire it until it is unlocked by
             * the caller (usually when the unique_lock guard goes out of scope)
             */

            virtual objref_t tryAcquireRefUnsafe(
                SAA_in          const iid_t&                iid,
                SAA_in_opt      os::mutex_unique_lock*      guard = nullptr
                ) NOEXCEPT = 0;

            /**
             * @brief Connects a reference to the proxy object. The proxy will keep
             * weak reference until it is disconnected
             */

            virtual void connect( SAA_inout Object* ref ) NOEXCEPT = 0;

            /**
             * @brief Disconnects the reference from the proxy object
             */

            virtual void disconnect( SAA_in_opt os::mutex_unique_lock* guard = nullptr ) NOEXCEPT = 0;

            /**
             * @brief Wrapper for Proxy::tryAcquireRef( const iid_t& )
             *
             * See description of the tryAcquireRefUnsafe( ... ) method above
             */

            template
            <
                typename T
            >
            ObjPtr< T > tryAcquireRef(
                SAA_in_opt      const iid_t&                iid = T::iid(),
                SAA_in_opt      os::mutex_unique_lock*      guard = nullptr
                ) NOEXCEPT
            {
                return ObjPtr< T >::attach( reinterpret_cast< T* >( tryAcquireRefUnsafe( iid, guard ) ) );
            }
        };

        typedef cpp::function < void () NOEXCEPT > onzerorefs_callback_t;

        /********************************************************
         * Private implementation helpers
         */

        namespace detail
        {
            template
            <
                typename T,
                typename U
            >
            inline objref_t acquireRef( SAA_inout U* ptr ) NOEXCEPT
            {
                static_cast< T* >( ptr ) -> addRef();
                return reinterpret_cast< objref_t >( static_cast< T* >( ptr ) );
            }

            template
            <
                typename T
            >
            inline objref_t ppv( SAA_in T* ptr ) NOEXCEPT
            {
                return reinterpret_cast< objref_t >( ptr );
            }

            /**
             * @brief The server lifetime tracker helper
             */

            template
            <
                bool isMultiThreaded = true
            >
            class ServerLifetimeTrackerT
            {
            private:

                typedef typename std::conditional< isMultiThreaded, std::atomic< long >, long >::type
                    counter_t;

                static counter_t                 g_refs;
                static onzerorefs_callback_t     g_cbOnZeroRefs;

            protected:

                ServerLifetimeTrackerT()
                {
                    ++g_refs;
                }

                ~ServerLifetimeTrackerT() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    if( 0L == --g_refs && g_cbOnZeroRefs )
                    {
                        /*
                         * g_cbOnZeroRefs callback should be a no-throw API
                         */

                        g_cbOnZeroRefs();
                    }

                    BL_NOEXCEPT_END()
                }

            public:

                static long outstandingRefs() NOEXCEPT
                {
                    return g_refs;
                }

                static void setCallback( SAA_in onzerorefs_callback_t&& cbOnZeroRefs = onzerorefs_callback_t() ) NOEXCEPT
                {
                    g_cbOnZeroRefs.swap( cbOnZeroRefs );
                }
            };

            template
            <
                bool isMultiThreaded
            >
            typename ServerLifetimeTrackerT< isMultiThreaded >::counter_t
            ServerLifetimeTrackerT< isMultiThreaded >::g_refs( 0L );

            template
            <
                bool isMultiThreaded
            >
            onzerorefs_callback_t
            ServerLifetimeTrackerT< isMultiThreaded >::g_cbOnZeroRefs;

            typedef ServerLifetimeTrackerT<> ServerLifetimeTracker;

        } // detail

        /********************************************************
         * Implement tryQI( ... ) / qi( ... ) wrappers
         */

        template
        <
            typename T,
            typename U
        >
        inline ObjPtr< T > tryQI( SAA_inout U* ptr, SAA_in const iid_t& iid = T::iid() ) NOEXCEPT
        {
            return ObjPtr< T >::attach( reinterpret_cast< T* >( ptr -> queryInterface( iid ) ) );
        }

        template
        <
            typename T,
            typename U
        >
        inline ObjPtr< T > tryQI( SAA_in const ObjPtr< U >& ptr, SAA_in const iid_t& iid = T::iid() ) NOEXCEPT
        {
            return tryQI< T, U >( ptr.get(), iid );
        }

        template
        <
            typename T,
            typename U
        >
        inline ObjPtr< T > tryQI( SAA_in const ObjPtrCopyable< U >& ptr, SAA_in const iid_t& iid = T::iid() ) NOEXCEPT
        {
            return tryQI< T, U >( ptr.get(), iid );
        }

        template
        <
            typename T,
            typename U
        >
        inline std::shared_ptr< T > tryQI( SAA_in const std::shared_ptr< U >& ptr, SAA_in const iid_t& iid = T::iid() ) NOEXCEPT
        {
            const auto ref = tryQI< T, U >( ptr.get(), iid );
            return std::shared_ptr< T >( ptr, ref.get() );
        }

        template
        <
            typename T,
            typename U
        >
        inline ObjPtr< T > qi( SAA_inout U* ptr, SAA_in const iid_t& iid = T::iid() )
        {
            auto ref = tryQI< T, U >( ptr, iid );

            BL_CHK_T(
                nullptr,
                ref.get(),
                InterfaceNotSupportedException(),
                BL_MSG()
                    << "The requested interface '"
                    << uuids::uuid2string( iid )
                    << "' is not supported"
                    );

            return ref;
        }

        template
        <
            typename T,
            typename U
        >
        inline ObjPtr< T > qi( SAA_in const ObjPtr< U >& ptr, SAA_in const iid_t& iid = T::iid() )
        {
            return qi< T, U >( ptr.get(), iid );
        }

        template
        <
            typename T,
            typename U
        >
        inline ObjPtr< T > qi( SAA_in const ObjPtrCopyable< U >& ptr, SAA_in const iid_t& iid = T::iid() )
        {
            return qi< T, U >( ptr.get(), iid );
        }

        template
        <
            typename T,
            typename U
        >
        inline std::shared_ptr< T > qi( SAA_in const std::shared_ptr< U >& ptr, SAA_in const iid_t& iid = T::iid() )
        {
            const auto ref = qi< T, U >( ptr.get(), iid );
            return std::shared_ptr< T >( ptr, ref.get() );
        }

        /********************************************************
         * API to wrap an opaque reference
         */

        template
        <
            typename T
        >
        inline ObjPtr< T > wrap( SAA_inout_opt objref_t ptr ) NOEXCEPT
        {
            return ObjPtr< T >::attach( reinterpret_cast< T* >( ptr ) );
        }

        /********************************************************
         * Implement areEqual( ... ) wrappers
         */

        template
        <
            typename T,
            typename U
        >
        inline bool areEqual( SAA_inout_opt T* ptr1, SAA_inout_opt U* ptr2 ) NOEXCEPT
        {
            if( detail::ppv( ptr1 ) == detail::ppv( ptr2 ) )
            {
                /*
                 * This will cover also the case when they're both nullptr
                 */

                return true;
            }

            if( nullptr == ptr1 || nullptr == ptr2 )
            {
                /*
                 * One of them is nullptr and one is not
                 */

                return false;
            }

            /*
             * If we're here then both pointers are non-null. In this case
             * we QI for Object and compare the pointers.
             */

            const auto o1 = tryQI< Object >( ptr1 );
            const auto o2 = tryQI< Object >( ptr2 );

            return ( o1.get() == o2.get() );
        }

        template
        <
            typename T,
            typename U
        >
        inline bool areEqual( SAA_inout_opt const ObjPtr< T >& ptr1, SAA_inout_opt const ObjPtr< U >& ptr2 ) NOEXCEPT
        {
            return areEqual( ptr1.get(), ptr2.get() );
        }

        template
        <
            typename T,
            typename U
        >
        inline bool areEqual( SAA_inout_opt const ObjPtr< T >& ptr1, SAA_inout_opt U* ptr2 ) NOEXCEPT
        {
            return areEqual( ptr1.get(), ptr2 );
        }

        template
        <
            typename T,
            typename U
        >
        inline bool areEqual( SAA_inout_opt T* ptr1, SAA_inout_opt const ObjPtr< U >& ptr2 ) NOEXCEPT
        {
            return areEqual( ptr1, ptr2.get() );
        }

        /********************************************************
         * Implement getSharedPtr( ... ) / makeShared( ... ) wrappers
         */

        template
        <
            typename T
        >
        inline std::shared_ptr< T > tryGetSharedPtr( SAA_inout T* ptr ) NOEXCEPT
        {
            const auto shared = tryQI< SharedPtr >( ptr );

            if( ! shared )
            {
                return std::shared_ptr< T >();
            }

            return std::shared_ptr< T >( shared -> getSharedPtr(), ptr );
        }

        template
        <
            typename T
        >
        inline std::shared_ptr< T > tryGetSharedPtr( SAA_inout_opt const ObjPtr< T >& ptr )
        {
            return tryGetSharedPtr< T >( ptr.get() );
        }

        template
        <
            typename T
        >
        inline std::shared_ptr< T > getSharedPtr( SAA_inout T* ptr )
        {
            const auto ref = tryGetSharedPtr( ptr );

            BL_CHK_T(
                nullptr,
                ref.get(),
                InterfaceNotSupportedException(),
                BL_MSG()
                    << "The requested interface '"
                    << uuids::uuid2string( SharedPtr::iid() )
                    << "' is not supported"
                    );

            return ref;
        }

        template
        <
            typename T
        >
        inline std::shared_ptr< T > getSharedPtr( SAA_inout_opt const ObjPtr< T >& ptr )
        {
            return getSharedPtr< T >( ptr.get() );
        }

        template
        <
            typename T
        >
        inline std::shared_ptr< T > makeShared( SAA_inout T* ptr )
        {
            /*
             * We must ensure that the deleter is nothrow copy-constructible
             */

            static_assert(
                std::is_nothrow_constructible< detail::Deleter, const detail::Deleter& >::value,
                "The object deleter must be nothrow copy-constructible"
                );

            const auto ref = tryGetSharedPtr( ptr );

            if( ref )
            {
                /*
                 * If the object supports SharedPtr we just get the object's shared_ptr
                 */

                return ref;
            }

            /*
             * If we're here that means the object doesn't support SharedPtr interface.
             * In this case we should create a new shared_ptr reference tied to a hard
             * reference
             */

            ptr -> addRef();
            return std::shared_ptr< T >( ptr, detail::Deleter() );
        }

        template
        <
            typename T
        >
        inline std::shared_ptr< T > makeShared( SAA_inout_opt const ObjPtr< T >& ptr )
        {
            return makeShared< T >( ptr.get() );
        }

        /********************************************************************
         * Implement ObjPtrDisposable< T > and lockDisposable( ... ) wrappers
         */

        /**
         * @brief A safe smart object template for object reference to a disposable object
         *
         * @warning This smart pointer will invoke dispose() when it goes out of scope even
         * if the underlying object is still referenced by other ObjPtrs. Use at your own risk!
         */

        template
        <
            typename T
        >
        class ObjPtrDisposable : public ObjPtr< T >
        {
            BL_CTR_COPY_DELETE( ObjPtrDisposable )
            BL_CTR_DEFAULT_NOTHROW( ObjPtrDisposable, public )

            BL_CTR_COPY_DEFAULT_T( ObjPtrDisposable, SAA_in_opt std::nullptr_t, base_type, public, NOEXCEPT )

        public:

            typedef ObjPtr< T > base_type;

            ObjPtrDisposable( SAA_in_opt T* other ) NOEXCEPT
                :
                base_type( other )
            {
            }

            ObjPtrDisposable& operator =( SAA_in_opt T* other ) NOEXCEPT
            {
                reset();

                base_type::operator =( other );
                return *this;
            }

            ObjPtrDisposable( SAA_inout_opt ObjPtrDisposable&& other ) NOEXCEPT
                :
                base_type( BL_PARAM_FWD( other ) )
            {
            }

            ObjPtrDisposable& operator =( SAA_inout_opt ObjPtrDisposable&& other ) NOEXCEPT
            {
                reset();

                base_type::operator =( BL_PARAM_FWD( other ) );
                return *this;
            }

            ObjPtrDisposable( SAA_inout_opt base_type&& other ) NOEXCEPT
                :
                base_type( BL_PARAM_FWD( other ) )
            {
            }

            ObjPtrDisposable& operator =( SAA_inout_opt base_type&& other ) NOEXCEPT
            {
                reset();

                base_type::operator =( BL_PARAM_FWD( other ) );
                return *this;
            }

            ~ObjPtrDisposable() NOEXCEPT
            {
                reset();
            }

            void reset( SAA_in_opt T* other = nullptr ) NOEXCEPT
            {
                const auto rawptr = base_type::get();

                if( rawptr )
                {
                    BL_WARN_NOEXCEPT_BEGIN()

                    const auto disposable = tryQI< Disposable >( rawptr );

                    if( disposable )
                    {
                        disposable -> dispose();
                    }

                    BL_WARN_NOEXCEPT_END( "ObjPtrDisposable::~ObjPtrDisposable()" )
                }

                base_type::reset( other );
            }

            static ObjPtrDisposable< T > attach( SAA_in_opt T* ptr ) NOEXCEPT
            {
                return ObjPtrDisposable< T >( ptr );
            }

            ObjPtr< T > detachAsObjPtr() NOEXCEPT
            {
                return ObjPtr< T >::attach( base_type::release() );
            }
        };

        template
        <
            typename T
        >
        inline ObjPtrDisposable< T > lockDisposable( SAA_inout_opt T* ptr ) NOEXCEPT
        {
            typedef ObjPtrDisposable< T > disposable_t;

            if( ptr )
            {
                ptr -> addRef();
                return disposable_t::attach( ptr );
            }

            return nullptr;
        }

        template
        <
            typename T
        >
        inline ObjPtrDisposable< T > lockDisposable( SAA_inout_opt const ObjPtr< T >& ptr ) NOEXCEPT
        {
            return lockDisposable( ptr.get() );
        }

        /********************************************************
         * Global lifetime tracking APIs
         */

        inline long outstandingObjectRefs() NOEXCEPT
        {
            return detail::ServerLifetimeTracker::outstandingRefs();
        }

        inline void setOnZeroRefsCallback( SAA_in onzerorefs_callback_t&& cbOnZeroRefs = onzerorefs_callback_t() ) NOEXCEPT
        {
            detail::ServerLifetimeTracker::setCallback( BL_PARAM_FWD( cbOnZeroRefs ) );
        }

        /********************************************************
         * define get_pointer( ... ) for ObjPtrCopyable< T > and
         * make boost happy wrt to boost::mem_fn and boost::bind
         */

        template
        <
            typename T,
            typename I
        >
        T* get_pointer( SAA_in const ObjPtrCopyable< T, I >& ptr )
        {
            return ptr.get();
        }

        /********************************************************
         * class SharedPtrImpl< enableSharedPtr >
         */

        /**
         * @brief The default implementation which allows us to enable shared_ptr
         * on objects
         */

        template
        <
            bool enableSharedPtr = false
        >
        class SharedPtrImpl;

        template
        <
        >
        class SharedPtrImpl< true /* enableSharedPtr */ > :
            public SharedPtr
        {
            BL_CTR_DEFAULT( SharedPtrImpl, public )
            BL_CTR_COPY_DELETE( SharedPtrImpl )
            BL_NO_POLYMORPHIC_BASE( SharedPtrImpl )

        protected:

            std::weak_ptr< Object >                         m_this;
            os::mutex                                       m_lock;

        public:

            virtual std::shared_ptr< Object > getSharedPtr() OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                auto ref = m_this.lock();

                if( ref )
                {
                    return ref;
                }

                addRef();
                std::shared_ptr< Object > newRef( this, detail::Deleter() );
                m_this = newRef;
                return newRef;
            }

            objref_t qiSharedPtr( SAA_in const iid_t& iid )
            {
                if( SharedPtr::iid() == iid )
                {
                    return detail::acquireRef< SharedPtr >( this );
                }

                return nullptr;
            }
        };

        /**
         * @brief The specialization which does not implement shared_ptr
         * on objects
         */

        template
        <
        >
        class SharedPtrImpl< false /* enableSharedPtr */ >
        {
        public:

            objref_t qiSharedPtr( SAA_in const iid_t& /* iid */ )
            {
                return nullptr;
            }
        };

        /**
         * @brief The implementation template for objects implementing one
         * or more interfaces (derived from Object)
         */

        template
        <
            typename T,
            bool enableSharedPtr = false,
            bool isMultiThreaded = true,
            typename LifetimeTracker = detail::ServerLifetimeTrackerT< isMultiThreaded >
        >
        class ObjectImpl FINAL :
            public RefCountedBase< isMultiThreaded >,
            public LifetimeTracker,
            public SharedPtrImpl< enableSharedPtr >,
            public T
        {
            BL_CTR_COPY_DELETE( ObjectImpl )
            BL_NO_POLYMORPHIC_BASE( ObjectImpl )

        public:

            typedef ObjectImpl< T, enableSharedPtr, isMultiThreaded, LifetimeTracker >              this_type;
            typedef RefCountedBase< isMultiThreaded >                                               refcounted_base_type;
            typedef SharedPtrImpl< enableSharedPtr >                                                shared_ptr_base_type;
            typedef T                                                                               base_type;

        private:

            BL_ENCAPSULATE_CREATE_INSTANCE( U, ObjectImpl, base_type, this_type )

        public:

            /*
             * The iid for the class is nil()
             */

            static const om::iid_t& iid() NOEXCEPT
            {
                return uuids::nil();
            }

            virtual objref_t queryInterface( SAA_in const iid_t& iid ) NOEXCEPT
            {
                auto ref = this_type::queryInterfaceInternal( iid );

                if( ref )
                {
                    return ref;
                }

                ref = shared_ptr_base_type::qiSharedPtr( iid );

                if( ref )
                {
                    return ref;
                }

                if( uuids::nil() == iid )
                {
                    return detail::acquireRef< this_type >( this );
                }

                return nullptr;
            }

            virtual void  addRef() NOEXCEPT
            {
                ++refcounted_base_type::m_refs;
            }

            virtual void  release() NOEXCEPT
            {
                if( 0L == --refcounted_base_type::m_refs )
                {
                    delete this;
                }
            }
        };

        /********************************************************
         * class ObjectDefaultBase - a default base for classes
         * which are going to implement only om::Object interface
         */

        class ObjectDefaultBase : public Object
        {
            BL_CTR_DEFAULT( ObjectDefaultBase, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( ObjectDefaultBase, Object )
        };

        /********************************************************
         * class DisposableObjectBase - a default base for classes
         * which are going to implement only om::Disposable interface
         */

        class DisposableObjectBase : public Disposable
        {
            BL_CTR_DEFAULT( DisposableObjectBase, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( DisposableObjectBase, Disposable )
        };

        /********************************************************
         * class BoxedValueObject - a boxed value object
         */

        template
        <
            typename T
        >
        class BoxedValueObject : public Object
        {
            BL_CTR_DEFAULT( BoxedValueObject, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( BoxedValueObject, Object )

        protected:

            T m_value;

            BoxedValueObject( SAA_in T&& value ) NOEXCEPT
            {
                moveOrCopyNothrow( std::forward< T >( value ) );
            }

        public:

            void moveOrCopyNothrow( SAA_in T&& value ) NOEXCEPT
            {
                static_assert(
                    std::is_nothrow_move_constructible< T >::value ||
                    std::is_nothrow_copy_constructible< T >::value,
                    "T must be nothrow_move_constructible or nothrow_copy_constructible"
                    );

                m_value = std::forward< T >( value );
            }

            void swapValue( SAA_in T&& value ) NOEXCEPT
            {
                m_value.swap( value );
            }

            const T& value() const NOEXCEPT
            {
                return m_value;
            }

            T& lvalue() NOEXCEPT
            {
                return m_value;
            }
        };

        /********************************************************
         * class ProxyImpl
         */

        template
        <
            typename E = void
        >
        class ProxyImplT : public Proxy
        {
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( ProxyImplT, Proxy )

        protected:

            Object*             m_ref;
            os::mutex           m_lock;
            bool                m_strongRef;

            ProxyImplT( SAA_in const bool strongRef = false )
                :
                m_ref( nullptr ),
                m_strongRef( strongRef )
            {
            }

            ~ProxyImplT()
            {
                BL_RT_ASSERT(
                    nullptr == m_ref,
                    "The proxy must be disconnected by the owner before the last reference to it was released"
                    );
            }

            void disconnectInternalNoLock()
            {
                if( m_ref && m_strongRef )
                {
                    m_ref -> release();
                }

                m_ref = nullptr;
            }

            virtual objref_t tryAcquireRefUnsafe(
                SAA_in          const iid_t&                iid,
                SAA_in_opt      os::mutex_unique_lock*      guard = nullptr
                ) NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock localGuard( m_lock );

                if( ! m_ref )
                {
                    return nullptr;
                }

                const auto result = m_ref -> queryInterface( iid );

                if( result && guard )
                {
                    localGuard.swap( *guard );
                }

                return result;
            }

        public:

            virtual void connect( SAA_inout Object* ref ) NOEXCEPT OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                disconnectInternalNoLock();

                m_ref = ref;

                if( m_ref && m_strongRef )
                {
                    m_ref -> addRef();
                }
            }

            virtual void disconnect( SAA_in_opt os::mutex_unique_lock* guard = nullptr ) NOEXCEPT OVERRIDE
            {
                os::mutex_unique_lock localGuard( m_lock );

                disconnectInternalNoLock();

                if( guard )
                {
                    localGuard.swap( *guard );
                }
            }
        };

        typedef ObjectImpl< ProxyImplT<> > ProxyImpl;

        /********************************************************
         * register_class_callback_t
         */

        typedef cpp::function
            <
                objref_t (
                    SAA_in       const iid_t&      iid,
                    SAA_inout    Object*           identity
                    )
            >
            register_class_callback_t;

        namespace detail
        {
            class LttLeaked {};

            /**
             * @brief The factory default implementation
             */

            template
            <
                typename E = void
            >
            class FactoryImplT : public Factory
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE( FactoryImplT, Factory )
                BL_CTR_DEFAULT( FactoryImplT, protected )

            private:

                std::unordered_map< clsid_t, register_class_callback_t > m_registrations;

            public:

                void registerClass(
                    SAA_in       const clsid_t&                  clsid,
                    SAA_in       register_class_callback_t&&     cb
                    )
                {
                    BL_CHK(
                        false,
                        m_registrations.find( clsid ) == m_registrations.end(),
                        BL_MSG()
                            << "Class with id '"
                            << clsid
                            << "' is already registered in the factory"
                        );

                    BL_CHK(
                        false,
                        !!cb,
                        BL_MSG()
                            << "Class registration callback is invalid"
                        );

                    auto& cbStore = m_registrations[ clsid ];
                    cbStore.swap( cb );
                }

                void unregisterClass( SAA_in const clsid_t& clsid )
                {
                    const auto pos = m_registrations.find( clsid );

                    BL_CHK(
                        false,
                        pos != m_registrations.end(),
                        BL_MSG()
                            << "Class with id '"
                            << clsid
                            << "' is not registered in the factory"
                        );

                    m_registrations.erase( pos );
                }

                /*
                 * Implementation of the Factory interface
                 */

                virtual objref_t createInstance(
                    SAA_in          const clsid_t&    clsid,
                    SAA_in          const iid_t&      iid,
                    SAA_inout_opt   Object*           identity = nullptr
                    ) OVERRIDE
                {
                    const auto instance = tryCreateInstance( clsid, iid, identity );

                    BL_CHK_T(
                        nullptr,
                        instance,
                        ClassNotFoundException(),
                        BL_MSG()
                            << "Class with id '"
                            << clsid
                            << "' is not registered"
                        );

                    return instance;
                }

                virtual objref_t tryCreateInstance(
                    SAA_in          const clsid_t&    clsid,
                    SAA_in          const iid_t&      iid,
                    SAA_inout_opt   Object*           identity = nullptr
                    )
                {
                    const auto pos = m_registrations.find( clsid );

                    if( pos == m_registrations.end() )
                    {
                        return nullptr;
                    }

                    return pos -> second( iid, identity );
                }
            };

            typedef ObjectImpl< FactoryImplT<> > FactoryImpl;

            typedef ObjectImpl< FactoryImplT<>, false /* enableSharedPtr */, true /* isMultiThreaded */, LttLeaked > FactoryImplDefault;

            /**
             * @brief The loader default implementation
             */

            template
            <
                typename E = void
            >
            class LoaderImplT : public Loader
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE( LoaderImplT, Loader )

            private:

                ObjPtr< FactoryImplDefault > m_factoryDefault;

                /*
                 * Note the resolver callback should be kept as a raw
                 * pointer and never freed as it is owned by the agent
                 * loader code
                 */

                Resolver* m_resolver;

            protected:

                typedef LoaderImplT< E > this_type;

                LoaderImplT()
                    :
                    m_factoryDefault( FactoryImplDefault::createInstance() ),
                    m_resolver( nullptr )
                {
                }

            public:

                ObjPtr< Factory > getDefaultFactory()
                {
                    return copy( qi< Factory >( m_factoryDefault ) );
                }

                ObjPtr< Resolver > getResolver() const NOEXCEPT
                {
                    return copy( m_resolver );
                }

                void setResolver( Resolver* resolver ) NOEXCEPT
                {
                    m_resolver = resolver;
                }

                void registerClass(
                    SAA_in       const clsid_t&                  clsid,
                    SAA_in       register_class_callback_t&&     cb
                    )
                {
                    m_factoryDefault -> registerClass( clsid, BL_PARAM_FWD( cb ) );
                }

                void unregisterClass( SAA_in const clsid_t& clsid )
                {
                    m_factoryDefault -> unregisterClass( clsid );
                }

                virtual objref_t getInstance(
                    SAA_in          const clsid_t&    clsid,
                    SAA_in          const iid_t&      iid,
                    SAA_inout_opt   Object*           identity = nullptr
                    ) OVERRIDE
                {
                    auto instance = m_factoryDefault -> tryCreateInstance( clsid, iid, identity );

                    if( instance )
                    {
                        return instance;
                    }

                    BL_CHK_T(
                        nullptr,
                        m_resolver,
                        ClassNotFoundException(),
                        BL_MSG()
                            << "Server id for class id '"
                            << clsid
                            << "' cannot be resolved, no resolver registered"
                        );

                    serverid_t serverid;

                    eh::EcUtils::checkErrorCode(
                        m_resolver -> resolveServer( clsid, serverid )
                        );

                    objref_t factoryRef;

                    eh::EcUtils::checkErrorCode(
                        m_resolver -> getFactory( serverid, factoryRef )
                        );

                    const auto factory = wrap< Factory >( factoryRef );

                    return factory -> createInstance( clsid, iid, identity );
                }

                void reset()
                {
                    m_factoryDefault = FactoryImplDefault::createInstance();
                    m_resolver = nullptr;
                }
            };

            typedef ObjectImpl< LoaderImplT<>, false /* enableSharedPtr */, true /* isMultiThreaded */, LttLeaked > LoaderImplDefault;

            /**
             * @brief A loader initialize helper to host the global loader state
             */

            template
            <
                typename E = void
            >
            class GlobalInitT
            {
                BL_DECLARE_STATIC( GlobalInitT )

            private:

                static ObjPtr< LoaderImplDefault > g_loader;

            public:

                static ObjPtr< LoaderImplDefault > getLoader()
                {
                    return copy( g_loader );
                }
            };

            typedef GlobalInitT<> GlobalInit;

            template
            <
                typename E
            >
            ObjPtr< LoaderImplDefault >
            GlobalInitT< E >::g_loader( LoaderImplDefault::createInstance() );

        } // detail

        inline void* registerResolverImpl( SAA_inout void* resolver )
        {
            const auto resolverPtr = reinterpret_cast< Resolver* >( resolver );
            const auto ldr = detail::GlobalInit::getLoader();
            ldr -> setResolver( resolverPtr );

            auto factoryDef = ldr -> getDefaultFactory();
            return reinterpret_cast< void* >( factoryDef.release() );
        }

        template
        <
            typename T
        >
        inline ObjPtr< T > createInstance(
            SAA_in          const clsid_t&    clsid,
            SAA_in          const iid_t&      iid = T::iid(),
            SAA_inout_opt   Object*           identity = nullptr
            )
        {
            const auto ldr = detail::GlobalInit::getLoader();
            return wrap< T >( ldr -> getInstance( clsid, iid, identity ) );
        }

        inline void registerClass(
            SAA_in       const clsid_t&                  clsid,
            SAA_in       register_class_callback_t&&     cb
            )
        {
            const auto ldr = detail::GlobalInit::getLoader();
            ldr -> registerClass( clsid, BL_PARAM_FWD( cb ) );
        }

        inline void unregisterClass( SAA_in const clsid_t& clsid )
        {
            const auto ldr = detail::GlobalInit::getLoader();
            ldr -> unregisterClass( clsid );
        }

        inline ObjPtr< Object > getCoreServices()
        {
            const auto resolver = detail::GlobalInit::getLoader() -> getResolver();

            BL_CHK(
                nullptr,
                resolver.get(),
                BL_MSG()
                    << "om::getCoreServices cannot be called before the resolver is set"
                );

            objref_t coreServices;
            eh::EcUtils::checkErrorCode( resolver -> getCoreServices( coreServices ) );

            return wrap< Object >( coreServices );
        }

        inline void setCoreServices( SAA_in const ObjPtr< Object >& coreServices )
        {
            const auto resolver = detail::GlobalInit::getLoader() -> getResolver();

            BL_CHK(
                nullptr,
                resolver.get(),
                BL_MSG()
                    << "om::setCoreServices cannot be called before the resolver is set"
                );

            eh::EcUtils::checkErrorCode(
                resolver -> setCoreServices( reinterpret_cast< objref_t >( coreServices.get() ) )
                );
        }

        template
        <
            typename T
        >
        inline ObjPtr< T > tryGetCoreService()
        {
            const auto coreServices = getCoreServices();

            BL_CHK(
                nullptr,
                coreServices.get(),
                BL_MSG()
                    << "Core services have not been configured yet"
                );

            return tryQI< T >( coreServices );
        }

        template
        <
            typename T
        >
        inline ObjPtr< T > getCoreService()
        {
            const auto coreServices = getCoreServices();

            BL_CHK(
                nullptr,
                coreServices.get(),
                BL_MSG()
                    << "Core services have not been configured yet"
                );

            return qi< T >( coreServices );
        }

        template
        <
            typename T
        >
        class SimpleFactoryImpl
        {
            BL_DECLARE_STATIC( SimpleFactoryImpl )

        public:

            static objref_t createInstance(
                SAA_in       const iid_t&      iid,
                SAA_inout    Object*           identity = nullptr
                )
            {
                BL_UNUSED( identity );
                return T::template createInstance< T >() -> queryInterface( iid );
            }
        };

    } // om

} // bl

namespace std
{
    /**
     * @brief Provide specializations of hash function in the std namespace
     * This will allow us to use ObjPtr, ObjPtrDisposable and ObjPtrCopyable
     * as a key in the new C++11 std containers
     */

    template
    <
        typename T
    >
    struct hash< bl::om::ObjPtr< T > >
    {
        std::size_t operator()( const bl::om::ObjPtr< T >& ptr ) const
        {
            std::hash< T* > hasher;
            return hasher( ptr.get() );
        }
    };

    template
    <
        typename T
    >
    struct hash< bl::om::ObjPtrDisposable< T > >
    {
        std::size_t operator()( const bl::om::ObjPtrDisposable< T >& ptr ) const
        {
            std::hash< T* > hasher;
            return hasher( ptr.get() );
        }
    };

    template
    <
        typename T
    >
    struct hash< bl::om::ObjPtrCopyable< T > >
    {
        std::size_t operator()( const bl::om::ObjPtrCopyable< T >& ptr ) const
        {
            std::hash< T* > hasher;
            return hasher( ptr.get() );
        }
    };

} // std

#endif /* __BL_OBJMODEL_H_ */
