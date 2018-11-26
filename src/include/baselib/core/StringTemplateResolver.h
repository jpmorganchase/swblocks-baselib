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

#ifndef __BL_STRINGTEMPLATERESOLVER_H_
#define __BL_STRINGTEMPLATERESOLVER_H_

#include <baselib/core/StringUtils.h>

#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/BaseIncludes.h>

#include <unordered_map>

namespace bl
{
    namespace str
    {
        /**
         * @brief class StringTemplateResolver - a simple string template resolver class
         *
         * It resolves string templates against a variables map which have the variables
         * defined using {{varName}} syntax
         *
         * The string template is considered as a collection of blocks which are replaced
         * atomically and if some variables cannot be resolved for a particular block the
         * behavior can be to consider it an error (i.e. throw an exception) or to skip
         * the block and not insert it in the resolved output (which behavior is desired
         * can be chosen in the constructor via the 'skipUndefined' parameter and by
         * default it is false which means it will throw an exception - NotFoundException)
         *
         * The main goal for resolving the string template as a sequence of blocks where
         * each block is either included or excluded (if some variables cannot be resolved)
         * is to support flexibility where the template resolution can be done flexibly if
         * not all the input variables are available
         *
         * Blocks in the template string can be defined implicitly or explicitly via
         * 'block separators' and the block is considered to include the block separator
         * at the end of it which marks the beginning of the next block and this is
         * important because some block separators may have text content while others
         * do not. There are two block separators that are currently supported: the
         * first one if the new line character ('\n') and it is considered implicit as
         * it is part of the text - i.e. is is included in the resolved text; the second
         * one is an empty variable name - i.e. {{}} - and this one is considered
         * explicit separator as there is nothing included in the resolved text
         *
         * Examples:
         *
         * Let's assume the input map contains two variables with names 'var1' & 'var2'
         * and they have two values - 'value1' & 'value2' respectively
         *
         * {{var1}} foo{{}}bar {{undefined}} baz{{}}_suffix
         * ->
         * value1 foo_suffix
         *
         * {{var1}} foo\nbar {{undefined}} baz\n_suffix
         * ->
         * value1 foo\n_suffix
         *
         * {{var1}} foo{{}}bar {{var2}} baz{{}}_suffix
         * ->
         * value1 foo_bar value2 baz_suffix
         */

        template
        <
            typename E = void
        >
        class StringTemplateResolverT : public om::ObjectDefaultBase
        {
        protected:

            typedef std::string::size_type                          size_type;

            typedef std::tuple
            <
                std::string                                         /* name */,
                std::string::const_iterator                         /* posBegin */,
                std::string::const_iterator                         /* posEnd */,
                bool                                                /* copyData */
            >
            marker_info_t;

            static const std::string                                g_varBegin;
            static const std::string                                g_varEnd;

            const std::string                                       m_templateText;
            const bool                                              m_skipUndefined;
            const std::vector< marker_info_t >                      m_markers;

            StringTemplateResolverT(
                SAA_in          std::string&&                       templateText,
                SAA_in          const bool                          skipUndefined = false
                )
                :
                m_templateText( BL_PARAM_FWD( templateText ) ),
                m_skipUndefined( skipUndefined ),
                m_markers( parseTemplate( m_templateText ) )
            {
            }

            static auto parseTemplate( SAA_in const std::string& templateText )
                -> std::vector< marker_info_t >
            {
                std::vector< marker_info_t > markers;

                auto blockPosBegin = templateText.cbegin();

                for( ;; )
                {
                    if( blockPosBegin == templateText.cend() )
                    {
                        /*
                         * No more blocks to process
                         */

                        break;
                    }

                    auto blockPosEnd = std::find( blockPosBegin, templateText.cend(), '\n' );

                    auto pos = blockPosBegin;

                    for( ;; )
                    {
                        pos = std::search(
                            pos,
                            blockPosEnd,
                            g_varBegin.cbegin(),
                            g_varBegin.cend()
                            );

                        if( pos == blockPosEnd )
                        {
                            /*
                             * No more variables in this block - insert marker if
                             * this was not the last block and then move to the next block
                             */

                            if( blockPosEnd != templateText.cend() )
                            {
                                markers.emplace_back(
                                    std::make_tuple(
                                        std::string()                               /* name */,
                                        cpp::copy( blockPosEnd )                    /* posBegin */,
                                        blockPosEnd + 1U                            /* posEnd */,
                                        true                                        /* copyData */
                                        )
                                    );

                                blockPosBegin = blockPosEnd + 1U;
                            }
                            else
                            {
                                blockPosBegin = templateText.cend();
                            }

                            break;
                        }

                        const auto namePosBegin = pos + g_varBegin.length();

                        const auto namePosEnd = std::search(
                            namePosBegin,
                            blockPosEnd,
                            g_varEnd.cbegin(),
                            g_varEnd.cend()
                            );

                        BL_CHK_T(
                            false,
                            namePosEnd != blockPosEnd,
                            InvalidDataFormatException()
                                << eh::errinfo_is_user_friendly( true ),
                            BL_MSG()
                                << "Variable end marker '}}' cannot be found while parsing string template"
                            );

                        const auto newPos = namePosEnd + g_varEnd.length();

                        /*
                         * If the variable name is empty string then this is an explicit block marker
                         * and in this case we should request copyData=false
                         */

                        markers.emplace_back(
                            std::make_tuple(
                                std::string( namePosBegin, namePosEnd )             /* name */,
                                cpp::copy( pos )                                    /* posBegin */,
                                cpp::copy( newPos )                                 /* posEnd */,
                                namePosBegin != namePosEnd                          /* copyData */
                                )
                            );

                        pos = newPos;
                    }
                }

                return markers;
            }

        public:

            template
            <
                typename MAP = std::unordered_map< std::string, std::string >
            >
            auto resolve( SAA_in const MAP& variables ) const -> std::string
            {
                cpp::SafeOutputStringStream result;
                cpp::SafeOutputStringStream block;

                const auto addBlock = [ & ]() -> void
                {
                    result << block.str();

                    block.str( str::empty() );
                    block.clear();
                };

                auto pos = m_templateText.cbegin();

                auto markerPos = m_markers.cbegin();

                while( markerPos != m_markers.cend() )
                {
                    const auto& marker = *markerPos;

                    const auto& name = std::get< 0 >( marker );
                    const auto& posBegin = std::get< 1 >( marker );
                    const auto& posEnd = std::get< 2 >( marker );
                    const auto& copyData = std::get< 3 >( marker );

                    /*
                     * First check to copy the text before the marker (if any)
                     */

                    if( pos != posBegin )
                    {
                        block << std::string( pos, posBegin );
                    }

                    if( name.empty() )
                    {
                        /*
                         * Block marker case - check to copy the marker data if necessary and
                         * then flush the block and continue
                         */

                        if( copyData )
                        {
                            block << std::string( posBegin, posEnd );
                        }

                        addBlock();
                    }
                    else
                    {
                        /*
                         * Variable replacement case - check if possible to do the replacement
                         * and if not then skip the block
                         *
                         * If replacement can be done then just continue
                         */

                        const auto posValue = variables.find( name );

                        if( posValue != variables.cend() )
                        {
                            block << posValue -> second;
                        }
                        else
                        {
                            BL_CHK_T(
                                false,
                                m_skipUndefined,
                                NotFoundException()
                                    << eh::errinfo_is_user_friendly( true ),
                                BL_MSG()
                                    << "Variable '"
                                    << name
                                    << "' is undefined when resolving a string template"
                                );

                            /*
                             * A variable in this block can't be resolved - skip the block
                             */

                            block.str( str::empty() );
                            block.clear();

                            /*
                             * First skip the current marker and then search for the next
                             * marker which is block delimiter
                             */

                            while(
                                markerPos != m_markers.cend() &&
                                ! std::get< 0 /* name */ >( *markerPos ).empty()
                                )
                            {
                                ++markerPos;
                            }
                        }
                    }

                    if( markerPos != m_markers.cend() )
                    {
                        pos = std::get< 2 >( *markerPos ) /* posEnd */;

                        ++markerPos;
                    }
                    else
                    {
                        pos = m_templateText.cend();
                    }
                }

                /*
                 * Add the last block (if any) and the last part of the template (if any)
                 */

                addBlock();

                if( pos != m_templateText.cend() )
                {
                    result << std::string( pos, m_templateText.cend() );
                }

                return result.str();
            }
        };

        BL_DEFINE_STATIC_CONST_STRING( StringTemplateResolverT, g_varBegin ) = "{{";
        BL_DEFINE_STATIC_CONST_STRING( StringTemplateResolverT, g_varEnd ) = "}}";

        typedef om::ObjectImpl< StringTemplateResolverT<> > StringTemplateResolver;

    } // str

} // bl

#endif /* __BL_STRINGTEMPLATERESOLVER_H_ */
