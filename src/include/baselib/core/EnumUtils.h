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

#ifndef __BL_ENUMUTILS_H_
#define __BL_ENUMUTILS_H_

#include <baselib/core/PreprocessorUtils.h>

/*
 * Simple enum declaration macros
 *
 * This macro assumes the string representation is the same as the enum name
 */

#define BL_DEFINE_ENUM_WITH_STRING_CONVERSIONS_TOSTRING( r, data, elem )            \
        case elem : return std::string( BL_PP_STRINGIZE( elem ) );                  \

#define BL_DEFINE_ENUM_WITH_STRING_CONVERSIONS_TOENUM( r, data, elem )              \
        if( s == BL_PP_STRINGIZE( elem ) )                                          \
        {                                                                           \
            result = elem;                                                          \
            return true;                                                            \
        }                                                                           \

#define BL_DEFINE_ENUM_WITH_STRING_CONVERSIONS( name, friendlyName, enumerators )   \
    class name                                                                      \
    {                                                                               \
    public:                                                                         \
                                                                                    \
        enum Enum                                                                   \
        {                                                                           \
            BL_PP_SEQ_ENUM( enumerators )                                           \
        };                                                                          \
                                                                                    \
        static std::string toString( SAA_in const Enum e )                          \
        {                                                                           \
            switch( e )                                                             \
            {                                                                       \
                BL_PP_SEQ_FOR_EACH(                                                 \
                    BL_DEFINE_ENUM_WITH_STRING_CONVERSIONS_TOSTRING,                \
                    name,                                                           \
                    enumerators                                                     \
                    )                                                               \
            }                                                                       \
                                                                                    \
            BL_THROW_USER(                                                          \
                BL_MSG()                                                            \
                    <<  friendlyName                                                \
                    << " has invalid integral value "                               \
                    << e                                                            \
                );                                                                  \
        }                                                                           \
                                                                                    \
        static bool tryToEnum( SAA_in const std::string& s, SAA_out Enum& result )  \
        {                                                                           \
            BL_PP_SEQ_FOR_EACH(                                                     \
                BL_DEFINE_ENUM_WITH_STRING_CONVERSIONS_TOENUM,                      \
                name,                                                               \
                enumerators                                                         \
                )                                                                   \
                                                                                    \
            return false;                                                           \
        }                                                                           \
                                                                                    \
        static Enum toEnum( SAA_in const std::string& s )                           \
        {                                                                           \
            Enum result;                                                            \
                                                                                    \
            BL_CHK_USER(                                                            \
                false,                                                              \
                tryToEnum( s, result ),                                             \
                BL_MSG()                                                            \
                    << friendlyName                                                 \
                    << " has invalid string value '"                                \
                    << s                                                            \
                    << "'"                                                          \
                );                                                                  \
                                                                                    \
            return result;                                                          \
        }                                                                           \
    };                                                                              \

/*
 * Advanced enum declaration macros (ENUM2)
 *
 * This macro allows you to define the string representation to be different from
 * the enum name
 */

#define BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS_TOSTRING( r, data, elem )           \
        case BL_PP_TUPLE_ELEM( 2, 0, elem ):                                        \
            return std::string( BL_PP_TUPLE_ELEM( 2, 1, elem ) );                   \

#define BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS_TOENUM( r, data, elem )             \
        if( s == BL_PP_TUPLE_ELEM( 2, 1, elem ) )                                   \
        {                                                                           \
            result = BL_PP_TUPLE_ELEM( 2, 0, elem );                                \
            return true;                                                            \
        }                                                                           \

#define BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS_ENUMVALUE( r, data, elem )          \
        BL_PP_TUPLE_ELEM( 2, 0, elem ),                                             \

#define BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS( name, friendlyName, enumerators )  \
    class name                                                                      \
    {                                                                               \
    public:                                                                         \
                                                                                    \
        enum Enum                                                                   \
        {                                                                           \
            BL_PP_SEQ_FOR_EACH(                                                     \
                BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS_ENUMVALUE,                  \
                name,                                                               \
                enumerators                                                         \
                )                                                                   \
        };                                                                          \
                                                                                    \
        static std::string toString( SAA_in const Enum e )                          \
        {                                                                           \
            switch( e )                                                             \
            {                                                                       \
                BL_PP_SEQ_FOR_EACH(                                                 \
                    BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS_TOSTRING,               \
                    name,                                                           \
                    enumerators                                                     \
                    )                                                               \
            }                                                                       \
                                                                                    \
            BL_THROW_USER(                                                          \
                BL_MSG()                                                            \
                    <<  friendlyName                                                \
                    << " has invalid integral value "                               \
                    << e                                                            \
                );                                                                  \
        }                                                                           \
                                                                                    \
        static bool tryToEnum( SAA_in const std::string& s, SAA_out Enum& result )  \
        {                                                                           \
            BL_PP_SEQ_FOR_EACH(                                                     \
                BL_DEFINE_ENUM2_WITH_STRING_CONVERSIONS_TOENUM,                     \
                name,                                                               \
                enumerators                                                         \
                )                                                                   \
                                                                                    \
            return false;                                                           \
        }                                                                           \
                                                                                    \
        static Enum toEnum( SAA_in const std::string& s )                           \
        {                                                                           \
            Enum result;                                                            \
                                                                                    \
            BL_CHK_USER(                                                            \
                false,                                                              \
                tryToEnum( s, result ),                                             \
                BL_MSG()                                                            \
                    << friendlyName                                                 \
                    << " has invalid string value '"                                \
                    << s                                                            \
                    << "'"                                                          \
                );                                                                  \
                                                                                    \
            return result;                                                          \
        }                                                                           \
    };                                                                              \

#endif /* __BL_ENUMUTILS_H_ */
