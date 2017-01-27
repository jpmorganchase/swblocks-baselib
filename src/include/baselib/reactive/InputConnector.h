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

#ifndef __BL_REACTIVE_INPUTCONNECTOR_H_
#define __BL_REACTIVE_INPUTCONNECTOR_H_

#include <baselib/reactive/ObserverBase.h>
#include <baselib/reactive/Observer.h>

#include <baselib/core/ErrorDispatcher.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace reactive
    {
        typedef cpp::function< bool ( SAA_in const cpp::any& value ) > input_connector_callback_t;

        /**
         * @brief class InputConnector - a simple connector implementation
         * which allows us to connect processing units
         */

        template
        <
            typename E = void
        >
        class InputConnectorT :
            public ObserverBase
        {
            BL_CTR_DEFAULT( InputConnectorT, protected )
            BL_DECLARE_OBJECT_IMPL( InputConnectorT )

        protected:

            const om::ObjPtr< ErrorDispatcher >                                     m_errorDispatcher;
            const input_connector_callback_t                                        m_inputCB;
            const cpp::void_callback_t                                              m_completedCB;

            InputConnectorT(
                SAA_in              input_connector_callback_t&&                    inputCB,
                SAA_in_opt          const om::ObjPtr< ErrorDispatcher >&            errorDispatcher,
                SAA_in              cpp::void_callback_t&&                          completedCB = cpp::void_callback_t()
                )
                :
                m_errorDispatcher( om::copy( errorDispatcher ) ),
                m_inputCB( inputCB ),
                m_completedCB( completedCB )
            {
                BL_ASSERT( m_inputCB );
            }

            static std::string getNameString( SAA_in const std::string& name )
            {
                if( name.empty() )
                {
                    return "";
                }

                return resolveMessage(
                    BL_MSG()
                        << "with name '"
                        << name
                        << "' "
                    );
            }

            void chkTargetActive() const
            {
                if( m_errorDispatcher )
                {
                    const auto task = om::tryQI< tasks::Task >( m_errorDispatcher );

                    if( task && task -> getState() != tasks::Task::Running )
                    {
                        /*
                         * If the target is a task object and it has completed
                         * then we have to force disconnect and unregister from
                         * the observable by throwing an exception
                         */

                        BL_THROW(
                            ObjectDisconnectedException(),
                            BL_MSG()
                                << "Observer object "
                                << getNameString( task -> name() )
                                << "is a task that is not running"
                            );
                    }
                }
            }

            virtual void onCompleted() OVERRIDE
            {
                std::exception_ptr eptr;

                try
                {
                    if( m_completedCB )
                    {
                        m_completedCB();
                        return;
                    }

                    ObserverBase::onCompleted();
                }
                catch( std::exception& )
                {
                    eptr = std::current_exception();
                }

                if( eptr )
                {
                    if( m_errorDispatcher )
                    {
                        m_errorDispatcher -> dispatchException( eptr );
                    }

                    cpp::safeRethrowException( eptr );
                }
            }

            virtual bool onNext( SAA_in const cpp::any& value ) OVERRIDE
            {
                bool handled = false;
                std::exception_ptr eptr;

                try
                {
                    handled = m_inputCB( value );

                    if( ! handled )
                    {
                        chkTargetActive();
                    }
                }
                catch( ObjectDisconnectedException& )
                {
                    /*
                     * The disconnected exceptions are not meant to be dispatched
                     * into the target
                     */

                    throw;
                }
                catch( std::exception& )
                {
                    eptr = std::current_exception();
                }

                if( eptr )
                {
                    if( m_errorDispatcher )
                    {
                        m_errorDispatcher -> dispatchException( eptr );
                    }

                    cpp::safeRethrowException( eptr );
                }

                return handled;
            }
        };

        typedef om::ObjectImpl< InputConnectorT<> > InputConnectorImpl;

        /*
         * A utility helper to create an input connector from a callback
         */

        inline om::ObjPtr< Observer > createInputConnector(
            SAA_in              input_connector_callback_t&&                    inputCB,
            SAA_in_opt          const om::ObjPtr< ErrorDispatcher >&            errorDispatcher,
            SAA_in              cpp::void_callback_t&&                          completedCB = cpp::void_callback_t()
            )
        {
            return InputConnectorImpl::createInstance< Observer >(
                std::forward< input_connector_callback_t >( inputCB ),
                errorDispatcher,
                std::forward< cpp::void_callback_t >( completedCB )
                );
        }

    } // reactive

} // bl

#endif /* __BL_REACTIVE_INPUTCONNECTOR_H_ */
