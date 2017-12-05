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

#ifndef __BL_MESSAGING_TCPBLOCKTRANSFERCOMMON_H_
#define __BL_MESSAGING_TCPBLOCKTRANSFERCOMMON_H_

#include <baselib/messaging/BackendProcessingBase.h>
#include <baselib/messaging/MessageBlockCompletionQueue.h>

#include <baselib/async/AsyncBaseIncludes.h>

#include <baselib/data/DataBlock.h>

#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/TcpBaseTasks.h>
#include <baselib/tasks/TcpSslBaseTasks.h>
#include <baselib/tasks/TaskControlToken.h>
#include <baselib/tasks/TasksIncludes.h>

#include <baselib/core/ErrorHandling.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace tasks
    {
        /**
         * @brief class BlockTransferDefs - static class for common blob transfer protocol code definitions
         */

        template
        <
            typename E = void
        >
        class BlockTransferDefsT
        {
            BL_DECLARE_STATIC( BlockTransferDefsT )

        private:

            /*
             * This non-nil() chunk id will be used for get/put/remove commands where the the block
             * type implies that the chunk id is irrelevant
             *
             * This will simplify the error handling and validation significantly
             */

            static const uuid_t                                                             g_chunkIdDefault;

        public:

            /*
             * Data block types
             */

            enum class BlockType : std::uint16_t
            {
                Normal                     = 0,
                Authentication,
                ServerState,
                TransferOnly,

                Count
            };

            static const uuid_t& chunkIdDefault() NOEXCEPT
            {
                return g_chunkIdDefault;
            }
        };

        BL_DEFINE_STATIC_MEMBER( BlockTransferDefsT, const uuid_t, g_chunkIdDefault ) =
            uuids::string2uuid( "6eb31c0f-efd2-4091-b364-2cb7ad8baf17" );

        typedef BlockTransferDefsT<> BlockTransferDefs;

        namespace detail
        {
            /*
             * CommandBlock class
             */

            struct CommandBlock
            {
                /*
                 * The blob transfer protocol version
                 *
                 * Should be incremented when there are changes in the protocol code
                 * which are not fully forward and backward compatible
                 */

                enum : std::uint32_t
                {
                    BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V1   = 1,
                    BLOB_TRANSFER_PROTOCOL_CLIENT_VERSION_V2   = 2,
                };

                enum : std::uint32_t
                {
                    BLOB_TRANSFER_PROTOCOL_SERVER_VERSION      = 2,
                };

                /*
                 * Control codes
                 */

                enum : std::uint16_t
                {
                    CntrlCodeNone                       = 0,
                    CntrlCodeGetProtocolVersion,
                    CntrlCodeSetProtocolVersion,
                    CntrlCodeGetDataBlockSize,
                    CntrlCodeGetDataBlock,
                    CntrlCodePutDataBlock,
                    CntrlCodeRemoveDataBlock,
                    CntrlCodePeerSessionsDataFlushRequest,
                };

                /*
                 * Flags
                 */

                enum : std::uint16_t
                {
                    /*
                     * @brief Indicates that this is an acknowledgment control
                     * block (usually sent from the blob server side to indicate
                     * server errors, if any, or to sync the execution of the
                     * command on the server side
                     */

                    AckBit                              = 0x0001,

                    /*
                     * @brief Indicates that a server error has occurred and
                     * data won't be transferred if such was requested
                     *
                     * If this bit is set that means the client should throw
                     * eh::ServerErrorException and if errorCode it should be
                     * attached to it; errorCode is assumed to be a generic
                     * error category for portability reasons (i.e. basically
                     * errno)
                     */

                    ErrBit                              = 0x0002,
                };

                /*
                 * Remove flags (for blockInfo.flags which is std::uint16_t)
                 */

                enum : std::uint16_t
                {
                    /*
                     * @brief Can be used for remove operations to request
                     * not found errors to be ignored
                     */

                    IgnoreIfNotFound                    = 0x0001,
                };

                union DataHeader
                {
                    /*
                     * Note that for numbers the reserved fields types and their count
                     * should match the actual real fields below and if a new real
                     * field is added then new reserved fields must be added, etc
                     *
                     * Note also that if new reserved fields are added then the
                     * network2Host() and host2Network() methods below should be also
                     * changed to do the appropriate conversions to and from network
                     * byte order of the respective fields
                     */

                    struct tagReserved
                    {
                        std::uint32_t                   reserved1;
                        std::uint32_t                   reserved2;
                        std::uint16_t                   reserved3;
                        std::uint16_t                   reserved4;
                        uuid_t                          reservedUuid;
                    }
                    reserved;

                    struct tagVersion
                    {
                        std::uint32_t                   value;
                    }
                    version;

                    /*
                     * Note that the underlying type of BlockTransferDefs::BlockType enum
                     * is std::uint16_t (which should match to 'reserved3' field above)
                     *
                     * 'protocolDataOffset' field matches to 'reserved2' field above and
                     * 'flags' + 'unused' fields which are both std::uint16_t match to
                     * the 'reserved1' field
                     *
                     * The 'reserved4' field (std::uint16_t) is not in use yet
                     */

                    struct tagBlockInfo
                    {
                        std::uint16_t                   flags;
                        std::uint16_t                   unused;
                        std::uint32_t                   protocolDataOffset;
                        BlockTransferDefs::BlockType    blockType;
                    }
                    blockInfo;

                    struct tagRaw
                    {
                        std::uint8_t                    bytes[ sizeof( tagReserved ) ];
                    }
                    raw;
                };

                /*
                 * There is no need for packing since these are
                 * 8 byte aligned already
                 */

                std::uint16_t                           cntrlCode;
                std::uint16_t                           flags;
                std::uint32_t                           errorCode;
                uuid_t                                  peerId;
                uuid_t                                  chunkId;
                std::uint32_t                           chunkSize;
                DataHeader                              data;

                CommandBlock() NOEXCEPT
                    :
                    cntrlCode( CntrlCodeNone ),
                    flags( 0U ),
                    errorCode( 0U ),
                    peerId( uuids::nil() ),
                    chunkId( uuids::nil() ),
                    chunkSize( 0U ),
                    data()
                {
                    static_assert( 0 == ( sizeof( *this ) % 8 ), "CommandBlock must be 8-byte aligned" );
                    static_assert( sizeof( data ) == sizeof( data.reserved ), "Reserved block must be the largest" );
                    static_assert( sizeof( data ) == sizeof( data.raw ), "Raw bytes sections size must match" );
                }

                void network2Host()
                {
                    cntrlCode = os::network2HostShort( cntrlCode );
                    flags = os::network2HostShort( flags );
                    errorCode = os::network2HostLong( errorCode );
                    chunkSize = os::network2HostLong( chunkSize );

                    data.reserved.reserved1 = os::network2HostLong( data.reserved.reserved1 );
                    data.reserved.reserved2 = os::network2HostLong( data.reserved.reserved2 );
                    data.reserved.reserved3 = os::network2HostShort( data.reserved.reserved3 );
                    data.reserved.reserved4 = os::network2HostShort( data.reserved.reserved4 );

                    BL_CHK(
                        false,
                        0U == data.blockInfo.unused,
                        BL_MSG()
                            << "The 'data.blockInfo.unused' field is expected to be zero but it is "
                            << data.blockInfo.unused
                        );
                }

                void host2Network()
                {
                    cntrlCode = os::host2NetworkShort( cntrlCode );
                    flags = os::host2NetworkShort( flags );
                    errorCode = os::host2NetworkLong( errorCode );
                    chunkSize = os::host2NetworkLong( chunkSize );

                    BL_CHK(
                        false,
                        0U == data.blockInfo.unused,
                        BL_MSG()
                            << "The 'data.blockInfo.unused' field is expected to be zero but it is "
                            << data.blockInfo.unused
                        );

                    data.reserved.reserved1 = os::host2NetworkLong( data.reserved.reserved1 );
                    data.reserved.reserved2 = os::host2NetworkLong( data.reserved.reserved2 );
                    data.reserved.reserved3 = os::host2NetworkShort( data.reserved.reserved3 );
                    data.reserved.reserved4 = os::host2NetworkShort( data.reserved.reserved4 );
                }
            };

            inline void chkPartialDataTransfer( SAA_in const bool cond )
            {
                BL_CHK(
                    false,
                    cond,
                    BL_MSG()
                        << "Socket unexpectedly closed or partial data transfer have occurred"
                    );
            }

            inline void chkChunkSize( SAA_in const bool cond )
            {
                BL_CHK(
                    false,
                    cond,
                    BL_MSG()
                        << "Invalid chunk size"
                    );
            }

        } // detail

        /*************************************************************************************************
         * The base connection
         */

        /**
         * @brief TcpBlockTransferConnectionBase
         */

        template
        <
            typename STREAM
        >
        class TcpBlockTransferConnectionBase :
            public STREAM
        {
            BL_DECLARE_OBJECT_IMPL( TcpBlockTransferConnectionBase )

        protected:

            typedef TcpBlockTransferConnectionBase< STREAM >                            this_type;
            typedef STREAM                                                              base_type;

            typedef detail::CommandBlock                                                CommandBlock;

            CommandBlock                                                                m_cmdBuffer;
            uuid_t                                                                      m_chunkId;

            const uuid_t                                                                m_peerId;
            uuid_t                                                                      m_remotePeerId;
            std::atomic< std::uint64_t >                                                m_noOfBlocksTransferred;

            TcpBlockTransferConnectionBase( SAA_in_opt const uuid_t& peerId ) NOEXCEPT
                :
                m_chunkId( uuids::nil() ),
                m_peerId( peerId ),
                m_remotePeerId( uuids::nil() ),
                m_noOfBlocksTransferred( 0U )
            {
            }

            void setErrorCode( SAA_in const eh::error_code& ec ) NOEXCEPT
            {
                m_cmdBuffer.errorCode = ec.value();

                m_cmdBuffer.flags |= CommandBlock::ErrBit;
            }

            virtual bool isExpectedException(
                SAA_in                  const std::exception_ptr&                       eptr,
                SAA_in                  const std::exception&                           exception,
                SAA_in_opt              const eh::error_code*                           ec
                ) NOEXCEPT OVERRIDE
            {
                if(
                    TcpSocketCommonBase::isExpectedSocketException(
                        base_type::isCanceled() /* isCancelExpected */,
                        ec
                        )
                    )
                {
                    return true;
                }

                return base_type::isExpectedException( eptr, exception, ec );
            }

            /**
             * @brief Returns true if the uuids are not nil() and identical
             */

            static bool isNonNullAndSame(
                SAA_in                  const uuid_t&                                   uuid1,
                SAA_in                  const uuid_t&                                   uuid2
                )
            {
                return uuid1 != uuids::nil() && uuid1 == uuid2;
            }

        public:

            enum
            {
                /*
                 * 128 MB is the maximum chunk size
                 */

                MAX_CHUNK_SIZE = 128 * 1024 * 1024,
            };

            const uuid_t& getChunkId() const NOEXCEPT
            {
                return m_chunkId;
            }

            const uuid_t& peerId() const NOEXCEPT
            {
                return m_peerId;
            }

            uuid_t remotePeerId() const NOEXCEPT
            {
                uuid_t remotePeerId;

                BL_NOEXCEPT_BEGIN()

                BL_MUTEX_GUARD( base_type::m_lock );

                remotePeerId = m_remotePeerId;

                BL_NOEXCEPT_END()

                return remotePeerId;
            }

            bool isRemotePeerIdAvailable() const NOEXCEPT
            {
                return uuids::nil() != remotePeerId();
            }

            std::uint64_t noOfBlocksTransferred() const NOEXCEPT
            {
                return m_noOfBlocksTransferred;
            }
        };

    } // tasks

} // bl

#endif /* __BL_MESSAGING_TCPBLOCKTRANSFERCOMMON_H_ */
