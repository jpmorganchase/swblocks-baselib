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

#ifndef __BL_REACTIVE_OBSERVER_H_
#define __BL_REACTIVE_OBSERVER_H_

#include <baselib/reactive/ReactiveIncludes.h>

BL_IID_DECLARE( Observer, "845ab241-8b20-4744-96f3-dbf675ba296b" )

namespace bl
{
    namespace reactive
    {
        /**
         * @brief Observer interface; modeled on the .NET design of IObserver< T >
         *
         * For more information see documentation of the IObserver< T > interface at:
         * http://msdn.microsoft.com/en-us/library/dd783449.aspx
         */
        class Observer : public om::Object
        {
            BL_DECLARE_INTERFACE( Observer )

        public:

            /**
             * @brief see documentation of the IObserver< T > interface at:
             * http://msdn.microsoft.com/en-us/library/dd783449.aspx
             */

            virtual void onCompleted() = 0;

            /**
             * @brief see documentation of the IObserver< T > interface at:
             * http://msdn.microsoft.com/en-us/library/dd783449.aspx
             */

            virtual void onError( SAA_in const std::exception_ptr& eptr ) = 0;

            /**
             * @brief see documentation of the IObserver< T > interface at:
             * http://msdn.microsoft.com/en-us/library/dd783449.aspx
             *
             * TODO: for now cpp::any is based on boost::any, but we should fix it
             * at some point since boost::any has naive implementation based on RTTI
             * which doesn't work across binaries and generally causes code bloat
             */

            virtual bool onNext( SAA_in const cpp::any& value ) = 0;
        };

    } // reactive

} // bl

#endif /* __BL_REACTIVE_OBSERVER_H_ */
