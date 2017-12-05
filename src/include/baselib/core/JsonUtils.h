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

#ifndef __BL_JSONUTILS_H_
#define __BL_JSONUTILS_H_

/*
 * Below define is used by the Boost Spirit headers to make the parser thread safe
 */

#include <baselib/core/StringUtils.h>
#include <baselib/core/Utils.h>

#include <iostream>

#define BOOST_SPIRIT_THREADSAFE

#include <json_spirit/json_spirit_reader_template.h>
#include <json_spirit/json_spirit_writer_template.h>

namespace bl
{
    namespace json
    {
        /*
         * We only import the types we need (below)
         *
         * We are going to use map based implementation of Object, so the properties
         * are always ordered alphabetically and output is naturally 'canonicalized'
         */

        typedef json_spirit::Output_options                         OutputOptions;
        typedef json_spirit::Value_type                             ValueType;

        namespace detail
        {
            struct ConfigMap
            {
                typedef std::string                                 String_type;
                typedef json_spirit::Value_impl< ConfigMap >        Value_type;
                typedef std::vector< Value_type >                   Array_type;
                typedef std::map< String_type, Value_type >         Object_type;
                typedef std::pair< String_type, Value_type >        Pair_type;

                static Value_type& add( Object_type& obj, const String_type& name, const Value_type& value )
                {
                    auto pair = obj.emplace( name, value );

                    if( ! pair.second )
                    {
                        BL_THROW_USER(
                            BL_MSG()
                                << "Duplicate entry encountered for property with name '"
                                << name
                                << "' while parsing a JSON object"
                            );
                    }

                    return pair.first -> second;
                }

                static String_type get_name( const Pair_type& pair )
                {
                    return pair.first;
                }

                static Value_type get_value( const Pair_type& pair )
                {
                    return pair.second;
                }
            };

        } // detail

        typedef detail::ConfigMap::Value_type                       Value;
        typedef detail::ConfigMap::Object_type                      Object;
        typedef detail::ConfigMap::Array_type                       Array;

        namespace detail
        {
            /*
             * These don't need to be exposed directly due to error handling issues,
             * but the readFromString and writeToString wrappers should be used instead
             */

            using json_spirit::Error_position;

            using json_spirit::read_string_or_throw;
            using json_spirit::read_string;
            using json_spirit::write_string;

            using json_spirit::read_stream_or_throw;
            using json_spirit::read_stream;
            using json_spirit::write_stream;

        } // detail

        namespace detail
        {
            /**
             * @brief class JsonUtils - JSON utility code
             */

            template
            <
                typename E = void
            >
            class JsonUtilsT
            {
                BL_DECLARE_STATIC( JsonUtilsT )

            private:

                enum
                {
                    MAX_DUMP_STRING_LENGTH = 1024
                };

                static os::mutex                                                        g_lock;
                static const str::regex                                                 g_valueTypeRegex;

                typedef cpp::function< void ( SAA_inout json::Value& rootValue ) >      read_callback_t;
                typedef cpp::function< bool ( SAA_inout json::Value& rootValue ) >      fast_read_callback_t;

                static json::Value readWrapper(
                    SAA_in                  const read_callback_t&                      callback,
                    SAA_in                  const fast_read_callback_t&                 fastCallback,
                    SAA_in_opt              const cpp::void_callback_t&                 dumpCallback = cpp::void_callback_t()
                    )
                {
                    /*
                     * As per:
                     * http://www.codeproject.com/Articles/20027/JSON-Spirit-A-C-JSON-Parser-Generator-Implemented
                     *
                     * The fast callback is ~ 3x faster, but does not throw an
                     * exception with the error info, so we need to call the slow
                     * callback when there is an error parsing to get the error information
                     */

                    json::Value rootValue;

                    if( fastCallback( rootValue ) )
                    {
                        return rootValue;
                    }

                    /*
                     * Even though defining BOOST_SPIRIT_THREADSAFE should make the parser
                     * thread safe, there is still a bug exposed when JSON parsing from
                     * multiple threads using the "slow" callback, hence a lock is used
                     */

                    BL_MUTEX_GUARD( g_lock );

                    try
                    {
                        rootValue = json::Value();

                        callback( rootValue );
                    }
                    catch( detail::Error_position& e )
                    {
                        if( dumpCallback )
                        {
                            dumpCallback();
                        }

                        /*
                         * The JSON spirit throws Error_position structure in case of
                         * parsing error, convert it to regular exception type
                         */

                        BL_THROW(
                            JsonException()
                                << eh::errinfo_parser_line( e.line_ )
                                << eh::errinfo_parser_column( e.column_ )
                                << eh::errinfo_parser_reason( e.reason_ )
                                ,
                            BL_MSG()
                                << "JSON parser error at line: "
                                << e.line_
                                << ", column: "
                                << e.column_
                                << ", reason: '"
                                << e.reason_
                                << "'"
                            );
                    }
                    catch( std::runtime_error& e )
                    {
                        remapIncorrectValueTypeException( e, std::current_exception(), "JSON string" );
                    }

                    /*
                     * If we are here then something is very wrong because the callback
                     * call above is expected to produce the same result as fastCallback
                     * (i.e. to fail), but throw an exception instead and the catch blocks
                     * are also expected to throw / re-throw
                     */

                    BL_RIP_MSG( "JSON parsing callback is expected to throw" );

                    return rootValue;
                }

            public:

                static json::Value readFromString( SAA_in const std::string& input )
                {
                    return readWrapper(
                        cpp::bind( &detail::read_string_or_throw< std::string, json::Value >, cpp::cref( input ), _1 ),
                        cpp::bind( &detail::read_string< std::string, json::Value >, cpp::cref( input ), _1 ), /* fast CB */
                        [ &input ]() -> void
                        {
                            /*
                             * This is the dump callback which will be called in case of an error
                             */

                            BL_LOG_MULTILINE(
                                Logging::debug(),
                                BL_MSG()
                                    << "Invalid JSON string:\n"
                                    << ( input.size() < MAX_DUMP_STRING_LENGTH ?
                                            input :
                                            input.substr( 0, MAX_DUMP_STRING_LENGTH ) + "..."
                                            )
                                );
                        }
                        );
                }

                template
                <
                    typename STREAM
                >
                static json::Value readFromStream( SAA_inout STREAM& input )
                {
                    input.exceptions( std::ios::badbit );

                    return readWrapper(
                        cpp::bind( &detail::read_stream_or_throw< STREAM, json::Value >, cpp::ref( input ), _1 ),
                        cpp::bind( &detail::read_stream< STREAM, json::Value >, cpp::ref( input ), _1 ), /* fast CB */
                        [ &input ]() -> void
                        {
                            /*
                             * This is the dump callback which will be called in case of an error
                             */

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Invalid JSON blob parsed from stream"
                                );
                        }
                        );
                }

                static std::string saveToString(
                    SAA_in          const json::Value&                      value,
                    SAA_in          const unsigned int                      options
                    )
                {
                    return detail::write_string( value, options );
                }

                static std::string saveToString(
                    SAA_in          const json::Object&                     rootObject,
                    SAA_in          const unsigned int                      options
                    )
                {
                    return saveToString( json::Value( rootObject ), options );
                }

                static std::string saveToString(
                    SAA_in          const json::Array&                      array,
                    SAA_in          const unsigned int                      options
                    )
                {
                    return saveToString( json::Value( array ), options );
                }

                template
                <
                    typename STREAM
                >
                static void saveToStream(
                    SAA_in          const json::Value&                      value,
                    SAA_inout       STREAM&                                 output,
                    SAA_in          const unsigned int                      options
                    )
                {
                    detail::write_stream( value, output, options );
                }

                static void remapIncorrectValueTypeException(
                    SAA_in      const std::runtime_error&           e,
                    SAA_in      const std::exception_ptr&           eptr,
                    SAA_in      const std::string&                  context,
                    SAA_in_opt  const bool                          userException = false
                    )
                {
                    /*
                     * JSON Spirit parser throws cryptic "value type is X not Y" exception
                     * when the requested type from a getter doesn't match the underlying
                     * variant value - see check_type() in json_spirit_value.h.
                     * Translate this to more informational exception here.
                     */

                    bool isUserFriendly = false;
                    std::string message;
                    str::cmatch results;

                    if( str::regex_match( e.what(), results, g_valueTypeRegex ) )
                    {
                        const std::string actualTypeStr = results[ 2 ];
                        const std::string expectedTypeStr = results[ 1 ];

                        message = resolveMessage(
                            BL_MSG()
                                << "JSON parsing error: expected value type is '"
                                << expectedTypeStr
                                << "' while actual type is '"
                                << actualTypeStr
                                << "' for "
                                << context
                            );

                        isUserFriendly = true;
                    }
                    else
                    {
                        message = e.what();
                    }

                    if( userException )
                    {
                        BL_THROW(
                            UserMessageException()
                                << eh::errinfo_nested_exception_ptr( eptr ),
                            message
                            );
                    }
                    else
                    {
                        if( isUserFriendly )
                        {
                            BL_THROW_USER_FRIENDLY(
                                JsonException()
                                    << eh::errinfo_nested_exception_ptr( eptr ),
                                message
                                );
                        }
                        else
                        {
                            BL_THROW(
                                JsonException()
                                    << eh::errinfo_nested_exception_ptr( eptr ),
                                message
                                );
                        }
                    }

                    throw e;
                }
            };

            BL_DEFINE_STATIC_MEMBER( JsonUtilsT, os::mutex, g_lock );
            BL_DEFINE_STATIC_MEMBER( JsonUtilsT, const str::regex, g_valueTypeRegex )( "get_value< (\\w+) > called on (\\w+) Value" );

            typedef JsonUtilsT<> JsonUtils;
        }

        inline json::Value readFromString( SAA_in const std::string& input )
        {
            return detail::JsonUtils::readFromString( input );
        }

        template
        <
            typename STREAM
        >
        inline json::Value readFromStream( SAA_in STREAM& input )
        {
            return detail::JsonUtils::readFromStream< STREAM >( input );
        }

        template
        <
            typename T
        >
        inline std::string saveToString(
            SAA_in          const T&                                json,
            SAA_in_opt      const bool                              prettyPrint = false,
            SAA_in_opt      const bool                              rawUTF8 = false
            )
        {
            return detail::JsonUtils::saveToString(
                json,
                ( prettyPrint ? OutputOptions::pretty_print : 0 ) | ( rawUTF8 ? OutputOptions::raw_utf8 : 0 )
                );
        }

        template
        <
            typename STREAM
        >
        inline void saveToStream(
            SAA_in          const json::Value&                      value,
            SAA_inout       STREAM&                                 output,
            SAA_in          const unsigned int                      options
            )
        {
            detail::JsonUtils::saveToStream( value, output, options );
        }

        inline static void remapIncorrectValueTypeException(
            SAA_in      const std::runtime_error&           e,
            SAA_in      const std::exception_ptr&           eptr,
            SAA_in      const std::string&                  context,
            SAA_in_opt  const bool                          userException = false
            )
        {
            detail::JsonUtils::remapIncorrectValueTypeException( e, eptr, context, userException );
        }

    } // json

} // bl

#endif /* __BL_JSONUTILS_H_ */

