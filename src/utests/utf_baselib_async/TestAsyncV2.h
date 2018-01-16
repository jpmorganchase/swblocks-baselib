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

#include <utests/baselib/TestAsyncCommon.h>

/*****************************************************************************************************************
 * Async executor implementation tests (v2)
 */

namespace asyncv2
{
    typedef bl::AsyncDataChunkStorage                                                       AsyncDataChunkStorage;
    typedef bl::AsyncDataChunkStorage::AsyncOperationStateImpl                              AsyncOperationStateImpl;

    /**
     * @brief Test async task
     */

    template
    <
        typename BASE
    >
    class AsyncTestTaskBaseT :
        public utest::AsyncTestTaskSharedBase< BASE, AsyncDataChunkStorage >
    {
    protected:

        typedef AsyncTestTaskBaseT< BASE >                                                  this_type;
        typedef utest::AsyncTestTaskSharedBase< BASE, AsyncDataChunkStorage >               base_type;

        enum : std::size_t
        {
            BLOCK_CAPACITY = 512U,
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

        typedef AsyncDataChunkStorage::OperationId                                          OperationId;
        typedef AsyncDataChunkStorage::CommandId                                            CommandId;

        const bl::uuid_t                                                                    m_sessionId;
        const bl::uuid_t                                                                    m_chunkIdLoad;
        const bl::uuid_t                                                                    m_chunkIdSave;
        const bl::uuid_t                                                                    m_chunkIdRemove;
        const bl::om::ObjPtr< utest::BackendImplTestImpl >&                                 m_backendImpl;

        AsyncTestTaskBaseT(
            SAA_in              const bl::om::ObjPtr< utest::BackendImplTestImpl >&         backendImpl,
            SAA_in              const bl::om::ObjPtr< AsyncDataChunkStorage >&              asyncStorage
            )
            :
            base_type( asyncStorage ),
            m_sessionId( bl::uuids::create() ),
            m_chunkIdLoad( bl::uuids::create() ),
            m_chunkIdSave( bl::uuids::create() ),
            m_chunkIdRemove( bl::uuids::create() ),
            m_backendImpl( backendImpl )
        {
            BL_ASSERT( m_backendImpl );
        }

        void startTask()
        {
            createOperation( OperationId::Get, m_chunkIdLoad );

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    &this_type::onLoad,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            m_started = true;
        }

        auto dataBlock() const NOEXCEPT -> const bl::om::ObjPtr< bl::data::DataBlock >&
        {
            return m_backendImpl -> getData();
        }

        void createOperation(
            SAA_in          const OperationId           operationId,
            SAA_in          const bl::uuid_t&           chunkId
            )
        {
            releaseOperation();

            m_operationState = m_wrapperImpl -> template createOperationState< AsyncOperationStateImpl >(
                operationId,
                m_sessionId,
                chunkId,
                bl::uuids::nil(),       /* sourcePeerId */
                bl::uuids::nil()        /* targetPeerId */
                );

            m_operation = m_wrapperImpl -> asyncExecutor() -> createOperation(
                bl::om::qi< bl::AsyncOperationState >( m_operationState )
                );

            BL_CHK(
                false,
                ! m_operationState -> data(),
                BL_MSG()
                    << "Operation data should be nullptr"
                );
        }

        void validate(
            SAA_in          const bool                  expectData,
            SAA_in          const bool                  expectValidData,
            SAA_in          const bl::uuid_t&           chunkIdExpected
            )
        {
            BL_CHK(
                false,
                ( expectData || expectValidData ) ?
                    nullptr != m_operationState -> data() : nullptr == m_operationState -> data(),
                BL_MSG()
                    << "Operation data cannot be nullptr"
                );

            if( expectValidData )
            {
                /*
                 * This is an allocated and data block; the size should be valid and it
                 * should match dataBlock() -> size()
                 */

                BL_CHK(
                    false,
                    dataBlock() -> size() == m_operationState -> data() -> size(),
                    BL_MSG()
                        << "Operation data size is not valid"
                    );
            }
            else if( expectData )
            {
                /*
                 * This is an allocated data block; the size should be set to zero
                 */

                BL_CHK(
                    false,
                    0U == m_operationState -> data() -> size(),
                    BL_MSG()
                        << "Operation data size is not valid"
                    );
            }

            BL_CHK(
                false,
                chunkIdExpected == m_operationState -> chunkId(),
                BL_MSG()
                    << "Operation data chunk id is invalid"
                );

            BL_CHK(
                false,
                m_sessionId == m_operationState -> sessionId(),
                BL_MSG()
                    << "Operation data session id is invalid"
                );
        }

        void onLoad( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            validate( true /* expectData */, true /* expectValidData */, m_chunkIdLoad /* chunkIdExpected */ );

            /*
             * Corrupt the block before we return it to ensure it won't be reused
             * directly and come as correct
             */

            BL_CHK(
                false,
                m_operationState -> data() -> size() > sizeof( m_sessionId ),
                BL_MSG()
                    << "Operation data cannot be nullptr"
                );

            /*
             * Just corrupt the data block with some random data to ensure that the onAlloc
             * and onSave will copy the correct data - otherwise onSave will fire an assert
             */

            ::memcpy( m_operationState -> data() -> pv(), &m_sessionId, sizeof( m_sessionId ) );

            createOperation( OperationId::Alloc, m_chunkIdSave );

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    &this_type::onAlloc,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            ++m_asyncCalls.lvalue();
            ++g_asyncCalls;

            BL_TASKS_HANDLER_END_NOTREADY()
        }

        void onAlloc( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            validate( true /* expectData */, false /* expectValidData */, m_chunkIdSave /* chunkIdExpected */ );

            const auto& referenceData = dataBlock();

            BL_CHK(
                false,
                m_operationState -> data() -> capacity() >= referenceData -> size(),
                BL_MSG()
                    << "Allocated block does not have enough capacity"
                );

            ::memcpy( m_operationState -> data() -> pv(), referenceData -> pv(), referenceData -> size() );
            m_operationState -> data() -> setSize( referenceData -> size() );

            m_operationState -> operationId( OperationId::Put );

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    &this_type::onSave,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            ++m_asyncCalls.lvalue();
            ++g_asyncCalls;

            BL_TASKS_HANDLER_END_NOTREADY()
        }

        void onSave( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            validate( true /* expectData */, true /* expectValidData */, m_chunkIdSave /* chunkIdExpected */ );

            createOperation( OperationId::Command, m_chunkIdRemove );
            m_operationState -> commandId( CommandId::Remove );

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    &this_type::onRemove,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            ++m_asyncCalls.lvalue();
            ++g_asyncCalls;

            BL_TASKS_HANDLER_END_NOTREADY()
        }

        void onRemove( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            validate( false /* expectData */, false /* expectValidData */, m_chunkIdRemove /* chunkIdExpected */ );

            createOperation( OperationId::Command, bl::uuids::nil() /* chunkId */ );
            m_operationState -> commandId( CommandId::FlushPeerSessions );

            m_wrapperImpl -> asyncExecutor() -> asyncBegin(
                m_operation,
                bl::cpp::bind(
                    &this_type::onFlushPeerSessions,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    _1
                    )
                );

            ++m_asyncCalls.lvalue();
            ++g_asyncCalls;

            BL_TASKS_HANDLER_END_NOTREADY()
        }

        void onFlushPeerSessions( SAA_in const bl::AsyncOperation::Result& result ) NOEXCEPT
        {
            TEST_ASYNC_TASK_HANDLER_BEGIN_CHK_ASYNC_RESULT()

            validate( false /* expectData */, false /* expectValidData */, bl::uuids::nil() /* chunkIdExpected */ );

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
        static void executeTests(
            SAA_in                      const bool                                                  noisyMode,
            SAA_in                      Functor&&                                                   cb
            )
        {
            {
                const auto backendImpl = bl::om::lockDisposable(
                    utest::BackendImplTestImpl::createInstance( BLOCK_CAPACITY )
                    );

                backendImpl -> setNoisyMode( noisyMode );

                {
                    const auto storage = bl::om::qi< bl::data::DataChunkStorage >( backendImpl );

                    const auto asyncStorage =
                        AsyncDataChunkStorage::createInstance(
                            storage /* writeBackend */,
                            storage /* readBackend */,
                            test::UtfArgsParser::threadsCount()
                            );

                    asyncStorage -> impl() -> blockCapacity( BLOCK_CAPACITY );

                    {
                        bl::tasks::scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
                            {
                                cb( backendImpl, asyncStorage, eq );
                            }
                            );
                    }
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
                false /* noisyMode */,
                [ & ]
                (
                    SAA_in              const bl::om::ObjPtr< utest::BackendImplTestImpl >&         backendImpl,
                    SAA_in              const bl::om::ObjPtr< AsyncDataChunkStorage >&              asyncStorage,
                    SAA_in              const bl::om::ObjPtr< bl::tasks::ExecutionQueue >&          eq
                ) -> void
                {
                    asyncStorage -> impl() -> blockCapacity( BLOCK_CAPACITY );

                    try
                    {
                        eq -> setOptions( bl::tasks::ExecutionQueue::OptionKeepAll );

                        const std::size_t maxIterations = ( testCancel ? 10U : 40U ) * 1024U;

                        const auto t1 = bl::time::microsec_clock::universal_time();

                        BL_LOG(
                            bl::Logging::debug(),
                            BL_MSG()
                                << "Executing "
                                << ( 5U * maxIterations )
                                << " async calls on task pool with size of "
                                << maxIterations
                            );

                        UTF_REQUIRE_EQUAL( 0U, g_asyncCalls );

                        try
                        {
                            for( std::size_t i = 0U; i < maxIterations; ++i )
                            {
                                const auto asyncTaskImpl =
                                    IMPL::template createInstance< IMPL >( backendImpl, asyncStorage );

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
                                        << ( 5U * maxIterations )
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
                            UTF_REQUIRE_EQUAL( 5U * maxIterations, g_asyncCalls );
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
                true /* noisyMode */,
                [ & ]
                (
                    SAA_in              const bl::om::ObjPtr< utest::BackendImplTestImpl >&         backendImpl,
                    SAA_in              const bl::om::ObjPtr< AsyncDataChunkStorage >&              asyncStorage,
                    SAA_in              const bl::om::ObjPtr< bl::tasks::ExecutionQueue >&          eq
                ) -> void
                {
                    asyncStorage -> impl() -> blockCapacity( BLOCK_CAPACITY );

                    const auto asyncTaskImpl =
                        IMPL::template createInstance< IMPL >( backendImpl, asyncStorage );

                    const auto asyncTask = bl::om::qi< bl::tasks::Task >( asyncTaskImpl );

                    UTF_REQUIRE_EQUAL( 0U, asyncTaskImpl -> asyncCalls() );
                    UTF_REQUIRE_EQUAL( 0U, g_asyncCalls );

                    eq -> push_back( asyncTask );

                    eq -> waitForSuccess( asyncTask );

                    UTF_REQUIRE_EQUAL( 5U, asyncTaskImpl -> asyncCalls() );
                    UTF_REQUIRE_EQUAL( 5U, g_asyncCalls );

                    g_asyncCalls = 0U;
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

        typedef AsyncTestTaskBaseT< bl::tasks::TaskBase >                                   base_type;

        AsyncTestTaskAsyncOnlyT(
            SAA_in              const bl::om::ObjPtr< utest::BackendImplTestImpl >&         backendImpl,
            SAA_in              const bl::om::ObjPtr< AsyncDataChunkStorage >&              asyncStorage
            )
            :
            base_type( backendImpl, asyncStorage )
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

        typedef AsyncTestTaskBaseT< bl::tasks::SimpleTaskBase >                             base_type;

        AsyncTestTaskAsyncFastStartT(
            SAA_in              const bl::om::ObjPtr< utest::BackendImplTestImpl >&         backendImpl,
            SAA_in              const bl::om::ObjPtr< AsyncDataChunkStorage >&              asyncStorage
            )
            :
            base_type( backendImpl, asyncStorage )
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

} // asyncv2

UTF_AUTO_TEST_CASE( AsyncV2_BasicTests )
{
    using namespace asyncv2;
    AsyncTestTaskAsyncOnlyImpl::executeBasicTests< AsyncTestTaskAsyncOnlyImpl >();
}

UTF_AUTO_TEST_CASE( AsyncV2_SmallPerfTests )
{
    using namespace asyncv2;
    AsyncTestTaskAsyncOnlyImpl::executePerfTests< AsyncTestTaskAsyncOnlyImpl >( false /* testCancel */ );
}

UTF_AUTO_TEST_CASE( AsyncV2_CancelTests )
{
    using namespace asyncv2;
    AsyncTestTaskAsyncFastStartImpl::executePerfTests< AsyncTestTaskAsyncFastStartImpl >( true /* testCancel */ );
}

