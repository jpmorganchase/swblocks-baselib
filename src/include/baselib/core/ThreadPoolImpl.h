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

#ifndef __BL_THREADPOOLIMPL_H_
#define __BL_THREADPOOLIMPL_H_

#include <baselib/core/ThreadPool.h>

#include <baselib/core/OS.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/Logging.h>
#include <baselib/core/Utils.h>
#include <baselib/core/BaseIncludes.h>

#include <algorithm>
#include <atomic>

namespace bl
{
    /**
     * @brief class ThreadPoolImpl
     */

    template
    <
        typename E = void
    >
    class ThreadPoolImplT : public ThreadPool
    {
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( ThreadPoolImplT )

        BL_QITBL_BEGIN()
            BL_QITBL_ENTRY( ThreadPool )
            BL_QITBL_ENTRY( om::Disposable )
        BL_QITBL_END( ThreadPool )

    public:

        typedef ThreadPoolImplT< E >                        this_type;

        static const std::size_t THREADS_COUNT_DEFAULT;
        static const std::size_t THREADS_COUNT_MAX;

    private:

        const os::AbstractPriority                          m_priority;

        std::vector< cpp::SafeUniquePtr< os::thread > >     m_threads;
        cpp::SafeUniquePtr< asio::io_service >              m_ioservice;
        cpp::SafeUniquePtr< asio::io_service::work >        m_work;
        os::mutex                                           m_lock;
        os::condition_variable                              m_cvNotifyReady;
        std::atomic< std::size_t >                          m_threadsReady;

        bool                                                m_shuttingDown;
        const bool                                          m_abortIfUnhandled;
        eh::eh_callback_t                                   m_ehCB;
        std::exception_ptr                                  m_lastException;

    protected:

        /**
         * @brief handles the exception and returns true if it has been handled and the thread
         * should resume processing tasks, or false if it should exit
         */

        bool handleException( SAA_in const std::exception_ptr& eptr )
        {
            if( ! m_shuttingDown )
            {
                m_lastException = eptr;

                if( m_ehCB && m_ehCB( eptr ) )
                {
                    return true;
                }

                utils::tryCatchLog(
                    "An exception was caught in a thread pool thread",
                    [ & ]() -> void
                    {
                        cpp::safeRethrowException( eptr );
                    }
                    );

                if( ! m_abortIfUnhandled )
                {
                    BL_RIP_MSG( "An exception was caught in a thread pool thread" );
                }
            }

            return false;
        }

        void run() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            bool firstRun = true;

            for( ;; )
            {
                std::exception_ptr eptr;

                try
                {
                    if( os::onUNIX() )
                    {
                        /*
                         * If we are not on Windows we need to call os::trySetAbstractPriority for each
                         * thread individually since they have separate I/O context by default and
                         * we need to be able to set the I/O priority
                         *
                         * We may not be able to set the priority if we're trying to raise it, but we
                         * don't have permissions to do so
                         */

                        if( ! os::trySetAbstractPriority( m_priority ) )
                        {
                            /*
                             * static_cast below is needed to workaround a bug in Clang 3.2
                             */

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Cannot set thread pool abstract priority to "
                                    << static_cast< std::uint16_t >( m_priority )
                                );
                        }
                    }

                    os::mutex_unique_lock guard( m_lock );

                    if( nullptr == m_ioservice || nullptr == m_work )
                    {
                        /*
                         * Both m_ioservice and m_work should be assigned in a transaction,
                         * so if one of them is null the other should be null too
                         *
                         * The only valid states are (null, null) and (non-null, non-null)
                         */

                        BL_ASSERT( ! m_ioservice );
                        BL_ASSERT( ! m_work );

                        decltype( m_work ) work;
                        decltype( m_ioservice ) ioservice;

                        if( os::onWindows() )
                        {
                            /*
                             * On Windows ASIO uses IOCP (I/O completion port) and the concurrency hint
                             * should be zero to leverage the scalability features of IOCP and avoid
                             * 'thread thrashing' problem on systems where the # of threads in the
                             * thread pool significantly exceeds the # of logical CPU cores
                             */

                            ioservice.reset( new asio::io_service( 0U /* concurrency_hint */ ) );
                        }
                        else
                        {
                            ioservice.reset( new asio::io_service() );
                        }

                        work.reset( new asio::io_service::work( *ioservice ) );

                        m_ioservice = std::move( ioservice );
                        m_work = std::move( work );
                    }

                    BL_ASSERT( m_ioservice );
                    BL_ASSERT( m_work );

                    if( firstRun )
                    {
                        ++m_threadsReady;
                        m_cvNotifyReady.notify_one();
                        firstRun = false;
                    }

                    guard.unlock();

                    m_ioservice -> run();
                    break;
                }
                catch( std::exception& )
                {
                    eptr = std::current_exception();
                }

                if( eptr )
                {
                    if( handleException( eptr ) )
                    {
                        eptr = nullptr;
                        continue;
                    }

                    /*
                     * If we're here it means the exception was not handled,
                     * and since this is a thread pool thread we don't have
                     * much of a choice than to re-throw it into the NOEXCEPT
                     * block which would terminate the application
                     */

                    cpp::safeRethrowException( eptr );
                }
            }

            BL_NOEXCEPT_END()
        }

        ThreadPoolImplT(
            SAA_in      const os::AbstractPriority          priority,
            SAA_in_opt  const std::size_t                   threadsCount = THREADS_COUNT_DEFAULT,
            SAA_in_opt  const bool                          abortIfUnhandled = false,
            SAA_in_opt  eh::eh_callback_t&&                 ehCB = eh::eh_callback_t()
            )
            :
            m_priority( priority ),
            m_threadsReady( 0U ),
            m_shuttingDown( false ),
            m_abortIfUnhandled( abortIfUnhandled ),
            m_ehCB( BL_PARAM_FWD( ehCB ) ),
            m_lastException( nullptr )
        {
            createThreads( limitThreadCount( threadsCount ) );
        }

        ~ThreadPoolImplT() NOEXCEPT
        {
            try
            {
                disposeInternal( true /* force */ );
            }
            catch( std::exception& e )
            {
                if( ! m_abortIfUnhandled )
                {
                    BL_RIP_MSG( e.what() );
                }
            }
        }

        static std::size_t limitThreadCount( SAA_in const std::size_t threadCount )
        {
            if( threadCount > THREADS_COUNT_MAX )
            {
                BL_LOG(
                    Logging::warning(),
                    BL_MSG()
                        << "Thread pool size "
                        << threadCount
                        << " exceeds limit "
                        << THREADS_COUNT_MAX
                    );

                return THREADS_COUNT_MAX;
            }

            return threadCount;
        }

        void createThreads( SAA_in const std::size_t threadCount )
        {
            os::mutex_unique_lock guard( m_lock );

            const auto currentSize = m_threads.size();

            if( threadCount > currentSize )
            {
                m_threads.reserve( threadCount );

                for( std::size_t i = currentSize; i < threadCount; ++i )
                {
                    m_threads.push_back(
                        cpp::SafeUniquePtr< os::thread >::attach(
                            new os::thread( cpp::bind( &this_type::run, this ) )
                            )
                        );
                }

                /*
                 * Wait until all threads are ready and executing
                 */

                const auto cb = [ this, &threadCount ]() -> bool
                {
                    if( threadCount == m_threadsReady )
                    {
                        return true;
                    }

                    return false;
                };

                m_cvNotifyReady.wait( guard, cb );
            }
        }

        void disposeInternal( SAA_in const bool force )
        {
            /*
             * Note: This function must be idempotent (i.e. it can be called
             * multiple times without a problem; if the object is already
             * disposed then it is a nop)
             */

            m_shuttingDown = true;

            m_work.reset();

            if( m_ioservice && force )
            {
                m_ioservice -> stop();
            }

            /*
             * Now let's wait for all threads to finish and then shut down
             * the I/O service object
             */

            for( auto i = m_threads.begin(); i != m_threads.end(); ++i )
            {
                /*
                 * This logic here is to ensure idempotency
                 */

                auto& thread = *i;

                if( thread.get() )
                {
                    if( ! thread -> timed_join( os::get_system_time() + time::minutes( 1 ) ) )
                    {
                        /*
                         * Thread pool thread is busy processing tasks and can't
                         * be joined for the specified timeout
                         *
                         * In this case we can't do much except to abort the
                         * application
                         */

                        BL_RIP_MSG(
                            "Thread pool thread is busy processing tasks and can't be joined "
                            "for the specified timeout of 1 minute"
                            );
                    }

                    thread.reset();
                }
            }

            m_threads.clear();

            m_ioservice.reset();
        }

    public:

        virtual std::size_t size() const NOEXCEPT OVERRIDE
        {
            return m_threads.size();
        }

        virtual std::size_t resize( SAA_in const std::size_t threadCount ) OVERRIDE
        {
            BL_CHK(
                true,
                m_shuttingDown,
                "Thread pool object has been disposed"
                );

            /*
             * The thread pool can only grow safely. Once a thread is executing,
             * it cannot be stopped and removed easily.
             */

            const auto currentSize = m_threads.size();

            if( threadCount < currentSize )
            {
                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Ignoring request to shrink thread pool from "
                        << currentSize
                        << " to "
                        << threadCount
                        << " threads"
                    );
            }
            else if( threadCount > currentSize )
            {
                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Growing thread pool from "
                        << currentSize
                        << " to "
                        << threadCount
                        << " threads"
                    );

                createThreads( limitThreadCount( threadCount ) );
            }

            return size();
        }

        virtual asio::io_service& aioService() OVERRIDE
        {
            BL_CHK(
                true,
                m_shuttingDown,
                "Thread pool object has been disposed"
                );

            BL_RT_ASSERT( m_ioservice, "I/O service not initialized" );

            return *m_ioservice;
        }

        virtual std::exception_ptr lastException() const OVERRIDE
        {
            return m_lastException;
        }

        virtual void dispose() OVERRIDE
        {
            disposeInternal( true /* force  */ );
        }
    };

    BL_DEFINE_STATIC_MEMBER( ThreadPoolImplT, const std::size_t, THREADS_COUNT_DEFAULT ) = 32;
    BL_DEFINE_STATIC_MEMBER( ThreadPoolImplT, const std::size_t, THREADS_COUNT_MAX )     = 1024;

    typedef om::ObjectImpl< ThreadPoolImplT<> > ThreadPoolImpl;

} // bl

#endif /* __BL_THREADPOOLIMPL_H_ */
