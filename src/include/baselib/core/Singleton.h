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

#ifndef __BL_SINGLETON_H_
#define __BL_SINGLETON_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    /**
     * @brief class Singleton
     *
     * Implements singleton pattern separately from the class that
     * provides the functionality.
     * It relies on the base class (provided by the Base
     * template parameter) to have a default constructor
     */

    template
    <
        typename Base
    >
    class Singleton FINAL : public Base
    {
        BL_NO_COPY_OR_MOVE( Singleton )

    public:

        typedef Singleton< Base >                                           this_type;

    private:

        /*
         * Singleton implementation
         */

        static os::mutex                                                    g_lock;

        /*
         * Will not be destructed intentionally to
         * prevent issues during global destruction phase
         */

        static this_type*                                                   g_impl;

        Singleton()
            :
            Base()
        {
        }

    public:

        /**
         * @brief Returns a reference to the singleton instance
         */

        static this_type& getInstance()
        {
            BL_MUTEX_GUARD( g_lock );

            if( ! g_impl )
            {
                g_impl = new this_type();
            }

            return *g_impl;
        }
    };

    template
    <
        typename E
    >
    Singleton< E >* Singleton< E >::g_impl = nullptr;

    BL_DEFINE_STATIC_MEMBER( Singleton, os::mutex, g_lock );

} // bl

#endif /* __BL_SINGLETON_H_ */
