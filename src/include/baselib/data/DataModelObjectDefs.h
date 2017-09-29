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

#ifndef __BL_DATA_DATAMODELOBJECTDEFS_H_
#define __BL_DATA_DATAMODELOBJECTDEFS_H_

#include <baselib/data/DataModelObject.h>

#include <baselib/core/EnumUtils.h>
#include <baselib/core/BaseIncludes.h>

#ifndef BL_DM_SERIALIZATION_CONTEXT_IMPL

#define BL_DM_SERIALIZATION_CONTEXT_IMPL \
    bl::dm::SerializationContextBase \

#undef BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_SERIALIZE
#define BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_SERIALIZE( context ) \
    BL_DM_SERIALIZATION_CONTEXT_IMPL context \

#undef BL_DM_SERIALIZATION_CONTEXT_IMPL_INVOKE_SERIALIZE
#define BL_DM_SERIALIZATION_CONTEXT_IMPL_INVOKE_SERIALIZE( context, canonicalize ) \
    serializeProperties( context, canonicalize ) \

#undef BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_DESERIALIZE
#define BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_DESERIALIZE( context, obj ) \
    BL_DM_SERIALIZATION_CONTEXT_IMPL context( obj ) \

#undef BL_DM_SERIALIZATION_CONTEXT_IMPL_DESERIALIZE_SETNAME
#define BL_DM_SERIALIZATION_CONTEXT_IMPL_DESERIALIZE_SETNAME( obj, value ) \
    do { } while( false )

#endif // BL_DM_SERIALIZATION_CONTEXT_IMPL

#define BL_DM_THROW_REQUIRED_PROPERTY_NOT_SET( name, operation ) \
    this_type::throwRequiredPropertyNotSet( #name, operation ); \

#define BL_DM_DEFINE_CHECK_READ_ONLY() \
    do \
    { \
        if( bl::dm::DataModelObject::readOnly() ) \
        { \
            this_type::readOnlyPropertyUpdateViolation(); \
        } \
    } \
    while( false );

#define BL_DM_DEFINE_CLASS_BEGIN_IMPL( className, baseClass ) \
template \
    < \
        typename E = void \
    > \
    class className ## T : public baseClass \
    { \
        BL_NO_POLYMORPHIC_BASE( className ## T ) \
        BL_NO_CREATE( className ## T ) \
        public: \
        typedef className ## T< E > this_type; \
        \
        static bool isPartial() NOEXCEPT \
        { \
            return false; \
        } \
        private: \

#define BL_DM_DEFINE_CLASS_BEGIN_BASE( className ) \
    BL_DM_DEFINE_CLASS_BEGIN_IMPL( className, bl::dm::DataModelObject )

#define BL_DM_DEFINE_CLASS_BEGIN( className ) \
    BL_DM_DEFINE_CLASS_BEGIN_BASE( className )

#define BL_DM_DEFINE_CLASS_END( className ) \
    }; \
    typedef bl::om::ObjectImpl< className ## T<> > className; \

/*
 * BL_DM_DECLARE_* and BL_DM_DEFINE_PROPERTY macros (for the serialization code)
 */

#define BL_DM_DEFINE_PROPERTY( className, propertyName ) \
    BL_DEFINE_STATIC_CONST_STRING_REF_INIT( className ## T, propertyName ## String ) \

#define BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \
    static const std::string& g_ ## name ## String; \
    static const std::string& name ## StringInit() \
    { \
        static const std::string g_## name ## StringImpl( #name ); \
        return g_## name ## StringImpl; \
    } \
    public: \
    static const std::string& name ## ToString() NOEXCEPT \
    { \
        return g_ ## name ## String;\
    } \
    private: \

#define BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, type ) \
    private: \
    bl::cpp::ScalarTypeIniter< type > m_ ## name; \
    bl::cpp::ScalarTypeIniter< bool > m_ ## name ## IsSet; \
    public: \
    type name() const NOEXCEPT { return m_ ## name; } \
    void name( SAA_in const type value ) \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        m_ ## name = value; \
        m_ ## name ## IsSet = true; \
    } \
    bool name ## IsSet() const NOEXCEPT { return m_ ## name ## IsSet; } \
    void name ## Reset() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        m_ ## name = type(); \
        m_ ## name ## IsSet = false; \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

#define BL_DM_DECLARE_PROPERTY_STRING_RO_IMPL( name ) \
    private: \
    std::string m_ ## name; \
    public: \
    const std::string& name() const NOEXCEPT { return m_ ## name; } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

#define BL_DM_DECLARE_PROPERTY_STRING_IMPL( name ) \
    BL_DM_DECLARE_PROPERTY_STRING_RO_IMPL( name ) \
    public: \
    std::string& name ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## name; \
    } \
    void name( SAA_in const std::string& value ) \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        m_ ## name = value; \
    } \
    \
    void name( SAA_in std::string&& value ) \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        value.swap( m_ ## name ); \
    } \
    private: \

#define BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, jsonGetter, isRequired ) \
    private: \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( canonicalize || m_ ## name ## IsSet ) \
        { \
            object.emplace( jsonProp, name() ); \
        } \
        else if( isRequired && ! m_ ## name ## IsSet ) \
        { \
            BL_DM_THROW_REQUIRED_PROPERTY_NOT_SET( name, "saving" ) \
        } \
    } \
    void name ## Deserialize( \
        SAA_in          const bl::json::Object&                         map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        const auto pos = map.find( jsonProp ); \
        \
        if( pos != map.end() ) \
        { \
            m_ ## name ## IsSet = true; \
            m_ ## name = pos -> jsonGetter; \
            \
            context.addProcessedProperty( jsonProp ); \
        } \
        else \
        { \
            if( isRequired ) \
            { \
                BL_DM_THROW_REQUIRED_PROPERTY_NOT_SET( name, "loading" ) \
            } \
            \
            m_ ## name ## IsSet = false; \
        } \
    } \

/*
 * BL_DM_DECLARE_BOOL_* macros
 */

#define BL_DM_DECLARE_BOOL_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, bool ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_bool(), false /* isRequired */ ) \

#define BL_DM_DECLARE_BOOL_REQUIRED_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, bool ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_bool(), true /* isRequired */ ) \

#define BL_DM_DECLARE_BOOL_ALTERNATE_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, bool ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_bool(), false /* isRequired */ ) \

#define BL_DM_DECLARE_BOOL_ALTERNATE_REQUIRED_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, bool ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_bool(), true /* isRequired */ ) \

/*
 * BL_DM_DECLARE_INT_* macros
 */

#define BL_DM_DECLARE_INT_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, int ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_int(), false /* isRequired */ ) \

#define BL_DM_DECLARE_INT_REQUIRED_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, int ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_int(), true /* isRequired */ ) \

#define BL_DM_DECLARE_INT_ALTERNATE_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, int ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_int(), false /* isRequired */ ) \

#define BL_DM_DECLARE_INT_ALTERNATE_REQUIRED_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, int ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_int(), true /* isRequired */ ) \

/*
 * BL_DM_DECLARE_UINT64_* macros
 */

#define BL_DM_DECLARE_UINT64_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, std::uint64_t ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_uint64(), false /* isRequired */ ) \

#define BL_DM_DECLARE_UINT64_REQUIRED_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, std::uint64_t ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_uint64(), true /* isRequired */ ) \

#define BL_DM_DECLARE_UINT64_ALTERNATE_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, std::uint64_t ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_uint64(), false /* isRequired */ ) \

#define BL_DM_DECLARE_UINT64_ALTERNATE_REQUIRED_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, std::uint64_t ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_uint64(), true /* isRequired */ ) \

/*
 * BL_DM_DECLARE_DOUBLE_* macros
 */

#define BL_DM_DECLARE_DOUBLE_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, double ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_real(), false /* isRequired */ ) \

#define BL_DM_DECLARE_DOUBLE_REQUIRED_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, double ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, #name, second.get_real(), true /* isRequired */ ) \

#define BL_DM_DECLARE_DOUBLE_ALTERNATE_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, double ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_real(), false /* isRequired */ ) \

#define BL_DM_DECLARE_DOUBLE_ALTERNATE_REQUIRED_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_PROPERTY_SCALAR_IMPL( name, double ) \
    BL_DM_DECLARE_SCALAR_SERIALIZATION( name, jsonProp, second.get_real(), true /* isRequired */ ) \

/*
 * BL_DM_DECLARE_STRING_* macros
 */

#define BL_DM_DECLARE_STRING_PROPERTY_SERIALIZE( name, jsonProp, isRequired ) \
    private: \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( canonicalize || ( ! name().empty() ) ) \
        { \
            object.emplace( jsonProp, name() ); \
        } \
        else if( isRequired ) \
        { \
            BL_DM_THROW_REQUIRED_PROPERTY_NOT_SET( name, "saving" ) \
        } \
        \
    } \

#define BL_DM_DECLARE_STRING_PROPERTY_DESERIALIZE( name, jsonProp, isRequired ) \
    private: \
    void name ## Deserialize( \
        SAA_in          const bl::json::Object&                         map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        const auto pos = map.find( jsonProp ); \
        if( \
            pos != map.end() && \
            pos -> second.type() != bl::json::ValueType::null_type \
            ) \
        { \
            m_ ## name = pos -> second.get_str(); \
            \
            context.addProcessedProperty( jsonProp ); \
        } \
        \
        if( isRequired && name().empty() ) \
        { \
            BL_DM_THROW_REQUIRED_PROPERTY_NOT_SET( name, "loading" ) \
        } \
    } \

#define BL_DM_DECLARE_STRING_PROPERTY_RO( name ) \
    BL_DM_DECLARE_PROPERTY_STRING_RO_IMPL( name ) \
    BL_DM_DECLARE_STRING_PROPERTY_DESERIALIZE( name, #name, false /* isRequired */ )

#define BL_DM_DECLARE_STRING_REQUIRED_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_STRING_IMPL( name ) \
    BL_DM_DECLARE_STRING_PROPERTY_SERIALIZE( name, #name, true /* isRequired */ ) \
    BL_DM_DECLARE_STRING_PROPERTY_DESERIALIZE( name, #name, true /* isRequired */ ) \

#define BL_DM_DECLARE_STRING_PROPERTY( name ) \
    BL_DM_DECLARE_PROPERTY_STRING_IMPL( name ) \
    BL_DM_DECLARE_STRING_PROPERTY_SERIALIZE( name, #name, false /* isRequired */ ) \
    BL_DM_DECLARE_STRING_PROPERTY_DESERIALIZE( name, #name, false /* isRequired */ ) \

#define BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY_IMPL( name, jsonProp, isRequired ) \
    BL_DM_DECLARE_PROPERTY_STRING_IMPL( name ) \
    BL_DM_DECLARE_STRING_PROPERTY_SERIALIZE( name, jsonProp, isRequired ) \
    BL_DM_DECLARE_STRING_PROPERTY_DESERIALIZE( name, jsonProp, isRequired ) \

#define BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY_IMPL( name, jsonProp, false /* isRequired */ ) \

#define BL_DM_DECLARE_STRING_ALTERNATE_REQUIRED_PROPERTY( name, jsonProp ) \
    BL_DM_DECLARE_STRING_ALTERNATE_PROPERTY_IMPL( name, jsonProp, true /* isRequired */ ) \


#define BL_DM_DECLARE_SIMPLE_CONTAINER_PROPERTY( name, jsonProp, item_type, jsonGetter, containerType, inserter ) \
    private: \
    containerType < item_type > m_ ## name; \
    \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( false == canonicalize && m_ ## name.size() == 0 ) \
        { \
            return; \
        } \
        \
        bl::json::Array items; \
        \
        for( const auto& item : m_ ## name ) \
        { \
            items.push_back( item ); \
        } \
        \
        object.emplace( #jsonProp, items ); \
    } \
    void name ## Deserialize( \
        SAA_in          const bl::json::Object&                         map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        const auto pos = map.find( #jsonProp ); \
        \
        if( pos == map.end() ) \
        { \
            return; \
        } \
        \
        auto& array = pos -> second.get_array(); \
        \
        containerType < item_type > temp; \
        \
        for( auto& item : array ) \
        { \
            if( ! std::is_same< item_type, std::string >::value && \
                item.type() == bl::json::ValueType::str_type ) \
            { \
                auto itemValue = item.get_str(); \
                temp.inserter( bl::utils::lexical_cast< item_type >( itemValue ) ); \
            } \
            else \
            { \
                temp.inserter( item.jsonGetter() ); \
            } \
        } \
        \
        m_ ##name .swap( temp ); \
        \
        context.addProcessedProperty( #jsonProp ); \
    } \
    \
    public: \
    const containerType < item_type >& name() const NOEXCEPT \
    { \
        return m_ ## name; \
    } \
    containerType < item_type >& name ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## name; \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

#define BL_DM_DECLARE_SIMPLE_VECTOR_PROPERTY( name, type, jsonGetter ) \
    BL_DM_DECLARE_SIMPLE_CONTAINER_PROPERTY( name, name, type, jsonGetter, std::vector, push_back ) \

#define BL_DM_DECLARE_SIMPLE_VECTOR_ALTERNATE_PROPERTY( name, jsonProp, type, jsonGetter ) \
    BL_DM_DECLARE_SIMPLE_CONTAINER_PROPERTY( name, jsonProp, type, jsonGetter, std::vector, push_back  ) \

/*
 * Note: std::set must be used to ensure the property values are always serialized
 * in canonical / stable order
 *
 * This is needed for the object hashing to work
 */

#define BL_DM_DECLARE_SIMPLE_SET_PROPERTY( name, type, jsonGetter ) \
    BL_DM_DECLARE_SIMPLE_CONTAINER_PROPERTY( name, name, type, jsonGetter, std::set, insert ) \

#define BL_DM_DECLARE_CUSTOM_PROPERTY( name ) \
    private: \
    bl::json::Value m_ ## name; \
    \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( canonicalize || ! m_ ## name.is_null() ) \
        { \
            object.emplace( #name, m_ ## name ); \
        } \
    } \
    void name ## Deserialize( \
        SAA_in          bl::json::Object&                               map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        auto pos = map.find( #name ); \
        \
        if( pos == map.end() ) \
        { \
            return; \
        } \
        \
        m_ ## name = std::move( pos -> second ); \
        \
        context.addProcessedProperty( #name ); \
    } \
    \
    public: \
    const bl::json::Value& name() const NOEXCEPT \
    { \
        return m_ ## name; \
    } \
    bl::json::Value& name ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## name; \
    } \
    void name( SAA_inout bl::json::Value&& value ) \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        m_ ## name = BL_PARAM_FWD( value ); \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

#define BL_DM_DECLARE_COMPLEX_PROPERTY( name, type ) \
    BL_DM_DECLARE_COMPLEX_ALTERNATE_PROPERTY( name, name, type ) \

#define BL_DM_DECLARE_COMPLEX_ALTERNATE_PROPERTY( name, jsonProp, type ) \
    BL_DM_DECLARE_COMPLEX_ALTERNATE_PROPERTY_IMPL( \
        name, \
        jsonProp, \
        type, \
        BL_DM_SERIALIZATION_CONTEXT_IMPL_INVOKE_SERIALIZE( tempContext, canonicalize ) \
        ) \

#define BL_DM_DECLARE_COMPLEX_ALTERNATE_PROPERTY_IMPL( name, jsonProp, type, invokeSerialize ) \
    private: \
    bl::om::ObjPtr< type > m_ ## name; \
    \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        \
        if( canonicalize || m_ ## name ) \
        { \
            BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_SERIALIZE( tempContext ); \
            if( m_ ## name ) \
            { \
                m_ ## name -> invokeSerialize; \
            } \
            \
            object.emplace( #jsonProp, tempContext.serializationDoc() ); \
        } \
    } \
    void name ## Deserialize( \
        SAA_in          bl::json::Object&                               map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        auto pos = map.find( #jsonProp ); \
        \
        if( pos == map.end() ) \
        { \
            return; \
        } \
        \
        auto ptr = type::createInstance(); \
        \
        BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_DESERIALIZE( tempContext, std::move( pos -> second.get_obj() ) ); \
        \
        tempContext.detectUnknownProperties( context.detectUnknownProperties() ); \
        \
        ptr -> serializeProperties( tempContext ); \
        \
        m_ ##name .swap( ptr ); \
        \
        context.addProcessedProperty( #jsonProp ); \
    } \
    \
    public: \
    const bl::om::ObjPtr< type >& name() const NOEXCEPT \
    { \
        return m_ ## name; \
    } \
    void name( SAA_in const bl::om::ObjPtr< type >& value ) \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        m_ ## name = bl::om::copy( value ); \
    } \
    void name( SAA_in bl::om::ObjPtr< type >&& value ) \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        m_ ## name = BL_PARAM_FWD( value ); \
    } \
    bl::om::ObjPtr< type >& name ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## name; \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

#define BL_DM_DECLARE_COMPLEX_VECTOR_PROPERTY( name, type ) \
    BL_DM_DECLARE_COMPLEX_VECTOR_ALTERNATE_PROPERTY( name, name, type ) \

#define BL_DM_DECLARE_COMPLEX_VECTOR_ALTERNATE_PROPERTY( name, jsonProp, type ) \
    private: \
    std::vector< bl::om::ObjPtr< type > > m_ ## name; \
    \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( false == canonicalize && m_ ## name.size() == 0 ) \
        { \
            return; \
        } \
        \
        bl::json::Array items; \
        \
        for( const auto& item : m_ ## name ) \
        { \
            BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_SERIALIZE( tempContext ); \
            item -> serializeProperties( tempContext ); \
            items.push_back( tempContext.serializationDoc() ); \
        } \
        \
        object.emplace( #jsonProp, items ); \
    } \
    void name ## Deserialize( \
        SAA_in          bl::json::Object&                               map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        const auto pos = map.find( #jsonProp ); \
        \
        if( pos == map.end() ) \
        { \
            return; \
        } \
        \
        auto& items = pos -> second.get_array(); \
        \
        std::vector< bl::om::ObjPtr< type > > temp; \
        \
        for( auto& item : items ) \
        { \
            BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_DESERIALIZE( tempContext, std::move( item.get_obj() ) ); \
            \
            tempContext.detectUnknownProperties( context.detectUnknownProperties() ); \
            \
            auto obj = type::createInstance(); \
            obj -> serializeProperties( tempContext ); \
            temp.push_back( std::move( obj ) ); \
        } \
        \
        m_ ## name.swap( temp ); \
        \
        context.addProcessedProperty( #jsonProp ); \
    } \
    \
    public: \
    const std::vector< bl::om::ObjPtr< type > >& name() const NOEXCEPT \
    { \
        return m_ ## name; \
    } \
    std::vector< bl::om::ObjPtr< type > >& name ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## name; \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

/*
 * `nameArg` to avoid collision with `name` setter inside
 */
#define BL_DM_DECLARE_COMPLEX_MAP_PROPERTY( nameArg, type ) \
    private: \
    std::map< std::string, bl::om::ObjPtr< type > > m_ ## nameArg; \
    \
    void nameArg ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( false == canonicalize && m_ ## nameArg .size() == 0 ) \
        { \
            return; \
        } \
        \
        bl::json::Object items; \
        \
        for( const auto& pair : m_ ## nameArg ) \
        { \
            BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_SERIALIZE( tempContext ); \
            pair.second -> serializeProperties( tempContext ); \
            items.emplace( pair.first, tempContext.serializationDoc() ); \
        } \
        \
        object.emplace( #nameArg, items ); \
    } \
    void nameArg ## Deserialize( \
        SAA_in          bl::json::Object&                               map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        const auto pos = map.find( #nameArg ); \
        \
        if( pos == map.end() ) \
        { \
            return; \
        } \
        \
        auto& items = pos -> second.get_obj(); \
        \
        std::map< std::string, bl::om::ObjPtr< type > > temp; \
        \
        for( auto& pair : items ) \
        { \
            BL_DM_SERIALIZATION_CONTEXT_IMPL_DECL_DESERIALIZE( tempContext, std::move( pair.second.get_obj() ) ); \
            \
            tempContext.detectUnknownProperties( context.detectUnknownProperties() ); \
            \
            auto obj = type::createInstance(); \
            obj -> serializeProperties( tempContext ); \
            BL_DM_SERIALIZATION_CONTEXT_IMPL_DESERIALIZE_SETNAME( obj, pair.first ); \
            temp.emplace( pair.first, std::move( obj ) ); \
        } \
        \
        m_ ##nameArg .swap( temp ); \
        \
        context.addProcessedProperty( #nameArg ); \
    } \
    \
    public: \
    const std::map< std::string, bl::om::ObjPtr< type > >& nameArg() const NOEXCEPT \
    { \
        return m_ ## nameArg; \
    } \
    std::map< std::string, bl::om::ObjPtr< type > >& nameArg ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## nameArg; \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( nameArg ) \
    private: \

#define BL_DM_DECLARE_MAP_PROPERTY( name, type ) \
    private: \
    std::map< std::string, type > m_ ## name; \
    \
    void name ## Serialize( \
        SAA_out         bl::json::Object&                               object, \
        SAA_in          const bool                                      canonicalize \
        ) \
    { \
        if( false == canonicalize && m_ ## name .size() == 0 ) \
        { \
            return; \
        } \
        \
        bl::json::Object items; \
        \
        for( const auto& pair : m_ ## name ) \
        { \
            items.emplace( pair.first, pair.second ); \
        } \
        \
        object.emplace( #name, items ); \
    } \
    void name ## Deserialize( \
        SAA_in          const bl::json::Object&                         map, \
        SAA_inout       BL_DM_SERIALIZATION_CONTEXT_IMPL&               context \
        ) \
    { \
        const auto pos = map.find( #name ); \
        \
        if( pos == map.end() ) \
        { \
            return; \
        } \
        \
        const auto& object = pos -> second.get_obj(); \
        \
        std::map< std::string, type > temp; \
        \
        for( const auto& pair : object ) \
        { \
            temp[ pair.first ] = pair.second.get_value< type >(); \
        } \
        \
        m_ ## name .swap( temp ); \
        \
        context.addProcessedProperty( #name ); \
    }\
    \
    public: \
    const std::map< std::string, type >& name() const NOEXCEPT \
    { \
        return m_ ## name; \
    } \
    std::map< std::string, type >& name ## Lvalue() \
    { \
        BL_DM_DEFINE_CHECK_READ_ONLY(); \
        return m_ ## name; \
    } \
    BL_DM_DECLARE_PROPERTY_TO_STRING( name ) \
    private: \

/*
 * BL_DM_PROPERTIES_IMPL_* macros (for serializeProperties method)
 */

#define BL_DM_PROPERTIES_IMPL_BEGIN() \
    public: \
    virtual void serializeProperties( \
        SAA_inout   BL_DM_SERIALIZATION_CONTEXT_IMPL&           context, \
        SAA_in_opt  const bool                                  canonicalize = false \
        ) \
    { \
        BL_UNUSED( canonicalize ); \

#define BL_DM_IMPL_PROPERTY( name ) \
    if( context.isSerialization() ) \
    { \
        name ## Serialize( context.serializationDoc(), canonicalize ); \
    } \
    else \
    { \
        try \
        { \
            name ## Deserialize( context.deserializationDoc(), context ); \
        } \
        catch( std::runtime_error& e ) \
        { \
            bl::json::remapIncorrectValueTypeException( \
                e, \
                std::current_exception(), \
                ( BL_MSG() \
                    <<"property '" \
                    << #name \
                    << "'" \
                ).text() \
                ); \
        } \
    } \

#define BL_DM_PROPERTIES_IMPL_HANDLE_UNMAPPED() \
        if( ! this_type::isPartial() ) \
        { \
            if( context.isSerialization() ) \
            { \
                auto& doc = context.serializationDoc(); \
                \
                for( const auto& pair : m_unmapped ) \
                { \
                    if( bl::cpp::contains( doc, pair.first ) ) \
                    { \
                        BL_LOG( \
                            bl::Logging::debug(), \
                            BL_MSG() \
                                << "Unmapped property '" \
                                << pair.first \
                                << "' also in document" \
                            ); \
                        \
                        BL_ASSERT( true ); \
                    } \
                    \
                    doc.emplace( pair.first, pair.second  ); \
                } \
            } \
            \
            if( ! context.isSerialization() ) \
            { \
                m_unmapped.clear(); \
                \
                for( const auto& pair : context.deserializationDoc() ) \
                { \
                    if( ! context.containsProcessedProperty( pair.first ) ) \
                    { \
                        m_unmapped.emplace( pair.first, pair.second ); \
                    } \
                } \
            } \
        } \

#define BL_DM_PROPERTIES_IMPL_END() \
        BL_DM_PROPERTIES_IMPL_HANDLE_UNMAPPED() \
    } \
    \
private: \


#define BL_DM_GET_AS_PRETTY_JSON_STRING( dataObject ) \
    bl::dm::DataModelUtils::getDocAsPrettyJsonString( dataObject )

#define BL_DM_LOAD_FROM_JSON_STRING( T, jsonText ) \
    bl::dm::DataModelUtils::loadFromJsonText< T >( jsonText )

/***********************************************************************************************
 * Some data model objects types that can be used in generic context
 */

namespace bl
{
    namespace dm
    {
        /**
         * @brief A placeholder object that can be used to represent a polymorphic data model
         * base object later to be converted to another concrete type (usually via
         * DataModelUtils::castTo< ... >( ... )
         */

        BL_DM_DEFINE_CLASS_BEGIN( Payload )
            BL_DM_PROPERTIES_IMPL_BEGIN()
            BL_DM_PROPERTIES_IMPL_END()
        BL_DM_DEFINE_CLASS_END( Payload )

    } // dm

} // bl

#endif /* __BL_DATA_DATAMODELOBJECTDEFS_H_ */
