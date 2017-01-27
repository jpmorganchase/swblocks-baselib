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

#ifndef __BL_REACTIVE_OBSERVABLE_H_
#define __BL_REACTIVE_OBSERVABLE_H_

#include <baselib/reactive/Observer.h>
#include <baselib/reactive/ReactiveIncludes.h>

BL_IID_DECLARE( Observable, "26fa3b29-6f5a-40c6-bc08-a7892655335b" )

namespace bl
{
    namespace reactive
    {
        /**
         * @brief Observable interface; modeled on the .NET design of IObservable< T >
         *
         * For more information see documentation of the IObservable< T > interface at:
         * http://msdn.microsoft.com/en-us/library/dd783449.aspx
         */
        class Observable : public om::Object
        {
            BL_DECLARE_INTERFACE( Observable )

        public:

            /**
             * @brief see documentation of the IObservable< T > interface at:
             * http://msdn.microsoft.com/en-us/library/dd990377.aspx
             */

            virtual om::ObjPtr< om::Disposable > subscribe( SAA_in const om::ObjPtr< Observer >& observer ) = 0;
        };

    } // reactive

} // bl

#endif /* __BL_REACTIVE_OBSERVABLE_H_ */
