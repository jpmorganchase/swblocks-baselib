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

#ifndef __BL_MESSAGING_DATACHUNKSTORAGE_H_
#define __BL_MESSAGING_DATACHUNKSTORAGE_H_

#include <baselib/data/DataBlock.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <cstdint>

BL_IID_DECLARE( DataChunkStorage, "d836b47a-03d3-4ce2-a2c0-5df276ccb84f" )

namespace bl
{
    namespace data
    {
        /**
         * @brief A data chunk block holder
         */

        struct DataChunkBlock
        {
            uuid_t                                   chunkId;
            om::ObjPtrCopyable< DataBlock >          data;

            DataChunkBlock()
                :
                chunkId( uuids::nil() )
            {
            }
        };

        /**
         * @brief DataChunkStorage class - this is interface to abstract
         * the data chunk storage
         */

        class DataChunkStorage :
            public om::Disposable
        {
            BL_DECLARE_INTERFACE( DataChunkStorage )

        public:

            typedef cpp::function
            <
                void ( SAA_in const om::ObjPtr< DataChunkStorage >& syncStorage )
            >
            data_storage_callback_t;

            /*
             * Storage support (load/save)
             */

            virtual void load(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                ) = 0;

            virtual void save(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId,
                SAA_in                  const om::ObjPtr< DataBlock >&                  data
                ) = 0;

            virtual void remove(
                SAA_in                  const uuid_t&                                   sessionId,
                SAA_in                  const uuid_t&                                   chunkId
                ) = 0;

            virtual void flushPeerSessions( SAA_in const uuid_t& peerId ) = 0;
        };

    } // data

} // bl

#endif /* __BL_MESSAGING_DATACHUNKSTORAGE_H_ */
