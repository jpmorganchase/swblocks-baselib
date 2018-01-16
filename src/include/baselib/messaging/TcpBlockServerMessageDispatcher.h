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

#ifndef __BL_MESSAGING_TCPBLOCKSERVERMESSAGEDISPATCHER_H_
#define __BL_MESSAGING_TCPBLOCKSERVERMESSAGEDISPATCHER_H_

#include <baselib/messaging/TcpBlockTransferServer.h>
#include <baselib/messaging/TcpBlockTransferClient.h>

#include <baselib/messaging/AsyncMessageDispatcherWrapper.h>

namespace bl
{
    namespace tasks
    {
        /*
         * Incoming connections
         */

        template
        <
            typename STREAM
        >
        using TcpBlockServerMessageDispatcherImpl =
            om::ObjectImpl< TcpBlockServerT< STREAM, AsyncMessageDispatcherWrapper, TcpServerPolicySmooth > >;

        typedef TcpBlockServerMessageDispatcherImpl< TcpSocketAsyncBase >    TcpBlockServerMessageDispatcher;
        typedef TcpBlockServerMessageDispatcherImpl< TcpSslSocketAsyncBase > TcpSslBlockServerMessageDispatcher;

        /*
         * Outgoing connections
         */

        template
        <
            typename STREAM
        >
        using TcpBlockServerMessageDispatcherOutgoingImpl =
            om::ObjectImpl< TcpBlockServerOutgoingT< STREAM, TcpServerPolicySmooth > >;

        typedef TcpBlockServerMessageDispatcherOutgoingImpl< TcpSocketAsyncBase >    TcpBlockServerOutgoingMessageDispatcher;
        typedef TcpBlockServerMessageDispatcherOutgoingImpl< TcpSslSocketAsyncBase > TcpSslBlockServerOutgoingMessageDispatcher;

    } // tasks

} // bl

#endif /* __BL_MESSAGING_TCPBLOCKSERVERMESSAGEDISPATCHER_H_ */
