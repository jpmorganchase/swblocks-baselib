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

#ifndef __BL_BOOSTINCLUDEGUARDPOP_H_
#define __BL_BOOSTINCLUDEGUARDPOP_H_

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

/*
 * Some boost headers don't seem to match push/pop properly, so we need to push and pop several
 * times to ensure that the warnings we disable don't get overridden
 */

#pragma warning( pop )

#endif // #if defined(_WIN32) && defined(_MSC_VER)

#endif // !defined(__CDT_PARSER__)

#endif /* __BL_BOOSTINCLUDEGUARDPOP_H_ */
