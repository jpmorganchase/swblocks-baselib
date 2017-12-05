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

#ifndef __BL_TEST_MACHINEGLOBALTESTLOCK_H_
#define __BL_TEST_MACHINEGLOBALTESTLOCK_H_

#include <baselib/core/Logging.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

namespace test
{
    /**
     * @brief class MachineGlobalTestLock - a machine global test lock
     */

    template
    <
        typename E = void
    >
    class MachineGlobalTestLockT
    {
        BL_NO_COPY_OR_MOVE( MachineGlobalTestLockT )

    private:

        typedef bl::os::RobustNamedMutex                mutex_t;

        static const std::string                        g_defaultLockName;

        const std::string                               m_name;
        mutex_t                                         m_lock;
        bl::cpp::SafeUniquePtr< mutex_t::Guard >        m_guard;

    public:

        MachineGlobalTestLockT( SAA_in_opt const std::string& name = g_defaultLockName )
            :
            m_name( name ),
            m_lock( name )
        {
            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "Trying to acquire the global machine test lock with name '"
                    << m_name
                    << "' ..."
                );

            m_guard.reset( new ( mutex_t::Guard )( m_lock ) );

            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "The global machine test lock with name '"
                    << m_name
                    << "' was acquired"
                );
        }

        ~MachineGlobalTestLockT() NOEXCEPT
        {
            if( m_guard )
            {
                /*
                 * Make sure we sleep one second before we release the lock to ensure that
                 * the acceptor socket(s) are indeed released and we don't hit issues of
                 * the type 'Cannot assign requested address'
                 */

                bl::os::sleep( bl::time::seconds( 1L ) );

                m_guard.reset();

                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "The global machine test lock with name '"
                        << m_name
                        << "' was released"
                    );
            }
        }
    };

    template
    <
        typename E
    >
    const std::string
    MachineGlobalTestLockT< E >::g_defaultLockName(
        "Test-Machine-Global-Lock-27f6db3c-4d61-4edb-8b66-c8ea32574c0c"
        );

    typedef MachineGlobalTestLockT<> MachineGlobalTestLock;

} // test

#endif /* __BL_TEST_MACHINEGLOBALTESTLOCK_H_ */
