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

#ifndef __BL_REACTIVE_PROCESSINGUNIT_H_
#define __BL_REACTIVE_PROCESSINGUNIT_H_

#include <baselib/reactive/ObservableBase.h>
#include <baselib/reactive/InputConnector.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <type_traits>

namespace bl
{
    namespace reactive
    {
        /**
         * @brief class ProcessingUnit< BASE, II > - a reactive processing unit
         */

        template
        <
            typename BASE,                  /* base class */
            typename II                     /* identity interface */
        >
        class ProcessingUnit :
            public BASE
        {
        public:

            typedef ProcessingUnit< BASE, II >                      this_type;
            typedef BASE                                            base_type;

            enum
            {
                /*
                 * Peripheral units are processing unit which have no outputs
                 */

                isPeripheralUnit = ! std::is_convertible< BASE*, Observable* >::value
            };

        private:

            BL_DECLARE_OBJECT_IMPL( ProcessingUnit )

        protected:

            BL_VARIADIC_CTOR( ProcessingUnit, base_type, protected )

            template
            <
                typename T,
                typename MEMFN_INPUTCB_T,
                typename MEMFN_COMPLETEDCB_T
            >
            om::ObjPtr< Observer > bindInputConnectorImpl(
                SAA_in              MEMFN_INPUTCB_T&&                               inputCB,
                SAA_in_opt          MEMFN_COMPLETEDCB_T&&                           completedCB
                )
            {
                static_assert(
                    std::is_convertible< T*, this_type* >::value || std::is_convertible< this_type*, T* >::value,
                    "memfn must be a member of this class - either derived of this_type or the BASE"
                    );

                typedef om::ObjPtrCopyable< T, II > refcounted_copyable_t;

                cpp::void_callback_t ccb = cpp::void_callback_t();

                if( completedCB )
                {
                    ccb = cpp::bind(
                        std::forward< MEMFN_COMPLETEDCB_T >( completedCB ),
                        refcounted_copyable_t::acquireRef( static_cast< T* >( this ) )
                        );
                }

                return createInputConnector(
                    cpp::bind(
                        std::forward< MEMFN_INPUTCB_T >( inputCB ),
                        refcounted_copyable_t::acquireRef( static_cast< T* >( this ) ),
                        _1
                        ),
                    om::tryQI< ErrorDispatcher >( static_cast< II* >( this ) ),
                    std::move( ccb )
                    );
            }

        public:

            /*
             * Binds a member function and returns an input connector to it
             */

            template
            <
                typename T
            >
            om::ObjPtr< Observer > bindInputConnector(
                SAA_in              bool ( T::* inputCB )( const cpp::any& )
                )
            {
                typedef void ( T::* callback_t )() const;
                return bindInputConnectorImpl< T >( inputCB, ( callback_t )( nullptr ) );
            }

            template
            <
                typename T
            >
            om::ObjPtr< Observer > bindInputConnector(
                SAA_in              bool ( T::* inputCB )( const cpp::any& ),
                SAA_in_opt          void ( T::* completedCB )()
                )
            {
                return bindInputConnectorImpl< T >( inputCB, completedCB );
            }

            template
            <
                typename T
            >
            om::ObjPtr< Observer > bindInputConnector(
                SAA_in              bool ( T::* inputCB )( const cpp::any& ),
                SAA_in_opt          void ( T::* completedCB )() const
                )
            {
                return bindInputConnectorImpl< T >( inputCB, completedCB );
            }

            template
            <
                typename T
            >
            om::ObjPtr< Observer > bindInputConnector(
                SAA_in              bool ( T::* inputCB )( const cpp::any& ) const
                )
            {
                typedef void ( T::* callback_t )() const;
                return bindInputConnectorImpl< T >( inputCB, ( callback_t )( nullptr ) );
            }

            template
            <
                typename T
            >
            om::ObjPtr< Observer > bindInputConnector(
                SAA_in              bool ( T::* inputCB )( const cpp::any& ) const,
                SAA_in_opt          void ( T::* completedCB )()
                )
            {
                return bindInputConnectorImpl< T >( inputCB, completedCB );
            }

            template
            <
                typename T
            >
            om::ObjPtr< Observer > bindInputConnector(
                SAA_in              bool ( T::* inputCB )( const cpp::any& ) const,
                SAA_in_opt          void ( T::* completedCB )() const
                )
            {
                return bindInputConnectorImpl< T >( inputCB, completedCB );
            }
        };

    } // reactive

} // bl

#endif /* __BL_REACTIVE_PROCESSINGUNIT_H_ */
