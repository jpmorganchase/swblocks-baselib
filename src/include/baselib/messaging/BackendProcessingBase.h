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

#ifndef __BL_MESSAGING_BACKENDPROCESSINGBASE_H_
#define __BL_MESSAGING_BACKENDPROCESSINGBASE_H_

#include <baselib/messaging/BackendProcessing.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/TcpBaseTasks.h>

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The core messaging backend processing base implementation class
         *
         * It supports QI for the relevant interfaces + some basic static helpers
         */

        template
        <
            typename E = void
        >
        class BackendProcessingBaseT : public BackendProcessing
        {
            BL_CTR_DEFAULT( BackendProcessingBaseT, protected )
            BL_DECLARE_OBJECT_IMPL_ONEIFACE_DISPOSABLE_NO_DESTRUCTOR( BackendProcessingBaseT, BackendProcessing )

        protected:

            om::ObjPtr< om::Proxy >                                                     m_hostServices;
            os::mutex                                                                   m_lock;

            ~BackendProcessingBaseT() NOEXCEPT
            {
                /*
                 * Must be disposed when we get here
                 */

                BL_ASSERT( ! m_hostServices );

                disposeInternal();
            }

            void disposeInternal() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                if( m_hostServices )
                {
                    m_hostServices -> disconnect();
                    m_hostServices.reset();
                }

                BL_NOEXCEPT_END()
            }

            void validateParameters(
                SAA_in                  const OperationId                               operationId,
                SAA_in                  const CommandId                                 commandId,
                SAA_in                  const bl::uuid_t&                               sessionId,
                SAA_in                  const bl::uuid_t&                               chunkId
                )
            {
                BL_CHK(
                    false,
                    BackendProcessing::OperationId::Put == operationId && BackendProcessing::CommandId::None == commandId,
                    BL_MSG()
                        << "Invalid values for operationId or commandId parameters"
                    );

                BL_UNUSED( sessionId );
                BL_UNUSED( chunkId );

                BL_CHK(
                    false,
                    nullptr != m_hostServices,
                    BL_MSG()
                        << "Host services are not configured properly"
                    );
            }

            template
            <
                typename SERVICE,
                typename RETURN
            >
            RETURN invokeHostService( SAA_in const cpp::function< RETURN ( SAA_in SERVICE* service ) >& callback )
            {
                /*
                 * Simply forward the call to the host services if one is connected and it supports
                 * the requested (SERVICE) service / interface
                 *
                 * Note that the callback is invoked while holding the proxy lock (via guard) so
                 * the host services can't be disconnected while making the call
                 */

                BL_MUTEX_GUARD( m_lock );

                BL_CHK(
                    false,
                    nullptr != m_hostServices,
                    BL_MSG()
                        << "Host services are not configured properly"
                    );

                os::mutex_unique_lock guard;

                const auto service = m_hostServices -> tryAcquireRef< SERVICE >( SERVICE::iid(), &guard );

                BL_CHK(
                    false,
                    nullptr != service,
                    BL_MSG()
                        << "Host services do not support the requested service type (iid): "
                        << SERVICE::iid()
                    );

                return callback( service.get() );
            }

        public:

            static auto wrapInServerError(
                SAA_in          const std::exception_ptr&           eptr,
                SAA_in          const std::exception&               exception,
                SAA_in          const std::string&                  messagePrefix,
                SAA_in          const eh::errc::errc_t              defaultError = eh::errc::success
                )
                -> ServerErrorException
            {
                ServerErrorException newException;

                const auto* errorNo = eh::get_error_info< eh::errinfo_errno >( exception );

                if( errorNo )
                {
                    newException << eh::errinfo_errno( *errorNo );
                }
                else if( defaultError != eh::errc::success )
                {
                    newException << eh::errinfo_errno( defaultError );
                }

                const auto* errorCode = eh::get_error_info< eh::errinfo_error_code >( exception );

                if( errorCode )
                {
                    newException << eh::errinfo_error_code( *errorCode );

                    /*
                     * If we are coping the error code then for consistency and logical reasons
                     * we should also copy the other closely related properties --
                     * i.e. eh::errinfo_error_code_message and eh::errinfo_category_name
                     */

                    const auto* errorCodeMessage =
                        eh::get_error_info< eh::errinfo_error_code_message >( exception );

                    if( errorCodeMessage )
                    {
                        newException << eh::errinfo_error_code_message( *errorCodeMessage );
                    }

                    const auto* errorCodeCategoryName =
                        eh::get_error_info< eh::errinfo_category_name >( exception );

                    if( errorCodeCategoryName )
                    {
                        newException << eh::errinfo_category_name( *errorCodeCategoryName );
                    }
                }
                else if( defaultError != eh::errc::success )
                {
                    const auto ecDefault = eh::errc::make_error_code( defaultError );

                    newException << eh::errinfo_error_code( ecDefault );
                    newException << eh::errinfo_error_code_message( ecDefault.message() );
                    newException << eh::errinfo_category_name( ecDefault.category().name() );
                }

                const bool* isExpected = eh::get_error_info< eh::errinfo_is_expected >( exception );

                if(
                    isExpected ||
                    tasks::TcpSocketCommonBase::isExpectedSocketException(
                        true /* isCancelExpected */,
                        errorCode
                        )
                    )
                {
                    newException << eh::errinfo_is_expected( isExpected ? *isExpected : true );
                }

                newException << eh::errinfo_nested_exception_ptr( eptr );

                return BL_EXCEPTION(
                    newException,
                    resolveMessage(
                        BL_MSG()
                            << messagePrefix
                            << " has failed"
                        )
                    );
            }

            template
            <
                typename RETURN
            >
            static RETURN chkToWrapInServerErrorAndThrowT(
                SAA_in          const cpp::function< RETURN () >&   callback,
                SAA_in          const std::string&                  messagePrefix,
                SAA_in          const eh::errc::errc_t              defaultError = eh::errc::success
                )
            {
                try
                {
                    return callback();
                }
                catch( ServerErrorException& )
                {
                    throw;
                }
                catch( std::exception& e )
                {
                    throw wrapInServerError( std::current_exception(), e, messagePrefix, defaultError );
                }
            }

            static void chkToWrapInServerErrorAndThrow(
                SAA_in          const cpp::void_callback_t&         callback,
                SAA_in          const std::string&                  messagePrefix,
                SAA_in          const eh::errc::errc_t              defaultError = eh::errc::success
                )
            {
                chkToWrapInServerErrorAndThrowT< void >( callback, messagePrefix, defaultError );
            }

            static auto chkToRemapToServerError(
                SAA_in          const std::exception_ptr&           eptr,
                SAA_in          const std::string&                  messagePrefix,
                SAA_in          const eh::errc::errc_t              defaultError = eh::errc::success
                )
                -> std::exception_ptr
            {
                try
                {
                    cpp::safeRethrowException( eptr );
                }
                catch( ServerErrorException& )
                {
                    /*
                     * It is already ServerErrorException - no need to re-map
                     */
                }
                catch( std::exception& e )
                {
                    return std::make_exception_ptr(
                        wrapInServerError( eptr, e, messagePrefix, defaultError )
                        );
                }

                return eptr;
            }

            /*
             * BackendProcessing implementation (smart defaults)
             *
             * E.g. if the derived class implementing BackendProcessing interface
             * has nothing to dispose he doesn't need to implement the dispose() method
             */

            virtual void dispose() NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                disposeInternal();

                BL_NOEXCEPT_END()
            }

            virtual bool autoBlockDispatching() const NOEXCEPT OVERRIDE
            {
                return true;
            }

            virtual void setHostServices( SAA_in om::ObjPtr< om::Proxy >&& hostServices ) NOEXCEPT OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( m_lock );

                m_hostServices = BL_PARAM_FWD( hostServices );

                BL_NOEXCEPT_END()
            }
        };

        typedef BackendProcessingBaseT<> BackendProcessingBase;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BACKENDPROCESSINGBASE_H_ */
