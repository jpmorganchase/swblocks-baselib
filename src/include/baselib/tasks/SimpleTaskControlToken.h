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

#ifndef __BL_TASKS_SIMPLETASKCONTROLTOKEN_H_
#define __BL_TASKS_SIMPLETASKCONTROLTOKEN_H_

#include <baselib/tasks/TaskControlToken.h>
#include <baselib/tasks/Task.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class SimpleTaskControlToken - a simple control token
         * implementation which tracks the canceled bit
         */

        template
        <
            typename E = void
        >
        class SimpleTaskControlTokenT :
            public TaskControlTokenRW
        {
            BL_DECLARE_OBJECT_IMPL( SimpleTaskControlTokenT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( TaskControlToken )
                BL_QITBL_ENTRY( TaskControlTokenRW )
            BL_QITBL_END( TaskControlTokenRW )

        protected:

            std::atomic< bool >                                                     m_canceled;
            std::unordered_set< om::ObjPtrCopyable< Task > >                        m_registeredCancelableTasks;
            mutable os::mutex                                                       m_lock;

            SimpleTaskControlTokenT() NOEXCEPT
                :
                m_canceled( false )
            {
            }

        public:

            virtual bool isCanceled() const NOEXCEPT OVERRIDE
            {
                return m_canceled;
            }

            virtual void requestCancel() OVERRIDE
            {
                m_canceled = true;

                BL_MUTEX_GUARD( m_lock );

                for( const auto& task : m_registeredCancelableTasks )
                {
                    task -> requestCancel();
                }

                m_registeredCancelableTasks.clear();
            }

            virtual void registerCancelableTask( SAA_in om::ObjPtrCopyable< Task >&& task ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                m_registeredCancelableTasks.insert( BL_PARAM_FWD( task ) );
            }

            virtual void unregisterCancelableTask( SAA_in om::ObjPtrCopyable< Task >&& task ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                m_registeredCancelableTasks.erase( BL_PARAM_FWD( task ) );
            }
        };

        typedef om::ObjectImpl< SimpleTaskControlTokenT<> > SimpleTaskControlTokenImpl;

    } // tasks

} // bl

#endif /* __BL_TASKS_SIMPLETASKCONTROLTOKEN_H_ */
