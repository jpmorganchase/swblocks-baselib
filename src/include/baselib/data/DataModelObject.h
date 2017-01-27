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

#ifndef __BL_DATA_DATAMODELOBJECT_H_
#define __BL_DATA_DATAMODELOBJECT_H_

#include <baselib/crypto/HashCalculator.h>

#include <baselib/core/JsonUtils.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/FileEncoding.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace dm
    {
        /**
         * @brief The base class for all serialization context objects
         */

        template
        <
            typename E = void
        >
        class SerializationContextBaseT
        {
            BL_NO_COPY_OR_MOVE( SerializationContextBaseT )

        protected:

            const bool                                          m_isSerialization;
            json::Object                                        m_serializationDoc;
            json::Object                                        m_deserializationDoc;
            cpp::ScalarTypeIniter< bool >                       m_detectUnknownProperties;
            std::unordered_set< std::string >                   m_processedProperties;

        public:

            SerializationContextBaseT( SAA_in_opt const bool isSerialization = true ) NOEXCEPT
                :
                m_isSerialization( isSerialization )
            {
            }

            SerializationContextBaseT( SAA_in const std::string& json )
                :
                m_isSerialization( false )
            {
                auto rootValue = json::readFromString( json );

                m_deserializationDoc.swap( rootValue.get_obj() );
            }

            SerializationContextBaseT( SAA_inout json::Object&& object ) NOEXCEPT
                :
                m_isSerialization( false )
            {
                m_deserializationDoc.swap( object );
            }

            bool detectUnknownProperties() const NOEXCEPT
            {
                return m_detectUnknownProperties;
            }

            void detectUnknownProperties( SAA_in const bool detectUnknownProperties ) NOEXCEPT
            {
                m_detectUnknownProperties = detectUnknownProperties;
            }

            bool isSerialization() const NOEXCEPT
            {
                return m_isSerialization;
            }

            json::Object& serializationDoc() NOEXCEPT
            {
                BL_ASSERT( isSerialization() );

                return m_serializationDoc;
            }

            json::Object& deserializationDoc() NOEXCEPT
            {
                BL_ASSERT( ! isSerialization() );

                return m_deserializationDoc;
            }

            void addProcessedProperty( SAA_in std::string&& name )
            {
                m_processedProperties.emplace( BL_PARAM_FWD( name ) );
            }

            bool containsProcessedProperty( SAA_in const std::string& name ) const
            {
                auto contains = cpp::contains( m_processedProperties, name );

                BL_CHK_USER(
                    true,
                    ! contains && detectUnknownProperties(),
                    BL_MSG()
                        << "Unrecognized property '"
                        << name
                        << "' found while parsing JSON document. Check if the property is typed correctly."
                    );

                return contains;
            }
        };

        typedef SerializationContextBaseT<> SerializationContextBase;

        /**
         * @brief The base class for all serialize-able data model objects
         */

        template
        <
            typename E = void
        >
        class DataModelObjectT : public om::Object
        {
            BL_DECLARE_OBJECT_IMPL_DEFAULT( DataModelObjectT )
            BL_NO_CREATE( DataModelObjectT )

        protected:

            cpp::ScalarTypeIniter< bool >                                                       m_readOnly;
            json::Object                                                                        m_unmapped;

            void readOnlyPropertyUpdateViolation()
            {
                BL_THROW(
                    UnexpectedException(),
                    BL_MSG()
                        << "Trying to modify a read only object"
                    );
            }

            void throwRequiredPropertyNotSet(
                SAA_in              const char*                                                 name,
                SAA_in              const char*                                                 operation
                )
            {
                BL_THROW(
                    UserMessageException(),
                    BL_MSG()
                        << "Required property '"
                        << name
                        << "' is not provided when "
                        << operation
                    );
            }

        public:

            static bool isPartial() NOEXCEPT
            {
                return true;
            }

            bool readOnly() const NOEXCEPT
            {
                return m_readOnly;
            }

            void readOnly( SAA_in const bool readOnly ) NOEXCEPT
            {
                m_readOnly = readOnly;
            }

            auto unmapped() const NOEXCEPT -> const json::Object&
            {
                return m_unmapped;
            }

            auto unmappedLvalue() NOEXCEPT -> json::Object&
            {
                return m_unmapped;
            }
        };

        typedef DataModelObjectT<> DataModelObject;

        /**
         * @brief Data model utility code
         */

        template
        <
            typename E = void
        >
        class DataModelUtilsT
        {
            BL_DECLARE_STATIC( DataModelUtilsT )

        public:

            /*************************************************************************************************
             * Serialize helpers
             */

            template
            <
                typename T
            >
            static auto getJsonObject(
                SAA_in              const om::ObjPtr< T >&                          dataObject,
                SAA_in_opt          const bool                                      canonicalize = false
                )
                -> json::Object
            {
                SerializationContextBase context;

                dataObject -> serializeProperties( context, canonicalize );

                return std::move( context.serializationDoc() );
            }

            template
            <
                typename T
            >
            static auto getJsonString(
                SAA_in              const om::ObjPtr< T >&                          dataObject,
                SAA_in_opt          const bool                                      prettyPrint = false,
                SAA_in_opt          const bool                                      canonicalize = false
                )
                -> std::string
            {
                const auto jsonObject = getJsonObject( dataObject, canonicalize );

                return json::saveToString( jsonObject, prettyPrint );
            }

            template
            <
                typename T
            >
            static auto getDocAsPrettyJsonString( SAA_in const om::ObjPtr< T >& dataObject ) -> std::string
            {
                return getJsonString( dataObject, true /* prettyPrint */ );
            }

            template
            <
                typename T
            >
            static auto getDocAsPackedJsonString( SAA_in const om::ObjPtr< T >& dataObject ) -> std::string
            {
                return getJsonString( dataObject, false /* prettyPrint */ );
            }

            template
            <
                typename T
            >
            static auto getObjectHash(
                SAA_in              const om::ObjPtr< T >&                          dataObject,
                SAA_in_opt          const std::string&                              salt = str::empty(),
                SAA_in_opt          const bool                                      canonicalize = false
                )
                -> std::string
            {
                const auto canonicalizedProperties =
                    getJsonString< T >( dataObject, false /* prettyPrint */, canonicalize );

                /*
                 * The salt can be a security domain id or some other tag / global identification
                 * which we want to be included in the hash (if such is provided)
                 */

                hash::HashCalculatorDefault hasher;

                if( ! salt.empty() )
                {
                    hasher.update( salt.c_str(), salt.size() );
                }

                hasher.update( canonicalizedProperties.c_str(), canonicalizedProperties.size() );

                hasher.finalize();

                return hasher.digestStr();
            }

            template
            <
                typename T
            >
            static auto getObjectHashCanonical(
                SAA_in              const om::ObjPtr< T >&                          dataObject,
                SAA_in_opt          const std::string&                              salt = str::empty()
                )
                -> std::string
            {
                return getObjectHash< T >( dataObject, salt, true /* canonicalize */ );
            }

            /*************************************************************************************************
             * Deserialize helpers
             */

            template
            <
                typename T
            >
            static auto loadFromJsonObject( SAA_in json::Object&& jsonObject ) -> om::ObjPtr< T >
            {
                SerializationContextBase context( std::move( jsonObject ) );

                auto dataObject = T::template createInstance();

                dataObject -> serializeProperties( context );

                return dataObject;
            }

            template
            <
                typename T
            >
            static auto loadFromJsonObject( SAA_in const json::Object& jsonObject ) -> om::ObjPtr< T >
            {
                return loadFromJsonObject< T >( cpp::copy( jsonObject ) );
            }

            template
            <
                typename T
            >
            static auto loadFromJsonValue( SAA_in const json::Value& jsonValue ) -> om::ObjPtr< T >
            {
                return loadFromJsonObject< T >( jsonValue.get_obj() );
            }

            template
            <
                typename T
            >
            static auto loadFromJsonText( SAA_in const std::string& jsonText ) -> om::ObjPtr< T >
            {
                return loadFromJsonValue< T >( json::readFromString( jsonText ) );
            }

            template
            <
                typename T
            >
            static auto loadFromFile( SAA_in const fs::path& fileName ) -> om::ObjPtr< T >
            {
                return loadFromJsonText< T >( encoding::readTextFile( fileName ) );
            }

            template
            <
                typename T,
                typename U
            >
            static auto castTo( SAA_in const om::ObjPtr< U >& from ) -> om::ObjPtr< T >
            {
                return loadFromJsonObject< T >( getJsonObject< U >( from ) );
            }
        };

        typedef DataModelUtilsT<> DataModelUtils;

    } // dm

} // bl

#endif /* __BL_DATA_DATAMODELOBJECT_H_ */
