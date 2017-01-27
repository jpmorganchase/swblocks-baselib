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

#ifndef __BL_CMDLINE_OPTIONBASE_H_
#define __BL_CMDLINE_OPTIONBASE_H_

#include <baselib/core/BaseIncludes.h>
#include <baselib/core/ProgramOptions.h>

namespace bl
{
    namespace cmdline
    {
        /**
         * @brief OptionFlags - command line option behavior modifiers.
         */

        enum OptionFlags : unsigned short
        {
            None            = 0,            ///< Standard optional command line option
            Required        = 1 << 0,       ///< Required command line option
            Hidden          = 1 << 1,       ///< Hide the option from help
            MultiValue      = 1 << 2,       ///< Option accepts more than one value (needs MultiXXXOption type)
            Positional      = 1 << 3,       ///< Positional option (up to the specified number of values)
            PositionalAll   = Positional | MultiValue,  ///< Positional option, all remaining values

            OneValue        = 0 << 4,       ///< Single value option (default)
            TwoValues       = 1 << 4,       ///< 2 values (needs MultiXXXOption type)
            ThreeValues     = 2 << 4,       ///< 3 values
            FourValues      = 3 << 4,       ///< 4 values
            FiveValues      = 4 << 4,       ///< 5 values
            SixValues       = 5 << 4,       ///< 6 values
            SevenValues     = 6 << 4,       ///< 7 values
            EightValues     = 7 << 4,       ///< 8 values

            Override        = 1 << 8,       ///< Override all missing required options if set

            RequiredMultiValue = Required | MultiValue,
            RequiredPositional = Required | Positional,

            // for internal use only
            ValueCountShift = 4,
            ValueCountMask  = 7 << ValueCountShift,
        };

        BL_DECLARE_BITMASK( OptionFlags )


        /**
         * @brief class OptionBase - command line option wrapper base class.
         */

        template
        <
            typename E = void
        >
        class OptionBaseT
        {
            BL_CTR_DEFAULT_DELETE( OptionBaseT )
            BL_NO_COPY_OR_MOVE( OptionBaseT )
            BL_NO_POLYMORPHIC_BASE( OptionBaseT )

        private:

            const std::string                   m_name;
            const std::string                   m_description;
            OptionFlags                         m_flags;

        protected:

            cpp::ScalarTypeIniter< bool >       m_hasValue;
            cpp::ScalarTypeIniter< bool >       m_hasDefault;

        protected:

            /**
             * @param name
             * The option name is specified _without_ the leading dashes. If you want to specify
             * both the long and the short name, delimit them with a comma, e.g. "verbose,v".
             *
             * @param description
             * Human-readable option description. Used only in the help message.
             */

            OptionBaseT(
                SAA_inout   std::string&&       name,
                SAA_inout   std::string&&       description,
                SAA_in_opt  OptionFlags         flags = None,
                SAA_in_opt  bool                hasDefault = false
                )
                :
                m_name( BL_PARAM_FWD( name ) ),
                m_description( BL_PARAM_FWD( description ) ),
                m_flags( flags ),
                m_hasDefault( hasDefault )
            {
            }

        public:

            const std::string& getName() const NOEXCEPT
            {
                return m_name;
            }

            const std::string& getDescription() const NOEXCEPT
            {
                return m_description;
            }

            std::string getDisplayName() const
            {
                if( isPositional() )
                {
                    return "<" + m_name + ">";
                }

                const auto pos = m_name.find( ',' );

                if( pos == std::string::npos )
                {
                    return "--" + m_name;
                }

                return "--" + m_name.substr( 0, pos );
            }

            OptionFlags getFlags() const NOEXCEPT
            {
                return m_flags;
            }

            bool isRequired() const NOEXCEPT
            {
                return !! ( m_flags & Required );
            }

            bool isMultiValue() const NOEXCEPT
            {
                return !! ( m_flags & MultiValue );
            }

            bool isHidden() const NOEXCEPT
            {
                return !! ( m_flags & Hidden );
            }

            bool isPositional() const NOEXCEPT
            {
                return !! ( m_flags & Positional );
            }

            bool isOverride() const NOEXCEPT
            {
                return !! ( m_flags & Override );
            }

            int getPositionalCount() const NOEXCEPT
            {
                if( m_flags & Positional )
                {
                    if( m_flags & MultiValue )
                    {
                        return -1;
                    }

                    return getValueCount();
                }

                return 0;
            }

            int getValueCount() const NOEXCEPT
            {
                return ( ( unsigned )( m_flags & ValueCountMask ) >> ValueCountShift ) + 1;
            }

            bool hasValue() const NOEXCEPT
            {
                return m_hasValue;
            }

            bool hasDefaultValue() const NOEXCEPT
            {
                return m_hasDefault;
            }

            void setFlags( SAA_in const OptionFlags flags )
            {
                m_flags |= flags;
            }

            void unsetFlags( SAA_in const OptionFlags flags )
            {
                m_flags &= ~flags;
            }

            virtual const po::value_semantic* getSemantic() = 0;

            virtual void toStream( SAA_inout std::ostream& str ) const = 0;
        };

        typedef OptionBaseT<> OptionBase;

        inline std::ostream& operator<<(
            SAA_inout   std::ostream&               str,
            SAA_in      const OptionBase&           option
            )
        {
            option.toStream( str );

            return str;
        }

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_OPTIONBASE_H_ */
