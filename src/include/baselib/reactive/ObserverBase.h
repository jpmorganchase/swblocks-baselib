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

#ifndef __BL_REACTIVE_OBSERVERBASE_H_
#define __BL_REACTIVE_OBSERVERBASE_H_

#include <baselib/reactive/Observer.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace reactive
    {
        /**
         * @brief class ObserverBase
         */
        template
        <
            typename E = void
        >
        class ObserverBaseT : public Observer
        {
            BL_CTR_DEFAULT( ObserverBaseT, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE( ObserverBaseT, Observer )

        public:

            virtual void onCompleted() OVERRIDE
            {
                BL_LOG(
                    Logging::debug(),
                    BL_MSG()
                        << "ObserverBase::onCompleted()"
                    );
            }

            virtual void onError( SAA_in const std::exception_ptr& eptr ) OVERRIDE
            {
                utils::tryCatchLog(
                    "ObserverBase:onError() was called with the following exception",
                    [ & ]() -> void
                    {
                        cpp::safeRethrowException( eptr );
                    },
                    cpp::void_callback_t(),
                    utils::LogFlags::DEBUG_ONLY
                    );
            }
        };

        typedef ObserverBaseT<> ObserverBase;

    } // reactive

} // bl

#endif /* __BL_REACTIVE_OBSERVERBASE_H_ */
