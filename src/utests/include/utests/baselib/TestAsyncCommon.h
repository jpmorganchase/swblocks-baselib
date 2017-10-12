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

#ifndef __UTEST_TESTASYNCCOMMON_H_
#define __UTEST_TESTASYNCCOMMON_H_

#include <baselib/async/AsyncOperation.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/TestTaskUtils.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

#include <atomic>

/*****************************************************************************************************************
 * Async executor implementation tests (common base)
 */

#define TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT() \
    chk2SleepRandomTime(); \
    m_canceled.lvalue() |= wasCanceled( result ); \
    BL_TASKS_HANDLER_BEGIN_CHK_ASYNC_RESULT() \
    chk2SleepRandomTime(); \

namespace utest
{
    /**
     * @brief Common base async task
     */

    template
    <
        typename BASE,
        typename WRAPPERIMPL
    >
    class AsyncTestTaskSharedBase :
        public BASE
    {
    protected:

        typedef AsyncTestTaskSharedBase< BASE, WRAPPERIMPL >                                this_type;
        typedef BASE                                                                        base_type;

        typedef typename WRAPPERIMPL::AsyncOperationStateImpl                               AsyncOperationStateImpl;

        static std::atomic< std::size_t >                                                   g_asyncCalls;

        const bl::om::ObjPtr< WRAPPERIMPL >&                                                m_wrapperImpl;

        bl::cpp::ScalarTypeIniter< std::size_t >                                            m_asyncCalls;
        bl::om::ObjPtr< AsyncOperationStateImpl >                                           m_operationState;
        bl::om::ObjPtr< bl::AsyncOperation >                                                m_operation;
        bl::cpp::ScalarTypeIniter< bool >                                                   m_canceled;
        bl::cpp::ScalarTypeIniter< bool >                                                   m_started;
        bl::cpp::ScalarTypeIniter< bool >                                                   m_allowSleeps;

        AsyncTestTaskSharedBase( SAA_in const bl::om::ObjPtr< WRAPPERIMPL >& wrapperImpl )
            :
            m_wrapperImpl( wrapperImpl )
        {
            BL_ASSERT( m_wrapperImpl );
        }

        ~AsyncTestTaskSharedBase() NOEXCEPT
        {
            releaseOperation();
        }

        virtual auto onTaskStoppedNothrow(
            SAA_in_opt              const std::exception_ptr&                   eptrIn = nullptr,
            SAA_inout_opt           bool*                                       isExpectedException = nullptr
            ) NOEXCEPT
            -> std::exception_ptr OVERRIDE
        {
            BL_NOEXCEPT_BEGIN()

            /*
             * The only legitimate case where m_operation would need to be
             * released here (i.e. non-null) is when an exception was thrown
             * from within an async callback or some other callback after
             * an async operation was completed
             *
             * Note that an exception can't be thrown after an async call is
             * started unless this operation can be waited on to complete
             * which implies the callback is ok to be called even after the
             * task has completed (which normally is not the case)
             */

            releaseOperation();

            BL_NOEXCEPT_END()

            return base_type::onTaskStoppedNothrow( eptrIn, isExpectedException );
        }

        virtual void cancelTask() OVERRIDE
        {
            if( m_operation )
            {
                m_operation -> cancel();
                releaseOperation();
            }

            base_type::cancelTask();
        }

        static unsigned char generateRandom( SAA_in unsigned char maxValue )
        {
            bl::random::uniform_int_distribution< unsigned short > dist(
                std::numeric_limits< unsigned char >::min(),
                std::numeric_limits< unsigned char >::max()
                );

            return dist( bl::tlsData().random.urng() ) % maxValue;
        }

        static bool diceRoll()
        {
            return 0U == generateRandom( 5U /* maxValue */ );
        }

        static bool coinToss()
        {
            return 0U == generateRandom( 1U /* maxValue */ );
        }

        void chk2SleepRandomTime()
        {
            if( ! m_allowSleeps )
            {
                return;
            }

            if( diceRoll() )
            {
                bl::os::sleep( bl::time::milliseconds( 10U + generateRandom( 20U ) ) );
            }
        }

        bool wasCanceled( SAA_in const bl::AsyncOperation::Result& result )
        {
            return ( base_type::isCanceled() || result.isCanceled() );
        }

        void releaseOperation() NOEXCEPT
        {
            if( m_operation )
            {
                m_wrapperImpl -> asyncExecutor() -> releaseOperation( m_operation );
                BL_ASSERT( ! m_operation );
            }

            m_operationState.reset();
        }

    public:

        std::size_t asyncCalls() const NOEXCEPT
        {
            return m_asyncCalls;
        }

        bool canceled() const NOEXCEPT
        {
            return m_canceled;
        }

        bool started() const NOEXCEPT
        {
            return m_started;
        }

        void allowSleeps( SAA_in const bool allowSleeps ) NOEXCEPT
        {
            m_allowSleeps = allowSleeps;
        }
    };

    template
    <
        typename BASE,
        typename WRAPPERIMPL
    >
    std::atomic< std::size_t >
    AsyncTestTaskSharedBase< BASE, WRAPPERIMPL>::g_asyncCalls( 0U );

} // utest

#endif // __UTEST_TESTASYNCCOMMON_H_
