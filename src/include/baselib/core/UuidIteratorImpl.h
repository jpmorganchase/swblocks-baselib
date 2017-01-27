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

#ifndef __BL_UUIDITERATORIMPL_H_
#define __BL_UUIDITERATORIMPL_H_

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/UuidIterator.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/BaseIncludes.h>

#include <vector>
#include <limits>

namespace bl
{
    /**
     * @brief class UuidIteratorImpl - simple implementation of UUID iterator
     */

    template
    <
        typename E = void
    >
    class UuidIteratorImplT : public UuidIterator
    {
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( UuidIteratorImplT, UuidIterator )

    protected:

        typedef const uuid_t*               iterator_t;

        const iterator_t                    m_begin;
        const iterator_t                    m_end;
        iterator_t                          m_pos;
        const om::ObjPtr< om::Object >      m_impl;

        UuidIteratorImplT(
            SAA_in              const iterator_t                                    begin,
            SAA_in              const iterator_t                                    end,
            SAA_in              const om::ObjPtr< om::Object >&                     impl = nullptr
            )
            :
            m_begin( begin ),
            m_end( end ),
            m_pos( begin ),
            m_impl( om::copy( impl ) )
        {
            BL_ASSERT( m_begin <= m_end );
        }

        UuidIteratorImplT(
            SAA_in              const std::vector< uuid_t >&                        data,
            SAA_in              const om::ObjPtr< om::Object >&                     impl = nullptr
            )
            :
            m_begin( &data.front() ),
            m_end( m_begin + data.size() ),
            m_pos( m_begin ),
            m_impl( om::copy( impl ) )
        {
            BL_ASSERT( m_begin <= m_end );
        }

    public:

        virtual bool hasCurrent() const NOEXCEPT OVERRIDE
        {
            return ( m_pos != m_end );
        }

        virtual void loadNext() OVERRIDE
        {
            BL_CHK(
                false,
                m_pos != m_end,
                BL_MSG() << "Iterator out of range"
                );

            ++m_pos;
        }

        virtual const uuid_t& current() const NOEXCEPT OVERRIDE
        {
            return *m_pos;
        }

        virtual void reset() OVERRIDE
        {
            m_pos = m_begin;
        }
    };

    typedef om::ObjectImpl< UuidIteratorImplT<> > UuidIteratorImpl;

} // bl

#endif /* __BL_UUIDITERATORIMPL_H_ */
