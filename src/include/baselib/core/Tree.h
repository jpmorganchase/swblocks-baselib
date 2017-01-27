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

#ifndef __BL_TREE_H_
#define __BL_TREE_H_

#include <baselib/core/BaseIncludes.h>

#include <vector>

namespace bl
{
    /**
     * @brief Simple unordered N-ary tree
     */

    template
    <
        typename T
    >
    class Tree
    {
        BL_NO_COPY( Tree )

    public:

        typedef T                                           value_type;
        typedef Tree< value_type >                          this_type;
        typedef cpp::SafeUniquePtr< this_type >             nodeptr_type;

    private:

        value_type                                          m_value;
        std::vector< nodeptr_type >                         m_children;

    public:

        explicit Tree( SAA_in value_type&& value )
            :
            m_value( BL_PARAM_FWD( value ) )
        {
        }

        Tree( SAA_in this_type&& other )
        {
            m_value = std::move( other.m_value );
            m_children = std::move( other.m_children );
        }

        this_type& operator=( SAA_in this_type&& other )
        {
            m_value = std::move( other.m_value );
            m_children = std::move( other.m_children );

            return *this;
        }

        virtual ~Tree() NOEXCEPT
        {
        }

        const value_type& value() const NOEXCEPT
        {
            return m_value;
        }

        value_type& lvalue() NOEXCEPT
        {
            return m_value;
        }

        const std::vector< nodeptr_type >& children() const NOEXCEPT
        {
            return m_children;
        }

        std::vector< nodeptr_type >& childrenLvalue() NOEXCEPT
        {
            return m_children;
        }

        bool hasChildren() const NOEXCEPT
        {
            return ! m_children.empty();
        }

        const this_type& child( SAA_in const std::size_t index ) const
        {
            checkIndex( index );

            return *m_children[ index ];
        }

        this_type& childLvalue( SAA_in const std::size_t index )
        {
            checkIndex( index );

            return *m_children[ index ];
        }

        /**
         * @brief Add a new child node with the specified value
         */

        this_type& addChild( SAA_in value_type&& value )
        {
            auto node = nodeptr_type::attach( new this_type( BL_PARAM_FWD( value ) ) );

            m_children.emplace_back( std::move( node ) );

            return *m_children.back();
        }

        /**
         * @brief Attach an existing child node subtree to this tree
         */

        this_type& addTree( SAA_in this_type&& tree )
        {
            auto node = nodeptr_type::attach( new this_type( BL_PARAM_FWD( tree ) ) );

            m_children.emplace_back( std::move( node ) );

            return *m_children.back();
        }

        /**
         * @brief Remove child node and its subtree
         */

        void removeChild( SAA_in const std::size_t index )
        {
            checkIndex( index );

            m_children.erase( m_children.begin() + index );
        }

    protected:

        void checkIndex( SAA_in const std::size_t index ) const
        {
            BL_CHK_T(
                false,
                index < m_children.size(),
                ArgumentException(),
                BL_MSG()
                    << "Tree child node index "
                    << index
                    << " is out of range, max="
                    << static_cast< typename std::vector< nodeptr_type >::difference_type >( m_children.size() ) - 1
                );
        }
    };

} // bl

#endif /* __BL_TREE_H_ */
