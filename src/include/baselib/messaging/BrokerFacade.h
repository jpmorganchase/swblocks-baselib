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

#ifndef __BL_MESSAGING_BROKERFACADE_H_
#define __BL_MESSAGING_BROKERFACADE_H_

#include <baselib/messaging/TcpBlockServerMessageDispatcher.h>
#include <baselib/messaging/AsyncMessageDispatcherWrapper.h>
#include <baselib/messaging/BrokerDispatchingBackendProcessing.h>
#include <baselib/messaging/BackendProcessing.h>

#include <baselib/tasks/SimpleTaskControlToken.h>
#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/TasksUtils.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        /**
         * @brief The top level messaging broker class which facilitates the
         * receiving/processing/sending of messages between remote peers
         */

        template
        <
            typename E = void
        >
        class BrokerFacadeT
        {
            BL_DECLARE_STATIC( BrokerFacadeT )

            typedef tasks::TaskControlTokenRW                           TaskControlTokenRW;

            static const std::string                                    g_anyNetworkInterfaceAddress;

        public:

            template
            <
                typename BACKEND = SslBrokerDispatchingBackendProcessingImpl,
                typename ASYNCWRAPPER = AsyncMessageDispatcherWrapper,
                typename ACCEPTOR = tasks::TcpSslBlockServerMessageDispatcher
            >
            static void execute(
                SAA_in          const om::ObjPtr< BackendProcessing >&                  processingBackend,
                SAA_in          const std::string&                                      privateKeyPem,
                SAA_in          const std::string&                                      certificatePem,
                SAA_in          const unsigned short                                    inboundPort,
                SAA_in          const unsigned short                                    outboundPort,
                SAA_in          const std::size_t                                       threadsCount,
                SAA_in          const std::size_t                                       maxConcurrentTasks,
                SAA_in_opt      const cpp::void_callback_t&                             callback = cpp::void_callback_t(),
                SAA_in_opt      om::ObjPtr< TaskControlTokenRW >&&                      controlToken = nullptr,
                SAA_in_opt      const om::ObjPtr< data::datablocks_pool_type >&         dataBlocksPool =
                    data::datablocks_pool_type::createInstance(),
                SAA_in_opt      time::time_duration&&                                   heartbeatInterval =
                    time::neg_infin,
                SAA_in_opt      const time::time_duration&                              startupDelay = time::seconds( 2L )
                )
            {
                if( ! controlToken )
                {
                    controlToken = tasks::SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();
                }

                const auto serverCallback = [ & ]() -> void
                {
                    const auto peerId = uuids::create();

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Broker peerId: "
                            << peerId
                        );

                    typedef typename BACKEND::connection_t                                      connection_t;

                    const auto dispatchingBackend = om::lockDisposable(
                        BACKEND::template createInstance< BackendProcessing >(
                            om::copy( processingBackend ),
                            om::copy( controlToken ),
                            dataBlocksPool,
                            cpp::copy( g_anyNetworkInterfaceAddress ),
                            outboundPort,
                            privateKeyPem,
                            certificatePem,
                            peerId,
                            heartbeatInterval.is_special() ?
                                time::seconds( connection_t::DEFAULT_HEARTBEAT_INTERVAL_IN_SECONDS ) :
                                BL_PARAM_FWD( heartbeatInterval )
                            )
                        );

                    const auto asyncWrapper = om::lockDisposable(
                        ASYNCWRAPPER::template createInstance(
                            om::copy( dispatchingBackend )                                      /* writeBackend */,
                            nullptr                                                             /* readBackend */,
                            threadsCount,
                            om::copyAs< tasks::TaskControlToken >( controlToken.get() ),
                            maxConcurrentTasks,
                            dataBlocksPool
                            )
                        );

                    const auto acceptor = ACCEPTOR::template createInstance(
                        om::copy( controlToken ),
                        dataBlocksPool,
                        cpp::copy( g_anyNetworkInterfaceAddress ),
                        inboundPort,
                        privateKeyPem,
                        certificatePem,
                        asyncWrapper,
                        peerId
                        );

                    controlToken -> registerCancelableTask(
                        om::ObjPtrCopyable< tasks::Task >( om::qi< tasks::Task >( acceptor ) )
                        );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Messaging server is starting..."
                        );

                    tasks::scheduleAndExecuteInParallel(
                        [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                        {
                            tasks::startAcceptor( acceptor, eq );
                        }
                        );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Messaging server was stopped"
                        );
                };

                if( ! callback )
                {
                    serverCallback();

                    return;
                }

                tasks::scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
                    {
                        BL_SCOPE_EXIT(
                            {
                                BL_NOEXCEPT_BEGIN()

                                /*
                                 * After the client is done we should request the server to also shutdown
                                 */

                                controlToken -> requestCancel();

                                BL_NOEXCEPT_END()
                            }
                            );

                        eq -> push_back( serverCallback );

                        /*
                         * Sleep for startupDelay before and after we execute the callback
                         *
                         * The sleep before the callback is to give a chance to the acceptor
                         * / backend to start and be fully ready before we attempt to use it
                         *
                         * The sleep after the callback is to give a chance to all
                         * outstanding tasks to be flushed before we attempt to shutdown the
                         * server / acceptor
                         */

                        os::sleep( startupDelay );

                        callback();

                        os::sleep( startupDelay );

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Messaging client callback has terminated"
                            );
                    }
                    );
            }
        };

        BL_DEFINE_STATIC_CONST_STRING( BrokerFacadeT, g_anyNetworkInterfaceAddress ) = "0.0.0.0";

        typedef BrokerFacadeT<> BrokerFacade;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BROKERFACADE_H_ */
