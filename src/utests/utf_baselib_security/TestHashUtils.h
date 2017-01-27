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

#include <baselib/crypto/HashCalculator.h>

#include <baselib/core/BaseIncludes.h>

#include <baselib/data/DataBlock.h>

#include <utests/baselib/UtfArgsParser.h>
#include <utests/baselib/Utf.h>

/************************************************************************
 * XmlUtils library code tests
 */

UTF_AUTO_TEST_CASE( HashUtils_Test512 )
{
    using namespace bl;
    using namespace bl::hash;

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\n******************************** Starting test: HashUtils_Test512 ********************************\n"
        );

    const std::string testString = "The quick brown fox jumps over the lazy dog";
    const std::string known512Digest = "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6";

    HashCalculator< bl::hash::detail::Sha512CalculatorImpl > hashCalculator;
    hashCalculator.update( testString.c_str(), testString.size() );
    hashCalculator.finalize();
    const auto digestStr = hashCalculator.digestStr();

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "\nHashUtils_Test512 Digest='"
            << digestStr
            << "'\nExpected Digest value = '"
            << known512Digest
            << "'"
        );

    UTF_REQUIRE_EQUAL( digestStr, known512Digest );
}

UTF_AUTO_TEST_CASE( HashUtils_Test512Update )
{
    using namespace bl;
    using namespace bl::hash;

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\n******************************** Starting test: HashUtils_Test512Update ********************************\n"
        );

    const std::string testString = "The quick brown fox ";
    const std::string testString2 = "jumps over the lazy dog";
    const std::string known512Digest = "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6";

    HashCalculator< bl::hash::detail::Sha512CalculatorImpl > hashCalculator;
    hashCalculator.update( testString.c_str(), testString.size() );
    hashCalculator.update( testString2.c_str(), testString2.size() );
    hashCalculator.finalize();
    const auto digestStr = hashCalculator.digestStr();

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "\nHashUtils_Test512 Digest='"
            << digestStr
            << "'\nExpected Digest value = '"
            << known512Digest
            << "'"
        );

    UTF_REQUIRE_EQUAL( digestStr, known512Digest );
}

UTF_AUTO_TEST_CASE( HashUtils_Test384 )
{
    using namespace bl;
    using namespace bl::hash;

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\n******************************** Starting test: HashUtils_Test384 ********************************\n"
        );

    const std::string testString = "The quick brown fox jumps over the lazy dog";
    const std::string known384Digest = "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a509cb1e5dc1e85a941bbee3d7f2afbc9b1";

    HashCalculator< bl::hash::detail::Sha384CalculatorImpl > hashCalculator;
    hashCalculator.update( testString.c_str(), testString.size() );
    hashCalculator.finalize();
    const auto digestStr = hashCalculator.digestStr();

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "\nHashUtils_Test384 Digest='"
            << digestStr
            << "'\nExpected Digest value = '"
            << known384Digest
            << "'"
        );

    UTF_REQUIRE_EQUAL( digestStr, known384Digest );
}

UTF_AUTO_TEST_CASE( HashUtils_TestPerformance )
{
    using namespace bl;
    using namespace bl::hash;

    BL_LOG_MULTILINE(
        Logging::debug(),
        BL_MSG()
            << "\n******************************** Starting test: HashUtils_TestPerformance ********************************\n"
        );

    auto dataBlock = bl::data::DataBlock::createInstance();

    dataBlock -> setSize( dataBlock -> capacity() );

    const auto size = dataBlock -> size();
    const auto& data = dataBlock -> begin();

    for( std::size_t i = 0; i < size; ++i )
    {
        data[ i ] = i % 128;
    }

    auto startTime = bl::time::microsec_clock::universal_time();

    for( std::size_t counter = 0; counter < 100; ++counter )
    {
        HashCalculator< bl::hash::detail::Sha512CalculatorImpl > hashCalculator;
        hashCalculator.update( dataBlock -> pv(), dataBlock -> size() );
        hashCalculator.finalize();
        const auto digestStr = hashCalculator.digestStr();
    }

    auto stopTime = bl::time::microsec_clock::universal_time();
    auto duration = stopTime - startTime;

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "Average time taken to calculate SHA512 digest was "
            << duration.total_microseconds()
            << " microseconds"
        );

    startTime = bl::time::microsec_clock::universal_time();

    for( std::size_t counter = 0; counter < 100; ++counter )
    {
        HashCalculator< bl::hash::detail::Sha384CalculatorImpl > hashCalculator;
        hashCalculator.update( dataBlock -> pv(), dataBlock -> size() );
        hashCalculator.finalize();
        const auto digestStr = hashCalculator.digestStr();
    }

    stopTime = bl::time::microsec_clock::universal_time();
    duration = stopTime - startTime;

    BL_LOG(
        bl::Logging::debug(),
        BL_MSG()
            << "Average time taken to calculate SHA384 digest was "
            << duration.total_microseconds()
            << " microseconds"
        );
}

UTF_AUTO_TEST_CASE( HashUtils_HashSet )
{
    using namespace bl;
    using namespace bl::hash;

    std::set< std::string > inputs;

    const auto emptyHash = bl::hash::HashCalculatorDefault::hash( inputs );

    UTF_REQUIRE( ! emptyHash.empty() );

    inputs.emplace( "one" );

    const auto oneHash = bl::hash::HashCalculatorDefault::hash( inputs );

    UTF_REQUIRE( emptyHash != oneHash );

    inputs.emplace( "two" );

    const auto twoHash = bl::hash::HashCalculatorDefault::hash( inputs );

    UTF_REQUIRE( twoHash != oneHash );

    inputs.clear();
    inputs.emplace( "two" );
    inputs.emplace( "one" );

    UTF_REQUIRE_EQUAL( twoHash, bl::hash::HashCalculatorDefault::hash( inputs ) );
}
