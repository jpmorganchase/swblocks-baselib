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

#include <utests/baselib/TestBlobTransferFilesystemUtils.h>

UTF_AUTO_TEST_CASE( BlobTransfer_FilesPackagerInMemoryTests )
{
    test::MachineGlobalTestLock lockBlobServer;

    utest::TestBlobTransferFilesystemUtilsImpl::executeTransferTestsWrap(
        bl::cpp::bind(
            &utest::TestBlobTransferFilesystemUtilsImpl::filesPackagerTestsWrap,
            test::UtfArgsParser::port() /* blobServerPort */,
            utest::TestBlobTransferUtils::getInMemoryMetadataStore(),
            utest::TestBlobTransferUtils::CancelType::NoCancel
            )
        );
}

UTF_AUTO_TEST_CASE( BlobTransfer_FilesPackagerInMemoryCancelUploadTests )
{
    test::MachineGlobalTestLock lockBlobServer;

    utest::TestBlobTransferFilesystemUtilsImpl::executeTransferTestsWrap(
        bl::cpp::bind(
            &utest::TestBlobTransferFilesystemUtilsImpl::filesPackagerTestsWrap,
            test::UtfArgsParser::port() /* blobServerPort */,
            utest::TestBlobTransferUtils::getInMemoryMetadataStore(),
            utest::TestBlobTransferUtils::CancelType::CancelUpload
            )
        );
}

UTF_AUTO_TEST_CASE( BlobTransfer_FilesPackagerInMemoryCancelDownloadTests )
{
    test::MachineGlobalTestLock lockBlobServer;

    utest::TestBlobTransferFilesystemUtilsImpl::executeTransferTestsWrap(
        bl::cpp::bind(
            &utest::TestBlobTransferFilesystemUtilsImpl::filesPackagerTestsWrap,
            test::UtfArgsParser::port() /* blobServerPort */,
            utest::TestBlobTransferUtils::getInMemoryMetadataStore(),
            utest::TestBlobTransferUtils::CancelType::CancelDownload
            )
        );
}

UTF_AUTO_TEST_CASE( BlobTransfer_FilesPackagerInMemoryCancelRemoveTests )
{
    test::MachineGlobalTestLock lockBlobServer;

    utest::TestBlobTransferFilesystemUtilsImpl::executeTransferTestsWrap(
        bl::cpp::bind(
            &utest::TestBlobTransferFilesystemUtilsImpl::filesPackagerTestsWrap,
            test::UtfArgsParser::port() /* blobServerPort */,
            utest::TestBlobTransferUtils::getInMemoryMetadataStore(),
            utest::TestBlobTransferUtils::CancelType::CancelRemove
            )
        );
}

UTF_AUTO_TEST_CASE( BlobTransfer_StartBlobServer )
{
    utest::TestBlobTransferFilesystemUtilsImpl::startBlobServer();
}

UTF_AUTO_TEST_CASE( BlobTransfer_StartBlobClient )
{
    utest::TestBlobTransferFilesystemUtilsImpl::startBlobClient();
}

/************************************************************************
 * Tests for the ProxyDataChunkStorageImpl
 */

UTF_AUTO_TEST_CASE( BlobTransfer_ProxyDataChunkBackendImplTests )
{
    utest::TestBlobTransferFilesystemUtilsImpl::proxyDataChunkBackendImplTests();
}

UTF_AUTO_TEST_CASE( BlobTransfer_StartBlobServerProxy )
{
    utest::TestBlobTransferFilesystemUtilsImpl::startBlobServerProxy();
}

UTF_AUTO_TEST_CASE( BlobTransfer_FilesPackagerInMemoryProxyTests )
{
    test::MachineGlobalTestLock lockBlobServer;

    utest::TestBlobTransferFilesystemUtilsImpl::executeTransferTestsWrap(
        bl::cpp::bind(
            &utest::TestBlobTransferFilesystemUtilsImpl::filesPackagerTestsWithProxyWrap,
            test::UtfArgsParser::port() /* blobServerPort */,
            utest::TestBlobTransferUtils::getInMemoryMetadataStore()
            )
        );
}

