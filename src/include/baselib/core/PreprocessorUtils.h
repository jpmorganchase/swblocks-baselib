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

#ifndef __BL_PREPROCESSORUTILS_H_
#define __BL_PREPROCESSORUTILS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/preprocessor.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#define BL_PP_CAT( a, b ) BOOST_PP_CAT( a, b )
#define BL_PP_COMMA_IF( cond ) BOOST_PP_COMMA_IF( cond )
#define BL_PP_SEQ_ENUM( seq ) BOOST_PP_SEQ_ENUM( seq )
#define BL_PP_SEQ_FOR_EACH_I( macro, data, seq ) BOOST_PP_SEQ_FOR_EACH_I( macro, data, seq )
#define BL_PP_SEQ_FOR_EACH( macro, data, seq ) BOOST_PP_SEQ_FOR_EACH( macro, data, seq )
#define BL_PP_STRINGIZE( text ) BOOST_PP_STRINGIZE( text )
#define BL_PP_TUPLE_ELEM( size, i, tuple ) BOOST_PP_TUPLE_ELEM( size, i, tuple )

#endif /* __BL_PREPROCESSORUTILS_H_ */
