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

#ifndef __BL_MESSAGING_BLOBSERVERFACADE_H_
#define __BL_MESSAGING_BLOBSERVERFACADE_H_

#include <baselib/messaging/DataChunkStorage.h>
#include <baselib/messaging/AsyncDataChunkStorage.h>
#include <baselib/messaging/TcpBlockServerDataChunkStorage.h>

#include <baselib/data/DataBlock.h>

#include <baselib/tasks/TasksUtils.h>
#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/Task.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/SimpleTaskControlToken.h>

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace messaging
    {
        template
        <
            typename STREAM
        >
        class BlobServerFacadeT
        {
            BL_DECLARE_ABSTRACT( BlobServerFacadeT )

        public:

            typedef tasks::TcpBlockServerDataChunkStorageImpl< STREAM >             StorageImpl;
            typedef typename StorageImpl::isauthenticationrequired_callback_t       isauthenticationrequired_callback_t;

            /**
             * @brief This will create start a blob server on top of a sync storage interface
             * (which could be any possible implementation)
             */

            static void startForSyncStorage(
                SAA_in                  const std::size_t                                       threadsCount,
                SAA_in                  const std::size_t                                       maxConcurrentTasks,
                SAA_in                  const om::ObjPtr< data::DataChunkStorage >&             writeSyncStorage,
                SAA_in                  const om::ObjPtr< data::DataChunkStorage >&             readSyncStorage,
                SAA_in                  const std::string&                                      privateKeyPem,
                SAA_in                  const std::string&                                      certificatePem,
                SAA_in                  const unsigned short                                    port,
                SAA_in_opt              const om::ObjPtr< tasks::TaskControlTokenRW >&          controlTokenIn = nullptr,
                SAA_in_opt              const om::ObjPtr< data::datablocks_pool_type >&         dataBlocksPoolIn = nullptr,
                SAA_in_opt              AsyncDataChunkStorage::datablock_callback_t&&           authenticationCallback =
                    AsyncDataChunkStorage::datablock_callback_t(),
                SAA_in_opt              isauthenticationrequired_callback_t&&                   isAuthenticationRequiredCallback =
                    isauthenticationrequired_callback_t()
                )
            {
                using namespace data;
                using namespace tasks;

                const om::ObjPtr< data::datablocks_pool_type > dataBlocksPool =
                    dataBlocksPoolIn ?
                        om::copy( dataBlocksPoolIn ) :
                        datablocks_pool_type::createInstance();

                const om::ObjPtr< tasks::TaskControlTokenRW > controlToken =
                    controlTokenIn ?
                        om::copy( controlTokenIn ) :
                        SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

                tasks::scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                    {
                        typedef AsyncDataChunkStorage AsyncDataChunkStorage;

                        const auto storage = om::lockDisposable(
                            AsyncDataChunkStorage::createInstance(
                                writeSyncStorage,
                                readSyncStorage,
                                threadsCount,
                                om::qi< TaskControlToken >( controlToken ),
                                maxConcurrentTasks,
                                dataBlocksPool,
                                std::move( authenticationCallback )
                                )
                            );

                        {
                            const auto acceptor = StorageImpl::createInstance(
                                controlToken,
                                dataBlocksPool,
                                "0.0.0.0",
                                port,
                                privateKeyPem,
                                certificatePem,
                                storage,
                                uuids::nil(),
                                std::move( isAuthenticationRequiredCallback )
                                );

                            startAcceptor( acceptor, eq );
                        }
                    });
            }
        };

        typedef BlobServerFacadeT< tasks::TcpSocketAsyncBase >    BlobServerFacade;
        typedef BlobServerFacadeT< tasks::TcpSslSocketAsyncBase > SslBlobServerFacade;

    } // messaging

} // bl

#endif /* __BL_MESSAGING_BLOBSERVERFACADE_H_ */
