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

#ifndef __BL_TRANSACTION_H_
#define __BL_TRANSACTION_H_

#include <baselib/core/BaseIncludes.h>

namespace bl
{
    /**
     * @brief Execute sequence of commands as one transaction
     * @note Usage:
     *     Transaction t;
     *     t.add( runA, revertA );
     *     t.add( runB, revertB );
     *     t.add( runC );
     *     t.run();
     */

    template
    <
        typename E = void
    >
    class TransactionT
    {
        BL_CTR_COPY_DELETE( TransactionT )
        BL_CTR_MOVE_DELETE( TransactionT )

    private:

        typedef std::vector< std::pair< cpp::void_callback_t, cpp::void_callback_t > > commands_t;

    private:

        commands_t m_commands;

    public:

        TransactionT(){}

        void add(
            SAA_in       const cpp::void_callback_t& doIt,
            SAA_in_opt   const cpp::void_callback_t& rollbackIt = cpp::void_callback_t()
            )
        {
            m_commands.emplace_back( doIt, rollbackIt );
        }

        void run() const
        {
            auto current = m_commands.cbegin();
            const auto end = m_commands.cend();

            for( /* empty */ ; current != end ; ++current )
            {
                try
                {
                    if( current -> first )
                    {
                        current -> first();
                    }
                }
                catch( std::exception& )
                {
                    doRollbacks( current, m_commands.cbegin() );

                    throw;
                }
            }
        }

    private:

        static void doRollbacks(
            SAA_in commands_t::const_iterator         current,
            SAA_in const commands_t::const_iterator   begin
            ) NOEXCEPT
        {
            while( current != begin )
            {
                --current;

                utils::tryCatchLog(
                    "Unable to rollback because of exception",
                    [ & ]() -> void
                    {
                        if( current -> second )
                        {
                            current -> second();
                        }
                    }
                    );
                }
        }
    };

    typedef TransactionT<> Transaction;

} // bl

#endif /* __BL_TRANSACTION_H_ */
