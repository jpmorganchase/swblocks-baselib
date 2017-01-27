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

#ifndef __BL_BOOSTASIOERRORCALLBACK_H_
#define __BL_BOOSTASIOERRORCALLBACK_H_

/*
 * Define the include guard for the default implementation in ASIO
 * so the default header does not get included
 */

#define BOOST_ASIO_DETAIL_THROW_ERROR_HPP

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/function.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/throw_exception.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

namespace bl
{
    /**
     * @brief class BoostAsioErrorCallback
     *
     * This code is to override the Boost ASIO default error handling with something
     * smarter that will extract relevant error information and attach it to the
     * exception appropriately
     *
     * It will also throw the exception as something different than system_error
     * (usually our SystemException class or derived such)
     */

    template
    <
        typename E = void
    >
    class BoostAsioErrorCallbackT
    {
    public:

        typedef boost::function
        <
            void (
                const boost::system::error_code&                    ec,
                const char*                                         location
                )
        >
        asio_error_callback_t;

    private:

        static asio_error_callback_t                                g_errorCallback;

        /*
         * Enforce that this is a static class and should never be instantiated
         */

        BoostAsioErrorCallbackT() {}

    public:

        static asio_error_callback_t installCallback( asio_error_callback_t&& errorCallback )
        {
            asio_error_callback_t oldCallback = std::move( g_errorCallback );
            g_errorCallback = std::move( errorCallback );
            return oldCallback;
        }

        static void defaultCallback(
            const boost::system::error_code&                    ec,
            const char*                                         location
            )
        {
            if( ! ec )
            {
                return;
            }

            if( g_errorCallback )
            {
                g_errorCallback( ec, location );
            }
            else
            {
                boost::system::system_error e( ec, location );
                boost::throw_exception( e );
            }
        }
    };

    template
    <
        typename E
    >
    typename BoostAsioErrorCallbackT< E >::asio_error_callback_t
    BoostAsioErrorCallbackT< E >::g_errorCallback;

    typedef BoostAsioErrorCallbackT<> BoostAsioErrorCallback;

} // bl

namespace boost
{
    namespace asio
    {
        namespace detail
        {
            inline void throw_error( const boost::system::error_code& ec )
            {
                bl::BoostAsioErrorCallback::defaultCallback( ec, nullptr );
            }

            inline void throw_error(
                const boost::system::error_code&                    ec,
                const char*                                         location
                )
            {
                bl::BoostAsioErrorCallback::defaultCallback( ec, location );
            }

        } // detail

    } // asio

} // boost

#endif /* __BL_BOOSTASIOERRORCALLBACK_H_ */
