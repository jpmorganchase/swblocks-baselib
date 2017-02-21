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

#include <baselib/messaging/AsyncDataChunkStorage.h>
#include <baselib/messaging/TcpBlockTransferClient.h>
#include <baselib/messaging/TcpBlockServerDataChunkStorage.h>

#include <baselib/http/SimpleHttpTask.h>

#include <baselib/tasks/utils/Pinger.h>

#include <baselib/tasks/TasksUtils.h>
#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/utils/ScanDirectoryTask.h>
#include <baselib/tasks/SimpleTaskControlToken.h>

#include <baselib/reactive/ProcessingUnit.h>
#include <baselib/reactive/ObservableBase.h>
#include <baselib/reactive/ObserverBase.h>
#include <baselib/reactive/Observer.h>

#include <baselib/transfer/ChunksTransmitter.h>
#include <baselib/transfer/FilesPackagerUnit.h>
#include <baselib/transfer/RecursiveDirectoryScanner.h>
#include <baselib/transfer/SendRecvContext.h>

#include <baselib/data/FilesystemMetadata.h>
#include <baselib/data/FilesystemMetadataInMemoryImpl.h>

#include <baselib/http/Globals.h>

#include <baselib/core/NumberUtils.h>
#include <baselib/core/BoxedObjects.h>
#include <baselib/core/OS.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/PathUtils.h>
#include <baselib/core/Utils.h>
#include <baselib/core/Random.h>
#include <baselib/core/BaseIncludes.h>

#include <cstdlib>
#include <cstdint>
#include <unordered_map>

#include <utests/baselib/MachineGlobalTestLock.h>
#include <utests/baselib/TestTaskUtils.h>
#include <utests/baselib/UtfArgsParser.h>
#include <utests/baselib/Utf.h>
#include <utests/baselib/TestFsUtils.h>

/************************************************************************
 * Tasks library code tests
 */

UTF_AUTO_TEST_CASE( Tasks_TaskBaseInterfaceTests )
{
    using namespace bl;
    using namespace bl::tasks;

    /*
     * Define MyTaskClass derived from SimpleTaskT just to expose the
     * enhanceException method for testing
     */

    class MyTaskClass : public SimpleTaskT<>
    {
        BL_CTR_DEFAULT( MyTaskClass, protected )

    public:

        typedef SimpleTaskT<> base_type;

        virtual void enhanceException( SAA_in eh::exception& exception ) const OVERRIDE
        {
            base_type::enhanceException( exception );
        }
    };

    typedef om::ObjectImpl< MyTaskClass > MyTaskClassImpl;

    const auto taskImpl = MyTaskClassImpl::createInstance();

    /*
     * Test the error handling enhancements
     */

    UnexpectedException exception;
    UTF_REQUIRE( nullptr == exception.timeThrown() );

    /*
     * Verify that once we call enhanceException( ... ) time_thrown is
     * injected and conforms to the expected format
     */

    taskImpl -> enhanceException( exception );
    const auto* timeThrown = exception.timeThrown();
    UTF_REQUIRE( timeThrown && ! timeThrown -> empty() );

    bl::str::regex regex( bl::time::regexLocalTimeISO() );
    bl::str::smatch results;
    UTF_CHECK( bl::str::regex_match( *timeThrown, results, regex ) );

    const auto timeThrownCopy = *timeThrown;

    /*
     * Once time_thrown is injected in the exception it will not be
     * updated if enhanceException( ... ) is called again
     */

    taskImpl -> enhanceException( exception );
    const auto* timeThrownNew = exception.timeThrown();
    UTF_REQUIRE( timeThrownNew && ! timeThrownNew -> empty() );
    UTF_REQUIRE_EQUAL( timeThrownCopy, *timeThrownNew );
}

namespace
{
    enum ThrowAction
    {
        None,
        FromCallback,
        FromScheduler,
    };

    bool algorithmsSimpleParallelTest(
        SAA_in          const ThrowAction                               throwAction = None,
        SAA_in          const std::size_t                               count = 50,
        SAA_in          const bool                                      prioritizeWaitCancel = false,
        SAA_in          const bool                                      keepSuccessful = false
        )
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace bl::transfer;

        const auto t1 = bl::time::microsec_clock::universal_time();

        scheduleAndExecuteInParallel(
            [ &throwAction, &count, &prioritizeWaitCancel, &keepSuccessful ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto cb = [ &throwAction, &count, &prioritizeWaitCancel, &keepSuccessful ](
                    SAA_in              const std::size_t                    id,
                    SAA_in              const std::size_t                    timeoutMilliseconds
                    ) -> void
                {
                    /*
                     * Sleep for timeoutMilliseconds and then and print message
                     */

                    os::sleep( time::milliseconds( timeoutMilliseconds ) );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Task executed: "
                            << id
                            << "; task took "
                            << timeoutMilliseconds
                            << " milliseconds"
                        );

                    BL_CHK( true, FromCallback == throwAction && 30 == id, BL_MSG() << "Problematic id: " << id );
                };

                /*
                 * Throttle limit to be enforced if prioritizeWaitCancel=true
                 */

                const std::size_t maxExecuting = 10;

                if( prioritizeWaitCancel )
                {
                    BL_ASSERT( count > ( maxExecuting * 4 ) );

                    eq -> setThrottleLimit( 10 /* maxExecuting */ );

                    /*
                     * Since we're going to wait on tasks we want to make
                     * sure we test the keepSuccessful case vs. not
                     */

                    eq -> setOptions( keepSuccessful ? ExecutionQueue::OptionKeepAll : ExecutionQueue::OptionKeepFailed );
                }

                std::vector< om::ObjPtr< Task > > toCancel;
                std::vector< om::ObjPtr< Task > > toWait;

                for( std::size_t i = 0; i < count; ++i )
                {
                    if( 0 == ( i % 2 ) )
                    {
                        auto task = eq -> push_front( cpp::bind< void >( cb, i, 100 + ( std::rand() % 200 ) ) );

                        if( prioritizeWaitCancel )
                        {
                            if( i > ( count / 2 ) && i < ( count - maxExecuting ) )
                            {
                                eq -> prioritize( task, true /* wait */ );
                            }

                            if( i < maxExecuting || i > ( count - maxExecuting ) )
                            {
                                toCancel.push_back( std::move( task ) );
                            }
                        }
                    }
                    else
                    {
                        auto task = eq -> push_back( cpp::bind< void >( cb, i, 100 + ( std::rand() % 200 ) ) );

                        if( prioritizeWaitCancel )
                        {
                            if( i < maxExecuting || i > ( count - maxExecuting ) )
                            {
                                toWait.push_back( std::move( task ) );
                            }
                        }
                    }

                    BL_CHK( true, FromScheduler == throwAction && 30 == i, BL_MSG() << "Problematic id: " << i );
                }

                if( prioritizeWaitCancel )
                {
                    for( const auto& task : toCancel )
                    {
                        eq -> cancel( task );
                    }

                    for( const auto& task : toWait )
                    {
                        eq -> wait( task );
                    }

                    if( keepSuccessful )
                    {
                        eq -> flush( false /* discardPending */, false /* nothrowIfFailed */, true /* discardReady */ );
                    }
                }
            }
            );

        const auto duration = bl::time::microsec_clock::universal_time() - t1;

        const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Test took "
                << durationInSeconds
                << " seconds"
            );

        return true;
    }
}

UTF_AUTO_TEST_CASE( Tasks_AlgorithmsTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "\n******************************** Starting test: Tasks_AlgorithmsTests ********************************\n" );

    UTF_REQUIRE( algorithmsSimpleParallelTest( ThrowAction::None /* throwAction */ ) );
}

UTF_AUTO_TEST_CASE( Tasks_WaitCancelAndPrioritizeTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "\n*************************** Starting test: Tasks_WaitCancelAndPrioritizeTests *************************\n" );

    UTF_REQUIRE(
        algorithmsSimpleParallelTest(
            ThrowAction::None   /* throwAction */,
            100                 /* count */,
            true                /* prioritizeWaitCancel */,
            true                /* keepSuccessful */
            )
        );

    UTF_REQUIRE(
        algorithmsSimpleParallelTest(
            ThrowAction::None   /* throwAction */,
            100                 /* count */,
            true                /* prioritizeWaitCancel */,
            false               /* keepSuccessful */
            )
        );
}

UTF_AUTO_TEST_CASE( Tasks_AlgorithmsFailedTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "\n******************************** Starting test: Tasks_AlgorithmsFailedTests ********************************\n" );

    try
    {
        ( void ) algorithmsSimpleParallelTest( FromCallback /* throwAction */ );
        UTF_FAIL( BL_MSG() << "Must throw exception" );
    }
    catch( bl::UnexpectedException& e )
    {
        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "Expected exception was thrown: "
                << *e.message()
            );

        UTF_REQUIRE_EQUAL( "Problematic id: 30", *e.message() );
    }
}

UTF_AUTO_TEST_CASE( Tasks_SchedulingFailureTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "\n******************************** Starting test: Tasks_SchedulingFailureTests ********************************\n" );

    try
    {
        ( void ) algorithmsSimpleParallelTest( FromScheduler /* throwAction */ );
        UTF_FAIL( BL_MSG() << "Must throw exception" );
    }
    catch( bl::UnexpectedException& e )
    {
        BL_LOG(
            bl::Logging::debug(),
            BL_MSG()
                << "Expected exception was thrown: "
                << *e.message()
            );

        UTF_REQUIRE_EQUAL( "Problematic id: 30", *e.message() );
    }
}

UTF_AUTO_TEST_CASE( Tasks_StrandTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    const auto t1 = bl::time::microsec_clock::universal_time();

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            /*
             * Let's make the queue stranded (i.e. only one task at a time will execute)
             */

            eq -> setThrottleLimit( 1 );

            std::size_t count = 50;

            const auto cb = [ count ](
                SAA_in              const std::shared_ptr< std::size_t >&        valuePrev,
                SAA_in              const std::size_t                            value
                ) -> void
            {
                UTF_REQUIRE_EQUAL( ( value - 1 ), *valuePrev );

                *valuePrev = value;

                BL_LOG( Logging::debug(), BL_MSG() << "Strand test: value: " << value );

                if( value == ( count / 2 ) )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Strand test: sleep for 300 milliseconds before we schedule second half"
                        );

                    os::sleep( time::milliseconds( 300 ) );
                }
            };

            std::shared_ptr< std::size_t > sharedValue( new std::size_t );
            *sharedValue = 0;

            for( std::size_t i = 0; i < count; ++i )
            {
                eq -> push_back( cpp::bind< void >( cb, sharedValue, i + 1 ) );
            }
        }
        );

    const auto duration = bl::time::microsec_clock::universal_time() - t1;

    const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Strand test took "
            << durationInSeconds
            << " seconds"
        );
}

UTF_AUTO_TEST_CASE( Tasks_ExecutionQueueCancelTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    const auto cbTest = []( SAA_in const bool keepCanceled ) -> void
    {
        scheduleAndExecuteInParallel(
            [ &keepCanceled ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                eq -> setOptions(
                    keepCanceled ?
                        ExecutionQueue::OptionKeepCanceled : ExecutionQueue::OptionKeepNone
                    );

                eq -> setThrottleLimit( 1 );

                bool executed1 = false;
                bool executed2 = false;

                const auto task1 = eq -> push_back(
                    [ &executed1 ]() -> void
                    {
                        os::sleep( time::seconds( 3 ) );
                        executed1 = true;
                    }
                    );

                const auto task2 = eq -> push_back(
                    [ &executed2 ]() -> void
                    {
                        os::sleep( time::milliseconds( 50 ) );
                        executed2 = true;
                    }
                    );

                os::sleep( time::seconds( 1 ) );

                /*
                 * Since the queue capacity is maxed out when the first
                 * task is submitted, the second task will not start, immediately
                 *
                 * In this case, true cancellation is possible for the
                 * second task, but not for the first task, which should
                 * already have started
                 */

                UTF_REQUIRE( ! eq -> cancel( task1, false /* wait */ ) );
                UTF_REQUIRE( eq -> cancel( task2, false /* wait */ ) );

                /*
                 * Flush the queue and verify that the tasks were executed
                 * and canceled respectively
                 */

                eq -> flush();

                UTF_REQUIRE( executed1 );
                UTF_REQUIRE( ! executed2 );

                if( keepCanceled )
                {
                    UTF_REQUIRE( ! eq -> isEmpty() );
                    const auto taskTop = eq -> pop( false /* wait */ );
                    UTF_REQUIRE( taskTop );
                    UTF_REQUIRE( om::areEqual( taskTop, task2 ) );
                }

                UTF_REQUIRE( eq -> isEmpty() );
            }
            );
    };

    cbTest( false /* keepCanceled */ );
    cbTest( true /* keepCanceled */ );
}

UTF_AUTO_TEST_CASE( Tasks_ExecutionQueueCancelRandomTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( ExecutionQueue::OptionKeepNone );

            std::vector< om::ObjPtr< Task > > tasks;

            const std::size_t maxCount = 200;

            tasks.reserve( maxCount );

            for( std::size_t i = 0; i < maxCount; ++i )
            {
                tasks.push_back(
                    eq -> push_back(
                        []() -> void
                        {
                            os::sleep( time::milliseconds( 50 ) );
                        }
                        )
                    );
            }

            /*
             * We just cancel all tasks to make sure we don't have a
             * threading issue
             *
             * And also cancel multiple times to ensure idempotency
             */

            for( const auto& task : tasks )
            {
                eq -> cancel( task, false /* wait */ );
            }
        }
        );
}

UTF_AUTO_TEST_CASE( Tasks_ExecutionQueueWaitNoPrioritizeTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( ExecutionQueue::OptionKeepNone );

            std::vector< om::ObjPtr< Task > > tasks;

            const std::size_t maxCount = 50;

            tasks.reserve( maxCount );

            for( std::size_t i = 0; i < maxCount; ++i )
            {
                tasks.push_back(
                    eq -> push_back(
                        []() -> void
                        {
                            os::sleep( time::milliseconds( 20 ) );
                        }
                        )
                    );
            }

            /*
             * We just call waitNoPrioritize on the last first and
             * then all of them in order
             */

            eq -> waitNoPrioritize( tasks[ tasks.size() - 1 ] );
            eq -> waitNoPrioritize( tasks[ 0 ] );

            for( const auto& task : tasks )
            {
                eq -> waitNoPrioritize( task );
            }
        }
        );
}

UTF_AUTO_TEST_CASE( Tasks_TaskContinuationsTests )
{
    using namespace bl;
    using namespace bl::tasks;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            typedef om::ObjPtr< Task > task_t;

            eq -> setOptions( ExecutionQueue::OptionKeepAll );

            const auto cbRandomWait = []() -> void
            {
                os::sleep( time::milliseconds( 20 ) );
            };

            /*
             * Continuations executing normally test
             */

            {
                const auto cbContinuationNormal = [](
                    SAA_inout               Task*               expectedFinishedTask,
                    SAA_inout               Task*               continuationTask,
                    SAA_inout               Task*               finishedTask
                    ) -> task_t
                {
                    UTF_REQUIRE( finishedTask );
                    UTF_REQUIRE_EQUAL( finishedTask, expectedFinishedTask );
                    return om::copy( continuationTask );
                };

                const auto taskImpl1 = SimpleTaskImpl::createInstance( cpp::copy( cbRandomWait ) );
                const auto taskImpl2 = SimpleTaskImpl::createInstance( cpp::copy( cbRandomWait ) );
                const auto taskImpl3 = SimpleTaskImpl::createInstance( cpp::copy( cbRandomWait ) );

                taskImpl1 -> setContinuationCallback(
                    cpp::bind< task_t >(
                        cbContinuationNormal,
                        static_cast< Task* >( taskImpl1.get() )         /* expectedFinishedTask */,
                        static_cast< Task* >( taskImpl2.get() )         /* continuationTask */,
                        _1                                              /* finishedTask */
                        )
                    );

                taskImpl2 -> setContinuationCallback(
                    cpp::bind< task_t >(
                        cbContinuationNormal,
                        static_cast< Task* >( taskImpl2.get() )         /* expectedFinishedTask */,
                        static_cast< Task* >( taskImpl3.get() )         /* continuationTask */,
                        _1                                              /* finishedTask */
                        )
                    );

                eq -> push_back( om::qi< Task >( taskImpl1 ) );

                eq -> flush();

                UTF_REQUIRE_EQUAL( eq -> pop(), om::qi< Task >( taskImpl1 ) );
                UTF_REQUIRE_EQUAL( eq -> pop(), om::qi< Task >( taskImpl2 ) );
                UTF_REQUIRE_EQUAL( eq -> pop(), om::qi< Task >( taskImpl3 ) );

                UTF_REQUIRE( eq -> isEmpty() );
            }

            /*
             * Several continuations executing one after another via SimpleTaskWithContinuation
             */

            {
                class LocalTester
                {
                public:

                    static auto incrementCounter(
                        SAA_inout               std::size_t&                counter,
                        SAA_in                  const std::size_t           callsRemaining
                        )
                        -> om::ObjPtr< Task >
                    {
                        if( ! callsRemaining )
                        {
                            return nullptr;
                        }

                        ++counter;

                        return SimpleTaskWithContinuation::createInstance< Task >(
                            cpp::bind(
                                &LocalTester::incrementCounter,
                                cpp::ref( counter ),
                                callsRemaining - 1U
                                )
                            );
                    }
                };

                std::size_t counter = 0U;

                const auto taskImpl1 =
                    SimpleTaskWithContinuation::createInstance(
                        cpp::bind(
                            &LocalTester::incrementCounter,
                            cpp::ref( counter ),
                            3U
                            )
                        );

                UTF_REQUIRE_EQUAL( counter, 0U );

                eq -> push_back( om::qi< Task >( taskImpl1 ) );

                eq -> flush();

                UTF_REQUIRE_EQUAL( eq -> size(), 4U );
                UTF_REQUIRE_EQUAL( eq -> pop(), om::qi< Task >( taskImpl1 ) );
                UTF_REQUIRE( eq -> pop() != om::qi< Task >( taskImpl1 ) );
                UTF_REQUIRE( eq -> pop() != om::qi< Task >( taskImpl1 ) );
                UTF_REQUIRE( eq -> pop() != om::qi< Task >( taskImpl1 ) );

                UTF_REQUIRE( eq -> isEmpty() );

                UTF_REQUIRE_EQUAL( counter, 3U );
            }

            /*
             * Continuations failing test
             */

            {
                const std::string expectedMessage( "Create continuation failed" );

                const auto cbContinuationFailure = [ &expectedMessage ](
                    SAA_inout               Task*               expectedFinishedTask,
                    SAA_inout               Task*               continuationTask,
                    SAA_inout               Task*               finishedTask
                    ) -> om::ObjPtr< Task >
                {
                    UTF_REQUIRE( finishedTask );
                    UTF_REQUIRE_EQUAL( finishedTask, expectedFinishedTask );
                    BL_UNUSED( continuationTask );

                    BL_THROW(
                        UnexpectedException(),
                        BL_MSG()
                            << expectedMessage
                        );
                };

                const auto taskImpl1 = SimpleTaskImpl::createInstance( cpp::copy( cbRandomWait ) );
                const auto taskImpl2 = SimpleTaskImpl::createInstance( cpp::copy( cbRandomWait ) );

                taskImpl1 -> setContinuationCallback(
                    cpp::bind< task_t >(
                        cbContinuationFailure,
                        static_cast< Task* >( taskImpl1.get() )         /* expectedFinishedTask */,
                        static_cast< Task* >( taskImpl2.get() )         /* continuationTask */,
                        _1                                              /* finishedTask */
                        )
                    );

                eq -> push_back( om::qi< Task >( taskImpl1 ) );

                const auto executedTask = eq -> pop();
                UTF_REQUIRE( executedTask -> isFailed() );
                UTF_REQUIRE( eq -> isEmpty() );

                try
                {
                    cpp::safeRethrowException( executedTask -> exception() );
                    UTF_FAIL( "cpp::safeRethrowException must throw" );
                }
                catch( UnexpectedException& e )
                {
                    const std::string msg( e.what() );
                    UTF_REQUIRE_EQUAL( msg, expectedMessage );
                }
            }
        }
        );
}

namespace
{
    class MyContinuationState : public bl::tasks::WrapperTaskBase
    {
        BL_DECLARE_OBJECT_IMPL( MyContinuationState )

    protected:

        typedef MyContinuationState             this_type;
        typedef bl::tasks::WrapperTaskBase      base_type;

        typedef bl::tasks::SimpleTaskImpl       SimpleTaskImpl;
        typedef bl::tasks::Task                 Task;

        const bl::om::ObjPtr< SimpleTaskImpl >  m_taskImpl1;
        const bl::om::ObjPtr< SimpleTaskImpl >  m_taskImpl2;
        const bl::om::ObjPtr< SimpleTaskImpl >  m_taskImpl3;
        bl::cpp::ScalarTypeIniter< int >        m_executions;

        MyContinuationState()
            :
            m_taskImpl1( SimpleTaskImpl::createInstance( &this_type::randomWait ) ),
            m_taskImpl2( SimpleTaskImpl::createInstance( &this_type::randomWait ) ),
            m_taskImpl3( SimpleTaskImpl::createInstance( &this_type::randomWait ) )
        {
            m_wrappedTask = bl::om::qi< Task >( m_taskImpl1 );
        }

        static void randomWait()
        {
            bl::os::sleep( bl::time::milliseconds( 20 ) );
        };

    public:

        auto taskImpl3() const NOEXCEPT -> const bl::om::ObjPtr< SimpleTaskImpl >&
        {
            return m_taskImpl3;
        }

        int executions() const NOEXCEPT
        {
            return m_executions;
        }

        virtual bl::om::ObjPtr< Task > continuationTask() OVERRIDE
        {
            auto task = base_type::handleContinuationForward();

            if( task )
            {
                return task;
            }

            BL_MUTEX_GUARD( m_lock );

            if( bl::om::areEqual( m_wrappedTask, m_taskImpl1 ) )
            {
                /*
                 * Transfer state from m_taskImpl1 and use some of it to
                 * setup m_taskImpl2
                 */

                m_wrappedTask = bl::om::qi< Task >( m_taskImpl2 );
                ++m_executions.lvalue();
            }
            else if( bl::om::areEqual( m_wrappedTask, m_taskImpl2 ) )
            {
                /*
                 * Transfer state from m_taskImpl2 and use some of it to
                 * setup m_taskImpl3
                 */

                m_wrappedTask = bl::om::qi< Task >( m_taskImpl3 );
                ++m_executions.lvalue();
            }
            else
            {
                return nullptr;
            }

            return bl::om::copyAs< Task >( this );
        }
    };

    typedef bl::om::ObjectImpl< MyContinuationState > MyContinuationStateImpl;

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_TaskContinuationsWithContextTests )
{
    using namespace bl;
    using namespace bl::tasks;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( ExecutionQueue::OptionKeepAll );

            const auto taskImpl = MyContinuationStateImpl::createInstance();

            eq -> push_back( om::qi< Task >( taskImpl ) );
            eq -> flush();

            UTF_REQUIRE_EQUAL( eq -> pop(), om::qi< Task >( taskImpl ) );
            UTF_REQUIRE( eq -> isEmpty() );
            UTF_REQUIRE_EQUAL( taskImpl -> executions(), 2 );

            UTF_REQUIRE( taskImpl -> getWrappedTask() );
            UTF_REQUIRE( taskImpl -> getWrappedTaskT< Task >() );

            const auto autoSimpleTaskImpl = taskImpl -> getWrappedTaskT< SimpleTaskImpl >();
            UTF_REQUIRE( autoSimpleTaskImpl );
            UTF_REQUIRE_EQUAL( autoSimpleTaskImpl, taskImpl -> taskImpl3() );
        }
        );
}

namespace
{
    class MyCounterObject : public bl::om::Object
    {
        BL_DECLARE_OBJECT_IMPL_DEFAULT( MyCounterObject )

    private:

        std::size_t m_value;

    protected:

        MyCounterObject( SAA_in const std::size_t value )
            :
            m_value( value )
        {
        }

    public:

        std::size_t getValue() const NOEXCEPT
        {
            return m_value;
        }
    };

    typedef bl::om::ObjectImpl< MyCounterObject > MyCounterObjectImpl;


    /**
     * @brief A simple observable which generates a sequence of numbers with some delay
     */

    template
    <
        typename E = void
    >
    class MonotonicCounterObservableT :
       public bl::reactive::ObservableBase
    {
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( MonotonicCounterObservableT )

    protected:

        typedef bl::reactive::ObservableBase                            base_type;

        const bool                                                      m_throwFromInnerLoop;
        const std::size_t                                               m_ticksCount;
        const std::size_t                                               m_intervalInMilliseconds;
        std::size_t                                                     m_tickCurrent;

        ~MonotonicCounterObservableT() NOEXCEPT
        {
            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "MonotonicCounterObservableT::~MonotonicCounterObservableT"
                );
        }

        virtual void tryStopObservable() OVERRIDE
        {
            /*
             * Nothing special to do in this case
             */

            BL_ASSERT( base_type::m_stopRequested );
        }

        virtual bl::time::time_duration chk2LoopUntilFinished() OVERRIDE
        {
            using namespace bl;
            using namespace bl::data;
            using namespace bl::tasks;
            using namespace bl::transfer;

            if( base_type::m_stopRequested )
            {
                return time::neg_infin;
            }

            if( m_throwFromInnerLoop && m_tickCurrent == ( m_ticksCount / 2 ) )
            {
                BL_CHK( false, false, BL_MSG() << "Throwing a test exception from the inner loop" );
            }

            om::ObjPtrCopyable< MyCounterObjectImpl > value =
                MyCounterObjectImpl::createInstance( m_tickCurrent );

            if( base_type::notifyOnNext( cpp::any( value ) ) )
            {
                /*
                 * Only update the counter of the event was pushed through
                 * the event queues
                 */

                ++m_tickCurrent;
            }
            else
            {
                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "MonotonicCounterObservable::run(): events queue full; waiting... "
                    );
            }

            const bool moreTicks = ( m_tickCurrent < m_ticksCount );

            if( moreTicks )
            {
                return time::milliseconds( m_intervalInMilliseconds );
            }

            /*
             * Notify we're finished
             */

            return time::neg_infin;
        }

    public:

        MonotonicCounterObservableT(
            SAA_in              const bool                              throwFromInnerLoop = false,
            SAA_in              const std::size_t                       ticksCount = 30,
            SAA_in              const std::size_t                       intervalInMilliseconds = 10
            )
            :
            m_throwFromInnerLoop( throwFromInnerLoop ),
            m_ticksCount( ticksCount ),
            m_intervalInMilliseconds( intervalInMilliseconds ),
            m_tickCurrent( 0U )
        {
        }
    };

    typedef bl::om::ObjectImpl< MonotonicCounterObservableT<>, true /* enableSharedPtr */ > MonotonicCounterObservableImpl;

    /**
     * @brief A simple observer
     */

    template
    <
        typename E = void
    >
    class MonotonicCounterObserverT :
        public bl::reactive::ObserverBase
    {
        BL_CTR_DEFAULT( MonotonicCounterObserverT, protected )
        BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( MonotonicCounterObserverT )

    protected:

        typedef bl::reactive::ObserverBase base_type;

        bl::cpp::ScalarTypeIniter< std::size_t >            m_lastValue;
        bl::cpp::ScalarTypeIniter< bool >                   m_onCompletedCalled;

        ~MonotonicCounterObserverT() NOEXCEPT
        {
            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "MonotonicCounterObserverT::~MonotonicCounterObserverT"
                );
        }

    public:

        bool onCompletedCalled() const NOEXCEPT
        {
            return m_onCompletedCalled;
        }

        virtual void onCompleted() OVERRIDE
        {
            base_type::onCompleted();

            m_onCompletedCalled = true;
        }

        virtual bool onNext( SAA_in const bl::cpp::any& value ) OVERRIDE
        {
            if( 0 == ( std::rand() % 2 ) )
            {
                /*
                 * At some random calls we will reject values (requesting
                 * these to be retried again)
                 */

                return false;
            }

            const auto v = bl::cpp::any_cast< bl::om::ObjPtrCopyable< MyCounterObjectImpl > >( value ) -> getValue();

            if( m_lastValue )
            {
                UTF_REQUIRE_EQUAL( m_lastValue + 1, v );
            }

            m_lastValue = v;

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "MonotonicCounterObserver::onNext(): "
                    << v
                );

            bl::os::sleep( bl::time::milliseconds( 30 ) );

            return true;
        }
    };

    typedef bl::om::ObjectImpl< MonotonicCounterObserverT<> > MonotonicCounterObserverImpl;

} // __unnnamed

namespace
{
    enum ReactiveTest
    {
        Default,
        DisconnectObserver,
        DisconnectObservable,
        ThrowFromInnerLoop,
    };

    void runReactiveTest( SAA_in const ReactiveTest test = Default, SAA_in const std::size_t maxPendingEvents = 0 )
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace bl::transfer;

        const auto t1 = bl::time::microsec_clock::universal_time();

        scheduleAndExecuteInParallel(
            [ &test, &maxPendingEvents ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto observableImpl = om::getSharedPtr(
                    MonotonicCounterObservableImpl::createInstance( ThrowFromInnerLoop == test )
                    );

                const auto observable = om::qi< reactive::Observable >( observableImpl );

                if( maxPendingEvents )
                {
                    observableImpl -> setThrottleLimit( maxPendingEvents );
                }

                observableImpl -> allowNoSubscribers( true );

                const auto observerImpl = MonotonicCounterObserverImpl::createInstance();
                const auto observer = om::qi< reactive::Observer >( observerImpl );

                const auto subscription = observable -> subscribe( observer );

                const auto task = om::qi< Task >( observable.get() );

                eq -> push_back( task );

                if( DisconnectObserver == test || DisconnectObservable == test )
                {
                    /*
                     * Disconnect after ~ 1 second (i.e. in the middle of the streaming)
                     */

                    os::sleep( time::milliseconds( 1000 ) );

                    if( DisconnectObserver == test )
                    {
                        subscription -> dispose();
                    }
                    else if( DisconnectObservable == test )
                    {
                        om::qi< om::Disposable >( observable ) -> dispose();
                    }
                }

                try
                {
                    eq -> waitForSuccess( task );
                }
                catch( eh::system_error& e )
                {
                    if( DisconnectObservable != test )
                    {
                        throw;
                    }

                    UTF_REQUIRE( e.code() == asio::error::operation_aborted );
                }

                if( DisconnectObserver != test )
                {
                    UTF_REQUIRE( observerImpl -> onCompletedCalled() );
                }
            }
            );

        const auto duration = bl::time::microsec_clock::universal_time() - t1;

        const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Reactive test took "
                << durationInSeconds
                << " seconds"
            );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_ReactiveTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "*** Default tests\n" );
    runReactiveTest( Default );
}

UTF_AUTO_TEST_CASE( Tasks_ReactiveTestsWithException )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "*** Default tests with throw\n" );

    try
    {
        runReactiveTest( ThrowFromInnerLoop );
        UTF_FAIL( "This must throw" );
    }
    catch( bl::UnexpectedException& e )
    {
        const auto msg = e.message();

        UTF_REQUIRE( msg );
        UTF_REQUIRE_EQUAL( *msg, "Throwing a test exception from the inner loop" );
    }
}

UTF_AUTO_TEST_CASE( Tasks_ReactiveTestsWithThrottle )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "*** Default tests (with throttle)\n" );
    runReactiveTest( Default, 5 /* maxPendingEvents */ );
}

UTF_AUTO_TEST_CASE( Tasks_ReactiveDisconnectObserverTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "*** DisconnectObserver tests\n" );
    runReactiveTest( DisconnectObserver );
}

UTF_AUTO_TEST_CASE( Tasks_ReactiveDisconnectObservableTests )
{
    BL_LOG_MULTILINE( bl::Logging::debug(), BL_MSG() << "*** DisconnectObservable tests\n" );
    runReactiveTest( DisconnectObservable );
}

UTF_AUTO_TEST_CASE( Tasks_ExecutionQueueOptionsTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;

    BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "*** ExecutionQueueOptions tests\n" );

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            const auto cbSuccessful = []() -> void
            {
            };

            const auto cbFailed = []() -> void
            {
                BL_CHK( false, false, BL_MSG() << "Expected test task failure" );
            };

            eq -> setOptions( ExecutionQueue::OptionKeepAll );
            const auto taskSuccessful = eq -> push_back( cbSuccessful );
            const auto taskFailed = eq -> push_back( cbFailed );
            eq -> flushNoThrowIfFailed();
            UTF_REQUIRE( eq -> pop( false /* wait */ ) );
            UTF_REQUIRE( eq -> pop( false /* wait */ ) );
            UTF_REQUIRE( eq -> isEmpty() );

            eq -> setOptions( ExecutionQueue::OptionKeepFailed );
            eq -> push_back( taskSuccessful );
            eq -> push_back( taskFailed );
            eq -> flushNoThrowIfFailed();
            UTF_REQUIRE( eq -> pop( false /* wait */ ) -> isFailed() );
            UTF_REQUIRE( eq -> isEmpty() );

            eq -> setOptions( ExecutionQueue::OptionKeepSuccessful );
            eq -> push_back( taskSuccessful );
            eq -> push_back( taskFailed );
            eq -> flushNoThrowIfFailed();
            UTF_REQUIRE( ! eq -> pop( false /* wait */ ) -> isFailed() );
            UTF_REQUIRE( eq -> isEmpty() );

            eq -> setOptions( ExecutionQueue::OptionKeepNone );
            eq -> push_back( taskSuccessful );
            eq -> push_back( taskFailed );
            eq -> flushNoThrowIfFailed();
            UTF_REQUIRE( eq -> isEmpty() );
        });
}

UTF_AUTO_TEST_CASE( Tasks_ScanDirectoryTaskTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;
    using namespace utest;
    using namespace fs;


    cpp::SafeUniquePtr< TmpDir > tmpDir;
    auto root = test::UtfArgsParser::path();

    if( root.empty() )
    {
        /*
         * Use system generated temporary directory if parameter is not provided
         */
        tmpDir = cpp::SafeUniquePtr< TmpDir >::attach( new TmpDir );
        root = tmpDir -> path().string();
        TestFsUtils dummyCreator;
        dummyCreator.createDummyTestDir( tmpDir -> path() );
    }

    BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "*** Recursive directory scanner tests\n" );

    const auto controlToken =
        test::UtfArgsParser::isRelaxedScanMode() ?
            utest::ScanningControlImpl::createInstance() :
            nullptr;

    const auto t1 = bl::time::microsec_clock::universal_time();

    scheduleAndExecuteInParallel(
        [ &root, &controlToken ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( ExecutionQueue::OptionKeepAll );

            {
                const auto boxedRootPath = bo::path::createInstance();
                boxedRootPath -> swapValue( root );

                const auto scanner = ScanDirectoryTaskImpl::createInstance(
                    root,
                    boxedRootPath,
                    controlToken ?
                        om::qi< DirectoryScannerControlToken >( controlToken ) :
                        nullptr /* controlToken */
                    );

                const auto scannerTask = om::qi< Task >( scanner );
                eq -> push_back( scannerTask );
            }

            long entriesCount = 0L;

            for( ;; )
            {
                const auto scannerTask = eq -> pop( true /* wait */ );

                if( ! scannerTask )
                {
                    break;
                }

                if( scannerTask -> isFailed() )
                {
                    cpp::safeRethrowException( scannerTask -> exception() );
                }

                const auto scanner = om::qi< ScanDirectoryTaskImpl >( scannerTask );

                for( const auto& entry : scanner -> entries() )
                {
                    ++entriesCount;

                    if( test::UtfArgsParser::isVerboseMode() )
                    {
                        utest::ScanningControlImpl::dumpEntryInfo( entry );
                    }
                }
            }

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Total number of entries is "
                    << entriesCount
                );
        });

    const auto duration = bl::time::microsec_clock::universal_time() - t1;

    const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Scanning "
            << root
            << " took "
            << durationInSeconds
            << " seconds"
        );
}

/************************************************************************
 * Test to demo how to create a simple timer task
 */

namespace
{
    template
    <
        typename E = void
    >
    class MyTimerTaskT :
        public bl::tasks::TimerTaskBase
    {
        BL_CTR_DEFAULT( MyTimerTaskT, protected )
        BL_DECLARE_OBJECT_IMPL( MyTimerTaskT )

    protected:

        virtual bl::time::time_duration run()
        {
            if( isCanceled() )
            {
                /*
                 * Returning time::neg_infin indicates the
                 * task wants to exit the timer loop
                 */

                return bl::time::neg_infin;
            }

            /*
             * Just do something and wait for some timeout
             * between each call of run (200 milliseconds)
             */

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "Timer fired..."
                );

            return bl::time::milliseconds( 200 );
        }
    };

    typedef bl::om::ObjectImpl< MyTimerTaskT<> > MyTimerTaskImpl;

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_SimpleTimerTaskTests )
{
    using namespace bl;
    using namespace bl::tasks;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( ExecutionQueue::OptionKeepFailed );

            const auto checker = MyTimerTaskImpl::createInstance();

            /*
             * Just a fake check to avoid the 'Test case <name> did not check any assertions' message
             */

            UTF_REQUIRE( checker );

            /*
             * Push the task into the queue, then wait for 1 second and then cancel the task
             */

            eq -> push_back( om::qi< Task >( checker ) );

            os::sleep( time::milliseconds( 1000 ) );

            checker -> requestCancel();
            eq -> flush();

            if( ! eq -> isEmpty() )
            {
                /*
                 * Since we requested ExecutionQueue::OptionKeepFailed above the only case
                 * when this would happen is when the task has failed
                 */

                const auto task = eq -> pop( false /* wait */ );
                BL_ASSERT( eq -> isEmpty() );

                BL_ASSERT( task && task -> isFailed() && task -> exception() );
                cpp::safeRethrowException( task -> exception() );
            }

            /*
             * Ensure canceled flag is reset when re-scheduling the same task
             */

            UTF_CHECK( checker -> isCanceled() );

            eq -> push_back( om::qi< Task >( checker ) );

            os::sleep( time::milliseconds( 1000 ) );

            UTF_CHECK( ! checker -> isCanceled() );

            checker -> requestCancel();

            eq -> flush();

            UTF_CHECK( checker -> isCanceled() );
        });
}

UTF_AUTO_TEST_CASE( Tasks_AdjustableTimerTaskTests )
{
    using namespace bl;
    using namespace bl::tasks;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            long longCounter = 0L;

            const auto adjustableDurationUpdater = [ &longCounter ]() -> time::time_duration
            {
                ++longCounter;

                if( longCounter <= 10L )
                {
                    return time::milliseconds( longCounter * 100L );
                }

                /*
                 * Request to stop the timer
                 */

                return time::neg_infin;
            };

            const auto task = AdjustableTimerTask::createInstance< Task >(
                adjustableDurationUpdater       /* callback */,
                time::seconds( 0L )             /* initDelay */
                );

            const auto start = time::microsec_clock::universal_time();

            eq -> push_back( task );
            eq -> wait( task );

            const auto elapsed = time::microsec_clock::universal_time() - start;

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Time elapsed in milliseconds: "
                    << elapsed.total_milliseconds()
                );

            /*
             * The expected elapsed time is expected to be ~ 5500 ms (5.5 seconds)
             *
             * Let's verify that it is within the 3 to 8 seconds range (to account
             * for errors due to machine slowness, etc)
             */

            UTF_REQUIRE( time::seconds( 3L ) < elapsed && elapsed < time::seconds( 8L ) );
        });
}

/************************************************************************
 * Tests for RecursiveDirectoryScanner
 */

namespace
{
    /**
     * @brief FileEntriesAnalyzer class
     */

    template
    <
        typename BASE = bl::om::ObjectDefaultBase
    >
    class FileEntriesAnalyzer :
        public BASE
    {
        BL_CTR_DEFAULT( FileEntriesAnalyzer, protected )
        BL_DECLARE_OBJECT_IMPL( FileEntriesAnalyzer )

    protected:

        typedef std::unordered_map< bl::fs::path, bl::fs::directory_entry > entries_map_t;

        entries_map_t                                                       m_entries;

        bl::cpp::ScalarTypeIniter< std::size_t >                            m_filesCount;
        bl::cpp::ScalarTypeIniter< std::size_t >                            m_dirsCount;
        bl::cpp::ScalarTypeIniter< std::size_t >                            m_symlinksCount;
        bl::cpp::ScalarTypeIniter< std::size_t >                            m_otherCount;

        bl::fs::path                                                        m_root;
        bl::cpp::ScalarTypeIniter< bool >                                   m_isVerbose;

    public:

        void setOptions( SAA_in const bl::fs::path& root, SAA_in const bool isVerbose = false )
        {
            m_entries.reserve( 400000 );
            m_root = root;
            m_isVerbose = isVerbose;
        }

        bool onDataArrived( SAA_in const bl::cpp::any& value )
        {
            using namespace bl;
            using namespace bl::data;
            using namespace bl::tasks;
            using namespace bl::transfer;

            const auto task = cpp::any_cast< om::ObjPtrCopyable< tasks::Task > >( value );
            const auto scanner = om::qi< tasks::ScanDirectoryTaskImpl >( task );

            for( const auto& entry : scanner -> entries() )
            {
                const auto status = entry.symlink_status();

                if( ! m_root.empty() )
                {
                    fs::path relPath;
                    BL_VERIFY( fs::getRelativePath( entry.path(), m_root, relPath ) );
                    BL_ASSERT( m_entries.find( relPath ) == m_entries.end() );

                    m_entries[ relPath ] = entry;
                }

                if( fs::is_regular_file( status ) )
                {
                    ++m_filesCount;
                }
                else if( fs::is_directory( status ) )
                {
                    ++m_dirsCount;
                }
                else if( fs::is_symlink( status ) )
                {
                    ++m_symlinksCount;
                }
                else if( fs::is_other( status ) )
                {
                    ++m_otherCount;
                }
                else
                {
                    BL_CHK( false, false, BL_MSG() << "Unexpected entry type" );
                }
            }

            return true;
        }

        bool onDummyDataArrivedConst( SAA_in const bl::cpp::any& /* value */ ) const
        {
            return true;
        }

        bool onDummyDataArrivedConstNoexcept( SAA_in const bl::cpp::any& /* value */ ) const NOEXCEPT
        {
            return true;
        }

        void logResults() const
        {
            BL_LOG_MULTILINE(
                bl::Logging::debug(),
                BL_MSG()
                    << "\nFileEntriesAnalyzer::logResults():\n"
                    << "\n    filesCount: "
                    << m_filesCount
                    << "\n    dirsCount: "
                    << m_dirsCount
                    << "\n    symlinksCount: "
                    << m_symlinksCount
                    << "\n    otherCount: "
                    << m_otherCount
                    << "\n"
                );

            if( m_isVerbose )
            {
                for( const auto& entry : m_entries )
                {
                    BL_LOG_MULTILINE(
                        bl::Logging::debug(),
                        BL_MSG()
                            << entry.first
                        );
                }
            }
        }
    };

    /**
     * @brief RecursiveDirectoryScannerObserver class
     */

    template
    <
        typename E = void
    >
    class RecursiveDirectoryScannerObserverT :
        public FileEntriesAnalyzer< bl::reactive::ObserverBase >
    {
    private:

        BL_CTR_DEFAULT( RecursiveDirectoryScannerObserverT, protected )
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( RecursiveDirectoryScannerObserverT, bl::reactive::Observer )

    public:

        virtual bool onNext( SAA_in const bl::cpp::any& value ) OVERRIDE
        {
            return onDataArrived( value );
        }
    };

    typedef RecursiveDirectoryScannerObserverT<> RecursiveDirectoryScannerObserver;

    typedef bl::om::ObjectImpl< RecursiveDirectoryScannerObserver > RecursiveDirectoryScannerObserverImpl;

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_RecursiveDirectoryScannerTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;
    using namespace reactive;
    using namespace utest;
    using namespace fs;

    cpp::SafeUniquePtr< TmpDir > tmpDir;
    fs::path root = test::UtfArgsParser::path();

    if( root.empty() )
    {
        /*
         * Use system generated temporary directory if parameter is not provided
         */
        tmpDir = cpp::SafeUniquePtr< TmpDir >::attach( new TmpDir );
        root = tmpDir -> path();
        TestFsUtils dummyCreator;
        dummyCreator.createDummyTestDir( root );
    }

    BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "*** Recursive directory scanner observable tests\n" );

    const auto t1 = bl::time::microsec_clock::universal_time();

    scheduleAndExecuteInParallel(
        [ &root ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            const auto controlToken =
                test::UtfArgsParser::isRelaxedScanMode() ?
                    utest::ScanningControlImpl::createInstance< DirectoryScannerControlToken >() :
                    nullptr;

            const auto scanner = RecursiveDirectoryScannerImpl::createInstance(
                root,
                controlToken
                );

            const auto scannerObserver = RecursiveDirectoryScannerObserverImpl::createInstance< reactive::Observer >();

            scanner -> subscribe( scannerObserver );

            const auto scannerTask = om::qi< Task >( scanner.get() );
            eq -> push_back( scannerTask );
            eq -> waitForSuccess( scannerTask );

            om::qi< RecursiveDirectoryScannerObserverImpl >( scannerObserver ) -> logResults();
        });

    const auto duration = bl::time::microsec_clock::universal_time() - t1;

    const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Scanning with RecursiveDirectoryScannerImpl "
            << root
            << " took "
            << durationInSeconds
            << " seconds"
        );
}

/************************************************************************
 * Tests for ProcessingUnit
 */

namespace
{
    template
    <
        typename BASE,
        typename II
    >
    void localProcessingUnitTester()
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace bl::transfer;
        using namespace reactive;
        using namespace utest;
        using namespace fs;

        cpp::SafeUniquePtr< TmpDir > tmpDir;
        fs::path root = test::UtfArgsParser::path();

        if( root.empty() )
        {
            /*
            * Use system generated temporary directory if parameter is not provided
            */
            tmpDir = cpp::SafeUniquePtr< TmpDir >::attach( new TmpDir );
            root = tmpDir -> path();
            TestFsUtils dummyCreator;
            dummyCreator.createDummyTestDir( root );
        }

        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "*** Processing units directory scanner observable tests\n" );

        const auto t1 = bl::time::microsec_clock::universal_time();

        scheduleAndExecuteInParallel(
            [ &root ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto controlToken =
                    test::UtfArgsParser::isRelaxedScanMode() ?
                        utest::ScanningControlImpl::createInstance< DirectoryScannerControlToken >() :
                        nullptr;

                const auto scanner = RecursiveDirectoryScannerImpl::createInstance(
                    root,
                    controlToken
                    );

                {
                    typedef om::ObjectImpl< ProcessingUnit< BASE, II > > unit_t;

                    const auto unit = unit_t::template createInstance< unit_t >();

                    const auto isVerbose = test::UtfArgsParser::isVerboseMode();

                    if( isVerbose )
                    {
                        unit -> setOptions( root, test::UtfArgsParser::isVerboseMode() );
                    }

                    scanner -> subscribe( unit -> bindInputConnector( &unit_t::onDataArrived, &unit_t::logResults ) );
                    scanner -> subscribe( unit -> bindInputConnector( &unit_t::onDummyDataArrivedConst ) );
                    scanner -> subscribe( unit -> bindInputConnector( &unit_t::onDummyDataArrivedConstNoexcept ) );
                }

                const auto scannerTask = om::qi< Task >( scanner.get() );
                eq -> push_back( scannerTask );
                eq -> waitForSuccess( scannerTask );
            });

        const auto duration = bl::time::microsec_clock::universal_time() - t1;

        const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Scanning with processing unit "
                << fs::path( root )
                << " took "
                << durationInSeconds
                << " seconds"
            );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_ProcessingUnitTests )
{
    localProcessingUnitTester< FileEntriesAnalyzer<>, bl::om::Object >();
}

UTF_AUTO_TEST_CASE( Tasks_ProcessingUnitWithObserverTests )
{
    localProcessingUnitTester< RecursiveDirectoryScannerObserver, bl::reactive::Observer >();
}

/************************************************************************
 * Tests for the FilesPackagerUnit
 */

namespace
{
    bl::om::ObjPtr< bl::data::FilesystemMetadataWO > createFSMD()
    {
        return bl::data::FilesystemMetadataInMemoryImpl::createInstance< bl::data::FilesystemMetadataWO >();
    }

    void executeTheFilesPackagerAndTransmitterPipeline(
        SAA_in              const bl::om::ObjPtrCopyable< bl::transfer::SendRecvContext >&  contextIn,
        SAA_in_opt          const std::string&                                              host = "localhost",
        SAA_in_opt          const unsigned short                                            port = 28100U
        )
    {
        using namespace bl;
        using namespace bl::data;
        using namespace bl::tasks;
        using namespace bl::transfer;
        using namespace reactive;
        using namespace utest;
        using namespace fs;

        cpp::SafeUniquePtr< TmpDir > tmpDir;
        fs::path root = test::UtfArgsParser::path();

        if( root.empty() )
        {
            /*
            * Use system generated temporary directory if parameter is not provided
            */
            tmpDir = cpp::SafeUniquePtr< TmpDir >::attach( new TmpDir );
            root = tmpDir -> path();
            TestFsUtils dummyCreator;
            dummyCreator.createDummyTestDir( root );
        }

        BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "*** Files packager processing unit's observable tests\n" );

        const auto t1 = bl::time::microsec_clock::universal_time();

        scheduleAndExecuteInParallel(
            [ &root, &contextIn, &host, &port ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto context = om::copy( contextIn.get() );

                /*
                 * Create the directory scanner unit
                 */

                const auto controlToken =
                    test::UtfArgsParser::isRelaxedScanMode() ?
                        utest::ScanningControlImpl::createInstance< DirectoryScannerControlToken >() :
                        nullptr;

                const auto scanner = RecursiveDirectoryScannerImpl::createInstance(
                    root,
                    controlToken
                    );

                const auto fsmd = createFSMD();

                /*
                 * Create the file packager unit
                 */

                typedef om::ObjectImpl
                    <
                        ProcessingUnit< FilesPackagerUnit, Observable >,
                        true /* enableSharedPtr */
                    > unit_packager_t;

                const auto unitPackager = unit_packager_t::createInstance( context, fsmd );

                scanner -> subscribe(
                    unitPackager -> bindInputConnector< unit_packager_t >(
                        &unit_packager_t::onFilesBatchArrived,
                        &unit_packager_t::onInputCompleted
                        )
                    );

                /*
                 * Create the chunks transmitter unit
                 */

                typedef om::ObjectImpl
                    <
                        ProcessingUnit< ChunksTransmitter, Observable >,
                        true /* enableSharedPtr */
                    > unit_transmitter_t;

                const auto selector = SimpleEndpointSelectorImpl::createInstance< EndpointSelector >(
                    cpp::copy( host ),
                    port
                    );

                const auto unitChunksTransmitter =
                        unit_transmitter_t::createInstance(
                            selector,
                            context,
                            fsmd,
                            test::UtfArgsParser::connections()
                            );

                unitPackager -> subscribe(
                    unitChunksTransmitter -> bindInputConnector< unit_transmitter_t >(
                        &unit_transmitter_t::onChunkArrived,
                        &unit_transmitter_t::onInputCompleted
                        )
                    );

                /*
                 * For the transmitter unit it is ok to have no subscribers
                 */

                unitChunksTransmitter -> allowNoSubscribers( true );

                /*
                 * Start the reactive units in the correct order and wait
                 * for completion
                 */

                eq -> push_back( om::qi< Task >( unitChunksTransmitter ) );
                eq -> push_back( om::qi< Task >( unitPackager ) );
                eq -> push_back( om::qi< Task >( scanner ) );

                executeQueueAndCancelOnFailure( eq );
            });

        const auto duration = bl::time::microsec_clock::universal_time() - t1;

        const auto durationInSeconds = duration.total_milliseconds() / 1000.0;

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Scanning with files packager processing unit "
                << root
                << " took "
                << durationInSeconds
                << " seconds"
            );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_FilesPackagerUnitTests )
{
    test::MachineGlobalTestLock lock;

    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::transfer;
    using namespace utest;
    using namespace test;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto context = SendRecvContext::createInstance(
        SimpleEndpointSelectorImpl::createInstance< EndpointSelector >(
            cpp::copy( UtfArgsParser::host() ),
            UtfArgsParser::port()
            )
        );

    const auto backendImpl = BackendImplTestImpl::createInstance();

    backendImpl -> setExpectRealData( true /* expectRealData */ );

    const auto storage = om::lockDisposable(
        AsyncDataChunkStorage::createInstance(
            om::qi< DataChunkStorage >( backendImpl ) /* writeBackend */,
            om::qi< DataChunkStorage >( backendImpl ) /* readBackend */,
            test::UtfArgsParser::threadsCount(),
            om::qi< TaskControlToken >( controlToken ),
            0U /* maxConcurrentTasks */,
            context -> dataBlocksPool()
            )
        );

    {
        TestTaskUtils::createAcceptorAndExecute< TcpBlockServerDataChunkStorage >(
            controlToken,
            cpp::bind(
                &executeTheFilesPackagerAndTransmitterPipeline,
                om::ObjPtrCopyable< SendRecvContext >::acquireRef( context.get() ),
                UtfArgsParser::host(),
                UtfArgsParser::port()
                ),
            context -> dataBlocksPool(),
            storage,
            std::string( UtfArgsParser::host() ),
            UtfArgsParser::port()
            );
    }
}

/************************************************************************
 * Tests for the pinger implementation
 */

UTF_AUTO_TEST_CASE( Tasks_PingerMatchersTests )
{
    using namespace bl;
    using namespace bl::tasks;

    if( os::onWindows() )
    {
        /*
         * Windows
         */

        const auto lineAllPackets = "Packets: Sent = 1, Received = 1, Lost = 0 (0% loss),";
        const auto lineAverageRTT = "Minimum = 1ms, Maximum = 1ms, Average = 1ms";

        double rtt;

        UTF_REQUIRE( ProcessPingerTaskImpl::matchPacketsArrived( lineAllPackets ) );
        UTF_REQUIRE( ProcessPingerTaskImpl::matchAverageRoundTripTime( lineAverageRTT, &rtt ) );
        UTF_REQUIRE( numbers::floatingPointEqual( rtt, 1.0 ) );
    }
    else if( os::onLinux() )
    {
        {
            /*
             * Ubuntu
             */

            const auto lineAllPackets = "1 packets transmitted, 1 received, 0% packet loss, time 3003ms";
            const auto lineAverageRTT = "rtt min/avg/max/mdev = 2.125/2.347/2.637/0.204 ms";

            double rtt;

            UTF_REQUIRE( ProcessPingerTaskImpl::matchPacketsArrived( lineAllPackets ) );
            UTF_REQUIRE( ProcessPingerTaskImpl::matchAverageRoundTripTime( lineAverageRTT, &rtt ) );
            UTF_REQUIRE( numbers::floatingPointEqual( rtt, 2.347 ) );
        }

        {
            /*
             * RHEL6
             */

            const auto lineAllPackets = "1 packets transmitted, 1 received, 0% packet loss, time 3008ms";
            const auto lineAverageRTT = "rtt min/avg/max/mdev = 5.895/6.385/6.885/0.352 ms";

            double rtt;

            UTF_REQUIRE( ProcessPingerTaskImpl::matchPacketsArrived( lineAllPackets ) );
            UTF_REQUIRE( ProcessPingerTaskImpl::matchAverageRoundTripTime( lineAverageRTT, &rtt ) );
            UTF_REQUIRE( numbers::floatingPointEqual( rtt, 6.385 ) );
        }

        {
            /*
             * RHEL5
             */

            const auto lineAllPackets = "1 packets transmitted, 1 received, 0% packet loss, time 3006ms";
            const auto lineAverageRTT = "rtt min/avg/max/mdev = 5.875/6.933/8.477/0.954 ms";

            double rtt;

            UTF_REQUIRE( ProcessPingerTaskImpl::matchPacketsArrived( lineAllPackets ) );
            UTF_REQUIRE( ProcessPingerTaskImpl::matchAverageRoundTripTime( lineAverageRTT, &rtt ) );
            UTF_REQUIRE( numbers::floatingPointEqual( rtt, 6.933 ) );
        }
    }
    else if( os::onDarwin() )
    {
        {
            /*
             * Darwin 15.6 (OS X El Capitan)
             */

            const auto lineAllPackets = "1 packets transmitted, 1 packets received, 0.0% packet loss";
            const auto lineAverageRTT = "round-trip min/avg/max/stddev = 0.048/0.048/0.048/0.000 ms";

            double rtt;

            UTF_REQUIRE( ProcessPingerTaskImpl::matchPacketsArrived( lineAllPackets ) );
            UTF_REQUIRE( ProcessPingerTaskImpl::matchAverageRoundTripTime( lineAverageRTT, &rtt ) );
            UTF_REQUIRE( numbers::floatingPointEqual( rtt, 0.048 ) );
        }
    }
    else
    {
        BL_THROW(
            NotSupportedException(),
            BL_MSG()
                << "ProcessPingerTaskImpl: current platform is not supported"
            );
    }
}

UTF_AUTO_TEST_CASE( Tasks_PingerManualTests )
{
    using namespace bl;
    using namespace bl::tasks;

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
        {
            PingerTaskFactory pingerFactory;

            const auto pinger = pingerFactory.createInstance(
                cpp::copy( test::UtfArgsParser::host() )
                );

            eq -> push_back( om::qi< Task >( pinger ) );

            eq -> flushAndDiscardReady();

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Destination '"
                    << pinger -> host()
                    << ( pinger -> isReachable() ? "' is reachable" : "' is *not* reachable" )
                    << "; round trip time is '"
                    << pinger -> roundTripTimeMs()
                    << "' ms"
                );
        }
        );
}

namespace
{
    void pingerAutoTests( SAA_in_opt const char* host2Add = nullptr )
    {
        using namespace bl;
        using namespace bl::tasks;

        test::MachineGlobalTestLock lock( PingerTaskFactory::getGlobalLockName() );

        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
            {
                PingerTaskFactory pingerFactory;

                /*
                 * Execute only a limited number of pingers in parallel and ignore failures
                 */

                eq -> setOptions( ExecutionQueue::OptionKeepNone );
                eq -> setThrottleLimit( pingerFactory.maxExecutingTasks() );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Thread pool sizes: "
                        << bl::ThreadPoolDefault::getDefault( bl::ThreadPoolId::GeneralPurpose ) -> size()
                        << "/"
                        << bl::ThreadPoolDefault::getDefault( bl::ThreadPoolId::NonBlocking ) -> size()
                        << ", max executing tasks: "
                        << pingerFactory.maxExecutingTasks()
                    );

                /*
                 * Let's ping some hosts, sort them by ping time by putting the
                 * unreachable at the end and print the results
                 */

                std::vector< om::ObjPtr< PingerTask > > list;

                static const char* hosts[] =
                {
                    "localhost",
                };

                const auto cbAddPinger = [ & ]( SAA_in const char* host )
                {
                    auto pinger = pingerFactory.createInstance( host );

                    eq -> push_back( om::qi< Task >( pinger ) );

                    list.push_back( std::move( pinger ) );
                };

                const std::size_t multiplier = pingerFactory.maxExecutingTasks() == 1 ? 1 : 10;

                for( std::size_t j = 0; j < multiplier; ++j )
                {
                    for( std::size_t i = 0; i < BL_ARRAY_SIZE( hosts ); ++i )
                    {
                        cbAddPinger( hosts[ i ] );
                    }

                    if( host2Add )
                    {
                        cbAddPinger( host2Add );
                    }
                }

                eq -> flush();

                PingerTask::sortPingerTasksByRoundTripTime( list );

                /*
                 * Print them in sorted order
                 */

                for( const auto& pinger : list )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Destination '"
                            << pinger -> host()
                            << ( pinger -> isReachable() ? "' is reachable" : "' is *not* reachable" )
                            << "; round trip time is '"
                            << pinger -> roundTripTimeMs()
                            << "' ms"
                        );
                }

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "************* dump info for non-reachable hosts *************\n\n"
                    );

                for( const auto& pinger : list )
                {
                    if( pinger -> isUnreachable() )
                    {
                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "*********************\n"
                                << "** host: "
                                << pinger -> host()
                            );

                        if( pinger -> isFailed() )
                        {
                            try
                            {
                                cpp::safeRethrowException( pinger -> exception() );
                            }
                            catch( std::exception& e )
                            {
                                BL_LOG_MULTILINE(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "** exception info:\n"
                                        << eh::diagnostic_information( e )
                                    );
                            }
                        }

                        BL_LOG_MULTILINE(
                            Logging::debug(),
                            BL_MSG()
                                << "*********************\n"
                            );
                    }
                }

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "\n************* end dump info for non-reachable hosts *************"
                    );

                /*
                 * Validate that the reachable hosts is at least 2 (out of 5)
                 *
                 * This will eliminate some expected timeouts with the DNS
                 * resolution taking more than 2 seconds (which is our limit
                 * per host)
                 *
                 * We also validate the expected unreachable host to be 0 or 1
                 * depending on if host2Add was provided
                 */

                std::size_t reachable = 0U;
                std::size_t expectedUnreachable = 0U;

                for( const auto& pinger : list )
                {
                    if( pinger -> isUnreachable() )
                    {
                        if( host2Add && pinger -> host() == host2Add )
                        {
                            ++expectedUnreachable;
                        }
                    }
                    else
                    {
                        ++reachable;
                    }
                }

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Number of reachable hosts is "
                        << reachable
                        << "; # of expected unreachable is "
                        << expectedUnreachable
                    );

                /*
                 * We expect at least one host to be reachable
                 */

                UTF_REQUIRE( reachable > 0U );

                if( ! host2Add )
                {
                    UTF_REQUIRE_EQUAL( 0U, expectedUnreachable );
                }
            }
            );
    }

} // __unnamed

UTF_AUTO_TEST_CASE( Tasks_PingerAutoTests )
{
    bl::utils::ExecutionTimer timer( "Tasks_PingerAutoTests" );

    const auto host2Add = test::UtfArgsParser::host();

    pingerAutoTests( host2Add.empty() ? nullptr : host2Add.c_str() );
}

UTF_AUTO_TEST_CASE( Tasks_PingerAutoAddUnreachableTests )
{
    bl::utils::ExecutionTimer timer( "Tasks_PingerAutoAddUnreachableTests" );

    pingerAutoTests( "yahoo.com" );
}

UTF_AUTO_TEST_CASE( Tasks_PingerAutoAddInvalidHostTests )
{
    bl::utils::ExecutionTimer timer( "Tasks_PingerAutoAddInvalidHostTests" );

    /*
     * Apparently invalid.host.com & invalidhost.com do exist (!) :-)
     * so we can't use these; host.invalid seems to be good
     */

    pingerAutoTests( "host.invalid" );
}

/************************************************************************
 * Tests for a parallel 'map' function
 */

UTF_AUTO_TEST_CASE( Tasks_ParallelMap_4 )
{
    const std::size_t size = 4U;

    std::vector< std::size_t > input;

    for( std::size_t i = 0; i < size; ++i )
    {
        input.push_back( i );
    }

    std::function< std::string (SAA_in const std::size_t&) > toString = []( SAA_in const std::size_t& i )
    {
        bl::os::sleep( bl::time::seconds( 1 ) );

        return std::to_string( i );
    };

    const auto start = bl::os::get_system_time();

    const std::vector< std::string > output = bl::tasks::parallelMap(
        input.begin(),
        input.end(),
        toString
        );

    const auto end = bl::os::get_system_time();

    const auto totalTime = end - start;

    UTF_CHECK( totalTime < bl::time::seconds( 4 ) );

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "Total time for parallelMap with 4 tasks, each task takes 1 second: "
            << totalTime.total_milliseconds()
            << "ms\n"
        );

    UTF_REQUIRE_EQUAL( output.size(), size );

    for( std::size_t i = 0; i < size; ++i )
    {
        UTF_CHECK_EQUAL( std::to_string( i ), output[ i ] );
    }
}

UTF_AUTO_TEST_CASE( Tasks_ParallelMap_1024 )
{
    const std::size_t size = 1024U;

    std::vector< std::size_t > input;

    for( std::size_t i = 0; i < size; ++i )
    {
        input.push_back( i );
    }

    std::function< std::string (SAA_in const std::size_t&) > toString = []( SAA_in const std::size_t& i )
    {
        return std::to_string( i );
    };

    const std::vector< std::string > output = bl::tasks::parallelMap(
        input.begin(),
        input.end(),
        toString
        );

    UTF_REQUIRE_EQUAL( output.size(), size );

    for( std::size_t i = 0; i < size; ++i )
    {
        UTF_CHECK_EQUAL( std::to_string( i ), output[ i ] );
    }
}

/************************************************************************
 * Tests for lambda functions capturing external variables in async tasks
 */

UTF_AUTO_TEST_CASE( Tasks_LambdaCapture )
{
    using namespace bl;
    using namespace bl::tasks;

    /*
     * Create an array of non-copyable token pointers
     */

    const int count = 20;

    typedef bl::om::ObjectImpl< bl::om::BoxedValueObject< int > > boxedInt;

    std::vector< bl::om::ObjPtr< boxedInt > > tokens;

    for( auto i = 0; i < count; ++i )
    {
        auto token = boxedInt::createInstance < boxedInt >();
        token -> lvalue() = i + 1;

        tokens.emplace_back( std::move( token ) );
    }

    /*
     * Launch parallel tasks for each token, make sure they execute out of order
     */

    scheduleAndExecuteInParallel(
        [ & ]( SAA_in const bl::om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            int i = 0;
            int j = 0;

            for( const auto& token : tokens )
            {
                ++i;
                ++j;
                const int number = i;

                UTF_REQUIRE_EQUAL( token -> value(), i );
                UTF_REQUIRE_EQUAL( token -> value(), j );
                UTF_REQUIRE_EQUAL( token -> value(), number );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "Creating task "
                        << number
                        << ", token: "
                        << token -> value()
                    );

                eq -> push_back(
                    SimpleTaskImpl::createInstance< Task >(
                        [ &, i, j, number ]()
                        {
                            /*
                             * "i" is captured by reference and its value is going to change
                             * "j" and "number" are captured by value and are reliable
                             */

                            UTF_CHECK_EQUAL( j, number );
                            UTF_CHECK_EQUAL( token -> value(), number );
                            UTF_CHECK_EQUAL( token -> value(), j );

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Starting task "
                                    << number
                                    << " (i="
                                    << i
                                    << ", j="
                                    << j
                                    << "), token: "
                                    << token -> value()
                                );

                            const auto value1 = token -> value();
                            const auto copy1 = bl::om::copy( token );

                            UTF_CHECK_EQUAL( copy1 -> value(), number );

                            /*
                             * Let the seeding task finish before continuing
                             */

                            bl::os::sleep( bl::time::milliseconds( 100 * ( count - number ) ) );

                            const auto value2 = token -> value();
                            const auto copy2 = bl::om::copy( token );

                            UTF_CHECK_EQUAL( value1, value2 );
                            UTF_CHECK_EQUAL( copy2 -> value(), number );
                            UTF_CHECK_EQUAL( copy1 -> value(), copy2 -> value() );

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Stopping task "
                                    << number
                                    << " (i="
                                    << i
                                    << ", j="
                                    << j
                                    << "), token: "
                                    << token -> value()
                                    << ", copy1: "
                                    << copy1 -> value()
                                    << ", copy2: "
                                    << copy2 -> value()
                                );
                        }
                        )
                    );
            }

            i = 888;
            j = 999;
        }
        );
}

UTF_AUTO_TEST_CASE( Tasks_RetryableWrapperTaskTests )
{
    using namespace bl;
    using namespace bl::tasks;

    const std::size_t maxRetryCount = 5U;
    const auto retryTimeout = time::seconds( 1 );

    std::vector< time::time_duration > executedTimestamps;

    const auto task1 = SimpleTaskImpl::createInstance(
        [ & ]() -> void
        {
            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Executing doomed task: "
                    << time::getCurrentLocalTimeISO()
                );

            executedTimestamps.emplace_back( time::durationSinceEpochUtc() );

            BL_THROW(
                bl::UnexpectedException(),
                BL_MSG()
                    << "Consistent error"
                );
        }
        );

    const std::size_t maxFailureCount = maxRetryCount - 2U;
    std::size_t currentFailureCount = 0U;

    const auto task2 = SimpleTaskImpl::createInstance(
        [ & ]() -> void
        {
            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Executing eventually successful task: "
                    << time::getCurrentLocalTimeISO()
                );

            ++currentFailureCount;

            if( currentFailureCount < maxFailureCount )
            {
                BL_THROW(
                    bl::UnexpectedException(),
                    BL_MSG()
                        << "Random glitch"
                    );
            }
        }
        );

    /*
     * This test probably needs to be re-factored at some point
     *
     * For now we skip task3 to avoid the confusion with taskImpl3 below
     */

    std::size_t currentFailureCount4 = 0U;

    const auto task4 = SimpleTaskImpl::createInstance(
        [ & ]() -> void
        {
            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Executing eventually successful task (server error): "
                    << time::getCurrentLocalTimeISO()
                );
        }
        );

    const auto task5 = SimpleTaskImpl::createInstance(
        [ & ]() -> void
        {
            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Executing eventually successful task (server ok): "
                    << time::getCurrentLocalTimeISO()
                );
        }
        );

    const auto task6 = SimpleTaskImpl::createInstance(
        [ & ]() -> void
        {
            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Executing eventually successful task (verify CB + throw): "
                    << time::getCurrentLocalTimeISO()
                );

            BL_THROW(
                bl::UnexpectedException(),
                BL_MSG()
                    << "Consistent error"
                );
        }
        );

    const auto verifyCallback = [ & ]( SAA_in const om::ObjPtr< Task >& task ) -> bool
    {
        if( bl::om::areEqual( task, task4 ) )
        {
            UTF_REQUIRE( task4 -> getState() >= bl::tasks::Task::PendingCompletion );
            UTF_REQUIRE( ! task4 -> isFailed() );

            ++currentFailureCount4;
            return currentFailureCount4 < maxFailureCount ? false : true;
        }
        else if( bl::om::areEqual( task, task5 ) )
        {
            UTF_REQUIRE( task5 -> getState() >= bl::tasks::Task::PendingCompletion );
            UTF_REQUIRE( ! task5 -> isFailed() );
            return true;
        }
        else if( bl::om::areEqual( task, task6 ) )
        {
            UTF_REQUIRE( task6 -> getState() >= bl::tasks::Task::PendingCompletion );
            UTF_REQUIRE( task6 -> isFailed() );
            return false;
        }
        else
        {
            UTF_FAIL( "This should never happen" );
        }

        return false;
    };

    scheduleAndExecuteInParallel(
        [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            const auto taskImpl1 =
                RetryableWrapperTask::createInstance(
                    [ & ]() -> om::ObjPtr< Task >
                    {
                        return om::qi< Task >( task1 );
                    },
                    maxRetryCount,
                    cpp::copy( retryTimeout )
                    );

            eq -> push_back( om::qi< Task >( taskImpl1 ) );

            UTF_CHECK_THROW( eq -> flush(), bl::UnexpectedException );

            bl::om::ObjPtr< SimpleHttpGetTaskImpl > getTask;

            const auto taskImpl2 =
                RetryableWrapperTask::createInstance(
                    [ & ]() -> om::ObjPtr< Task >
                    {
                        getTask = SimpleHttpGetTaskImpl::createInstance(
                            "host.invalid"      /* host */,
                            1001                /* port */,
                            "/invalid_page"     /* urlPath */
                            );

                        return om::qi< Task >( getTask );
                    },
                    maxRetryCount,
                    cpp::copy( retryTimeout )
                    );

            eq -> push_back( om::qi< Task >( taskImpl2 ) );

            UTF_CHECK_THROW( eq -> flush(), SystemException );

            UTF_CHECK( getTask -> getResponse().empty() );

            const auto taskImpl3 =
                RetryableWrapperTask::createInstance(
                    [ & ]() -> om::ObjPtr< Task >
                    {
                        return om::qi< Task >( task2 );
                    },
                    maxRetryCount,
                    cpp::copy( retryTimeout )
                    );

            eq -> push_back( om::qi< Task >( taskImpl3 ) );

            UTF_CHECK_NO_THROW( eq -> flush() );

            const auto taskImpl4 =
                RetryableWrapperTask::createInstance(
                    [ & ]() -> om::ObjPtr< Task >
                    {
                        return om::qi< Task >( task4 );
                    },
                    maxRetryCount,
                    cpp::copy( retryTimeout ),
                    bl::cpp::copy( verifyCallback )
                    );

            eq -> push_back( om::qi< Task >( taskImpl4 ) );

            UTF_CHECK_NO_THROW( eq -> flush() );

            const auto taskImpl5 =
                RetryableWrapperTask::createInstance(
                    [ & ]() -> om::ObjPtr< Task >
                    {
                        return om::qi< Task >( task5 );
                    },
                    maxRetryCount,
                    cpp::copy( retryTimeout ),
                    bl::cpp::copy( verifyCallback )
                    );

            eq -> push_back( om::qi< Task >( taskImpl5 ) );

            UTF_CHECK_NO_THROW( eq -> flush() );

            const auto taskImpl6 =
                RetryableWrapperTask::createInstance(
                    [ & ]() -> om::ObjPtr< Task >
                    {
                        return om::qi< Task >( task6 );
                    },
                    maxRetryCount,
                    cpp::copy( retryTimeout ),
                    bl::cpp::copy( verifyCallback )
                    );

            eq -> push_back( om::qi< Task >( taskImpl6 ) );

            UTF_CHECK_THROW( eq -> flush(), bl::UnexpectedException );
        }
        );

    UTF_CHECK_EQUAL( executedTimestamps.size(), maxRetryCount );
}

UTF_AUTO_TEST_CASE( Tasks_ShutdownContinuationTests )
{
    using namespace bl;
    using namespace bl::tasks;

    /*
     * Define MyTaskClass derived from SimpleTimerTaskT<> to override and
     * implement the shutdown continuation for testing
     */

    class MyTaskClass : public SimpleTimerTaskT<>
    {
    protected:

        typedef SimpleTimerTaskT<> base_type;

        cpp::ScalarTypeIniter< std::size_t >                            m_noOfPosponements;
        cpp::ScalarTypeIniter< bool >                                   m_isThrowInShutdownContinuation;
        cpp::ScalarTypeIniter< bool >                                   m_isThrowSameException;

        MyTaskClass( SAA_in cpp::bool_callback_t&& callback )
            :
            base_type(
                BL_PARAM_FWD( callback ),
                time::milliseconds( 200 )       /* duration */,
                time::milliseconds( 200 )       /* initDelay */
                )
        {
        }

        virtual bool scheduleTaskFinishContinuation( SAA_in_opt const std::exception_ptr& eptrIn = nullptr ) OVERRIDE
        {
            if( m_isThrowSameException )
            {
                BL_ASSERT( eptrIn );

                bl::cpp::safeRethrowException( eptrIn );
            }

            if( m_isThrowInShutdownContinuation )
            {
                BL_THROW(
                    bl::UnexpectedException(),
                    BL_MSG()
                        << "scheduleTaskFinishContinuation() failed"
                    );
            }

            if( m_noOfPosponements )
            {
                base_type::scheduleTimerInternal( time::milliseconds( 200 ) );
                --m_noOfPosponements.lvalue();

                return true;
            }

            return false;
        }

    public:

        void noOfPosponements( SAA_in const std::size_t noOfPosponements ) NOEXCEPT
        {
            m_noOfPosponements = noOfPosponements;
        }

        void isThrowInShutdownContinuation( SAA_in const bool isThrowInShutdownContinuation ) NOEXCEPT
        {
            m_isThrowInShutdownContinuation = isThrowInShutdownContinuation;
        }

        void isThrowSameException( SAA_in const bool isThrowSameException ) NOEXCEPT
        {
            m_isThrowSameException = isThrowSameException;
        }
    };

    typedef om::ObjectImpl< MyTaskClass > MyTaskClassImpl;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            /*
             * Test the case when the task does not schedule any shutdown continuations
             */

            {
                std::size_t noOfTimesCalled = 0;

                const auto taskImpl = MyTaskClassImpl::createInstance(
                    [ &noOfTimesCalled ]() -> bool
                    {
                        ++noOfTimesCalled;
                        return false;
                    }                           /* callback */
                    );

                eq -> push_back( om::qi< Task >( taskImpl ) );

                eq -> flush();

                UTF_REQUIRE_EQUAL( noOfTimesCalled, 1U );
            }

            /*
             * Test the case when the task does schedule shutdown continuations
             */

            const auto testPosponed = [ &eq ]( SAA_in const std::size_t noOfPosponements ) -> void
            {
                std::size_t noOfTimesCalled = 0;

                const auto taskImpl = MyTaskClassImpl::createInstance(
                    [ &noOfTimesCalled ]() -> bool
                    {
                        ++noOfTimesCalled;
                        return false;
                    }                           /* callback */
                    );

                taskImpl -> noOfPosponements( noOfPosponements );

                eq -> push_back( om::qi< Task >( taskImpl ) );

                eq -> flush();

                UTF_REQUIRE_EQUAL( noOfTimesCalled, noOfPosponements + 1 );
            };

            testPosponed( 1 /* noOfPosponements */ );
            testPosponed( 5 /* noOfPosponements */ );

            /*
             * Test the case when the scheduling of shutdown continuation throws
             */

            const auto testFailed = [ &eq ](
                SAA_in              const bool                      taskFailed,
                SAA_in              const bool                      isThrowSameException
                )
                -> void
            {
                const auto taskImpl = MyTaskClassImpl::createInstance(
                    [ & ]() -> bool
                    {
                        if( taskFailed || isThrowSameException )
                        {
                            BL_THROW(
                                bl::ArgumentException(),
                                BL_MSG()
                                    << "callback() failed"
                                );
                        }

                        return false;
                    }                           /* callback */
                    );

                taskImpl -> isThrowInShutdownContinuation( true );

                if( isThrowSameException )
                {
                    taskImpl -> isThrowSameException( true );
                }

                eq -> push_back( om::qi< Task >( taskImpl ) );

                if( isThrowSameException )
                {
                    UTF_REQUIRE_THROW_MESSAGE(
                        eq -> flush(),
                        bl::ArgumentException,
                        "callback() failed"
                        );
                }
                else
                {
                    UTF_REQUIRE_THROW_MESSAGE(
                        eq -> flush(),
                        bl::UnexpectedException,
                        "scheduleTaskFinishContinuation() failed"
                        );
                }

                /*
                 * This call below is necessary to clear up the failed task
                 * (the one that is expected to fail), so the rest of the tests
                 * can continue
                 */

                eq -> forceFlushNoThrow();
            };

            testFailed( false /* taskFailed */, false /* isThrowSameException */ );

            /*
             * Test the case when the scheduling of shutdown continuation throws
             * and the task has failed
             *
             * In this case the original exception is logged as a warning and  discarded
             *
             * The test needs to redirect the logging temporarily and check that the
             * code has logged correctly
             */

            const auto testFailedWithTaskException = [ & ]( SAA_in bool isThrowSameException ) -> void
            {
                bl::cpp::SafeOutputStringStream os;

                {
                    const Logging::line_logger_t ll(
                        cpp::bind(
                            &Logging::defaultLineLoggerWithLock,
                            _1,
                            _2,
                            _3,
                            _4,
                            true /*addNewLine */,
                            cpp::ref( os )
                            )
                        );

                    Logging::LineLoggerPusher pushLogger( ll );

                    Logging::LevelPusher pushLevel( Logging::LL_DEBUG );

                    testFailed( true /* taskFailed */, isThrowSameException );
                }

                std::string line;
                cpp::SafeInputStringStream is( os.str() );

                std::size_t lineNo = 0;

                while( ! is.eof() )
                {
                    std::getline( is, line );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << line
                        );

                    if( 0U == lineNo )
                    {
                        const auto endText =
                            "] Ignoring original exception overridden by "
                            "shutdown continuation exception [callback() failed]";

                        UTF_REQUIRE( str::starts_with( line, "WARNING: [" ) );
                        UTF_REQUIRE( str::ends_with( line, endText ) );
                    }
                    else if( line.empty() )
                    {
                        /*
                         * Only the last line is allowed to be empty
                         */

                        UTF_REQUIRE( is.eof() );
                        break;
                    }
                    else
                    {
                        UTF_REQUIRE( str::starts_with( line, "DEBUG: [" ) );
                    }

                    ++lineNo;
                }
            };

            testFailedWithTaskException( false /* isThrowSameException */ );
            testFailedWithTaskException( true /* isThrowSameException */ );
        }
        );
}

UTF_AUTO_TEST_CASE( Tasks_ExternalCompletionTaskTests )
{
    using namespace bl;
    using namespace bl::tasks;

    const std::size_t maxTasks = 100U;

    os::mutex lock;
    std::vector< std::pair< CompletionCallback, std::exception_ptr > > completionQueue;
    std::map< std::size_t, om::ObjPtr< Task > > tasksMap;
    std::size_t scheduledCount = 0U;
    bool cancelHasStarted = false;

    completionQueue.resize( maxTasks );

    const auto ifScheduleCallback = [ & ](
        SAA_in              const bool                                      scheduled,
        SAA_in              const std::size_t                               pos,
        SAA_in              const CompletionCallback&                       onReady
        )
        -> bool
    {
        BL_MUTEX_GUARD( lock );

        BL_CHK(
            true,
            cancelHasStarted,
            BL_MSG()
                << "Task can't be scheduled because cancellation is in progress"
            );

        const bool coinToss = ( 0 == random::getUniformRandomUnsignedValue< int >( 1 /* maxValue */ ) );

        const auto eptr = coinToss ?
            std::make_exception_ptr(
                BL_EXCEPTION(
                    UnexpectedException(),
                    bl::resolveMessage(
                        BL_MSG()
                            << "External completion exception"
                        )
                    )
                )
                :
                std::exception_ptr();

        completionQueue[ pos ] =
            std::make_pair( scheduled ? onReady : CompletionCallback(), eptr );

        ++scheduledCount;

        if( ! scheduled && eptr )
        {
            cpp::safeRethrowException( eptr );
        }

        return scheduled;
    };

    const auto scheduleCallback = [ &ifScheduleCallback ](
        SAA_in              const std::size_t                               pos,
        SAA_in              const CompletionCallback&                       onReady
        )
        -> void
    {
        BL_ASSERT( onReady );
        BL_VERIFY( ifScheduleCallback( true /* scheduled */, pos, onReady ) );
    };

    scheduleAndExecuteInParallel(
        [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            std::size_t timerRetries = 0U;
            const std::size_t maxTimerRetries = 10U;

            eq -> push_back(
                SimpleTimerTask::createInstance< Task >(
                    [ & ]() -> bool
                    {
                        BL_MUTEX_GUARD( lock );

                        ++timerRetries;

                        if( scheduledCount < maxTasks && timerRetries < maxTimerRetries )
                        {
                            return true;
                        }

                        /*
                         * If we are here that means that either all external completion tasks
                         * are scheduled or that we have exhausted the wait time
                         *
                         * In either case we can now start canceling the tasks, etc
                         */

                        cancelHasStarted = true;

                        for( const auto& pair : completionQueue )
                        {
                            const auto& completionCallback = pair.first;

                            if( completionCallback )
                            {
                                completionCallback( pair.second /* eptr */ );
                            }
                        }

                        return false;
                    },
                    time::seconds( 1 ) /* duration */,
                    time::seconds( 1 ) /* initDelay */
                    )
                );

            std::vector< om::ObjPtr< ExternalCompletionTaskImpl > > alwaysScheduled;
            std::vector< std::pair< om::ObjPtr< ExternalCompletionTaskIfImpl >, bool > > conditionallyScheduled;

            for( std::size_t i = 0U; i < maxTasks; ++i )
            {
                const bool coinToss = ( 0 == random::getUniformRandomUnsignedValue< int >( 1 /* maxValue */ ) );

                om::ObjPtr< Task > task;

                if( coinToss )
                {
                    const bool scheduled = ( 0 == random::getUniformRandomUnsignedValue< int >( 1 /* maxValue */ ) );

                    auto taskImpl = ExternalCompletionTaskIfImpl::createInstance(
                        cpp::bind< bool /* RETURN */ >(
                            ifScheduleCallback,
                            scheduled,
                            i,
                            _1
                            )
                        );

                    task = om::qi< Task >( taskImpl );

                    conditionallyScheduled.push_back( std::make_pair( std::move( taskImpl ), scheduled ) );
                }
                else
                {
                    auto taskImpl = ExternalCompletionTaskImpl::createInstance(
                        cpp::bind< void /* RETURN */ >( scheduleCallback, i, _1 )
                        );

                    task = om::qi< Task >( taskImpl );

                    alwaysScheduled.push_back( std::move( taskImpl ) );
                }

                eq -> push_back( task );

                tasksMap.emplace( i, std::move( task ) );
            }

            eq -> flush(
                false /* discardPending */,
                true /* nothrowIfFailed */,
                true /* discardReady */,
                false /* cancelExecuting */
                );

            {
                BL_MUTEX_GUARD( lock );

                std::size_t executedSync = 0U;
                std::size_t executedAsync = 0U;

                for( std::size_t i = 0U, count = completionQueue.size(); i < count; ++i )
                {
                    const auto& task = tasksMap[ i ];

                    UTF_REQUIRE( task -> getState() == Task::Completed );

                    if( completionQueue[ i ].first )
                    {
                        ++executedAsync;
                    }
                    else
                    {
                        ++executedSync;
                    }

                    if( ! task -> exception() )
                    {
                        UTF_REQUIRE( ! completionQueue[ i ].second );
                        continue;
                    }

                    UTF_REQUIRE( completionQueue[ i ].second );

                    UTF_REQUIRE_THROW_MESSAGE(
                        cpp::safeRethrowException( completionQueue[ i ].second ),
                        UnexpectedException,
                        "External completion exception"
                        );
                }

                UTF_REQUIRE( executedSync > 1 );
                UTF_REQUIRE( executedAsync > 1 );

                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "executedSync="
                        << executedSync
                        << "; executedAsync="
                        << executedAsync
                    );

                for( std::size_t i = 0U, count = alwaysScheduled.size(); i < count; ++i )
                {
                    UTF_REQUIRE_EQUAL( false, alwaysScheduled[ i ] -> completedSynchronously() );
                }

                for( std::size_t i = 0U, count = conditionallyScheduled.size(); i < count; ++i )
                {
                    const auto& pair = conditionallyScheduled[ i ];

                    const auto& taskImpl = pair.first;
                    const bool scheduled = pair.second;

                    UTF_REQUIRE_EQUAL( ! scheduled, taskImpl -> completedSynchronously() );
                }
            }
        }
        );
}

UTF_AUTO_TEST_CASE( Tasks_SimpleTimerTests )
{
    using namespace bl;
    using namespace bl::tasks;

    std::atomic< std::size_t > counter( 0U );

    const auto defaultSuration = time::seconds( 1L );

    const auto updater = [ &counter, defaultSuration ]() -> time::time_duration
    {
        ++counter;

        return defaultSuration;
    };

    {
        counter = 0U;

        SimpleTimer timer(
            updater                         /* callback */,
            cpp::copy( defaultSuration ),
            time::seconds( 5L )             /* initDelay */,
            false                           /* dontStart */
            );

        os::sleep( time::seconds( 2L ) );
        UTF_REQUIRE( 0U == counter );

        os::sleep( time::seconds( 4L ) );
        UTF_REQUIRE( counter > 0 );

        os::sleep( time::seconds( 4L ) );
        UTF_REQUIRE( counter > 3 );
    }

    {
        counter = 0U;

        SimpleTimer timer(
            updater                         /* callback */,
            cpp::copy( defaultSuration ),
            time::seconds( 0L )             /* initDelay */,
            true                            /* dontStart */
            );

        os::sleep( time::seconds( 2L ) );
        UTF_REQUIRE( 0U == counter );

        timer.start();
        os::sleep( time::seconds( 2L ) );
        UTF_REQUIRE( counter > 0 );

        os::sleep( time::seconds( 4L ) );
        UTF_REQUIRE( counter > 3 );

        timer.stop();
        const std::size_t frozenCounter = counter;
        os::sleep( time::seconds( 2L ) );
        UTF_REQUIRE_EQUAL( frozenCounter, counter );
    }

    {
        counter = 0U;

        SimpleTimer timer(
            updater                         /* callback */,
            cpp::copy( defaultSuration ),
            time::seconds( 0L )             /* initDelay */,
            false                           /* dontStart */
            );

        os::sleep( time::seconds( 1L ) );
        UTF_REQUIRE( counter > 0 );

        /*
         * Test that start() and stop() are idempotent and that start()
         * can't be called after stop was called
         */

        timer.start();
        timer.start();

        timer.stop();
        timer.stop();

        UTF_REQUIRE_THROW_MESSAGE(
            timer.start(),
            UnexpectedException,
            "Attempting to start a simple timer object that is already disposed"
            );
    }

    {
        /*
         * Verify that timers are always stopped when they are destructed
         */

        const std::size_t frozenCounter = counter;
        os::sleep( time::seconds( 2L ) );
        UTF_REQUIRE_EQUAL( frozenCounter, counter );
    }

    {
        long longCounter = 0L;

        const auto adjustableDurationUpdater = [ &longCounter ]() -> time::time_duration
        {
            ++longCounter;

            if( longCounter <= 10L )
            {
                return time::milliseconds( longCounter * 100L );
            }

            /*
             * Request to stop the timer
             */

            return time::neg_infin;
        };

        const auto start = time::microsec_clock::universal_time();

        SimpleTimer timer(
            adjustableDurationUpdater       /* callback */,
            cpp::copy( defaultSuration ),
            time::seconds( 0L )             /* initDelay */,
            false                           /* dontStart */
            );

        timer.wait();

        const auto elapsed = time::microsec_clock::universal_time() - start;

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "Time elapsed in milliseconds: "
                << elapsed.total_milliseconds()
            );

        /*
         * The expected elapsed time is expected to be ~ 5500 ms (5.5 seconds)
         *
         * Let's verify that it is within the 3 to 8 seconds range (to account
         * for errors due to machine slowness, etc)
         */

        UTF_REQUIRE( time::seconds( 3L ) < elapsed && elapsed < time::seconds( 8L ) );
    }
}

UTF_AUTO_TEST_CASE( Tasks_EarlyCancelTests )
{
    using namespace bl;
    using namespace bl::tasks;

    scheduleAndExecuteInParallel(
        []( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( ExecutionQueue::OptionKeepAll );

            bool callbackInvoked = false;

            const auto testCallback = [ & ]() -> void
            {
                callbackInvoked = true;
            };

            const auto task = SimpleTaskImpl::createInstance< Task >( cpp::copy( testCallback ) );

            task -> requestCancel();

            eq -> push_back( task );

            eq -> flushNoThrowIfFailed();

            UTF_REQUIRE_EQUAL( eq -> pop(), task );

            UTF_REQUIRE( task -> isFailed() );

            try
            {
                cpp::safeRethrowException( task -> exception() );

                UTF_FAIL( "cpp::safeRethrowException must throw" );
            }
            catch( SystemException& e )
            {
                UTF_REQUIRE_EQUAL( e.code(), asio::error::operation_aborted );
            }

            UTF_REQUIRE_EQUAL( callbackInvoked, false );

            eq -> push_back( SimpleTaskImpl::createInstance< Task >( cpp::copy( testCallback ) ) );

            eq -> flush();

            UTF_REQUIRE_EQUAL( callbackInvoked, true );
        }
        );
}
