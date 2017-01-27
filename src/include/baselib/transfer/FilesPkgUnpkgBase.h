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

#ifndef __BL_TRANSFER_FILESPKGUNPKGBASE_H_
#define __BL_TRANSFER_FILESPKGUNPKGBASE_H_

#include <baselib/transfer/FilesPkgUnpkgIncludes.h>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class FilesPkgUnpkgBase - the base class for the packager and unpackager
         * processing units
         */

        template
        <
            typename STREAM
        >
        class FilesPkgUnpkgBase :
            public DataTransferUnitBase< STREAM >
        {
        public:

            typedef DataTransferUnitBase< STREAM >                                          base_type;

        protected:

            using base_type::m_eqWorkerTasks;

        private:

            BL_DECLARE_OBJECT_IMPL( FilesPkgUnpkgBase )

        protected:

            enum
            {
                /*
                 * The maximum file tasks in ready or executing mode; this will
                 * limit the # of open files we have at any point in time
                 */

                MAX_OPEN_FILES_PENDING = 256,
            };

            enum ProcessTaskResult
            {
                /**
                 * @brief Indicates the top task is ready, but the data could not be pushed because
                 * one of the subscribers returned false (so we must wait)
                 */

                ReturnFalse,

                /**
                 * @brief Indicates the top task is ready and available to be picked up for
                 * rescheduling a new file on it
                 */

                ReturnTrue,

                /**
                 * @brief Indicates the top ready task was processed, but it was re-scheduled
                 * and it is not available; we must continue searching...
                 */

                TaskRescheduled,
            };

            cpp::ScalarTypeIniter< std::uint64_t >                                          m_fileSizeTotal;
            cpp::ScalarTypeIniter< std::uint64_t >                                          m_dataSizeTotal;
            cpp::ScalarTypeIniter< std::uint64_t >                                          m_filesTotal;
            cpp::ScalarTypeIniter< bool >                                                   m_statsLogged;

            FilesPkgUnpkgBase(
                SAA_in              const om::ObjPtr< SendRecvContextImpl< STREAM > >&      context,
                SAA_in              std::string&&                                           taskName,
                SAA_in              const std::size_t                                       tasksPoolSize = base_type::DEFAULT_POOL_SIZE
                )
                :
                base_type(
                    context,
                    BL_PARAM_FWD( taskName ),

                    /*
                     * The tasksPoolSize should be at least the size of the general purpose
                     * thread pool or bigger (if bigger such is requested via tasksPoolSize)
                     */

                    std::max< std::size_t >(
                        ThreadPoolDefault::getDefault( ThreadPoolId::GeneralPurpose ) -> size(),
                        tasksPoolSize
                        )
                    )
            {
            }

            virtual auto processTopReadyTask( SAA_in const om::ObjPtr< tasks::Task >& topReady ) -> ProcessTaskResult = 0;

            virtual std::string getStats()
            {
                return "";
            }

            bool waitForReadyWorkerTask( SAA_in const bool force, SAA_in const bool unwinding )
            {
                for( ;; )
                {
                    const auto topReady = m_eqWorkerTasks -> top( false /* wait */ );

                    if( ! topReady )
                    {
                        return false;
                    }

                    if( force )
                    {
                        /*
                         * We're in the process of forcing a shutdown (due to an error
                         * or some other reason). Just keep popping tasks until we're
                         * done.
                         */

                        BL_VERIFY( m_eqWorkerTasks -> pop( false /* wait */ ) );

                        continue;
                    }
                    else
                    {
                        /*
                         * Check to throw if the task has failed
                         */

                         if( false == base_type::isFailedOrFailing() && topReady -> isFailed() )
                         {
                            cpp::safeRethrowException( topReady -> exception() );
                         }
                    }

                    const auto result = processTopReadyTask( topReady );

                    if( TaskRescheduled == result )
                    {
                        /*
                         * The top ready task was processed, but it was re-scheduled
                         * and it is not available; we must continue searching...
                         */

                        continue;
                    }
                    else if( ReturnFalse == result )
                    {
                        /*
                         * The top task is ready, but the data could not be pushed because
                         * one of the subscribers returned false (so we must wait)
                         */

                        return false;
                    }

                    BL_ASSERT( ReturnTrue == result );

                    /*
                     * If we're here that means result is ReturnTrue which indicates
                     * the top task is ready and available to be picked up for rescheduling
                     * a new file on it
                     */

                    if( unwinding )
                    {
                        /*
                         * We're unwinding the queue; just keep popping until
                         * all are done.
                         */

                        BL_VERIFY( m_eqWorkerTasks -> pop( false /* wait */ ) );

                        continue;
                    }

                    /*
                     * The top packager task is done pushing all chunks and
                     * now ready to accept a new file for chunking
                     */

                    break;
                }

                return true;
            }

            virtual bool canAcceptReadyTask() OVERRIDE
            {
                /*
                 * We can accept new tasks only when we have idle packager tasks
                 * or we have not started all packaging tasks yet; note that
                 * if the derived class chooses to start all the packaging tasks
                 * in advance (e.g. in createSeedingTask) this check is still valid
                 */

                if( m_eqWorkerTasks -> size() < base_type::m_tasksPoolSize )
                {
                    return true;
                }

                return waitForReadyWorkerTask( false /* force */, false /* unwinding */ );
            }

            virtual bool flushAllPendingTasks() OVERRIDE
            {
                /*
                 * Just flush out all packaging tasks
                 */

                BL_VERIFY( ! waitForReadyWorkerTask( base_type::isFailedOrFailing() /* force */, true /* unwinding */ ) );

                return m_eqWorkerTasks -> isEmpty();
            }

            virtual std::size_t maxReadyOrExecuting() const NOEXCEPT OVERRIDE
            {
                return MAX_OPEN_FILES_PENDING;
            }

            virtual time::time_duration chk2LoopUntilShutdownFinished() NOEXCEPT OVERRIDE
            {
                const auto duration = base_type::chk2LoopUntilShutdownFinished();

                if( duration.is_special() )
                {
                    logStats();
                }

                return duration;
            }

        public:

            void logStats() NOEXCEPT
            {
                if( m_statsLogged )
                {
                    return;
                }

                BL_LOG_MULTILINE(
                    Logging::debug(),
                    BL_MSG()
                        << "\nFilesPkgUnpkgBase::logStats():\n"
                        << "\n    taskName: "
                        << base_type::m_name
                        << "\n    filesTotal: "
                        << m_filesTotal
                        << "\n    fileSizeTotal: "
                        << m_fileSizeTotal
                        << "\n    dataSizeTotal: "
                        << m_dataSizeTotal
                        << getStats()
                        << "\n"
                    );

                m_statsLogged = true;
            }
        };

    } // transfer

} // bl

#endif /* __BL_TRANSFER_FILESPKGUNPKGBASE_H_ */
