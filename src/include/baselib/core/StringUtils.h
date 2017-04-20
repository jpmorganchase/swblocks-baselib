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

#ifndef __BL_STRINGUTILS_H_
#define __BL_STRINGUTILS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/algorithm/string.hpp>
#include <boost/locale/encoding.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <baselib/core/BaseIncludes.h>

#include <cstdarg>
#include <cstdio>
#include <limits>
#include <string>
#include <sstream>

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace bl
{
    namespace str
    {
        using boost::escaped_list_separator;
        using boost::escaped_list_error;
        using boost::is_any_of;
        using boost::is_from_range;
        using boost::is_space;
        using boost::split;
        using boost::tokenizer;
        using boost::to_lower;
        using boost::to_lower_copy;
        using boost::to_upper;
        using boost::to_upper_copy;

        using boost::regex;
        using boost::regex_match;
        using boost::regex_search;
        using boost::regex_iterator;
        using boost::sregex_iterator;
        using boost::cmatch;
        using boost::smatch;

        using boost::algorithm::ends_with;
        using boost::algorithm::iends_with;
        using boost::algorithm::icontains;
        using boost::algorithm::iequals;
        using boost::algorithm::istarts_with;
        using boost::algorithm::starts_with;
        using boost::algorithm::join;
        using boost::algorithm::replace_all;
        using boost::algorithm::ireplace_all;
        using boost::algorithm::replace_all_copy;
        using boost::algorithm::ireplace_all_copy;
        using boost::algorithm::replace_first;
        using boost::algorithm::ireplace_first;
        using boost::algorithm::replace_first_copy;
        using boost::algorithm::ireplace_first_copy;
        using boost::algorithm::replace_last;
        using boost::algorithm::ireplace_last;
        using boost::algorithm::replace_last_copy;
        using boost::algorithm::ireplace_last_copy;
        using boost::algorithm::trim;
        using boost::algorithm::trim_copy;
        using boost::algorithm::trim_copy_if;
        using boost::algorithm::trim_if;
        using boost::algorithm::trim_left;
        using boost::algorithm::trim_left_copy;
        using boost::algorithm::trim_left_copy_if;
        using boost::algorithm::trim_left_if;
        using boost::algorithm::trim_right;
        using boost::algorithm::trim_right_copy;
        using boost::algorithm::trim_right_copy_if;
        using boost::algorithm::trim_right_if;

        using boost::locale::conv::between;
        using boost::locale::conv::from_utf;
        using boost::locale::conv::to_utf;
        using boost::locale::conv::utf_to_utf;
        using boost::locale::conv::method_type;

        namespace regex_constants = boost::regex_constants;

        using boost::iterator_range;
        using boost::make_iterator_range;

        namespace format
        {
            using boost::format;
            using boost::str;

        } // format

        using cpp::secureWipe;

        namespace detail
        {
            const std::uint64_t KiloByte = 1024ul;
            const std::uint64_t MegaByte = KiloByte * KiloByte;
            const std::uint64_t GigaByte = KiloByte * MegaByte;
            const std::uint64_t TeraByte = KiloByte * GigaByte;

            /**
             * @brief class StringUtils - string utilities
             */

            template
            <
                typename E = void
            >
            class StringUtilsT FINAL
            {
                BL_DECLARE_STATIC( StringUtilsT )

                static const char                                       g_safeChars[];
                static const char                                       g_dec2Hex[];
                static const char                                       g_hex2Dec[];

            public:

                static const std::string                                g_emptyString;
                static const std::vector< std::string >                 g_emptyStringVector;
                static const std::set< std::string >                    g_emptyStringSet;

                static const std::string                                g_defaultSeparator;
                static const std::string                                g_defaultLastSeparator;
                static const std::string                                g_defaultHeader;
                static const std::string                                g_defaultFooter;
                static const std::string                                g_newLineSeparator;

                static std::string uriEncode(
                    SAA_in      const std::string&                      uri,
                    SAA_in      const std::string&                      excludedChars
                    )
                {
                    const char*             pSrc    = uri.c_str();
                    const std::size_t       srcLength = uri.length();

                    std::unique_ptr< char[] > pStart( new char[ srcLength * 3 ] );
                    char* pEnd = pStart.get();
                    const char* const       srcEnd = pSrc + srcLength;

                    for( ; pSrc < srcEnd; ++pSrc )
                    {
                        if(
                            g_safeChars[ static_cast< int >( *pSrc ) ] ||
                            excludedChars.find( *pSrc ) != std::string::npos
                            )
                        {
                            *pEnd++ = *pSrc;
                        }
                        else
                        {
                            /*
                             * escape this character
                             */

                            *pEnd++ = '%';
                            *pEnd++ = g_dec2Hex[ *pSrc >> 4 ];
                            *pEnd++ = g_dec2Hex[ *pSrc & 0x0F ];
                        }
                    }

                    return std::string( pStart.get(), pEnd );
                }

                static std::string uriDecode( SAA_in const std::string& uri )
                {
                    /* RFC 1630: sequences which start with a percent sign but are not
                     * followed by two hexadecimal characters (0-9, A-F) are reserved
                     * for future extension
                     *
                     * http://www.ietf.org/rfc/rfc1630.txt
                     */

                    const char*             pSrc    = uri.c_str();
                    const std::size_t       srcLength = uri.length();
                    const char* const       srcEnd = pSrc + srcLength;

                    /*
                     * last decodable '%' ( if any )
                     */

                    const char* const       srcLastDecodable = srcEnd - 2;

                    std::unique_ptr< char[] > pStart( new char[ srcLength ] );
                    char* pEnd = pStart.get();

                    while( pSrc < srcLastDecodable )
                    {
                        if( *pSrc == '%' )
                        {
                            char dec1, dec2;
                            if( -1 != ( dec1 = g_hex2Dec[ static_cast< int >( *( pSrc + 1 ) ) ] ) &&
                                -1 != ( dec2 = g_hex2Dec[ static_cast< int >( *( pSrc + 2 ) ) ] ) )
                            {

                                /*
                                 * decode this character
                                 */

                                *pEnd++ = ( dec1 << 4 ) + dec2;
                                pSrc   += 3;
                                continue;
                            }
                        }

                        *pEnd++ = *pSrc++;
                    }

                    while( pSrc < srcEnd )
                    {
                        *pEnd++ = *pSrc++;
                    }

                    return std::string( pStart.get(), pEnd );
                }

                template
                <
                    typename CONTAINER
                >
                static std::string joinFormattedImpl(
                    SAA_in      const CONTAINER&                                                    container,
                    SAA_in      const std::string&                                                  separator,
                    SAA_in      const std::string&                                                  lastSeparator,
                    SAA_in      const cpp::function<
                                    void(
                                        SAA_inout   std::ostream&                                   stream,
                                        SAA_in      const typename CONTAINER::value_type&           value
                                        )
                                    >                                                               formatter,
                    SAA_in_opt  const std::string&                                                  header = g_emptyString,
                    SAA_in_opt  const std::string&                                                  footer = g_emptyString
                    )
                {
                    cpp::SafeOutputStringStream stream;

                    stream << header;

                    for( typename CONTAINER::const_iterator i = container.begin(); i != container.end(); ++i )
                    {
                        if( i != container.begin() )
                        {
                            if( std::next( i ) != container.end() )
                            {
                                stream << separator;
                            }
                            else
                            {
                                stream << lastSeparator;
                            }
                        }

                        formatter( stream, *i );
                    }

                    stream << footer;

                    return stream.str();
                }

                template
                <
                    typename T
                >
                static std::string joinFormatted(
                    SAA_in      const std::vector< T >&                                             values,
                    SAA_in      const std::string&                                                  separator,
                    SAA_in      const std::string&                                                  lastSeparator,
                    SAA_in      const cpp::function<
                                    void(
                                        SAA_inout   std::ostream&                                   stream,
                                        SAA_in      const T&                                        value
                                        )
                                    >                                                               formatter
                    )
                {
                    return joinFormattedImpl( values, separator, lastSeparator, formatter );
                }

                static std::string dataRateFormatter(
                    SAA_in      const std::uint64_t                                                 value,
                    SAA_in      const int                                                           precision
                    )
                {
                    cpp::SafeOutputStringStream oss;

                    if( value < KiloByte )
                    {
                        oss << value << " B";
                    }
                    else if( value < MegaByte )
                    {
                        oss << std::fixed
                            << std::setprecision( precision )
                            << static_cast< double >( value ) / KiloByte
                            << " KB";
                    }
                    else if( value < GigaByte )
                    {
                        oss << std::fixed
                            << std::setprecision( precision )
                            << static_cast< double >( value ) / MegaByte
                            << " MB";
                    }
                    else if( value < TeraByte )
                    {
                        oss << std::fixed
                            << std::setprecision( precision )
                            << static_cast< double >( value ) / GigaByte
                            << " GB";
                    }
                    else
                    {
                        oss << std::fixed
                            << std::setprecision( precision )
                            << static_cast< double >( value ) / TeraByte
                            << " TB";
                    }

                    return oss.str();
                }

                static std::uint64_t dataRateParser( SAA_in const std::string& input )
                {
                    cpp::SafeInputStringStream iss( input );

                    double value = 0;
                    std::string unit;

                    if ( iss >> value )
                    {
                        if( iss >> unit )
                        {
                            str::to_upper( unit );

                            if( unit == "B" )
                            {
                            }
                            else if( unit == "K" || unit == "KB" )
                            {
                                value *= KiloByte;
                            }
                            else if( unit == "M" || unit == "MB" )
                            {
                                value *= MegaByte;
                            }
                            else if( unit == "G" || unit == "GB" )
                            {
                                value *= GigaByte;
                            }
                            else if( unit == "T" || unit == "TB" )
                            {
                                value *= TeraByte;
                            }
                            else
                            {
                                BL_THROW(
                                    ArgumentException(),
                                    BL_MSG()
                                        << "invalid size unit '"
                                        << unit
                                        << "'"
                                    );
                            }
                        }

                        if( value >= 0 && value <= std::numeric_limits< std::uint64_t >::max() )
                        {
                            return static_cast< std::uint64_t >( value );
                        }
                    }

                    BL_THROW(
                        ArgumentException(),
                        "invalid value"
                        );
                }

                static std::string formatPercent(
                    SAA_in      const double                                                        value,
                    SAA_in      const double                                                        total,
                    SAA_in      const int                                                           precision
                    )
                {
                    cpp::SafeOutputStringStream oss;

                    oss << std::fixed
                        << std::setprecision( precision )
                        << value / total * 100
                        << "%";

                    return oss.str();
                }

                /**
                 * @brief vprintf-like message formatter to support 3rd party C code
                 */

                static std::string formatMessage(
                    SAA_in      const char*                                 format,
                    SAA_in      va_list                                     args
                    )
                {
                    BL_CHK_T(
                        nullptr,
                        format,
                        ArgumentNullException(),
                        "format must not be null"
                        );

                    std::string result;

                    /*
                     * Determine the required buffer size using a copy of the argument list
                     * (MSVC does not implement va_copy yet)
                     */

#if defined( _WIN32 )
                    va_list copy = args;
#else
                    va_list copy;
                    va_copy( copy, args );
#endif

                    auto count = ::vsnprintf( nullptr, 0, format, copy );

                    va_end( copy );

                    if( count > 0 )
                    {
                        /*
                         * Format the message into the buffer
                         */

                        auto buffer = cpp::SafeUniquePtr< char[] >::attach( new char[ count + 1 ] );

                        count = ::vsnprintf( buffer.get(), count + 1, format, args );

                        if( count > 0 )
                        {
                            result.assign( buffer.get(), count );
                        }
                    }

                    return result;
                }
            };

            BL_DEFINE_STATIC_CONST_STRING( StringUtilsT, g_emptyString );
            BL_DEFINE_STATIC_MEMBER( StringUtilsT, const std::vector< std::string >, g_emptyStringVector );
            BL_DEFINE_STATIC_MEMBER( StringUtilsT, const std::set< std::string >, g_emptyStringSet );

            BL_DEFINE_STATIC_CONST_STRING( StringUtilsT, g_defaultSeparator )       = ", " ;
            BL_DEFINE_STATIC_CONST_STRING( StringUtilsT, g_defaultLastSeparator )   = " and ";
            BL_DEFINE_STATIC_CONST_STRING( StringUtilsT, g_defaultHeader )          = "{";
            BL_DEFINE_STATIC_CONST_STRING( StringUtilsT, g_defaultFooter )          = "}";
            BL_DEFINE_STATIC_CONST_STRING( StringUtilsT, g_newLineSeparator )       = "\n";

            /*
             * Safe characters as defined by http://www.ietf.org/rfc/rfc1630.txt
             */

            BL_DEFINE_STATIC_MEMBER( StringUtilsT, const char, g_safeChars[] ) =
            {
                /*      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
                /* 0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* 1 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* 2 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* 3 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,

                /* 4 */ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                /* 5 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
                /* 6 */ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                /* 7 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,

                /* 8 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* 9 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* A */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* B */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

                /* C */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* D */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* E */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                /* F */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };

            BL_DEFINE_STATIC_MEMBER( StringUtilsT, const char, g_hex2Dec[] ) =
            {
                /*       0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
                /* 0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 1 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 2 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 3 */  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,

                /* 4 */ -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 5 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 6 */ -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 7 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                /* 8 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* 9 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* A */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* B */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                /* C */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* D */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* E */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                /* F */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
            };

            BL_DEFINE_STATIC_MEMBER( StringUtilsT, const char, g_dec2Hex[] ) = "0123456789ABCDEF";

            typedef StringUtilsT<> StringUtils;

        } // detail

        template
        <
            typename CharT
        >
        inline auto is_equal_to( SAA_in const CharT ch ) -> decltype( is_from_range( ch, ch ) )
        {
            return is_from_range( ch, ch );
        }

        inline const std::string& empty() NOEXCEPT
        {
            BL_ASSERT( detail::StringUtils::g_emptyString.empty() );

            return detail::StringUtils::g_emptyString;
        }

        inline const std::vector< std::string >& emptyVector() NOEXCEPT
        {
            BL_ASSERT( detail::StringUtils::g_emptyStringVector.empty() );

            return detail::StringUtils::g_emptyStringVector;
        }

        inline const std::set< std::string >& emptySet() NOEXCEPT
        {
            BL_ASSERT( detail::StringUtils::g_emptyStringSet.empty() );

            return detail::StringUtils::g_emptyStringSet;
        }

        inline const std::string& defaultSeparator() NOEXCEPT
        {
            return detail::StringUtils::g_defaultSeparator;
        }

        inline const std::string& defaultLastSeparator() NOEXCEPT
        {
            return detail::StringUtils::g_defaultLastSeparator;
        }

        inline std::string uriEncode(
            SAA_in      const std::string&                          uri,
            SAA_in_opt  const std::string&                          excludedChars = empty()
            )
        {
            return detail::StringUtils::uriEncode( uri, excludedChars );
        }

        inline std::string uriDecode( SAA_in const std::string& uri )
        {
            return detail::StringUtils::uriDecode( uri );
        }

        inline bool toBool( SAA_in const std::string& input )
        {
            cpp::SafeInputStringStream is( to_lower_copy( input ) );
            bool b;

            is >> std::boolalpha >> b;

            if( is.fail() )
            {
                BL_THROW_USER_FRIENDLY(
                    ArgumentException(),
                    BL_MSG()
                        << "The string '"
                        << input
                        << "' cannot be converted to boolean"
                    );
            }

            return b;
        }

        inline std::string getBeginning(
            SAA_in      const std::string&                          text,
            SAA_in      const std::size_t                           length
            )
        {
            if( length < text.size() )
            {
                return std::string( text.begin(), text.begin() + length );
            }

            return text;
        }

        inline std::string getEnding(
            SAA_in      const std::string&                          text,
            SAA_in      const std::size_t                           length
            )
        {
            if( length < text.size() )
            {
                return std::string( text.end() - length, text.end() );
            }

            return text;
        }

        template
        <
            typename T
        >
        inline void defaultFormatter(
            SAA_inout   std::ostream&                               stream,
            SAA_in      const T&                                    value
            )
        {
            stream << value;
        }

        template
        <
            typename K,
            typename V
        >
        inline void defaultPairFormatter(
            SAA_inout   std::ostream&                               stream,
            SAA_in      const std::pair< K, V >&                    pair
            )
        {
            stream
                << detail::StringUtils::g_defaultHeader
                << pair.first
                << detail::StringUtils::g_defaultSeparator
                << pair.second
                << detail::StringUtils::g_defaultFooter;
        }

        template
        <
            typename T
        >
        inline void quoteFormatter(
            SAA_inout   std::ostream&                               stream,
            SAA_in      const T&                                    value
            )
        {
            stream << "'" << value << "'";
        }

        inline std::string dataRateFormatter(
            SAA_in      const std::uint64_t                         value,
            SAA_in      const int                                   precision = 3
            )
        {
            return detail::StringUtils::dataRateFormatter( value, precision );
        }

        inline std::uint64_t dataRateParser( SAA_in const std::string& input )
        {
            return detail::StringUtils::dataRateParser( input );
        }

        /**
         * @brief Calculates percentage and returns formatted string
         * Result would look like "12.3%" with the default precision
         * Templates were added to allow passing e.g. double 'value' and
         * uint_64 'total' w/o compiler complaining about data loss
         */

        template
        <
            typename T,
            typename U
        >
        inline std::string formatPercent(
            SAA_in      const T                                                             value,
            SAA_in      const U                                                             total,
            SAA_in      const int                                                           precision = 1
            )
        {
            return detail::StringUtils::formatPercent(
                static_cast< double >( value ),
                static_cast< double >( total ),
                precision
                );
        }

        inline std::string formatTime(
            SAA_in      const std::time_t           time,
            SAA_in      const char*                 format,
            SAA_in_opt  const bool                  useUtcTime = false
            )
        {
            struct ::tm tm;

#if defined( _WIN32 )
            const bool validTime = ( useUtcTime ? ::gmtime_s( &tm, &time ) : ::localtime_s( &tm, &time ) ) == 0;
#else
            const bool validTime = ( useUtcTime ? ::gmtime_r( &time, &tm ) : ::localtime_r( &time, &tm ) ) != nullptr;
#endif
            if( ! validTime )
            {
                return "Invalid time";
            }

            char buffer[ 64 ];
            buffer[ 0 ] = '\0';

            BL_CHK(
                0U,
                std::strftime( buffer, BL_ARRAY_SIZE( buffer ), format, &tm ),
                BL_MSG()
                    << "Unable to format time: insufficient buffer size for format string '"
                    << format
                    << "'"
                );

            return buffer;
        }

        template
        <
            typename T
        >
        inline std::string joinFormatted(
            SAA_in      const std::vector< T >&                     values,
            SAA_in_opt  const std::string&                          separator       = detail::StringUtils::g_defaultSeparator,
            SAA_in_opt  const std::string&                          lastSeparator   = detail::StringUtils::g_defaultLastSeparator
            )
        {
            return detail::StringUtils::joinFormatted< T >( values, separator, lastSeparator, &defaultFormatter< T > );
        }

        template
        <
            typename T
        >
        inline std::string joinQuoteFormatted(
            SAA_in      const std::vector< T >&                     values,
            SAA_in_opt  const std::string&                          separator       = detail::StringUtils::g_defaultSeparator,
            SAA_in_opt  const std::string&                          lastSeparator   = detail::StringUtils::g_defaultLastSeparator
            )
        {
            return detail::StringUtils::joinFormatted< T >( values, separator, lastSeparator, &quoteFormatter< T > );
        }

        template
        <
            typename T
        >
        inline std::string joinNewLineFormatted( SAA_in const std::vector< T >& values )
        {
            return detail::StringUtils::joinFormatted< T >(
                values,
                detail::StringUtils::g_newLineSeparator,
                detail::StringUtils::g_newLineSeparator,
                &defaultFormatter< T >
                );
        }

        template
        <
            typename MAP
        >
        inline std::string joinQuoteFormattedKeys(
            SAA_in      const MAP&                                  map,
            SAA_in_opt  const std::string&                          separator       = detail::StringUtils::g_defaultSeparator,
            SAA_in_opt  const std::string&                          lastSeparator   = detail::StringUtils::g_defaultLastSeparator
            )
        {
            typedef typename MAP::key_type key_t;

            std::vector< key_t > keys;
            keys.reserve( map.size() );

            for( const auto& pair : map )
            {
                keys.emplace_back( pair.first );
            }

            return detail::StringUtils::joinFormatted< key_t >( keys, separator, lastSeparator, &quoteFormatter< key_t > );
        }

        template
        <
            typename T
        >
        inline std::string joinFormatted(
            SAA_in      const std::vector< T >&                     values,
            SAA_in      const std::string&                          separator,
            SAA_in      const std::string&                          lastSeparator,
            SAA_in      const cpp::function<
                            void(
                                SAA_inout   std::ostream&           stream,
                                SAA_in      const T&                value
                                )
                            >                                       formatter
            )
        {
            return detail::StringUtils::joinFormatted< T >( values, separator, lastSeparator, formatter );
        }

        template
        <
            typename T
        >
        inline std::string vectorToString( SAA_in const std::vector< T >& values )
        {
            return detail::StringUtils::joinFormattedImpl(
                values,
                detail::StringUtils::g_defaultSeparator, /* separator */
                detail::StringUtils::g_defaultSeparator, /* lastSeparator */
                &defaultFormatter< T >,
                detail::StringUtils::g_defaultHeader, /* header */
                detail::StringUtils::g_defaultFooter  /* footer */
                );
        }

        template
        <
            typename T
        >
        inline std::string setToString( SAA_in const std::set< T >& values )
        {
            return detail::StringUtils::joinFormattedImpl(
                values,
                detail::StringUtils::g_defaultSeparator, /* separator */
                detail::StringUtils::g_defaultSeparator, /* lastSeparator */
                &defaultFormatter< T >,
                detail::StringUtils::g_defaultHeader, /* header */
                detail::StringUtils::g_defaultFooter  /* footer */
                );
        }

        template
        <
            typename T
        >
        inline std::string setToString( SAA_in const std::unordered_set< T >& values )
        {
            return detail::StringUtils::joinFormattedImpl(
                values,
                detail::StringUtils::g_defaultSeparator, /* separator */
                detail::StringUtils::g_defaultSeparator, /* lastSeparator */
                &defaultFormatter< T >,
                detail::StringUtils::g_defaultHeader, /* header */
                detail::StringUtils::g_defaultFooter  /* footer */
                );
        }

        template
        <
            typename K,
            typename V
        >
        inline std::string mapToString( SAA_in const std::map< K, V >& values )
        {
            return detail::StringUtils::joinFormattedImpl(
                values,
                detail::StringUtils::g_defaultSeparator, /* separator */
                detail::StringUtils::g_defaultSeparator, /* lastSeparator */
                &defaultPairFormatter< K, V >,
                detail::StringUtils::g_defaultHeader, /* header */
                detail::StringUtils::g_defaultFooter  /* footer */
                );
        }

        template
        <
            typename K,
            typename V
        >
        inline std::string mapToString( SAA_in const std::unordered_map< K, V >& values )
        {
            return detail::StringUtils::joinFormattedImpl(
                values,
                detail::StringUtils::g_defaultSeparator, /* separator */
                detail::StringUtils::g_defaultSeparator, /* lastSeparator */
                &defaultPairFormatter< K, V >,
                detail::StringUtils::g_defaultHeader, /* header */
                detail::StringUtils::g_defaultFooter  /* footer */
                );
        }

        inline std::string formatMessage(
            SAA_in      const char*                                 format,
            SAA_in      va_list                                     args
            )
        {
            return detail::StringUtils::formatMessage( format, args );
        }

        inline std::vector< std::string > splitString(
            SAA_in      const std::string&                          text,
            SAA_in      const std::string&                          separator,
            SAA_in_opt  const std::size_t                           startPos = 0,
            SAA_in_opt  std::size_t                                 endPos = std::string::npos
            )
        {
            std::vector< std::string > result;

            const auto length = text.length();
            const auto separatorLength = separator.length();

            if( endPos == std::string::npos )
            {
                endPos = text.length();
            }

            BL_CHK_T(
                false,
                separatorLength && startPos <= length && endPos <= length && startPos <= endPos,
                ArgumentException(),
                BL_MSG()
                    << "Invalid arguments passed to splitString"
                );

            if( separatorLength > ( endPos - startPos ) )
            {
                result.push_back( text );

                return result;
            }

            auto currentPos = startPos;

            for( ;; )
            {
                const auto pos = text.find( separator, currentPos );

                if( pos != std::string::npos && pos < endPos )
                {
                    result.push_back( text.substr( currentPos, pos - currentPos ) );

                    currentPos = pos + separatorLength;

                    if( currentPos != endPos )
                    {
                        continue;
                    }
                }

                result.push_back( text.substr( currentPos, endPos - currentPos ) );
                break;
            }

            return result;
        }

        inline std::string quoteString( SAA_in const std::string& text )
        {
            cpp::SafeOutputStringStream buffer;

            quoteFormatter( buffer, text );

            return buffer.str();
        }

        inline std::string unquoteString( SAA_in std::string&& textIn )
        {
            auto text( BL_PARAM_FWD( textIn ) );

            if( text.size() > 1 && text.front() == '"' && text.back() == '"' )
            {
                text.erase( 0, 1 );
                text.pop_back();
            }

            return text;
        }

        template
        <
            typename MAP = std::unordered_map< std::string, std::string >
        >
        inline auto parsePropertiesList(
            SAA_in      const std::string&                          properties,
            SAA_in_opt  const char                                  separator = ';'
            )
            -> MAP
        {
            MAP result;

            std::vector< std::string > propertiesList;

            str::split( propertiesList, properties, str::is_equal_to( separator ) );

            for( auto& property : propertiesList )
            {
                str::trim( property );

                if( property.empty() )
                {
                    continue;
                }

                const auto pos = property.find( '=' );

                BL_CHK_T(
                    false,
                    std::string::npos != pos && 0U != pos,
                    InvalidDataFormatException()
                        << eh::errinfo_is_user_friendly( true ),
                    BL_MSG()
                        << "Cannot parse property in 'name=value' format"
                    );

                const auto pair = result.emplace(
                    property.substr( 0, pos )       /* name */,
                    property.substr( pos + 1U )     /* value */
                    );

                BL_CHK_T(
                    false,
                    pair.second,
                    InvalidDataFormatException()
                        << eh::errinfo_is_user_friendly( true ),
                    BL_MSG()
                        << "Duplicate property encountered name while parsing "
                        << "concatenated properties in 'name=value' format"
                    );
            }

            return result;
        }

    } // str

} // bl

#endif /* __BL_STRINGUTILS_H_ */
