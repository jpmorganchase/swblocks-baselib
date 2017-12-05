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

#ifndef __BL_GRAMMARUTILS_H_
#define __BL_GRAMMARUTILS_H_

#define BOOST_SPIRIT_THREADSAFE
#define BOOST_SPIRIT_DEBUG

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/phoenix_function.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_utree.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <complex>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#define BL_GRAMMAR_ADAPT_STRUCT( NAME, ATTRIBUTES ) BOOST_FUSION_ADAPT_STRUCT( NAME, ATTRIBUTES )
#define BL_GRAMMAR_DEBUG_NODE( r ) BOOST_SPIRIT_DEBUG_NODE( r )

namespace bl
{
    namespace ebnf
    {
        namespace ascii
        {
            using boost::spirit::ascii::char_;
            using boost::spirit::ascii::space;
            using boost::spirit::ascii::space_type;
            using boost::spirit::ascii::string;

        } // ascii

        using boost::spirit::qi::grammar;
        using boost::spirit::qi::lexeme;
        using boost::spirit::qi::lit;
        using boost::spirit::qi::phrase_parse;
        using boost::spirit::qi::rule;

    } // ebnf

    namespace visitor
    {
        using boost::apply_visitor;
        using boost::static_visitor;

    } // visitor

    using boost::variant;

} // bl

#endif /* __BL_GRAMMARUTILS_H_ */
