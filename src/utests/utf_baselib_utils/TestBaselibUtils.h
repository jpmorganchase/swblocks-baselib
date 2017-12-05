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

#include <baselib/core/StringTemplateResolver.h>

#include <utests/baselib/Utf.h>

UTF_AUTO_TEST_CASE( StringTemplateTests )
{
    using namespace bl;

    typedef str::StringTemplateResolver                                     resolver_t;
    typedef std::unordered_map< std::string, std::string >                  variables_list_t;

    const auto testResolve = [](
        SAA_in          const variables_list_t&         variables,
        SAA_in          std::string&&                   templateText,
        SAA_in          const std::string&              expectedResolvedText
        )
        -> void
    {
        const auto resolver =
            resolver_t::createInstance( BL_PARAM_FWD( templateText ), true /* skipUndefined */ );

        UTF_REQUIRE_EQUAL( resolver -> resolve( variables ), expectedResolvedText );

        UTF_REQUIRE_EQUAL( resolver -> resolve( variables ), expectedResolvedText );
    };

    const auto testNegativeResolve = [](
        SAA_in          const variables_list_t&         variables,
        SAA_in          std::string&&                   templateText,
        SAA_in          const std::string&              expectedResolvedText,
        SAA_in          const bool                      skipUndefined
        )
        -> void
    {
        const auto resolver =
            resolver_t::createInstance( BL_PARAM_FWD( templateText ), skipUndefined );

        UTF_REQUIRE_EQUAL( resolver -> resolve( variables ), expectedResolvedText );

        UTF_FAIL( "This call is expected to throw" );
    };

    variables_list_t vars;

    testResolve( vars, "" /* templateText */, "" /* expectedResolvedText */ );
    testResolve( vars, "test" /* templateText */, "test" /* expectedResolvedText */ );

    testResolve( vars, "\n" /* templateText */, "\n" /* expectedResolvedText */ );
    testResolve( vars, "\n\n" /* templateText */, "\n\n" /* expectedResolvedText */ );
    testResolve( vars, "\n\n\n" /* templateText */, "\n\n\n" /* expectedResolvedText */ );

    testResolve( vars, "{{}}" /* templateText */, "" /* expectedResolvedText */ );
    testResolve( vars, "{{}}{{}}" /* templateText */, "" /* expectedResolvedText */ );
    testResolve( vars, "{{}}\n{{}}" /* templateText */, "\n" /* expectedResolvedText */ );
    testResolve( vars, "a{{}}\n{{}}b" /* templateText */, "a\nb" /* expectedResolvedText */ );

    testResolve( vars, "test\n\n\n" /* templateText */, "test\n\n\n" /* expectedResolvedText */ );
    testResolve( vars, "\n\n\ntest" /* templateText */, "\n\n\ntest" /* expectedResolvedText */ );
    testResolve( vars, "test\n\n\ntest" /* templateText */, "test\n\n\ntest" /* expectedResolvedText */ );
    testResolve( vars, "test\n\nab\ntest" /* templateText */, "test\n\nab\ntest" /* expectedResolvedText */ );

    vars[ "var1" ] = "value1";
    vars[ "var2" ] = "value2";
    vars[ "var3" ] = "value3";

    /*
     * Test some simple replacement cases first
     */

    testResolve( vars, "{{var1}}" /* templateText */, "value1" /* expectedResolvedText */ );

    testResolve(
        vars,
        "prefix_{{var1}}_suffix"                                        /* templateText */,
        "prefix_value1_suffix"                                          /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{var2}}prefix_{{var1}}_suffix"                                /* templateText */,
        "value2prefix_value1_suffix"                                    /* expectedResolvedText */
        );

    testResolve(
        vars,
        "prefix_{{var1}}_suffix{{var2}}"                                /* templateText */,
        "prefix_value1_suffixvalue2"                                    /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{var2}}prefix_{{var1}}_suffix{{var3}}"                        /* templateText */,
        "value2prefix_value1_suffixvalue3"                              /* expectedResolvedText */
        );

    /*
     * Test more complex cases - e.g. implicit and explicit blocks and undefined variables
     */

    testResolve(
        vars,
        "{{var2}}pr\nefix_{{var1}}_suffix{{var3}}\n"                    /* templateText */,
        "value2pr\nefix_value1_suffixvalue3\n"                          /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{var2}}pr{{}}efix_{{var1}}_suffix{{var3}}\n"                  /* templateText */,
        "value2prefix_value1_suffixvalue3\n"                            /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{undef}}pr{{}}efix_{{var1}}_suffix{{var3}}\n"                 /* templateText */,
        "efix_value1_suffixvalue3\n"                                    /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{var2}}pr{{}}efix_{{undef}}_suffix{{var3}}\n"                 /* templateText */,
        "value2pr"                                                      /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{var2}}pr{{}}efix_{{undef}}_suffix{{var3}}test\n"             /* templateText */,
        "value2pr"                                                      /* expectedResolvedText */
        );

    testResolve(
        vars,
        "{{var2}}pr{{}}efix_{{undef}}_suffix{{var3}}\ntest"             /* templateText */,
        "value2prtest"                                                  /* expectedResolvedText */
        );

    /*
     * Test some negative examples
     */

    UTF_REQUIRE_THROW_MESSAGE(
        testNegativeResolve(
            vars,
            "{{ foo bar"                                                /* templateText */,
            "n/a"                                                       /* expectedResolvedText */,
            true                                                        /* skipUndefined */
            ),
        InvalidDataFormatException,
        "Variable end marker '}}' cannot be found while parsing string template"
        );

    UTF_REQUIRE_THROW_MESSAGE(
        testNegativeResolve(
            vars,
            "{{var1\n}}"                                                /* templateText */,
            "n/a"                                                       /* expectedResolvedText */,
            true                                                        /* skipUndefined */
            ),
        InvalidDataFormatException,
        "Variable end marker '}}' cannot be found while parsing string template"
        );

    UTF_REQUIRE_THROW_MESSAGE(
        testNegativeResolve(
            vars,
            "{{undefinedVar}}"                                          /* templateText */,
            "n/a"                                                       /* expectedResolvedText */,
            false                                                       /* skipUndefined */
            ),
        NotFoundException,
        "Variable 'undefinedVar' is undefined when resolving a string template"
        );
}
