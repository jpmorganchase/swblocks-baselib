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

#ifndef __BL_OBJMODELDEFS_H_
#define __BL_OBJMODELDEFS_H_

#include <baselib/core/Uuid.h>
#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>

namespace bl
{
    namespace om
    {
        /*
         * Define the iid_t / clsid_t / serverid_t to be used for
         * readability purpose
         */

        typedef uuid_t iid_t;
        typedef uuid_t clsid_t;
        typedef uuid_t serverid_t;

        /**
         * @brief Opaque object reference
         */

        typedef struct objref_tag { /* opaque */ } *objref_t;

    } // om

} // bl

/**
 * @brief Macros to define interface ids and class ids
 */

#define BL_IID_DECLARE( name, value ) \
    BL_UUID_DECLARE_FULL( iids, name, value ) \

#define BL_CLSID_DECLARE( name, value ) \
    BL_UUID_DECLARE_FULL( clsids, name, value ) \

/**
 * @brief Convenience macro to declare interfaces. It defines the class as
 * non-copyable, non-movable and non-creatable and then it also
 * defines a static convenience function iid() that will return
 * the interface id assuming it is already defined with
 * BL_IID_DECLARE (see some examples below)
 */

#define BL_DECLARE_INTERFACE( className ) \
    BL_NO_CREATE( className ) \
    BL_NO_COPY_OR_MOVE( className ) \
    BL_NO_POLYMORPHIC_BASE( className ) \
    public: \
    static const bl::om::iid_t& iid() NOEXCEPT \
    { \
        return iids::className(); \
    } \
    private: \

/**
 * @brief The object implementation template and related macros
 * see ObjectImpl below for more details
 */

#define BL_QITBL_BEGIN() \
    protected: \
    bl::om::objref_t queryInterfaceInternal( SAA_in const bl::om::iid_t& iid ) NOEXCEPT \
    { \

#define BL_QITBL_ENTRY_IID( iface, iface_iid ) \
        if( iface_iid == iid ) \
        { \
            return bl::om::detail::acquireRef< iface >( this ); \
        } \

#define BL_QITBL_ENTRY_CHAIN_BASE( base_type ) \
        { \
            auto refBase = base_type::queryInterfaceInternal( iid ); \
            if( refBase ) \
            { \
                return refBase; \
            } \
        } \

#define BL_QITBL_END( identity_iface ) \
        if( bl::om::Object::iid() == iid ) \
        { \
            return bl::om::detail::acquireRef< identity_iface >( this ); \
        } \
        \
        return nullptr; \
    } \
    private: \

#define BL_QITBL_ENTRY( iface ) \
    BL_QITBL_ENTRY_IID( iface, iface::iid() ) \

#define BL_QITBL_DECLARE( identity_iface ) \
    BL_QITBL_BEGIN() \
        BL_QITBL_ENTRY( identity_iface ) \
    BL_QITBL_END( identity_iface ) \

#define BL_QITBL_DECLARE_DISPOSABLE( identity_iface ) \
    BL_QITBL_BEGIN() \
        BL_QITBL_ENTRY( identity_iface ) \
        BL_QITBL_ENTRY( bl::om::Disposable ) \
    BL_QITBL_END( identity_iface ) \

#define BL_QITBL_DECLARE_DEFAULT() \
    BL_QITBL_DECLARE( bl::om::Object ) \

/************************************************************
 * Convenience macros to declare implementation classes
 */


/**
 * @brief This macro will disable the use of className::iid() for anything
 * that is incomplete class, but not an interface. className::iid() will
 * only be valid for: 1) interfaces and 2) the final implementation class
 * - i.e. the flavor in om::ObjectImpl< ... > which returns uuids::nil()
 */

#define BL_DECLARE_OBJECT_IMPL_DISABLE_IID() \
    private: \
    static const bl::om::iid_t& iid() NOEXCEPT; \

#define BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( className ) \
    BL_CTR_COPY_DELETE( className ) \
    BL_DECLARE_OBJECT_IMPL_DISABLE_IID() \

#define BL_DECLARE_OBJECT_IMPL( className ) \
    BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( className ) \
    BL_NO_POLYMORPHIC_BASE( className ) \

#define BL_DECLARE_OBJECT_IMPL_ONEIFACE( className, identity_iface ) \
    BL_DECLARE_OBJECT_IMPL( className ) \
    BL_QITBL_DECLARE( identity_iface ) \

#define BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE( className, identity_iface ) \
    BL_DECLARE_OBJECT_IMPL( className ) \
    BL_QITBL_DECLARE_DISPOSABLE( identity_iface ) \

#define BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE_NO_DESTRUCTOR( className, identity_iface ) \
    BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( className ) \
    BL_QITBL_DECLARE_DISPOSABLE( identity_iface ) \

#define BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( className, identity_iface ) \
    BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( className ) \
    BL_QITBL_DECLARE( identity_iface ) \

#define BL_DECLARE_OBJECT_IMPL_DEFAULT( className ) \
    BL_DECLARE_OBJECT_IMPL_ONEIFACE( className, bl::om::Object ) \

#endif /* __BL_OBJMODELDEFS_H_ */
