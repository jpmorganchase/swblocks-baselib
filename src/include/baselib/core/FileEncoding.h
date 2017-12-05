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

#ifndef __BL_FILEENCODING_H_
#define __BL_FILEENCODING_H_

#include <baselib/core/FsUtils.h>
#include <baselib/core/OS.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace encoding
    {
        enum class TextFileEncoding : std::uint32_t
        {
            Unknown             = 0U,
            Utf8_NoPreamble     = 1U,
            Utf16LE_NoPreamble  = 2U,

            Preamble            = 1U << 8,

            Utf8                = Utf8_NoPreamble | Preamble,
            Utf16LE             = Utf16LE_NoPreamble | Preamble,

            Ascii               = Utf8_NoPreamble,
        };

        namespace detail
        {
            /**
             * @brief class FileEncoding
             */

            template
            <
                typename E = void
            >
            class FileEncodingT FINAL
            {
            public:

                static void writeTextFile(
                    SAA_in      const fs::path&                             fileName,
                    SAA_in      const std::string&                          content,
                    SAA_in      const TextFileEncoding                      encoding
                    )
                {
                    const fs::path parentDir = fileName.parent_path();

                    if( ! parentDir.empty() )
                    {
                        fs::safeMkdirs( parentDir );
                    }

                    const auto file = os::fopen( fileName, "wb" );

                    switch( encoding )
                    {
                        case TextFileEncoding::Utf8:
                            os::fwrite( file, g_utf8Preamble.data(), g_utf8Preamble.size() );
                            /* no break */

                        case TextFileEncoding::Utf8_NoPreamble:
                            if( ! content.empty() )
                            {
                                os::fwrite( file, content.data(), content.size() );
                            }
                            break;

#if defined( _WIN32 )

                        case TextFileEncoding::Utf16LE:
                            os::fwrite( file, g_utf16LEPreamble.data(), g_utf16LEPreamble.size() );
                            /* no break */

                        case TextFileEncoding::Utf16LE_NoPreamble:
                            if( ! content.empty() )
                            {
                                /*
                                 * NOTE: wchar_t is a platform-specific type and it cannot be portably used
                                 * as a UTF-16 / UCS-2 'character' type (it just happens to be the case on Windows)
                                 */

                                cpp::wstring_convert_t conv;

                                const auto wideContent = conv.from_bytes( content );

                                os::fwrite( file, wideContent.data(), wideContent.size() * sizeof( wchar_t ) );
                            }
                            break;

#else // defined( _WIN32 )

                        case TextFileEncoding::Utf16LE:
                        case TextFileEncoding::Utf16LE_NoPreamble:
                            BL_THROW(
                                NotSupportedException(),
                                BL_MSG()
                                    << "Writing UTF-16LE encoded file "
                                    << fileName
                                    << " is not supported on this platform"
                                );
                            break;

#endif // defined( _WIN32 )

                        default:
                            BL_THROW(
                                UnexpectedException(),
                                BL_MSG()
                                    << "Invalid TextFileEncoding: "
                                    << ( int ) encoding
                                );
                    }
                }

                static std::string readTextFile(
                    SAA_in      const fs::path&                             fileName,
                    SAA_in_opt  TextFileEncoding*                           encodingPtr
                    )
                {
                    TextFileEncoding encoding = TextFileEncoding::Unknown;

                    std::string result;

                    const std::uint64_t size64 = fs::file_size( fileName );

                    /*
                     * This check is necessary to ensure the cast bellow to
                     * std::size_t is correct and safe
                     */

                    BL_CHK_USER_FRIENDLY(
                        false,
                        size64 < std::numeric_limits< std::size_t >::max(),
                        BL_MSG()
                            << "File "
                            << fileName
                            << " is too large ("
                            << size64
                            << " bytes)"
                        );

                    std::size_t size = ( std::size_t ) size64;

                    const auto file = os::fopen( fileName, "rb" );

                    if( checkFilePreamble( file, size, g_utf8Preamble ) )
                    {
                        encoding = TextFileEncoding::Utf8;

                        size -= g_utf8Preamble.size();

                        if( size > 0 )
                        {
                            result.resize( size );

                            os::fread( file, &result[ 0 ], size );
                        }
                    }
                    else if( checkFilePreamble( file, size, g_utf16LEPreamble ) )
                    {
                        encoding = TextFileEncoding::Utf16LE;

                        size -= g_utf16LEPreamble.size();

#if defined( _WIN32 )

                        BL_CHK_USER_FRIENDLY(
                            false,
                            size % sizeof( wchar_t ) == 0,
                            BL_MSG()
                                << "The size of Unicode file "
                                << fileName
                                << " "
                                << size
                                << " is not divisible by wchar_t size "
                                << sizeof( wchar_t )
                            );

                        if( size > 0 )
                        {
                            const auto length = size / sizeof( wchar_t );
                            std::wstring buffer( length, wchar_t() );

                            os::fread( file, &buffer[ 0 ], size );

                            /*
                             * See the comment for typedef wstring_convert_t
                             */

                            cpp::wstring_convert_t conv;

                            result = conv.to_bytes( buffer );
                        }

#else // defined( _WIN32 )

                        BL_THROW(
                            NotSupportedException(),
                            BL_MSG()
                                << "Reading UTF-16LE encoded file "
                                << fileName
                                << " is not supported on this platform"
                            );

#endif // defined( _WIN32 )
                    }
                    else
                    {
                        /*
                         * Files without a preamble need to be analyzed to guess their encoding
                         *
                         * Assume they are simple ASCII files for now
                         */

                        encoding = TextFileEncoding::Ascii;

                        if( size > 0 )
                        {
                            result.resize( size );

                            os::fread( file, &result[ 0 ], size );
                        }
                    }

                    if( encodingPtr )
                    {
                        *encodingPtr = encoding;
                    }

                    return result;
                }

            private:

                static bool checkFilePreamble(
                    SAA_in          const os::stdio_file_ptr&           file,
                    SAA_in          const std::size_t                   size,
                    SAA_in          const std::string&                  preamble
                    )
                {
                    if( size >= preamble.size() )
                    {
                        std::string buffer( preamble.size(), char() );

                        os::fread( file, &buffer[ 0 ], buffer.size() );

                        if( buffer == preamble )
                        {
                            return true;
                        }

                        os::fseek( file, bl::os::ftell( file ) - buffer.size(), SEEK_SET );
                    }

                    return false;
                }

                static const std::string g_utf8Preamble;
                static const std::string g_utf16LEPreamble;
            };

            BL_DEFINE_STATIC_CONST_STRING( FileEncodingT, g_utf8Preamble )          = "\xef\xbb\xbf";
            BL_DEFINE_STATIC_CONST_STRING( FileEncodingT, g_utf16LEPreamble )       = "\xff\xfe";

            typedef FileEncodingT<> FileEncoding;

        } // detail

        /**
         * @brief Reads a text file into a UTF-8 string and returns the original encoding
         *
         * The path is expected to be absolute
         */

        inline std::string readTextFile(
            SAA_in      const fs::path&                             fileName,
            SAA_in_opt  TextFileEncoding*                           encodingPtr = nullptr
            )
        {
            return detail::FileEncoding::readTextFile( fileName, encodingPtr );
        }

        /**
         * @brief Writes a UTF-8 string as a text file with the specified encoding
         *
         * The path is expected to be absolute
         *
         * Creates the parent path if it doesn't exist
         */

        inline void writeTextFile(
            SAA_in      const fs::path&                             fileName,
            SAA_in      const std::string&                          content,
            SAA_in_opt  const TextFileEncoding                      encoding = TextFileEncoding::Utf8
            )
        {
            return detail::FileEncoding::writeTextFile( fileName, content, encoding );
        }

    } // encoding

} // bl

#endif /* __BL_FILEENCODING_H_ */
