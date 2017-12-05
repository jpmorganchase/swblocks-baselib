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

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueueImpl.h>

#include <baselib/core/OS.h>
#include <baselib/core/ThreadPoolImpl.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

/************************************************************************
 * Abstract process priority tests
 *
 * NOTE:
 * on Unix, it is possible to call nice/setpriority and lower a process'
 * priority, but one can't raise it back to what it was at process creation
 * unless its owner has superuser privileges.
 *
 * for this reason, the test below has a side effect on the process (i.e., it
 * lowers its priority) and, hence, we must make it a standalone test.
 *
 */

UTF_AUTO_TEST_CASE( Tasks_BasicTests )
{
    using namespace bl;
    using namespace bl::tasks;

    BL_LOG_MULTILINE( Logging::debug(), BL_MSG() << "\n******************************** Starting test: Tasks_BasicTests ********************************\n" );

    const auto tp = ThreadPoolImpl::createInstance< ThreadPool >( os::AbstractPriority::Background );
    {
        const auto disposeLock = om::lockDisposable( tp );
        {
            bool called = false;

            const auto cb = [ &called ]( SAA_in const std::size_t id, SAA_in const std::size_t timeoutMilliseconds ) -> void
            {
                /*
                 * Sleep between 200 - 400 milliseconds and print message
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

                called = true;
            };

            const auto task = SimpleTaskImpl::createInstance< Task >( cpp::bind< void >( cb, 1, 200 + ( std::rand() % 200 ) ) );

            const auto eq = om::lockDisposable(
                ExecutionQueueImpl::createInstance< ExecutionQueue >(
                    ExecutionQueue::OptionKeepAll
                    )
                );

            eq -> setLocalThreadPool( tp.get() );

            const auto lock = om::lockDisposable( eq );
            {
                UTF_CHECK_EQUAL( Task::Created, task -> getState() );
                eq -> push_back( task, true /* dontSchedule */ );
                os::sleep( time::milliseconds( 50 ) );
                UTF_CHECK_EQUAL( Task::Created, task -> getState() );

                eq -> push_back( task );
                const auto executedTask = eq -> pop( true /* wait */ );

                UTF_CHECK( executedTask );
                UTF_CHECK( om::areEqual( task, executedTask ) );
                UTF_CHECK_EQUAL( Task::Completed, task -> getState() );
                UTF_CHECK( eq -> isEmpty() );

                const auto task1 = eq -> push_back( cpp::bind< void >( cb, 2, 200 + ( std::rand() % 200 ) ) );
                const auto task2 = eq -> push_back( cpp::bind< void >( cb, 3, 200 + ( std::rand() % 200 ) ) );
                const auto task3 = eq -> push_front( cpp::bind< void >( cb, 2000, 200 + ( std::rand() % 200 ) ) );
                const auto task4 = eq -> push_front( cpp::bind< void >( cb, 3000, 200 + ( std::rand() % 200 ) ) );

                eq -> waitForSuccess( task1 );

                const auto top1 = eq -> top( true /* wait */ );
                const auto top2 = eq -> top( false /* wait */ );
                UTF_CHECK_EQUAL( top1, top2 );

                /*
                 * Test re-schedule
                 */

                eq -> push_back( top1 );

                eq -> flush( false /* discardPending */, false /* nothrowIfFailed */, true /* discardReady */ );
                UTF_CHECK( eq -> isEmpty() );
            }
        }
    }
}
