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

#ifndef __BL_APPINITDONE_H_
#define __BL_APPINITDONE_H_

#include <baselib/core/FsUtils.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

#include <cstdio>

namespace bl
{
    /**
     * @brief class AppInitDoneImplDefault - a generic implementation of the init / done interface
     * expected by the generic init done template below (AppInitDoneT< IMPL >)
     */

    class AppInitDoneImplDefault
    {
        BL_NO_CREATE( AppInitDoneImplDefault )
        BL_NO_COPY_OR_MOVE( AppInitDoneImplDefault )
        BL_NO_POLYMORPHIC_BASE( AppInitDoneImplDefault )

    protected:

        void phase1Init()
        {
        }

        void phase2Init()
        {
        }

        void phase1Done() NOEXCEPT
        {
        }

        void phase2Done() NOEXCEPT
        {
        }
    };

    /**
     * @brief class AppInitDoneT< IMPL > - a generic init done template for baselib based applications
     */

    template
    <
        typename IMPL = AppInitDoneImplDefault
    >
    class AppInitDoneT FINAL : public IMPL
    {
        BL_NO_COPY_OR_MOVE( AppInitDoneT )

    protected:

        os::GlobalProcessInit                       m_processInit;
        Logging::LevelPusher                        m_pushLevel;

        om::ObjPtrDisposable< ThreadPool >          m_threadPoolGeneralPurpose;
        om::ObjPtrDisposable< ThreadPool >          m_threadPoolNonBlocking;

        const bool                                  m_hasThreadPool;

    public:

        /*
         * Create a global leaked thread pool which supports SharedPtr
         * and configure it as default
         */

        class LttLeaked {};

        typedef om::ObjectImpl<
            ThreadPoolImplT<>,
            true /* enableSharedPtr */,
            true /* isMultiThreaded */,
            LttLeaked
            >
            ThreadPoolImplDefault;

        AppInitDoneT(
            SAA_in_opt          const int                           loggingLevel = Logging::LL_DEFAULT,
            SAA_in_opt          const std::size_t                   threadsCount = ThreadPoolImpl::THREADS_COUNT_DEFAULT,
            SAA_in_opt          ThreadPool*                         sharedThreadPool = nullptr,
            SAA_in_opt          ThreadPool*                         sharedNonBlockingThreadPool = nullptr,
            SAA_in_opt          const bool                          noThreadPool = false
            )
            :
            m_pushLevel( loggingLevel, true /* global */ ),
            m_hasThreadPool( ! noThreadPool )
        {
            /*
             * By default we set the main thread in whatever
             * is the default abstract priority
             */

            if( ! os::trySetAbstractPriority( os::getAbstractPriorityDefault() ) )
            {
                /*
                 * static_cast below is needed to workaround a bug in Clang 3.2
                 */

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Cannot set main thread abstract priority to "
                        << static_cast< std::uint16_t >( os::getAbstractPriorityDefault() )
                    );
            }

            /*
             * fs::path should always convert to UTF8 to support internationalization
             * of path names
             */

            fs::imbueUtf8Paths();

            IMPL::phase1Init();

            auto g = BL_SCOPE_GUARD(
                dispose();
                );

            /*
             * Initialize the thread pool
             */

            if( m_hasThreadPool )
            {
                /*
                 * The default thread pools will push all the thread pool threads
                 * in whatever is the default abstract priority
                 *
                 * The passed in threadsCount applies to the general purpose thread
                 * pool, the I/O thread pool has fixed small # of threads
                 * (ThreadPoolDefault::IO_THREADS_COUNT)
                 */

                if( sharedThreadPool )
                {
                    ThreadPoolDefault::setDefault( sharedThreadPool, ThreadPoolId::GeneralPurpose );
                }
                else
                {
                    m_threadPoolGeneralPurpose = ThreadPoolImplDefault::template createInstance< ThreadPool >(
                        os::getAbstractPriorityDefault(),
                        threadsCount
                        );

                    ThreadPoolDefault::setDefault(
                        m_threadPoolGeneralPurpose.get(),
                        ThreadPoolId::GeneralPurpose
                        );
                }

                if( sharedNonBlockingThreadPool )
                {
                    ThreadPoolDefault::setDefault( sharedNonBlockingThreadPool, ThreadPoolId::NonBlocking );
                }
                else
                {
                    m_threadPoolNonBlocking = ThreadPoolImplDefault::template createInstance< ThreadPool >(
                        os::getAbstractPriorityDefault(),
                        ThreadPoolDefault::IO_THREADS_COUNT
                        );

                    ThreadPoolDefault::setDefault(
                        m_threadPoolNonBlocking.get(),
                        ThreadPoolId::NonBlocking
                        );
                }
            }

            IMPL::phase2Init();

            g.dismiss();
        }

        ~AppInitDoneT() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            IMPL::phase2Done();

            dispose();

            IMPL::phase1Done();

            BL_NOEXCEPT_END()
        }

        void dispose() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            /*
             * Note that we should only call ThreadPoolDefault::setDefault( nullptr, id )
             * in the case when we own the thread pool's lifetime and we can first dispose it
             *
             * If the thread pool isn't disposed before we clear the global default thread
             * pool pointer then there may be outstanding tasks in it and they may be using
             * the global pointer, so we can't set it to nullptr before these tasks are all
             * fully flushed out and the threads in the thread pool have exited
             *
             * In the case when we have shared thread pool the global pointer will never
             * be cleared for the plug-in binaries which don't own the shared thread pool,
             * but thread pool objects have C++ interface and can't be shared between
             * binaries so this should never be a problem (the shared thread pool can work
             * only for sharing between plug-ins which are part of the same binary such is
             * the case with the 'merged' agent plug-in)
             */

            if( m_hasThreadPool )
            {
                ThreadPoolDefault::disposeGlobalThreadPool( m_threadPoolGeneralPurpose, ThreadPoolId::GeneralPurpose );
                ThreadPoolDefault::disposeGlobalThreadPool( m_threadPoolNonBlocking, ThreadPoolId::NonBlocking );
            }

            BL_NOEXCEPT_END()
        }
    };

    typedef AppInitDoneT<> AppInitDoneDefault;

} // bl

#endif /* __BL_APPINITDONE_H_ */
