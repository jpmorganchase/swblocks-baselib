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

#ifndef __BL_COMPILER_H_
#define __BL_COMPILER_H_

#include <baselib/core/Annotations.h>
#include <baselib/core/BaseDefs.h>

#include <utility>
#include <type_traits>


#define BL_VARIADIC_CTOR( className, base_type, visibility ) \
    visibility: \
    template \
    < \
        typename... Args \
    > \
    className( SAA_in Args&&... args ) \
        : \
        base_type( std::forward< Args >( args )... ) \
    { \
    } \
    private: \

#define BL_VARIADIC_CREATE_INSTANCE( T, this_type, visibility ) \
    visibility: \
    template \
    < \
        typename T = this_type, \
        typename... Args \
    > \
    static bl::om::ObjPtr< T > createInstance( SAA_in Args&&... args ) \
    { \
        return bl::om::ObjPtr< T >::attach( \
            new this_type( std::forward< Args >( args )... ) \
            ); \
    } \
    private: \

/**
 * @brief Implements and encapsulates createInstance method using variadics
 */

#define BL_ENCAPSULATE_CREATE_INSTANCE( T, className, base_type, this_type ) \
    BL_VARIADIC_CTOR( className, base_type, protected ) \
    BL_VARIADIC_CREATE_INSTANCE( T, this_type, public ) \

#endif /* __BL_COMPILER_H_ */

