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

#ifndef __BL_ANNOTATIONS_H_
#define __BL_ANNOTATIONS_H_

/**
 * Generic static analysis annotations will be in this file. They will map to whatever is appropriate
 * for each toolchain (SAL for MSVC and respective attributes for GCC)
 */

#if defined(_MSC_VER)

#include <sal.h>

#if !defined( BL_DEVENV_VERSION ) || BL_DEVENV_VERSION < 3
#include <codeanalysis\sourceannotations.h>
#endif

#define SAA_assume( expr )               __analysis_assume( expr )
#define SAA_noreturn                     __declspec( noreturn )

#define SAA_in                           __in
#define SAA_in_opt                       __in_opt
#define SAA_out                          __out
#define SAA_out_opt                      __out_opt
#define SAA_inout                        __inout
#define SAA_inout_opt                    __inout_opt
#define SAA_deref_out                    __deref_out
#define SAA_deref_out_opt                __deref_out_opt

#define SAA_deref_out_bcount( size )    __deref_out_bcount( size )
#define SAA_deref_out_ecount( size )    __deref_out_ecount( size )

#define SAA_in_bcount( size )            __in_bcount( size )
#define SAA_in_bcount_opt( size )        __in_bcount_opt( size )
#define SAA_in_ecount( size )            __in_ecount( size )
#define SAA_out_bcount( size )           __out_bcount( size )
#define SAA_out_bcount_opt( size )       __out_bcount_opt( size )
#define SAA_out_ecount( size )           __out_ecount( size )

#else

#define SAA_assume( expr )
#define SAA_noreturn                     __attribute__( ( __noreturn__ ) )

#define SAA_in
#define SAA_in_opt
#define SAA_out
#define SAA_out_opt
#define SAA_inout
#define SAA_inout_opt
#define SAA_deref_out
#define SAA_deref_out_opt

#define SAA_deref_out_bcount( size )
#define SAA_deref_out_ecount( size )

#define SAA_in_bcount( size )
#define SAA_in_bcount_opt( size )
#define SAA_in_ecount( size )
#define SAA_out_bcount( size )
#define SAA_out_bcount_opt( size )
#define SAA_out_ecount( size )

#endif

#endif /* __BL_ANNOTATIONS_H_ */

