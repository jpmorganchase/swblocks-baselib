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

#ifndef __BL_TRANSFER_RECURSIVEDIRECTORYSCANNER_H_
#define __BL_TRANSFER_RECURSIVEDIRECTORYSCANNER_H_

#include <baselib/reactive/FanoutTasksObservable.h>

#include <baselib/tasks/utils/ScanDirectoryTask.h>
#include <baselib/tasks/utils/DirectoryScannerControlToken.h>

#include <baselib/core/BoxedObjects.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class RecursiveDirectoryScanner
         */
        template
        <
            typename E = void
        >
        class RecursiveDirectoryScannerT :
            public reactive::FanoutTasksObservable
        {
            BL_DECLARE_OBJECT_IMPL( RecursiveDirectoryScannerT )

        protected:

            typedef tasks::DirectoryScannerControlToken                                     DirectoryScannerControlToken;

            om::ObjPtr< tasks::ScanDirectoryTaskImpl >                                      m_seed;
            om::ObjPtrCopyable< tasks::Task >                                               m_current;

            RecursiveDirectoryScannerT(
                SAA_in              const fs::path&                                         root,
                SAA_in_opt          const om::ObjPtr< DirectoryScannerControlToken >&       cbControl = nullptr
                )
            {
                const auto boxedRootPath = bo::path::createInstance();
                boxedRootPath -> swapValue( fs::path( root ) );

                m_seed = tasks::ScanDirectoryTaskImpl::createInstance(
                        root,
                        boxedRootPath,
                        cbControl
                        );

                m_name = "success:Directory_Scanner";
            }

            virtual om::ObjPtr< Task > createSeedingTask() OVERRIDE
            {
                return om::qi< tasks::Task >( m_seed );
            }

            virtual bool canAcceptReadyTask() OVERRIDE
            {
                return flushAllPendingTasks();
            }

            virtual bool pushReadyTask( SAA_in const om::ObjPtrCopyable< Task >& task ) OVERRIDE
            {
                BL_ASSERT( nullptr == m_current );
                m_current = task;

                return true;
            }

            virtual bool flushAllPendingTasks() OVERRIDE
            {
                if( isFailedOrFailing() )
                {
                    m_current.reset();

                    return true;
                }

                if( m_current )
                {
                    if( ! notifyOnNext( cpp::any( m_current ) ) )
                    {
                        return false;
                    }

                    m_current.reset();
                }

                return true;
            }

            virtual bool isWaitingExternalInput() NOEXCEPT OVERRIDE
            {
                return false;
            }
        };

        typedef om::ObjectImpl< RecursiveDirectoryScannerT<>, true /* enableSharedPtr */ > RecursiveDirectoryScannerImpl;

    } // transfer

} // bl

#endif /* __BL_TRANSFER_RECURSIVEDIRECTORYSCANNER_H_ */
