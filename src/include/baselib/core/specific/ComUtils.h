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

#ifndef __BL_OS_SPECIFIC_COMUTILS_H_
#define __BL_OS_SPECIFIC_COMUTILS_H_

#if ! defined( _WIN32 )
#error This file can only be included when compiling on Windows platform
#endif

#include <objbase.h>
#include <objidl.h>

#define BL_CHK_HR( expression ) \
    bl::os::specific::detail::verifyHresult( expression ) \

#define BL_TYPEDEF_COM_POINTER( interface, name ) \
    typedef bl::cpp::SafeUniquePtr< interface, bl::os::specific::detail::ComDeleterT< interface > > name \

#define BL_COM_INIT_SINGLETHREADED() \
    bl::os::specific::detail::ComInitializer BL_ANONYMOUS_VARIABLE( g )( false /* multiThreaded */ )

#define BL_COM_INIT_MULTITHREADED() \
    bl::os::specific::detail::ComInitializer BL_ANONYMOUS_VARIABLE( g )( true /* multiThreaded */ )

namespace bl
{
    namespace os
    {
        namespace specific
        {
            namespace detail
            {
                static void verifyHresult( SAA_in const HRESULT hr )
                {
                    if( SUCCEEDED( hr ) )
                    {
                        return;
                    }

                    const eh::error_code ec( hr, bl::eh::system_category() );

                    const std::string message = "Win32 COM error has occurred";

                    BL_THROW( SystemException::create( ec, message ), message );
                }

                template
                <
                    typename E = void
                >
                class ComInitializerT
                {
                    cpp::ScalarTypeIniter< bool >                   m_inited;

                public:

                    ComInitializerT( SAA_in const bool multiThreaded )
                    {
                        BL_CHK_HR(
                            ::CoInitializeEx(
                                0,
                                multiThreaded ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED
                                )
                            );

                        m_inited = true;
                    }

                    ~ComInitializerT() NOEXCEPT
                    {
                        if( m_inited )
                        {
                            ::CoUninitialize();
                        }
                    }
                };

                typedef ComInitializerT<> ComInitializer;

                template
                <
                    typename I
                >
                class ComDeleterT
                {

                public:

                    void operator ()( SAA_in I* ptr ) const NOEXCEPT
                    {
                        ptr -> Release();
                    }
                };
            } // detail

        } // specific

    } // os

} // bl

#endif /* __BL_OS_SPECIFIC_COMUTILS_H_ */
