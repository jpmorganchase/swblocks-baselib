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

#ifndef __BL_OSBOOSTIMPORTS_H_
#define __BL_OSBOOSTIMPORTS_H_

/*
 * This header must be included before we include Boost ASIO
 * headers, so we can override the error handling if we need to
 */

#include <baselib/core/detail/BoostAsioErrorCallback.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/thread/tss.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/concepts.hpp>

#include <baselib/core/detail/BoostIncludeGuardPop.h>

#include <iosfwd>
#include <mutex>
#include <condition_variable>

namespace bl
{
    namespace os
    {
        /* thread related imports */
        using boost::thread;
        using boost::system_time;
        using boost::get_system_time;
        using boost::thread_specific_ptr;

        using std::mutex;
        using std::unique_lock;
        using std::lock_guard;
        using std::condition_variable;

        /*
         * shared_mutex and shared_lock are not part of std in C++11, but
         * will be in C++14, so for now we will use boost implementations
         */

        using boost::shared_mutex;
        using boost::shared_lock;
        using boost::upgrade_lock;
        using boost::upgrade_to_unique_lock;

        /* returned by condition_variable::wait_for/wait_until in vc11 */
#if defined( _MSC_VER ) && _MSC_VER == 1700
        using std::cv_status::cv_status;
#else
        using std::cv_status;
#endif

        /* required by condition_variable::wait_for/wait_until */
        namespace chrono = std::chrono;

        typedef lock_guard< mutex >     mutex_guard;
        typedef unique_lock< mutex >    mutex_unique_lock;

        namespace ipc = boost::interprocess;

    } // os

    /* import boost::asio namespace wholesale */
    namespace asio = boost::asio;

} // bl

#endif /* __BL_OSBOOSTIMPORTS_H_ */
