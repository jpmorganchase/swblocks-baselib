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

#ifndef __UTESTS_BASELIB_TESTBLOBTRANSFERFILESYSTEMUTILS_H_
#define __UTESTS_BASELIB_TESTBLOBTRANSFERFILESYSTEMUTILS_H_

#include <baselib/messaging/DataChunkStorageFilesystem.h>

#include <utests/baselib/TestBlobTransferUtils.h>

namespace utest
{
    /**
     * @brief class TestBlobTransferFilesystemUtils< STORAGEIMPL > - shared test code for the blob transfer
     * code testing with filesystem backend
     */

    template
    <
        typename STORAGEIMPL
    >
    class TestBlobTransferFilesystemUtils : public TestBlobTransferUtils
    {
        BL_DECLARE_ABSTRACT( TestBlobTransferFilesystemUtils )

    protected:

        static void wrapFilesystemDataStorage(
            SAA_in                  const bl::data::DataChunkStorage::data_storage_callback_t&      callback
            )
        {
            const auto syncStorage = bl::om::lockDisposable(
                STORAGEIMPL::template createInstance< bl::data::DataChunkStorage >()
                );

             callback( syncStorage );
        }

        static void executeFilesystemTransferTests(
            SAA_in                  const bl::cpp::void_callback_t&                                 cbTransferTest,
            SAA_in                  const bl::om::ObjPtrCopyable< bl::tasks::TaskControlTokenRW >&  controlToken,
            SAA_in                  const bl::om::ObjPtrCopyable< bl::transfer::SendRecvContext >&  context,
            SAA_in                  const unsigned short                                            blobServerPort,
            SAA_in_opt              const std::size_t                                               threadsCount = 0U,
            SAA_in_opt              const std::size_t                                               maxConcurrentTasks = 0U
            )
        {
            wrapFilesystemDataStorage(
                bl::cpp::bind(
                    &base_type::executeTransferTests,
                    cbTransferTest,
                    _1,
                    controlToken,
                    context,
                    blobServerPort,
                    threadsCount,
                    maxConcurrentTasks
                    )
                );
        }

    public:

        typedef TestBlobTransferUtils                                                               base_type;
        typedef base_type::CancelType                                                               CancelType;

        static void filesPackagerTestsWrap(
            SAA_in                  const unsigned short                                                blobServerPort,
            SAA_in                  const bl::om::ObjPtrCopyable< FilesystemMetadataStore >&            metadataStore,
            SAA_in                  const CancelType                                                    cancelType
            )
        {
            filesPackagerTestsWrapInternal(
                bl::cpp::bind(
                    &executeFilesystemTransferTests,
                    _1      /* cbTransferTest */,
                    _2      /* controlToken */,
                    _3      /* context */,
                    _4      /* blobServerPort */,
                    0U      /* threadsCount */,
                    0U      /* maxConcurrentTasks */
                    ),
                blobServerPort,
                metadataStore,
                cancelType
                );
        }

        static void executeTransferTestsWrap( SAA_in const bl::cpp::void_callback_t& cbTest )
        {
            cbTest();
        }

        static void startBlobServer()
        {
            using namespace bl;
            using namespace bl::data;
            using namespace bl::tasks;
            using namespace bl::transfer;
            using namespace bl::messaging;
            using namespace utest;
            using namespace test;

            if( ! UtfArgsParser::isServer() )
            {
                return;
            }

            cpp::SafeUniquePtr< test::MachineGlobalTestLock > lock;

            if( UtfArgsParser::port() == UtfArgsParser::PORT_DEFAULT )
            {
                /*
                 * We only acquire the global lock if we use the default
                 * port (UtfArgsParser::PORT_DEFAULT); otherwise this is
                 * a private instance and there is no need for locking
                 */

                lock.reset( new test::MachineGlobalTestLock );
            }

            wrapFilesystemDataStorage(
                [ & ] ( SAA_in const om::ObjPtr< DataChunkStorage >& syncStorage ) -> void
                {
                    BlobServerFacade::startForSyncStorage(
                        UtfArgsParser::threadsCount(),
                        0U /* maxConcurrentTasks */,
                        syncStorage /* writeBackend */,
                        syncStorage /* readBackend */,
                        str::empty() /* privateKeyPem */,
                        str::empty() /* certificatePem */,
                        UtfArgsParser::port()
                        );
                }
                );
        }

        static void filesPackagerTestsWithProxyWrap(
            SAA_in                  const unsigned short                                                blobServerPort,
            SAA_in                  const bl::om::ObjPtrCopyable< FilesystemMetadataStore >&            metadataStore
            )
        {
            using namespace bl;
            using namespace bl::data;

            base_type::filesPackagerTestsWithProxyWrapInternal(
                [](
                    SAA_in          const DataChunkStorage::data_storage_callback_t&                cbProxyTransferTests,
                    SAA_in          const bl::om::ObjPtrCopyable< bl::tasks::TaskControlTokenRW >&  controlToken,
                    SAA_in          const bl::om::ObjPtrCopyable< bl::transfer::SendRecvContext >&  context,
                    SAA_in          const unsigned short                                            blobServerPrivatePort
                    )
                {
                    const auto cbLocalCacheTest = [ & ]() -> void
                    {
                        wrapFilesystemDataStorage(
                            [ & ] ( SAA_in const om::ObjPtr< DataChunkStorage >& syncCacheStorage ) -> void
                            {
                                cbProxyTransferTests( syncCacheStorage );
                            }
                            );
                    };

                    executeFilesystemTransferTests(
                        cbLocalCacheTest,
                        controlToken,
                        context,
                        blobServerPrivatePort
                        );
                },
                blobServerPort,
                metadataStore
                );
        }

        static void proxyDataChunkBackendImplTests()
        {
            using namespace bl;
            using namespace bl::data;

            base_type::proxyDataChunkBackendImplTestsInternal(
                [](
                    SAA_in          const DataChunkStorage::data_storage_callback_t&                cbProxyTransferTests,
                    SAA_in          const bl::om::ObjPtrCopyable< bl::tasks::TaskControlTokenRW >&  controlToken,
                    SAA_in          const bl::om::ObjPtrCopyable< bl::transfer::SendRecvContext >&  context,
                    SAA_in          const unsigned short                                            blobServerPrivatePort
                    )
                {
                    const auto cbLocalCacheTest = [ & ]() -> void
                    {
                        wrapFilesystemDataStorage(
                            [ & ] ( SAA_in const om::ObjPtr< DataChunkStorage >& syncCacheStorage ) -> void
                            {
                                cbProxyTransferTests( syncCacheStorage );
                            }
                            );
                    };

                    executeTransferTestsWrap(
                        cpp::bind(
                            &executeFilesystemTransferTests,
                            cbLocalCacheTest,
                            controlToken,
                            om::ObjPtrCopyable< bl::transfer::SendRecvContext >::acquireRef( context.get() ),
                            blobServerPrivatePort,
                            0U /* threadsCount */,
                            0U /* maxConcurrentTasks */
                            )
                        );
                }
                );
        }
    };

    typedef TestBlobTransferFilesystemUtils
    <
        bl::data::DataChunkStorageFilesystemMultiFiles
    >
    TestBlobTransferFilesystemUtilsImpl;

} // utest

#endif /* __UTESTS_BASELIB_TESTBLOBTRANSFERFILESYSTEMUTILS_H_ */
