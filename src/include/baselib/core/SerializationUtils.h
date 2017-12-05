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

#ifndef __BL_SERIALIZATIONUTILS_H_
#define __BL_SERIALIZATIONUTILS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/FsUtils.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace serial
    {
        using boost::archive::iterators::transform_width;
        using boost::archive::iterators::binary_from_base64;
        using boost::archive::iterators::base64_from_binary;

        typedef transform_width
            <
                binary_from_base64< std::string::const_iterator >,                  /* Base */
                8,                                                                  /* BitsOut */
                6                                                                   /* BitsIn */
            >
            from_base64_iterator_t;

        typedef base64_from_binary
            <
                transform_width
                <
                    const unsigned char*,                                           /* Base */
                    6,                                                              /* BitsOut */
                    8                                                               /* BitsIn */
                >
            >
            to_base64_iterator_t;

    } // serial

    /**
     * @brief class SerializationUtils
     */

    template
    <
        typename E = void
    >
    class SerializationUtilsT
    {
    public:

        /********************************************************************************************
         * Base64 encoding and decoding
         */

        /**
         * @brief Implements Base 64 Encoding with padding as per RFC4648
         * (more info also here: http://en.wikipedia.org/wiki/Base64)
         */

        static auto base64Encode(
            SAA_in_bcount_opt( bufferSize )     const void*                             buffer,
            SAA_in_opt                          const std::size_t                       bufferSize
            )
            -> std::string
        {
            if( ! bufferSize )
            {
                return std::string();
            }

            const std::size_t paddingCharactersCount = ( 3 - bufferSize % 3 ) % 3;

            const unsigned char* begin =  ( const unsigned char* ) buffer;

            std::string result(
                serial::to_base64_iterator_t( begin ),
                serial::to_base64_iterator_t( begin + bufferSize /* end */ )
                );

            result.append( paddingCharactersCount, '=' );

            return result;
        }

        /**
         * @brief Implements Base 64 Decoding with padding as per RFC4648
         * (more info also here: http://en.wikipedia.org/wiki/Base64)
         *
         * T can be anything that can be initialized off of a unsigned char
         * begin and end iterators - e.g. std::vector< unsigned char > or
         * std::string as well as support a default constructor for empty
         * buffer initialization
         *
         * It should be able to initialize like this:
         *
         * T value( begin(), end() );
         * T emptyValue;
         */

        template
        <
            typename T
        >
        static auto base64DecodeT( SAA_in const std::string& encoded ) -> T
        {
            if( ! encoded.size() )
            {
                return T();
            }

            std::size_t paddingCharactersCount = 0U;
            auto pos = encoded.crbegin();

            while( pos != encoded.crend() && '=' == *pos )
            {
                ++pos;
                ++paddingCharactersCount;
            }

            const auto end = encoded.begin() + ( encoded.size() - paddingCharactersCount );

            return T(
                serial::from_base64_iterator_t( encoded.begin() ),
                serial::from_base64_iterator_t( end )
                );
        }

        /**
         * @brief Wrapper for base64Encode
         */

        static auto base64EncodeString( SAA_in const std::string& text ) -> std::string
        {
            return base64Encode( text.c_str(), text.size() );
        }

        /**
         * @brief Wrapper for base64DecodeT
         */

        static auto base64DecodeString( SAA_in const std::string& encoded ) -> std::string
        {
            return base64DecodeT< std::string >( encoded );
        }

        /**
         * @brief Wrapper for base64DecodeT
         */

        static auto base64DecodeVector( SAA_in const std::string& encoded ) -> std::vector< unsigned char >
        {
            return base64DecodeT< std::vector< unsigned char > >( encoded );
        }

        /********************************************************************************************
         * Base64Url encoding and decoding
         */

        /**
         * @brief Implement Base 64 Encoding with URL and Filename Safe Alphabet
         * as per RFC4648 Section 5 (http://tools.ietf.org/html/rfc4648#section-5)
         *
         * A reference implementation in C# can be found here:
         * http://tools.ietf.org/html/draft-ietf-jose-json-web-signature-27#appendix-C
         */

        static auto base64UrlEncode(
            SAA_in_bcount_opt( bufferSize )     const void*                             buffer,
            SAA_in_opt                          const std::size_t                       bufferSize
            )
            -> std::string
        {
            /*
             * Basically base64 encode and then replace 62nd and 63rd chars and remove
             * trailing padding chars if any
             */

            auto encoded = base64Encode( buffer, bufferSize );

            str::trim_right_if( encoded, str::is_equal_to( '=' ) );

            std::replace( encoded.begin(), encoded.end(), '+', '-' );
            std::replace( encoded.begin(), encoded.end(), '/', '_' );

            return encoded;
        }

        /**
         * @brief Implement Base 64 Decoding with URL and Filename Safe Alphabet
         * as per RFC4648 Section 5 (http://tools.ietf.org/html/rfc4648#section-5)
         *
         * A reference implementation in C# can be found here:
         * http://tools.ietf.org/html/draft-ietf-jose-json-web-signature-27#appendix-C
         *
         * T can be anything that can be initialized off of a unsigned char
         * begin and end iterators - e.g. std::vector< unsigned char > or
         * std::string as well as support a default constructor for empty
         * buffer initialization
         *
         * It should be able to initialize like this:
         *
         * T value( begin(), end() );
         * T emptyValue;
         */

        template
        <
            typename T
        >
        static auto base64UrlDecodeT( SAA_in std::string&& encoded ) -> T
        {
            if( ! encoded.size() )
            {
                return T();
            }

            /*
             * Basically add trailing padding chars (if necessary), replace 62nd and
             * 63rd chars and then base64 decode
             */

            switch( encoded.size() % 4 )
            {
                default:
                    /*
                     * This should never happen
                     */
                    BL_ASSERT( false );
                    break;

                case 0:
                    /*
                     * No padding chars have to be added in this cases
                     */
                    break;

                case 1:
                    BL_THROW(
                        ArgumentException()
                            << eh::errinfo_string_value( encoded ),
                        BL_MSG()
                            << "Invalid base64url encoded string"
                        );
                    break;

                case 2:
                    encoded.append( 2U, '=' );
                    break;

                case 3:
                    encoded.append( 1U, '=' );
                    break;
            }

            std::replace( encoded.begin(), encoded.end(), '-', '+' );
            std::replace( encoded.begin(), encoded.end(), '_', '/' );

            return base64DecodeT< T >( encoded );
        }

        /**
         * @brief Wrapper for base64UrlDecodeT
         */

        template
        <
            typename T
        >
        static auto base64UrlDecodeT( SAA_in const std::string& encoded ) -> T
        {
            return base64UrlDecodeT< T >( cpp::copy( encoded ) );
        }

        /**
         * @brief Wrapper for base64UrlEncode
         */

        static auto base64UrlEncodeString( SAA_in const std::string& text ) -> std::string
        {
            return base64UrlEncode( text.c_str(), text.size() );
        }

        /**
         * @brief Wrapper for base64UrlDecodeT
         */

        static auto base64UrlDecodeString( SAA_in const std::string& encoded ) -> std::string
        {
            return base64UrlDecodeT< std::string >( encoded );
        }

        /**
         * @brief Wrapper for base64UrlDecodeT
         */

        static auto base64UrlDecodeVector( SAA_in const std::string& encoded ) -> std::vector< unsigned char >
        {
            return base64UrlDecodeT< std::vector< unsigned char > >( encoded );
        }

        static std::string encodeFromFileToBase64String(
            SAA_in          const fs::path&             path,
            SAA_out_opt     std::size_t*                originalFileSize = nullptr,
            SAA_in_opt      const std::size_t           maxSize = std::numeric_limits< std::size_t >::max()
            )
        {
            fs::SafeInputFileStreamWrapper file( path );
            auto& is = file.stream();
            is.seekg( 0, std::ios::end );

            const auto fileSize = static_cast< std::size_t >( is.tellg() );

            BL_CHK_USER_FRIENDLY(
                false,
                ( static_cast< std::istream::pos_type >( fileSize ) == is.tellg() ) && ( fileSize < maxSize ),
                BL_MSG()
                    << "Size of file "
                    << path
                    << " is too big ("
                    << is.tellg()
                    << " bytes). Maximum allowed is "
                    << maxSize
                    << " bytes"
                );

            is.seekg( 0, std::ios::beg );

            const auto buffer = cpp::SafeUniquePtr< char[] >::attach( new char[ fileSize ] );

            is.read( buffer.get(), fileSize );

            auto result = base64Encode( static_cast< const void* >( buffer.get() ), fileSize );

            if( originalFileSize )
            {
                *originalFileSize = fileSize;
            }

            return result;
        }

        static void decodeFromBase64StringToFile(
            SAA_in          const std::string&          encodedString,
            SAA_in          const fs::path&             outputPath
            )
        {
            const auto decodedString = base64DecodeString( encodedString );

            const auto parentDir = outputPath.parent_path();

            if( ! parentDir.empty() )
            {
                fs::safeMkdirs( parentDir );
            }

            fs::SafeOutputFileStreamWrapper file( outputPath );

            auto& os = file.stream();

            std::copy(
                decodedString.begin(),
                decodedString.end(),
                std::ostream_iterator< char >( os )
                );
        }
    };

    typedef SerializationUtilsT<> SerializationUtils;

} // bl

#endif /* __BL_SERIALIZATIONUTILS_H_ */

