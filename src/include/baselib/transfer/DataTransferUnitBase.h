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

#ifndef __BL_TRANSFER_DATATRANSFERUNITBASE_H_
#define __BL_TRANSFER_DATATRANSFERUNITBASE_H_

#include <baselib/reactive/FixedWorkerPoolUnitBase.h>
#include <baselib/reactive/ObservableBase.h>
#include <baselib/transfer/SendRecvContext.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/Pool.h>
#include <baselib/core/BaseIncludes.h>

#include <cstdint>
#include <cstdio>

namespace bl
{
    namespace transfer
    {
        /**
         * @brief class DataTransferUnitBase
         *
         * A base class to for data transfer processing units which utilize
         * a shared data transfer context to facilitate connection pooling
         */

        template
        <
            typename STREAM
        >
        class DataTransferUnitBase :
            public reactive::FixedWorkerPoolUnitBase< STREAM >
        {
        public:

            typedef reactive::FixedWorkerPoolUnitBase< STREAM >                             base_type;
            typedef SendRecvContextImpl< STREAM >                                           context_t;

        private:

            BL_DECLARE_OBJECT_IMPL( DataTransferUnitBase )

        protected:

            const om::ObjPtr< context_t >                                                   m_context;

            DataTransferUnitBase(
                SAA_in              const om::ObjPtr< context_t >&                          context,
                SAA_in              std::string&&                                           taskName,
                SAA_in              const std::size_t                                       tasksPoolSize =
                    base_type::DEFAULT_POOL_SIZE
                )
                :
                base_type( BL_PARAM_FWD( taskName ), tasksPoolSize ),
                m_context( om::copy( context ) )
            {
                /*
                 * A context that allows to pool and reuse shared resources must be provided -
                 * see the note at the definition of SendRecvContext
                 */

                BL_ASSERT( m_context );
            }
        };

    } // transfer

} // bl

#endif /* __BL_TRANSFER_DATATRANSFERUNITBASE_H_ */
