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

#include <utests/baselib/UtfBaseLibCommon.h>
#include <utests/baselib/TestUtils.h>

#include <baselib/parsing/SpiritLex.h>

namespace utest
{
    namespace lex
    {
        enum
        {
            ID_WORD = 1000,
            ID_EOL,
            ID_CHAR,
        };

        template
        <
            typename Lexer = bl::lex::lexertl::lexer<>
        >
        struct WordsLexerT : bl::lex::lexer< Lexer >
        {
            WordsLexerT()
            {
                /*
                 * define tokens (the regular expression to match and the corresponding
                 * token id) and add them to the lexer
                 *
                 * ID_WORD: words (anything except ' ', '\t' or '\n')
                 * ID_EOL: newline characters
                 * ID_CHAR: anything else is a plain character
                 */

                this -> self.add
                    ( "\\w+", ID_WORD )
                    ( "\n", ID_EOL )
                    ( ".", ID_CHAR )
                    ;
            }
        };

        typedef WordsLexerT<> WordsLexer;

        struct TokensVisitor
        {
            /*
             * The function operator gets called for each of the matched tokens
             */

            template
            <
                typename Token
            >
            bool operator()(
                SAA_in          const Token&                                token,
                SAA_inout       std::map< std::string, std::size_t >&       wordsMap
                ) const
            {
                switch( token.id() )
                {
                    default:
                        BL_LOG(
                            bl::Logging::notify(),
                            BL_MSG()
                                << "token id: "
                                << token.id()
                                << "; token value: "
                                << token.value()
                            );

                        break;

                    case ID_WORD:

                        /*
                         * matched a word
                         * since we're using a default token type in this example, every
                         * token instance contains a `iterator_range<BaseIterator>` as its token
                         * attribute pointing to the matched character sequence in the input
                         */

                        {
                            const auto& iterator = token.value();
                            const std::string key( iterator.begin(), iterator.end() );

                            auto& counter = wordsMap[ key ];
                            ++counter;
                        }

                        break;

                    case ID_EOL:
                    case ID_CHAR:

                        /*
                         * ID_EOL:      matched a newline character
                         * ID_CHAR:     matched something else
                         */

                        break;
                }

                /*
                 * always continue to tokenize
                 */

                return true;
            }
        };

    } // lex

} // utest

UTF_AUTO_TEST_CASE( BaseLib_ParsingGetTokens )
{
    using namespace bl;
    using namespace bl::lex;
    using namespace utest::lex;

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    if( test::UtfArgsParser::path().empty() )
    {
        BL_LOG(
            Logging::notify(),
            BL_MSG()
                << "The --path parameter must be provided"
            );

        return;
    }

    const WordsLexer lexer;

    std::map< std::string, std::size_t > wordsMap;

    const auto processFile = [ & ]( SAA_in const fs::path& path ) -> void
    {
        const auto fileText = encoding::readTextFile( path );

        char const* first = fileText.c_str();
        char const* last = &first[ fileText.size() ];

        const bool result = lex::tokenize(
            first,
            last,
            lexer,
            cpp::bind< bool >( TokensVisitor(), _1, cpp::ref( wordsMap ) )
            );

        BL_CHK(
            false,
            result,
            BL_MSG()
                << "Lexical analysis failed"
            );
    };

    const auto processDirectory = [ & ](
        SAA_in          const fs::path&                                 rootDir,
        SAA_in_opt      const std::set< std::string >&                  extensions
        )
        -> void
    {
        for( fs::recursive_directory_iterator end, it( rootDir ) ; it != end; ++it  )
        {
            if( ! fs::is_regular_file( it -> status() ) )
            {
                /*
                 * Skip non-file entries
                 */

                continue;
            }

            if( ! extensions.empty() )
            {
                const auto pathw = it -> path().native();
                const std::string path( pathw.begin(), pathw.end() );

                const auto pos = path.rfind( '.' );
                if( std::string::npos == pos )
                {
                    /*
                     * Skip files without extensions
                     */

                    continue;
                }

                const auto ext = path.substr( pos );
                if( extensions.find( ext ) == extensions.end() )
                {
                    /*
                     * Skip all extensions we don't care about
                     */

                    continue;
                }
            }

            processFile( it -> path() );
        }
    };

    const fs::path inputPath = test::UtfArgsParser::path();

    if( fs::path_exists( inputPath ) && fs::is_directory( inputPath ) )
    {
        processDirectory( inputPath, std::set< std::string >() /* extensions */ );
    }
    else
    {
        processFile( inputPath );
    }

    const auto& outputPath = test::UtfArgsParser::outputPath();

    if( ! outputPath.empty() )
    {
        /*
         * Generate CSV file for analysis
         */

        fs::SafeOutputFileStreamWrapper outputFile( outputPath );
        auto& os = outputFile.stream();

        os
            << "Word,Count"
            << "\n";

        for( const auto& pair : wordsMap )
        {
            os
                << pair.first /* word */
                << ","
                << pair.second /* count */
                << "\n";
        }
    }
    else
    {
        for( const auto& pair : wordsMap )
        {
            BL_LOG(
                Logging::notify(),
                BL_MSG()
                    << pair.first /* word */
                    << " : "
                    << pair.second /* count */
                );
        }
    }
}

