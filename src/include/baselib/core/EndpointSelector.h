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

#ifndef __BL_ENDPOINTSELECTOR_H_
#define __BL_ENDPOINTSELECTOR_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( EndpointCircularIterator, "b0295f4b-e7e5-4a80-821c-9653dc3c8c65" )
BL_IID_DECLARE( EndpointSelector, "e5631409-60dc-415d-820f-cc67e2447687" )

namespace bl
{
    /**
     * @brief class EndpointCircularIterator - the endpoint circular
     * iterator interface
     */

    class EndpointCircularIterator : public om::Object
    {
        BL_DECLARE_INTERFACE( EndpointCircularIterator )

    public:

        enum : std::size_t
        {
            DEFAULT_MAX_RETRY_COUNT = 15U,
        };

        enum : std::size_t
        {
            DEFAULT_RETRY_TIMEOUT_IN_SECONDS = 6U,
        };

        virtual bool selectNext() = 0;
        virtual std::size_t count() const NOEXCEPT = 0;
        virtual const std::string& host() const NOEXCEPT = 0;
        virtual unsigned short port() const NOEXCEPT = 0;

        virtual void resetRetry() NOEXCEPT = 0;
        virtual bool canRetry() NOEXCEPT = 0;
        virtual bool canRetryNow( SAA_inout_opt time::time_duration* timeout = nullptr ) NOEXCEPT = 0;

        virtual std::size_t maxRetryCount() NOEXCEPT = 0;
        virtual const time::time_duration& retryTimeout() NOEXCEPT = 0;
    };

    /**
     * @brief class EndpointSelector - the endpoint selector interface
     *
     * Note: This is an immutable interface and its only purpose is to
     * shell out EndpointCircularIterator objects which can be mutated
     */

    class EndpointSelector : public om::Object
    {
        BL_DECLARE_INTERFACE( EndpointSelector )

    public:

        virtual om::ObjPtr< EndpointCircularIterator > createIterator() = 0;
        virtual std::size_t count() const NOEXCEPT = 0;
    };

} // bl

#endif /* __BL_ENDPOINTSELECTOR_H_ */
