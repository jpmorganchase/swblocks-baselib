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

#ifndef __BL_ASYNC_ASYNCOPERATION_H_
#define __BL_ASYNC_ASYNCOPERATION_H_

#include <baselib/tasks/Task.h>

#include <baselib/core/Uuid.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

BL_IID_DECLARE( AsyncOperation, "60b4ef3c-cd9a-4719-a4d9-c4ba3da03753" )

namespace bl
{
    /**
     * @brief interface AsyncOperation - The async base interface
     *
     * The only method in the base interface is cancel() and currently it
     * always works in async fashion, but in the future we may provide 'wait'
     * parameter to enable sync cancellation
     *
     * Currently sync cancellation is not provided for two main reasons:
     *
     * 1) it is not necessary in most, if not all, cases
     * 2) there are very severe restrictions on where it can be called to avoid
     *    causing deadlocks and other problems
     *
     * In the future if sync cancellation is necessary we can provide it,
     * but we also have to provide more safety and protection in the code to
     * ensure correct usage
     *
     * When you call cancel() and it returns there is no guarantee that the
     * callback will not be called or that the cancel call will actually
     * cancel anything
     *
     * The only guarantee cancel() provides is that the operation will be
     * canceled if possible and also that your callback will be called in
     * both cases -- if the operation was canceled successfully you will get
     * called with Result indicating cancellation via Result::code being equal
     * to asio::error::operation_aborted while if the operation was executed
     * you will get called with whatever the result was from execution and it
     * of course can be success or failure, etc
     *
     * The cancel() method can be called from anywhere, including from any
     * thread and also from the callback itself and it will not block on
     * anything
     *
     * Note also that after you call cancel on an async operation you can no
     * longer use the object other than to release it as the data it carries
     * cannot be guaranteed to be available (this is part of the requirement
     * that the data can only be available while the async operation is in
     * progress so we can control memory usage)
     *
     * Normally the best practice is that after you call cancel() you simply
     * release the pointer and if your handler was subsequently called with
     * Result indicating success, but cancel was called you will know that the
     * operation has been canceled by checking the operation pointer for
     * nullptr (this will also normally be indicated by the m_cancelRequested
     * member being set to true); note that the helper prolog macros
     * BL_TASKS_HANDLER_CHK_ASYNC_RESULT_IMPL() and
     * BL_TASKS_HANDLER_CHK_ASYNC_RESULT_IMPL_THROW() and
     * BL_TASKS_HANDLER_BEGIN_CHK_ASYNC_RESULT() defined in TaskBase account
     * for this case and handle it correctly
     */

    class AsyncOperation : public om::Object
    {
        BL_DECLARE_INTERFACE( AsyncOperation )

    public:

        struct Result
        {
            Result(
                SAA_in_opt     const std::exception_ptr&    exceptionIn = nullptr,
                SAA_in_opt     const eh::error_code&        codeIn = eh::error_code(),
                SAA_in_opt     om::ObjPtr< tasks::Task >&&  taskIn = nullptr
                )
                :
                exception( exceptionIn ),
                code( codeIn ),
                task( BL_PARAM_FWD( taskIn ) )
            {
            }

            bool isFailed() const NOEXCEPT
            {
                return ( exception || code );
            }

            bool isCanceled() const NOEXCEPT
            {
                if( code && asio::error::operation_aborted == code )
                {
                    return true;
                }

                if( exception )
                {
                    const eh::error_code* ec = nullptr;

                    try
                    {
                        cpp::safeRethrowException( exception );
                    }
                    catch( eh::system_error& e )
                    {
                        if( e.code() && asio::error::operation_aborted == e.code() )
                        {
                            return true;
                        }

                        ec = eh::get_error_info< eh::errinfo_error_code >( e );
                    }
                    catch( std::exception& e )
                    {
                        ec = eh::get_error_info< eh::errinfo_error_code >( e );
                    }

                    if( ec && asio::error::operation_aborted == *ec )
                    {
                        return true;
                    }
                }

                return false;
            }

            const std::exception_ptr                exception;
            const eh::error_code                    code;
            const om::ObjPtrCopyable< tasks::Task > task;
        };

        typedef cpp::function< void( SAA_in const Result& exception ) NOEXCEPT > callback_t;

        virtual void cancel() NOEXCEPT = 0;
    };

} // bl

#endif /* __BL_ASYNC_ASYNCOPERATION_H_ */
