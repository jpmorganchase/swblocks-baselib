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

#ifndef __BL_JNI_DIRECTBYTEBUFFER_H_
#define __BL_JNI_DIRECTBYTEBUFFER_H_

#include <baselib/jni/JniEnvironment.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/BaseIncludes.h>

namespace bl
{
    namespace jni
    {
        template
        <
            typename E = void
        >
        class DirectByteBufferT
        {
            BL_NO_COPY_OR_MOVE( DirectByteBufferT )

        private:

            om::ObjPtr< data::DataBlock >                       m_buffer;
            GlobalReference< jobject >                          m_javaBuffer;

        public:

            DirectByteBufferT( SAA_in const std::size_t capacity )
                :
                DirectByteBufferT( data::DataBlock::createInstance( capacity ) )
            {
            }

            DirectByteBufferT( SAA_in om::ObjPtr< data::DataBlock >&& buffer )
                :
                m_buffer( BL_PARAM_FWD( buffer ) )
            {
                const auto& environment = JniEnvironment::instance();

                m_javaBuffer = environment.createGlobalReference< jobject >(
                    environment.createDirectByteBuffer( m_buffer -> pv(), m_buffer -> capacity() )
                    );
            }

            auto getBuffer() const NOEXCEPT -> const om::ObjPtr< data::DataBlock >&
            {
                return m_buffer;
            }

            void prepareForWrite() const
            {
                m_buffer -> setOffset1( 0U );
                m_buffer -> setSize( 0U );
            }

            void prepareForRead() const
            {
                const auto& environment = JniEnvironment::instance();

                environment.flipByteBuffer( m_javaBuffer.get() );

                m_buffer -> setOffset1( 0U );
                m_buffer -> setSize( environment.getByteBufferLimit( m_javaBuffer.get() ) );
            }

            void prepareForJavaRead() const
            {
                const auto& environment = JniEnvironment::instance();

                environment.setByteBufferPosition(
                    m_javaBuffer.get(),
                    static_cast< jint >( 0 )
                    );

                environment.setByteBufferLimit(
                    m_javaBuffer.get(),
                    numbers::safeCoerceTo< jint >( m_buffer -> size() )
                    );
            }

            void prepareForJavaWrite() const
            {
                JniEnvironment::instance().clearByteBuffer( m_javaBuffer.get() );
            }

            auto getJavaBuffer() const NOEXCEPT -> const GlobalReference< jobject >&
            {
                return m_javaBuffer;
            }
        };

        typedef DirectByteBufferT<> DirectByteBuffer;

    } // jni

} // bl

#endif /* __BL_JNI_DIRECTBYTEBUFFER_H_ */
