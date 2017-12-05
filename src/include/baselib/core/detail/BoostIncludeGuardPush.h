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

#ifndef __BL_BOOSTINCLUDEGUARDPUSH_H_
#define __BL_BOOSTINCLUDEGUARDPUSH_H_

/*
 * Wrap this in #if !defined(__CDT_PARSER__) to make sure we're not messing up with the
 * CDT parser
 */
#if !defined(__CDT_PARSER__)

#if defined(_WIN32) && defined(_MSC_VER)

/*
 * Microsoft compiler is being used. We must turn off certain warnings that show up in /analyze,
 * so we can keep our code clean from static analysis issues
 */

#pragma warning( push )

// Local declaration of 'i' hides declaration of the same name in outer scope.
#pragma warning( disable : 6246 )

// Return value ignored: 'UnregisterWaitEx'.
#pragma warning( disable : 6031 )

// 'Temp_value_#10999' could be '0':  this does not adhere to the specification for the function 'GetProcAddress'
#pragma warning( disable : 6387 )

// 'Temp_value_#11121' could be '0':  this does not adhere to the specification for the function 'GetProcAddress'
#pragma warning( disable : 6387 )

// Using TerminateThread does not allow proper thread clean up.
#pragma warning( disable : 6258 )

// Dereferencing NULL pointer 'q'
#pragma warning( disable : 6011 )

// Index '624' is out of valid index range '0' to '623' for possibly stack allocated buffer 'x'
#pragma warning( disable : 6201 )

// Index '624' is out of valid index range '0' to '623' for possibly stack allocated buffer 'x'
#pragma warning( disable : 6294 )

// Reading invalid data from 'x':  the readable size is '2496' bytes, but 'i' bytes may be read.
#pragma warning( disable : 6385 )

// Potential comparison of a constant with another constant.
#pragma warning( disable : 6326 )

// 'char' passed as _Param_(1) when 'unsigned char' is required in call to 'ispunct'
#pragma warning( disable : 6330 )

// sizeof operator applied to an expression with an operator might yield unexpected results.
#pragma warning( disable : 6334 )

#endif // #if defined(_WIN32) && defined(_MSC_VER)

#endif // !defined(__CDT_PARSER__)

#if defined(__clang__)

/*
 * Turn off some warnings in boost when clang is enabled
 */

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#pragma GCC diagnostic ignored "-Wmismatched-tags"

/*
 * TODO: there is a bug in boost which is causing clang to issue this;
 * it will be fixed in the next release (see link and info below)
 *
 * Once boost is fixed we can get rid of this warning suppression
 *
 * http://lists.boost.org/boost-users/2013/01/77108.php
 *
 * boost/detail/utf8_codecvt_facet.hpp:171:17: warning:
 * 'boost::filesystem::detail::utf8_codecvt_facet::do_length'* hides overloaded virtual function [-Woverloaded-virtual]
 */
#pragma GCC diagnostic ignored "-Woverloaded-virtual"

#endif // #if defined(__clang__)

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 8

#pragma GCC diagnostic ignored "-Wunused-local-typedefs"

#endif // #if defined(__GNUC__)

#endif /* __BL_BOOSTINCLUDEGUARDPUSH_H_ */
