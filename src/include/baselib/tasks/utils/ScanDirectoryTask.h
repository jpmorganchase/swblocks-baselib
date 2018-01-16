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

#ifndef __BL_TASKS_UTILS_SCANDIRECTORYTASK_H_
#define __BL_TASKS_UTILS_SCANDIRECTORYTASK_H_

#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/utils/DirectoryScannerControlToken.h>

#include <baselib/core/BoxedObjects.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

#include <vector>

namespace bl
{
    namespace tasks
    {
        template
        <
            typename E = void
        >
        class ScanDirectoryTaskT;

        typedef om::ObjectImpl< ScanDirectoryTaskT<> > ScanDirectoryTaskImpl;

        /******************************************************************************************
         * =================================== ScanDirectoryTask ==================================
         */

        /**
         * @brief class ScanDirectoryTask
         */

        template
        <
            typename E
        >
        class ScanDirectoryTaskT :
            public SimpleTaskBase
        {
            BL_DECLARE_OBJECT_IMPL( ScanDirectoryTaskT )

        public:

            typedef ScanDirectoryTaskT< E >                                                 this_type;

        protected:

            fs::path                                                                        m_path;
            const om::ObjPtr< bo::path >                                                    m_rootPath;
            std::vector< fs::directory_entry >                                              m_entries;
            const om::ObjPtr< DirectoryScannerControlToken >                                m_cbControl;

            ScanDirectoryTaskT(
                SAA_in              const fs::path&                                         path,
                SAA_in              const om::ObjPtr< bo::path >&                           rootPath,
                SAA_in_opt          const om::ObjPtr< DirectoryScannerControlToken >&       cbControl = nullptr
                )
                :
                m_path( path ),
                m_rootPath( om::copy( rootPath ) ),
                m_cbControl( om::copy( cbControl ) )
            {
            }

            virtual void onExecute() NOEXCEPT OVERRIDE
            {
                std::vector< fs::path > dirsToScan;

                const auto cbControl = om::copy( m_cbControl );
                const auto eq = m_eq;

                {
                    BL_TASKS_HANDLER_BEGIN()

                    try
                    {
                        eh::error_code ec;
                        fs::directory_iterator end, it( m_path, ec );

                        if( ec && ( nullptr == m_cbControl || false == m_cbControl -> isErrorAllowed( ec ) ) )
                        {
                            BL_CHK_EC_USER_FRIENDLY(
                                ec,
                                BL_MSG()
                                    << "Cannot open directory "
                                    << m_path
                                    );
                        }

                        if( ! ec )
                        {
                            for( ; it != end; ++it )
                            {
                                const auto& entry = *it;

                                const auto status = entry.symlink_status();

                                if( m_cbControl )
                                {
                                    if( ! m_cbControl -> isEntryAllowed( entry ) )
                                    {
                                        /*
                                         * This entry must be ignored; continue...
                                         */

                                        continue;
                                    }
                                }
                                else
                                {
                                    /*
                                     * When control token is not provided the default
                                     * is to only allow regular files, directories and
                                     * symlinks
                                     */

                                    BL_CHK_USER_FRIENDLY(
                                        true,
                                        fs::is_other( status ),
                                        BL_MSG()
                                            << "Path "
                                            << entry.path()
                                            << " not a regular file, directory or a symlink"
                                        );
                                }

                                if( false == fs::is_other( status ) && false == fs::is_symlink( status ) && fs::is_directory( status ) )
                                {
                                    /*
                                     * Here we're still holding the task lock, so we can't
                                     * call back into the execution queue to post new tasks
                                     * and we only save these. After we exit the lock guard
                                     * section we're going to schedule the tasks
                                     */

                                    dirsToScan.push_back( entry.path() );
                                }

                                m_entries.push_back( std::move( entry ) );
                            }
                        }

                        m_eq.reset();
                    }
                    catch( std::exception& )
                    {
                        m_eq.reset();

                        throw;
                    }

                    BL_TASKS_HANDLER_END_NOTREADY()
                }

                BL_TASKS_HANDLER_BEGIN_NOLOCK()

                /*
                 * Push new tasks for all directories which need to be scanned
                 *
                 * Note: BL_TASKS_HANDLER_BEGIN_NOLOCK() - the task lock is not held!
                 */

                for( const auto& path : dirsToScan )
                {
                    const auto scanner = ScanDirectoryTaskImpl::createInstance( path, m_rootPath, cbControl );
                    eq -> push_back( om::qi< Task >( scanner ) );
                }

                BL_TASKS_HANDLER_END()
            }

        public:

            const std::vector< fs::directory_entry >& entries() const NOEXCEPT
            {
                return m_entries;
            }

            std::vector< fs::directory_entry >& entriesLvalue() NOEXCEPT
            {
                return m_entries;
            }

            const fs::path& rootPath() const NOEXCEPT
            {
                return m_rootPath -> value();
            }
        };

    } // tasks

} // bl

#endif /* __BL_TASKS_UTILS_SCANDIRECTORYTASK_H_ */
