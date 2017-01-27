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

#ifndef __BL_TABLE_H_
#define __BL_TABLE_H_

#include <baselib/core/BaseIncludes.h>

#include <vector>

namespace bl
{
    namespace detail
    {
        template
        <
            typename T
        >
        class Grid
        {

        private:

            const std::size_t                                   m_columnCount;

        protected:

            typedef std::vector< T >                            RowT;

            std::vector< RowT >                                 m_rows;

        public:

            Grid( SAA_in const std::size_t columnCount )
                :
                m_columnCount( columnCount )
            {
            }

            std::size_t getColumnCount() const NOEXCEPT
            {
                return m_columnCount;
            }

            std::size_t getRowCount() const NOEXCEPT
            {
                return m_rows.size();
            }

            template
            <
                typename Value,
                typename... Values
            >
            void addRow(
                SAA_in          Value&&                         value,
                SAA_in          Values&&...                     values
                )
            {
                RowT row;

                insertValues( row, value, std::forward< Values >( values )... );

                addRowInternal( std::move( row ) );
            }

            const T& getCell(
                SAA_in              const std::size_t           rowIndex,
                SAA_in              const std::size_t           columnIndex
                ) const
            {
                return getRow( rowIndex ).at( columnIndex );
            }

        protected:

            void addRowInternal( SAA_in RowT&& row )
            {
                verifyValidColumnCount( row );

                m_rows.emplace_back( BL_PARAM_FWD( row ) );
            }

            const RowT& getRow( SAA_in const std::size_t index ) const
            {
                return m_rows.at( index );
            }

            template
            <
                typename Value,
                typename... Values
            >
            static void insertValues(
                SAA_inout       RowT&                           row,
                SAA_in          Value&&                         value,
                SAA_in          Values&&...                     values
                )
            {
                row.emplace_back( BL_PARAM_FWD( value ) );

                insertValues( row, std::forward< Values >( values )... );
            }

            /*
             * Empty base case for recursion
             */

            static void insertValues( SAA_in RowT& )
            {
            }

            const T& getCell(
                SAA_in              const RowT&                 row,
                SAA_in              const std::size_t           index
                ) const
            {
                return row.at( index );
            }

            void verifyValidColumnCount( SAA_in const RowT& row ) const
            {
                BL_CHK(
                    false,
                    row.size() == getColumnCount(),
                    BL_MSG()
                        << "Invalid number of columns specified. Expected: "
                        << getColumnCount()
                        << " Actual: "
                        << row.size()
                    );
            }
        };
    }

    template
    <
        typename T,
        typename ColumnHeader
    >
    class Table : public detail::Grid< T >
    {

    public:

        typedef std::vector< ColumnHeader >                 ColumnHeaders;

    private:

        ColumnHeaders                                       m_columnHeaders;

    public:

        typedef detail::Grid< T >                           base_type;

        Table(
            SAA_in          const ColumnHeaders&            columnHeaders
            )
            :
            base_type( columnHeaders.size() ),
            m_columnHeaders( columnHeaders )
        {
        }

        const ColumnHeaders& getColumnHeaders() const NOEXCEPT
        {
            return m_columnHeaders;
        }
    };

} // bl

#endif /* __BL_TABLE_H_ */
