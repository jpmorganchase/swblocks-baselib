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

#ifndef __BL_CONFIGURATIONUTILS_H_
#define __BL_CONFIGURATIONUTILS_H_

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

namespace bl
{
    namespace config
    {
        using boost::property_tree::ptree;
        using boost::property_tree::ptree_error;
        using boost::property_tree::ptree_bad_path;
        using boost::property_tree::ptree_bad_data;
        using boost::property_tree::xml_parser_error;

    } // config

} // bl

#endif /* __BL_CONFIGURATIONUTILS_H_ */
