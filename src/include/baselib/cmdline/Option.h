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

#ifndef __BL_CMDLINE_OPTION_H_
#define __BL_CMDLINE_OPTION_H_

#include <baselib/cmdline/OptionBase.h>

namespace bl
{
    namespace cmdline
    {
        namespace detail
        {
            template
            <
                typename T,
                bool ForceDefault = false
            >
            struct OptionImpl;

            template
            <
                typename T
            >
            class Semantics;

        } // detail

        /**
         * @brief class Option - generic command line option with value.
         */

        template
        <
            typename T,
            typename IMPL = detail::OptionImpl< T >
        >
        class Option : public OptionBase
        {
        public:

            typedef T                                                   value_type;
            typedef typename cpp::TypeTraits< T >::prvalue              prvalue_type;

        protected:

            value_type                                                  m_value;

            /**
             * default value (if provided), otherwise it will default to
             * the value_type's default value (0, false, "", ...)
             */

            value_type                                                  m_defaultValue;

        public:

            Option(
                SAA_inout   std::string&&       name,
                SAA_inout   std::string&&       description,
                SAA_in_opt  OptionFlags         flags = None
                )
                :
                OptionBase( BL_PARAM_FWD( name ), BL_PARAM_FWD( description ), flags, IMPL::isForceDefault ),
                m_value(),
                m_defaultValue()
            {
            }

            Option(
                SAA_inout   std::string&&       name,
                SAA_inout   std::string&&       description,
                SAA_in      prvalue_type        defaultValue,
                SAA_in_opt  const OptionFlags   flags = None
                )
                :
                OptionBase( BL_PARAM_FWD( name ), BL_PARAM_FWD( description ), flags, true /* has default */ ),
                m_value( defaultValue ),
                m_defaultValue( defaultValue )
            {
            }

            auto getValue() const NOEXCEPT -> prvalue_type
            {
                return m_hasValue ? m_value : m_defaultValue;
            }

            auto getValue( SAA_inout value_type&& defaultValue ) const NOEXCEPT -> value_type
            {
                return m_hasValue ? m_value : BL_PARAM_FWD( defaultValue );
            }

            auto getDefaultValue() const NOEXCEPT -> prvalue_type
            {
                return m_defaultValue;
            }

            /**
             * @return true of the option value is readable (either the explicit or
             * the default values are available)
             */

            bool isReadable() const NOEXCEPT
            {
                return m_hasValue || m_hasDefault;
            }

            virtual const po::value_semantic* getSemantic() OVERRIDE
            {
                /*
                 * NOTE: The parser holds a raw pointer to the value until it is destructed,
                 * therefore Option<> cannot be copied/moved anywhere because that would
                 * invalidate the pointer and cause memory corruption.
                 */

                auto* semantic = detail::Semantics< value_type >::create( *this, &m_value );

                /*
                 * Get notified when the option value is set (either explicitly or with the default
                 * value but we can't tell which so we intentionally don't provide the default below)
                 */

                semantic = semantic -> notifier(
                    [ this ] ( const value_type& /* value */ )
                    {
                        m_hasValue = true;
                    }
                    );

                if( isMultiValue() )
                {
                    semantic = semantic -> multitoken();
                }

                return IMPL::decorateSemantic( semantic );
            }

            virtual void toStream( SAA_inout std::ostream& str ) const OVERRIDE
            {
                toStream( str, getValue() );
            }

        private:

            template
            <
                typename V
            >
            static void toStream(
                SAA_inout   std::ostream&               str,
                SAA_in      const V&                    value
                )
            {
                str << value;
            }

            template
            <
                typename V
            >
            static void toStream(
                SAA_inout   std::ostream&               str,
                SAA_in      const std::vector< V >&     values
                )
            {
                bool delimiter = false;

                for( const auto& value : values )
                {
                    if( delimiter )
                    {
                        str << ", ";
                    }

                    toStream( str, value );

                    delimiter = true;
                }
            }
        };

        namespace detail
        {
            /**
             * @brief Option semantics with min/max argument count overrides.
             */

            template
            <
                typename T
            >
            class Semantics : public po::typed_value< T >
            {
                int                                                 m_valueCount;

            protected:

                Semantics(
                    SAA_in_opt  T*      valueStore = nullptr,
                    SAA_in_opt  int     valueCount = -1
                    )
                    :
                    semantic_t( valueStore ),
                    m_valueCount( valueCount )
                {
                }

            public:

                typedef po::typed_value< T >                        semantic_t;

                virtual unsigned min_tokens() const OVERRIDE
                {
                    return m_valueCount >= 0 ? m_valueCount : semantic_t::min_tokens();
                }

                virtual unsigned max_tokens() const OVERRIDE
                {
                    return m_valueCount >= 0 ? m_valueCount : semantic_t::max_tokens();
                }

                static semantic_t* create(
                    SAA_in      const OptionBase&   option,
                    SAA_in_opt  T*                  valueStore = nullptr
                    )
                {
                    const auto valueCount = option.getValueCount();

                    if( valueCount > 1 )
                    {
                        return new Semantics< T >( valueStore, valueCount );
                    }

                    return new semantic_t( valueStore );
                }
            };

            /**
             * @brief Type-specific option implementation details.
             */

            template
            <
                typename T,
                bool ForceDefault
            >
            struct OptionImpl
            {
                typedef po::typed_value< T >                        semantic_t;

                static const bool                                   isForceDefault = ForceDefault;

                static const po::value_semantic* decorateSemantic( SAA_inout semantic_t* semantic )
                {
                    return semantic;
                }
            };

            template
            <
                typename T,
                bool ForceDefault
            >
            struct SwitchImpl
            {
                typedef po::typed_value< T >                        semantic_t;

                static const bool                                   isForceDefault = ForceDefault;

                static const po::value_semantic* decorateSemantic( SAA_inout semantic_t* semantic )
                {
                    /*
                     * Switches are value-less
                     */

                    return semantic -> zero_tokens();
                }
            };

        } // detail

        /*
         * Predefined option types (more value types should be possible)
         */

        typedef Option< std::string >                           StringOption;
        typedef Option< int >                                   IntOption;
        typedef Option< long >                                  LongOption;
        typedef Option< short >                                 ShortOption;
        typedef Option< unsigned int >                          UIntOption;
        typedef Option< unsigned long >                         ULongOption;
        typedef Option< unsigned short >                        UShortOption;
        typedef Option< float >                                 FloatOption;
        typedef Option< double >                                DoubleOption;
        typedef Option< char >                                  CharOption;
        typedef Option< std::vector< std::string > >            MultiStringOption;
        typedef Option< std::vector< int > >                    MultiIntOption;

        /**
         * The bool option accepts several case-insensitive values on the command line:
         * - "1", "on", "yes", "true" meaning True
         * - "0", "off", "no", "false" meaning False
         * - if the option is missing, it defaults to False
         */

        typedef Option< bool, detail::OptionImpl< bool, true > >  BoolOption;

        /**
         * The switch does not accept any value on the command line.
         * If it is missing on the command line, its value is False. If it is present, its value is True.
         */

        typedef Option< bool, detail::SwitchImpl< bool, true > >  BoolSwitch;

        typedef Option< std::vector< std::string >, detail::SwitchImpl< std::vector< std::string >, true > >  BoolSwitchOrMultiStringOption;

        /**
         * @brief Declare a class encapsulating a particular command line option
         * and initialize it with the specified arguments
         */

#define BL_CMDLINE_OPTION_CLASS( className, baseClass, ... ) \
        template \
        < \
            typename E1 = void \
        > \
        class BL_CONCATENATE( className, T ) FINAL : public ::bl::cmdline::baseClass \
        { \
        public: \
            BL_CONCATENATE( className, T )() \
                : \
                ::bl::cmdline::baseClass( __VA_ARGS__ ) \
            { \
            } \
        }; \
        typedef BL_CONCATENATE( className, T )<> className;

#define BL_CMDLINE_OPTION_CLASS_TEMP_NAME( instanceName, baseType ) \
        BL_CONCATENATE( baseType, BL_CONCATENATE( _, instanceName ) )

        /**
         * @brief Declare a command line option class and its instance at once
         *
         * The option instance class name is auto-generated
         */

#define BL_CMDLINE_OPTION( instanceName, baseClass, ... ) \
        BL_CMDLINE_OPTION_CLASS( BL_CMDLINE_OPTION_CLASS_TEMP_NAME( instanceName, baseClass ), baseClass, __VA_ARGS__ ) \
        BL_CMDLINE_OPTION_CLASS_TEMP_NAME( instanceName, baseClass )    instanceName;

        /*
         * Common reusable command line options
         */

        BL_CMDLINE_OPTION_CLASS( HelpSwitch,     BoolSwitch,    "help,?",       "Show the available arguments", bl::cmdline::Override )
        BL_CMDLINE_OPTION_CLASS( VersionSwitch,  BoolSwitch,    "version",      "Display program version", bl::cmdline::Override )
        BL_CMDLINE_OPTION_CLASS( VerboseSwitch,  BoolSwitch,    "verbose,v",    "Run in most verbose mode" )
        BL_CMDLINE_OPTION_CLASS( QuietSwitch,    BoolSwitch,    "quiet,q",      "Run in quiet mode" )
        BL_CMDLINE_OPTION_CLASS( DryRunSwitch,   BoolSwitch,    "dryrun,n",     "Run in test mode without making any changes" )
        BL_CMDLINE_OPTION_CLASS( DebugSwitch,    BoolSwitch,    "debug",        "Run in debugging mode", bl::cmdline::Hidden )
        BL_CMDLINE_OPTION_CLASS( TraceSwitch,    BoolSwitch,    "trace",        "Run in tracing mode", bl::cmdline::Hidden )

    } // cmdline

} // bl

#endif /* __BL_CMDLINE_OPTION_H_ */
