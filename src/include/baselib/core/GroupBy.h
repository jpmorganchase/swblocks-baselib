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

#ifndef __BL_GROUPBY_H_
#define __BL_GROUPBY_H_

#include <iterator>
#include <vector>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace details
    {
        template
        <
            typename Iterator,
            typename Result
        >
        struct GroupBy
        {
            typedef Result key_type;
            typedef std::vector< typename std::iterator_traits< Iterator >::value_type > value_type;

            typedef std::map< key_type, value_type > map_type;
        };

    } // details

    /*
     * @brief groupBy groups items from an input container with a given predicate into a map
     *
     * Because of issue with MSVC 2010 it works only with std::function at the moment but no
     *      functions, functors and lambdas
     *
     * map<V', [V]> groupBy( [V], V' F(V) )
     *      where [V]           is a collection of objects of V type
     *            V' F(V)       is a functor transforming V type into V' type
     *            map<V', [V]>  is a map of collections [V] grouped by F functor's result
     *
     * groupBy( [1, 3, 2, 3, 2, 3], [](int i){return 1.0*i*i;})
     *      == { {1.0, [1]},
     *           {4.0, [2, 2]},
     *           {9.0, [3, 3, 3]} }
     *
     * groupBy( [(3, d), (2, b), (1, a), (3, e), (2, c), (3, f) ], []( pair ){ return pair.first } )
     *      == { {1, [(1, a)]},
     *           {2, [(2, b), (2, c)]},
     *           {3, [(3, d), (3, e), (3, f)]} }
     *
     */

    template
    <
        typename Iterator,
        typename Result,
        typename Arg
    >
    typename details::GroupBy< Iterator, Result >::map_type
    groupBy(
        SAA_in Iterator                                     begin,
        SAA_in const Iterator                               end,
        SAA_in std::function< Result ( SAA_in Arg ) >       functor
        )
    {
        typename details::GroupBy< Iterator, Result >::map_type result;

        for( /*empty*/; begin != end; ++begin )
        {
            result[ functor( *begin ) ].push_back( *begin );
        }

        return result;
    }

} // bl

#endif /* __BL_GROUPBY_H_ */
