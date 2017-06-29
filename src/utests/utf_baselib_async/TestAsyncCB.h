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

#include <baselib/async/AsyncExecutorWrapperCallback.h>

#include <utests/baselib/TestAsyncCommon.h>

/*****************************************************************************************************************
 * Async executor implementation tests (for AsyncExecutorWrapperCallbackImpl)
 */

namespace asynccb
{
    typedef bl::AsyncExecutorWrapperCallbackImpl                             AsyncExecutorWrapperCallbackImpl;
    typedef bl::AsyncExecutorWrapperCallbackImpl::AsyncOperationStateImpl    AsyncOperationStateImpl;

    /**
     * @brief Test async task
     */

    template
    <
        typename BASE
    >
    class AsyncTestTaskBaseT :
        public utest::AsyncTestTaskSharedBase< BASE, AsyncExecutorWrapperCallbackImpl >
    {
    protected:

        typedef AsyncExecutorWrapperCallbackImpl                            wrapper_t;

        typedef AsyncTestTaskBaseT< BASE >                                  this_type;
        typedef utest::AsyncTestTaskSharedBase< BASE, wrapper_t >           base_type;

        typedef bl::cpp::function
        <
            void (
                SAA_in      const bl::om::ObjPtrCopyable< this_type >&      thisObj,
                SAA_in      const bl::AsyncOperation::Result&               result
                ) NOEXCEPT
        >
        async_callback_t;

        enum : std::size_t
        {
            NO_OF_CALLS = 4U,
        };

        using base_type::g_asyncCalls;
        using base_type::m_wrapperImpl;
        using base_type::m_asyncCalls;
        using base_type::m_operationState;
        using base_type::m_operation;
        using base_type::m_canceled;
        using base_type::m_started;
        using base_type::m_allowSleeps;

        using base_type::releaseOperation;
        using base_type::chk2SleepRandomTime;
        using base_type::wasCanceled;
        using base_type::coinToss;

        bl::cpp::ScalarTypeIniter< bool >                                   m_tasksAtEvenCalls;
        bl::cpp::ScalarTypeIniter< std::size_t >                            m_callNo;
        bl::cpp::ScalarTypeIniter< std::size_t >                            m_createTaskCalls;

        AsyncTestTaskBaseT(
            SAA_in              const bl::om::ObjPtr< wrapper_t >&          wrapperImpl,
            SAA_in_opt          const bool                                  tasksAtEvenCalls = coinToss()
            )
            :
            base_type( wrapperImpl ),
            m_tasksAtEvenCalls( tasksAtEvenCalls )
        {
        }

        void incrementCallNoWithRandomDelays( SAA_in const bool expectOddNumber )
        {
            chk2SleepRandomTime();

            if( expectOddNumber )
            {
                BL_CHK(
                    false,
                    0 != ( m_callNo % 2 ),
                    BL_MSG()
                        << "The sync call # is expected to be an odd number while it is "
                        << m_callNo.value()
                    );
            }
            else
            {
                BL_CHK(
                    false,
                    0 == ( m_callNo % 2 ),
                    BL_MSG()
                        << "The sync call # is expected to be an even number while it is "
                        << m_callNo.value()
                    );
            }

            ++m_callNo.lvalue();

            chk2SleepRandomTime();
        }

        bl::om::ObjPtr< bl::tasks::Task > createTask()
        {
            ++m_createTaskCalls.lvalue();

            if(
                ( 0 == ( m_callNo % 2 ) && m_tasksAtEvenCalls ) ||
                ( 0 != ( m_callNo % 2 ) && ! m_tasksAtEvenCalls )
                )
            {
                return bl::tasks::SimpleTaskImpl::createInstance< bl::tasks::Task >(
                    bl::cpp::bind(
                        &this_type::incrementCallNoWithRandomDelays,
                        bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                        0 != ( m_callNo % 2 ) /* expectOddNumber ) */
                        )
                    );
            }

            return nullptr;
        }

        void startTask()
        {
            m_callNo = 0U;

            createOperation();

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    &this_type::onCall1,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            m_started = true;
        }

        void createOperation()
        {
            releaseOperation();

            m_operationState = m_wrapperImpl -> template createOperationState< AsyncOperationStateImpl >(
                bl::cpp::bind(
                    &this_type::incrementCallNoWithRandomDelays,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    m_tasksAtEvenCalls ? true : false /* expectOddNumber ) */
                    ),
                bl::cpp::bind(
                    &this_type::createTask,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this )
                    )
                );

            m_operation = m_wrapperImpl -> asyncExecutor() -> createOperation(
                bl::om::qi< bl::AsyncOperationState >( m_operationState )
                );

            BL_CHK(
                false,
                !! m_operationState -> callback(),
                BL_MSG()
                    << "Operation callback should be not be empty"
                );
        }

        void onHandleCall(
            SAA_in              const bl::AsyncOperation::Result&           result,
            SAA_in              const std::size_t                           expectedCallNo,
            SAA_in              const async_callback_t&                     nextCall
            ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            /*
             * depending on the value of m_tasksAtEvenCalls and the m_callNo we need
             * to check if result.task is set as expected
             */

            if( m_tasksAtEvenCalls )
            {
                UTF_REQUIRE( 0 != ( m_callNo % 2 ) ? !! result.task : ! result.task );
            }
            else
            {
                UTF_REQUIRE( 0 == ( m_callNo % 2 ) ? !! result.task : ! result.task );
            }

            UTF_REQUIRE_EQUAL( m_callNo.value(), expectedCallNo );

            if( coinToss() )
            {
                /*
                 * At random we either create a new operation or execute new call
                 * on the same operation and operation state object
                 */

                createOperation();
            }

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    nextCall,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            ++m_asyncCalls.lvalue();
            ++g_asyncCalls;

            BL_TASKS_HANDLER_END_NOTREADY()
        }

        void onCall1( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            onHandleCall( result, 1U /* expectedCallNo */, bl::cpp::mem_fn( &this_type::onCall2 ) /* nextCall */ );
        }

        void onCall2( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            onHandleCall( result, 2U /* expectedCallNo */, bl::cpp::mem_fn( &this_type::onCall3 ) /* nextCall */ );
        }

        void onCall3( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            onHandleCall( result, 3U /* expectedCallNo */, bl::cpp::mem_fn( &this_type::onCall4 ) /* nextCall */ );
        }

        void onCall4( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            /*
             * result.task needs to be checked depending on m_tasksAtEvenCalls
             */

            UTF_REQUIRE( m_tasksAtEvenCalls ? ! result.task : !! result.task );

            UTF_REQUIRE_EQUAL( m_callNo.value(), NO_OF_CALLS );

            UTF_REQUIRE_EQUAL( m_createTaskCalls.value(), NO_OF_CALLS );

            releaseOperation();

            ++m_asyncCalls.lvalue();
            ++g_asyncCalls;

            BL_TASKS_HANDLER_END()
        }

    public:

        template
        <
            typename Functor
        >
        static void executeTests( SAA_in Functor&& cb )
        {
            {
                const auto wrapperImpl =
                    AsyncExecutorWrapperCallbackImpl::createInstance();

                {
                    bl::tasks::scheduleAndExecuteInParallel(
                        [ & ]( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
                        {
                            cb( wrapperImpl, eq );
                        }
                        );
                }
            }
        }

        template
        <
            typename IMPL
        >
        static void executePerfTests( SAA_in const bool testCancel )
        {
            this_type::executeTests(
                [ & ]
                (
                    SAA_in              const bl::om::ObjPtr< AsyncExecutorWrapperCallbackImpl >&   wrapperImpl,
                    SAA_in              const bl::om::ObjPtr< bl::tasks::ExecutionQueue >&          eq
                ) -> void
                {
                    try
                    {
                        eq -> setOptions( bl::tasks::ExecutionQueue::OptionKeepAll );

                        const std::size_t maxIterations = ( testCancel ? 10U : 40U ) * 1024U;

                        const auto t1 = bl::time::microsec_clock::universal_time();

                        BL_LOG(
                            bl::Logging::debug(),
                            BL_MSG()
                                << "Executing "
                                << ( NO_OF_CALLS * maxIterations )
                                << " async calls on task pool with size of "
                                << maxIterations
                            );

                        UTF_REQUIRE_EQUAL( 0U, g_asyncCalls );

                        try
                        {
                            for( std::size_t i = 0U; i < maxIterations; ++i )
                            {
                                const auto asyncTaskImpl = IMPL::template createInstance< IMPL >( wrapperImpl );

                                asyncTaskImpl -> allowSleeps( testCancel );

                                eq -> push_back( bl::om::qi< bl::tasks::Task >( asyncTaskImpl ) );
                            }

                            {
                                const auto duration = bl::time::microsec_clock::universal_time() - t1;
                                const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

                                BL_LOG(
                                    bl::Logging::debug(),
                                    BL_MSG()
                                        << "Scheduling "
                                        << ( NO_OF_CALLS * maxIterations )
                                        << " async calls on task pool with size of "
                                        << maxIterations
                                        << " took "
                                        << durationInSeconds
                                        << " seconds"
                                    );
                            }

                            if( testCancel )
                            {
                                bl::os::sleep( bl::time::seconds( 2 ) );

                                std::size_t canceledTests = 0U;

                                eq -> cancelAll( false /* wait */ );

                                for( ;; )
                                {
                                    const auto asyncTask = eq -> pop( true /* wait */ );

                                    if( ! asyncTask )
                                    {
                                        break;
                                    }

                                    if( asyncTask -> isFailed() )
                                    {
                                        const auto taskImpl = bl::om::qi< IMPL >( asyncTask );

                                        if( taskImpl -> started() )
                                        {
                                            if( ! taskImpl -> canceled() )
                                            {
                                                /*
                                                 * This is a strange case as the task is expected to either
                                                 * complete successfully or fail due to cancellation or due
                                                 * to bl::asio::error::operation_aborted
                                                 *
                                                 * Let's dump the exception info and call BL_RIP_MSG to catch
                                                 * and debug this case when it happens
                                                 */

                                                const auto printExceptionAndRip = []( SAA_inout std::exception& e )
                                                {
                                                    const auto msg = bl::resolveMessage(
                                                        BL_MSG()
                                                            << "Task failed but was not canceled; exception details:\n"
                                                            << bl::eh::diagnostic_information( e )
                                                        );

                                                    BL_LOG(
                                                        bl::Logging::debug(),
                                                        BL_MSG()
                                                            << msg
                                                        );

                                                    BL_RIP_MSG( msg.c_str() );
                                                };

                                                try
                                                {
                                                    bl::cpp::safeRethrowException( taskImpl -> exception() );
                                                }
                                                catch( bl::eh::system_error& e )
                                                {
                                                    if( bl::asio::error::operation_aborted != e.code() )
                                                    {
                                                        printExceptionAndRip( e );
                                                    }
                                                }
                                                catch( std::exception& e )
                                                {
                                                    printExceptionAndRip( e );
                                                }
                                            }
                                        }

                                        try
                                        {
                                            bl::cpp::safeRethrowException( asyncTask -> exception() );
                                        }
                                        catch( bl::eh::system_error& e )
                                        {
                                            BL_UNUSED( e );
                                            BL_ASSERT( bl::asio::error::operation_aborted == e.code() );
                                        }

                                        ++canceledTests;
                                    }
                                }

                                BL_LOG(
                                    bl::Logging::debug(),
                                    BL_MSG()
                                        << "Canceled "
                                        << canceledTests
                                        << " async tasks"
                                    );
                            }
                            else
                            {
                                eq -> flushAndDiscardReady();
                            }

                            BL_ASSERT( eq -> isEmpty() );
                        }
                        catch( std::exception& )
                        {
                            g_asyncCalls = 0U;
                            throw;
                        }

                        const auto duration = bl::time::microsec_clock::universal_time() - t1;
                        const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

                        BL_LOG(
                            bl::Logging::debug(),
                            BL_MSG()
                                << "Executing async operations took "
                                << durationInSeconds
                                << " seconds; "
                                << "speed is "
                                << ( g_asyncCalls / durationInSeconds )
                                << " async calls/s"
                            );

                        if( ! testCancel )
                        {
                            UTF_REQUIRE_EQUAL( NO_OF_CALLS * maxIterations, g_asyncCalls );
                            g_asyncCalls = 0U;
                        }
                    }
                    catch( std::exception& )
                    {
                        eq -> forceFlushNoThrow();
                        throw;
                    }
                }
                );
        }

        template
        <
            typename IMPL
        >
        static void executeBasicTests()
        {
            this_type::executeTests(
                [ & ]
                (
                    SAA_in              const bl::om::ObjPtr< AsyncExecutorWrapperCallbackImpl >&   wrapperImpl,
                    SAA_in              const bl::om::ObjPtr< bl::tasks::ExecutionQueue >&          eq
                ) -> void
                {
                    const auto testOneTime = [ & ]( SAA_in_opt const bool tasksAtEvenCalls ) -> void
                    {
                        const auto asyncTaskImpl =
                            IMPL::template createInstance< IMPL >( wrapperImpl, tasksAtEvenCalls );

                        asyncTaskImpl -> allowSleeps( true );

                        const auto asyncTask = bl::om::qi< bl::tasks::Task >( asyncTaskImpl );

                        UTF_REQUIRE_EQUAL( 0U, asyncTaskImpl -> asyncCalls() );
                        UTF_REQUIRE_EQUAL( 0U, g_asyncCalls );

                        eq -> push_back( asyncTask );

                        eq -> waitForSuccess( asyncTask );

                        UTF_REQUIRE_EQUAL( NO_OF_CALLS, asyncTaskImpl -> asyncCalls() );
                        UTF_REQUIRE_EQUAL( NO_OF_CALLS, g_asyncCalls );

                        g_asyncCalls = 0U;
                    };

                    testOneTime( true /* tasksAtEvenCalls */ );
                    testOneTime( false /* tasksAtEvenCalls */ );
                }
                );
        }
    };

    /**
     * @brief The async only flavor
     */

    template
    <
        typename E = void
    >
    class AsyncTestTaskAsyncOnlyT :
        public AsyncTestTaskBaseT< bl::tasks::TaskBase >
    {
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( AsyncTestTaskAsyncOnlyT )

    protected:

        typedef AsyncExecutorWrapperCallbackImpl                            wrapper_t;

        typedef AsyncTestTaskBaseT< bl::tasks::TaskBase >                   base_type;

        AsyncTestTaskAsyncOnlyT(
            SAA_in              const bl::om::ObjPtr< wrapper_t >&          wrapperImpl,
            SAA_in_opt          const bool                                  tasksAtEvenCalls = coinToss()
            )
            :
            base_type( wrapperImpl, tasksAtEvenCalls )
        {
        }

        virtual void scheduleTask( SAA_in const std::shared_ptr< bl::tasks::ExecutionQueue >& eq ) OVERRIDE
        {
            BL_UNUSED( eq );

            base_type::startTask();
        }
    };

    typedef bl::om::ObjectImpl< AsyncTestTaskAsyncOnlyT<> > AsyncTestTaskAsyncOnlyImpl;

    /**
     * @brief The fast start flavor
     */

    template
    <
        typename E = void
    >
    class AsyncTestTaskAsyncFastStartT :
        public AsyncTestTaskBaseT< bl::tasks::SimpleTaskBase >
    {
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( AsyncTestTaskAsyncFastStartT )

    protected:

        typedef AsyncExecutorWrapperCallbackImpl                            wrapper_t;

        typedef AsyncTestTaskBaseT< bl::tasks::SimpleTaskBase >             base_type;

        AsyncTestTaskAsyncFastStartT(
            SAA_in              const bl::om::ObjPtr< wrapper_t >&          wrapperImpl,
            SAA_in_opt          const bool                                  tasksAtEvenCalls = coinToss()
            )
            :
            base_type( wrapperImpl, tasksAtEvenCalls )
        {
        }

        virtual void onExecute() NOEXCEPT OVERRIDE
        {
            BL_TASKS_HANDLER_BEGIN()

            base_type::startTask();

            BL_TASKS_HANDLER_END_NOTREADY()
        }
    };

    typedef bl::om::ObjectImpl< AsyncTestTaskAsyncFastStartT<> > AsyncTestTaskAsyncFastStartImpl;

} // asynccb

UTF_AUTO_TEST_CASE( AsyncCB_BasicTests )
{
    using namespace asynccb;
    AsyncTestTaskAsyncOnlyImpl::executeBasicTests< AsyncTestTaskAsyncOnlyImpl >();
}

UTF_AUTO_TEST_CASE( AsyncCB_SmallPerfTests )
{
    using namespace asynccb;
    AsyncTestTaskAsyncOnlyImpl::executePerfTests< AsyncTestTaskAsyncOnlyImpl >( false /* testCancel */ );
}

UTF_AUTO_TEST_CASE( AsyncCB_CancelTests )
{
    using namespace asynccb;
    AsyncTestTaskAsyncFastStartImpl::executePerfTests< AsyncTestTaskAsyncFastStartImpl >( true /* testCancel */ );
}

