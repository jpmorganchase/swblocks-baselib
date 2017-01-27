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

#ifndef __BL_THREADPOOL_H_
#define __BL_THREADPOOL_H_

#include <baselib/core/OS.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( ThreadPool, "e1b1b01e-99db-419b-bc81-880c943e6d76" )

namespace bl
{
    enum class ThreadPoolId : std::uint16_t
    {
        GeneralPurpose = 0,
        NonBlocking = 1,
    };

    /**
     * @brief class ThreadPool - The thread pool interface
     */

    class ThreadPool : public om::Disposable
    {
        BL_DECLARE_INTERFACE( ThreadPool )

    public:

        virtual std::size_t size() const NOEXCEPT = 0;

        virtual std::size_t resize( SAA_in const std::size_t threadCount ) = 0;

        virtual asio::io_service& aioService() = 0;

        virtual std::exception_ptr lastException() const = 0;
    };

    /**
     * @brief ThreadPoolDefault class - the default/global TP interface
     */

    template
    <
        typename E = void
    >
    class ThreadPoolDefaultT
    {
        BL_DECLARE_STATIC( ThreadPoolDefaultT )

    protected:

        /**
         * @brief The default thread pool
         *
         * Note: It is not owned here
         */

        enum : std::uint16_t
        {
            THREAD_POOLS_COUNT = static_cast< std::uint16_t >( ThreadPoolId::NonBlocking ) + 1,
        };

        static os::mutex        g_locks[ THREAD_POOLS_COUNT ];
        static ThreadPool*      g_threadPools[ THREAD_POOLS_COUNT ];

        static auto getDefaultInternal( SAA_in_opt const ThreadPoolId id = ThreadPoolId::GeneralPurpose ) -> om::ObjPtr< ThreadPool >
        {
            const auto pos = static_cast< std::uint16_t >( id );

            BL_RT_ASSERT( pos < THREAD_POOLS_COUNT, "Incorrect thread pool id" );

            om::ObjPtr< ThreadPool > threadPool;

            {
                BL_MUTEX_GUARD( g_locks[ pos ] );
                threadPool = om::copy( g_threadPools[ pos ] );
            }

            return threadPool;
        }

    public:

        enum
        {
            /*
             * 4 general-purpose threads are an absolute minimum
             */

            MIN_GENERAL_THREADS_COUNT = 4,

            /*
             * 4 threads for I/O only should be plenty enough
             */

            IO_THREADS_COUNT = 4,
        };

        static void disposeGlobalThreadPool(
            SAA_inout               om::ObjPtrDisposable< ThreadPool >&         threadPool,
            SAA_in                  const ThreadPoolId                          id
            ) NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            const auto defaultTP = getDefaultInternal( id );
            BL_UNUSED( defaultTP );

            if( ! threadPool )
            {
                BL_ASSERT( ! defaultTP );
                return;
            }

            if( defaultTP )
            {
                BL_ASSERT( om::areEqual( defaultTP, threadPool.get() ) );
            }

            threadPool -> dispose();
            threadPool.reset();

            setDefault( nullptr, id );

            BL_NOEXCEPT_END()
        }

        /**
         * @brief Sets the default thread pool
         *
         * Note: The default thread pool is not owned here
         * It has to be owned and released at some global scope
         */

        static void setDefault(
            SAA_inout_opt           ThreadPool*                                 threadPool,
            SAA_in_opt              const ThreadPoolId                          id = ThreadPoolId::GeneralPurpose
            )
        {
            const auto pos = static_cast< std::uint16_t >( id );
            BL_RT_ASSERT( pos < THREAD_POOLS_COUNT, "Incorrect thread pool id" );

            BL_MUTEX_GUARD( g_locks[ pos ] );
            g_threadPools[ pos ] = threadPool;
        }

        /**
         * @brief Returns a reference to the default/global thread pool
         *
         * Note: It can return nullptr if the default TP is not set
         */

        static auto getDefault( SAA_in_opt const ThreadPoolId id = ThreadPoolId::GeneralPurpose ) -> om::ObjPtr< ThreadPool >
        {
            return getDefaultInternal( id );
        }
    };

    template
    <
        typename E
    >
    os::mutex
    ThreadPoolDefaultT< E >::g_locks[ THREAD_POOLS_COUNT ];

    template
    <
        typename E
    >
    ThreadPool*
    ThreadPoolDefaultT< E >::g_threadPools[ THREAD_POOLS_COUNT ] = { nullptr };

    typedef ThreadPoolDefaultT<> ThreadPoolDefault;

} // bl

#endif /* __BL_THREADPOOL_H_ */
