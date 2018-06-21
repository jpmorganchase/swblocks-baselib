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

#include <baselib/jni/JavaBridgeRestHelper.h>
#include <baselib/jni/JvmHelpers.h>

#include <baselib/jni/JavaVirtualMachine.h>
#include <baselib/jni/JniEnvironment.h>
#include <baselib/jni/JniResourceWrappers.h>
#include <baselib/jni/DirectByteBuffer.h>
#include <baselib/jni/JavaBridge.h>

#include <baselib/core/FsUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <utests/baselib/Utf.h>
#include <utests/baselib/UtfArgsParser.h>

namespace
{
    using namespace bl;
    using namespace bl::jni;

    fs::path getJavaLibraryPath()
    {
        return fs::normalize(
            fs::path( os::getCurrentExecutablePath() ) / ".." / "utf-baselib-jni-lib"
            );
    }

    std::string getJavaClassPath()
    {
        const auto libsPath = getJavaLibraryPath();
        const auto jarPath = libsPath / "utf_baselib_jni.jar";

        BL_CHK_T(
            false,
            fs::exists( jarPath ),
            JavaException(),
            BL_MSG()
                << "Failed to locate jar file "
                << fs::normalizePathParameterForPrint( jarPath )
            );

        return jarPath.string();
    }

    class JniTestGlobalFixture
    {
    public:

        JniTestGlobalFixture()
        {
            JavaVirtualMachineConfig jvmConfig;

            jvmConfig.setClassPath( getJavaClassPath() );
            jvmConfig.setThreadStackSize( "128M" );
            jvmConfig.setInitialHeapSize( "64M" );
            jvmConfig.setMaximumHeapSize( "512M" );

            jvmConfig.setCheckJni( true );
            jvmConfig.setVerboseJni( false );
            jvmConfig.setPrintGCDetails( false );
            jvmConfig.setTraceClassLoading( false );
            jvmConfig.setTraceClassUnloading( false );

            if( test::UtfArgsParser::debugPort() )
            {
                jvmConfig.setDebugPort( std::to_string( test::UtfArgsParser::debugPort() ) );
            }

            JavaVirtualMachine::setConfig( std::move( jvmConfig ) );

            ( void ) JavaVirtualMachine::instance();
        }

        ~JniTestGlobalFixture() NOEXCEPT
        {
            BL_NOEXCEPT_BEGIN()

            JavaVirtualMachine::destroy();

            BL_NOEXCEPT_END()
        }
    };

    template
    <
        typename T
    >
    void fastRequireEqual( const T& t1, const T& t2 )
    {
        if( t1 == t2 )
        {
            return;
        }

        UTF_REQUIRE_EQUAL( t1, t2 );
    }

} // __unnamed

UTF_GLOBAL_FIXTURE( JniTestGlobalFixture )

UTF_AUTO_TEST_CASE( Jni_CreateJniEnvironments )
{
    using namespace bl;
    using namespace bl::jni;

    const auto createJniEnvironment = []( SAA_in const bool detachJniEnvAfterTest )
    {
        const auto& environment = JniEnvironment::instance();
        UTF_REQUIRE_EQUAL( environment.getVersion(), JNI_VERSION_1_8 );

        if( detachJniEnvAfterTest )
        {
            JniEnvironment::detach();
        }
    };

    createJniEnvironment( false /* detachJniEnvAfterTest */ );

    const int numThreads = 10;

    os::thread threads[ numThreads ];

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i] = os::thread( createJniEnvironment, i % 2 == 0 /* detachJniEnvAfterTest */ );
    }

    for( int i = 0; i < numThreads; ++i )
    {
        threads[i].join();
    }
}

UTF_AUTO_TEST_CASE( Jni_LocalGlobalReferences )
{
    using namespace bl;
    using namespace bl::jni;

    const auto& environment = JniEnvironment::instance();

    const auto localReference = environment.findJavaClass( "java/lang/String" );
    UTF_REQUIRE( localReference.get() != nullptr );

    const auto globalReference = environment.createGlobalReference< jclass >( localReference );
    UTF_REQUIRE( globalReference.get() != nullptr );

    {
        /*
         * Verify local and global references in main and non main threads.
         */

        const auto verifyReferences = [ &localReference, &globalReference ]( SAA_in const bool isMainThread )
        {
            const auto& environment = JniEnvironment::instance();
            JNIEnv* jniEnv = environment.getRawPtr();

            if( isMainThread )
            {
                UTF_REQUIRE_EQUAL( jniEnv -> GetObjectRefType( localReference.get() ), JNILocalRefType );
            }

            UTF_REQUIRE_EQUAL( jniEnv -> GetObjectRefType( globalReference.get() ), JNIGlobalRefType );
        };

        verifyReferences( true /* isMainThread */ );

        os::thread thread( verifyReferences, false /* isMainThread */ );
        thread.join();
    }
}

UTF_AUTO_TEST_CASE( Jni_JavaExceptions )
{
    using namespace bl;
    using namespace bl::jni;

    const auto& environment = JniEnvironment::instance();

    UTF_CHECK_EXCEPTION(
        ( void ) environment.findJavaClass( "no/such/class" ),
        JavaException,
        []( SAA_in const JavaException& e ) -> bool
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "Expected exception:\n"
                    << eh::diagnostic_information( e )
                );

            if( e.what() != std::string( "no/such/class" ) )
            {
                return false;
            }

            const auto* hintPtr = eh::get_error_info< eh::errinfo_hint >( e );

            if( ! hintPtr || *hintPtr != "Java class 'no/such/class' not found" )
            {
                return false;
            }

            const auto* typePtr = eh::get_error_info< eh::errinfo_original_type >( e );

            if( ! typePtr || *typePtr != "java.lang.NoClassDefFoundError" )
            {
                return false;
            }

            const auto* threadPtr = eh::get_error_info< eh::errinfo_original_thread_name >( e );

            if( ! threadPtr || *threadPtr != "Thread-1" )
            {
                return false;
            }

            const auto* stringPtr = eh::get_error_info< eh::errinfo_string_value >( e );

            if( ! stringPtr || *stringPtr != "java.lang.NoClassDefFoundError: no/such/class" )
            {
                return false;
            }

            const auto* stackPtr = eh::get_error_info< eh::errinfo_original_stack_trace >( e );

            if( ! stackPtr || ! cpp::contains( *stackPtr, "java.lang.ClassNotFoundException" ) )
            {
                return false;
            }

            return true;
        }
        );

    const auto threadClass = environment.findJavaClass( "java/lang/Thread" );

    const auto threadGetName = environment.getMethodID( threadClass.get(), "getName", "()Ljava/lang/String;" );
    UTF_REQUIRE( threadGetName != nullptr );

    UTF_CHECK_EXCEPTION(
        environment.getMethodID( threadClass.get(), "foo", "()Ljava/lang/String;" ),
        JavaException,
        []( SAA_in const JavaException& e ) -> bool
        {
            const auto* hintPtr = eh::get_error_info< eh::errinfo_hint >( e );

            if( ! hintPtr || *hintPtr != "Method 'foo' with signature '()Ljava/lang/String;' not found in class 'java.lang.Thread'" )
            {
                return false;
            }

            return true;
        }
        );

    const auto threadCurrentThread = environment.getStaticMethodID( threadClass.get(), "currentThread", "()Ljava/lang/Thread;" );
    UTF_REQUIRE( threadCurrentThread != nullptr );

    UTF_CHECK_EXCEPTION(
        environment.getStaticMethodID( threadClass.get(), "foo", "()Ljava/lang/Thread;" ),
        JavaException,
        []( SAA_in const JavaException& e ) -> bool
        {
            const auto* hintPtr = eh::get_error_info< eh::errinfo_hint >( e );

            if( ! hintPtr || *hintPtr != "Static method 'foo' with signature '()Ljava/lang/Thread;' not found in class 'java.lang.Thread'" )
            {
                return false;
            }

            return true;
        }
        );
}

UTF_AUTO_TEST_CASE( Jni_JavaBridge )
{
    using namespace bl;
    using namespace bl::jni;

    enum TestCase : std::int32_t
    {
        PerfTest = 0,
        ObjectInstanceTest = 1
    };

    const std::string javaBridgeClassName           = "org/swblocks/baselib/test/JavaBridge";
    const std::string javaBridgeSingletonClassName  = "org/swblocks/baselib/test/JavaBridgeSingleton";

    /*
     * Object instance test
     */

    const auto objectInstanceTest = [](
        SAA_in  const std::string&          javaClassName,
        SAA_in  const int32_t               expectedIndex
        )
    {
        const JavaBridge javaBridge( javaClassName );

        const std::size_t bufferSize = 128U;

        const DirectByteBuffer inDirectByteBuffer( bufferSize );
        const DirectByteBuffer outDirectByteBuffer( bufferSize );

        inDirectByteBuffer.prepareForWrite();
        inDirectByteBuffer.getBuffer() -> write( TestCase::ObjectInstanceTest );

        javaBridge.dispatch( inDirectByteBuffer, outDirectByteBuffer );

        const auto& outBuffer = outDirectByteBuffer.getBuffer();

        std::string outClassName;
        outBuffer -> read( &outClassName );
        UTF_REQUIRE_EQUAL( str::replace_all_copy( javaClassName, "/", "." ), outClassName );

        std::int32_t objectIndex;
        outBuffer -> read( &objectIndex );
        UTF_REQUIRE_EQUAL( objectIndex, expectedIndex );

        UTF_REQUIRE_EQUAL( outBuffer -> offset1(), outBuffer -> size() );
    };

    for( std::int32_t index = 0; index < 10; ++index )
    {
        objectInstanceTest( javaBridgeClassName, index );
        objectInstanceTest( javaBridgeSingletonClassName, 0 /* expectedIndex */ );
    }

    /*
     * Performance test
     */

    const auto perfTest = [](
        SAA_in  const std::string&          javaClassName,
        SAA_in  const int                   count
        )
    {
        const JavaBridge javaBridge( javaClassName );

        const std::size_t bufferSize = 64U;

        const DirectByteBuffer inDirectByteBuffer( bufferSize );
        const DirectByteBuffer outDirectByteBuffer( bufferSize );

        for( int i = 0; i < count; ++i )
        {
            /*
             * Write into input buffer
             */

            inDirectByteBuffer.prepareForWrite();

            const auto& inBuffer = inDirectByteBuffer.getBuffer();

            inBuffer -> write( TestCase::PerfTest );

            inBuffer -> write( std::int8_t( 123 ) );
            inBuffer -> write( std::int16_t( 12345 ) );
            inBuffer -> write( std::int32_t( 123456 ) );
            inBuffer -> write( std::int64_t( 12345678L ) );

            std::string inString = "the string " + std::to_string( i );
            inBuffer -> write( inString );

            /*
             * Call JavaBridge and read from output buffer
             */

            javaBridge.dispatch( inDirectByteBuffer, outDirectByteBuffer );

            const auto& outBuffer = outDirectByteBuffer.getBuffer();
            fastRequireEqual( inBuffer -> size() - sizeof( TestCase ),  outBuffer -> size() );

            std::int8_t int8;
            outBuffer -> read( &int8 );
            fastRequireEqual( int8, static_cast< std::int8_t >( 123 ) );

            std::int16_t int16;
            outBuffer -> read( &int16 );
            fastRequireEqual( int16, static_cast< std::int16_t >( 12345 ) );

            std::int32_t int32;
            outBuffer -> read( &int32 );
            fastRequireEqual( int32, static_cast< std::int32_t >( 123456 ) );

            std::int64_t int64;
            outBuffer -> read( &int64 );
            fastRequireEqual( int64, static_cast< std::int64_t >( 12345678L ) );

            std::string outString;
            outBuffer -> read( &outString );
            fastRequireEqual( outString, str::to_upper_copy( inString ) );

            fastRequireEqual( outBuffer -> offset1(), outBuffer -> size() );
        }
    };

    const auto runPerfTest = [ &perfTest ]( SAA_in const std::string& javaClassName )
    {
        const auto now = time::microsec_clock::universal_time();

        perfTest( javaClassName, 50000 /* count */ );

        const auto elapsed = time::microsec_clock::universal_time() - now;

        BL_LOG(
            Logging::debug(),
            BL_MSG()
                << "JavaBridge dispatch test time: "
                << elapsed
                << " ["
                << javaClassName
                << "]"
            );

         UTF_REQUIRE( elapsed < time::milliseconds( 5000 ) );
    };

    runPerfTest( javaBridgeClassName );
    runPerfTest( javaBridgeSingletonClassName );

    {
        /*
         * Test that exception in Java code is converted to C++ JavaException.
         */

        const int invalidTestCase = -1;

        const JavaBridge javaBridge( javaBridgeClassName );

        const std::size_t bufferSize = 128U;

        const DirectByteBuffer inDirectByteBuffer( bufferSize );
        const DirectByteBuffer outDirectByteBuffer( bufferSize );

        inDirectByteBuffer.prepareForWrite();
        inDirectByteBuffer.getBuffer() -> write( invalidTestCase );

        UTF_CHECK_THROW_MESSAGE(
            javaBridge.dispatch( inDirectByteBuffer, outDirectByteBuffer ),
            JavaException,
            "Invalid test case: " + std::to_string( invalidTestCase )
            );
    };
}

UTF_AUTO_TEST_CASE( Jni_JavaBridgeCallback )
{
    std::vector< std::string > words;

    const auto callback = [ &words ](
        SAA_in const bl::jni::DirectByteBuffer& cbInDirectByteBuffer,
        SAA_in const bl::jni::DirectByteBuffer& cbOutDirectByteBuffer
        ) -> void
    {
        const auto& inBuffer = cbInDirectByteBuffer.getBuffer();

        std::string word;
        inBuffer -> read( &word );
        UTF_REQUIRE_EQUAL( inBuffer -> offset1(), inBuffer -> size() );

        if( word == "STD::RUNTIME_ERROR" )
        {
            throw std::runtime_error( "Exception from JNI native callback" );
        }

        words.push_back(word);

        const std::string word2 = word + word;
        const auto& outBuffer = cbOutDirectByteBuffer.getBuffer();
        outBuffer -> write( word2 );
    };

    const std::string javaClassName = "org/swblocks/baselib/test/JavaBridgeCallback";
    const std::string javaClassNativeCallbackName = "nativeCallback";

    const JavaBridge javaBridge( javaClassName, javaClassNativeCallbackName, callback );

    const std::size_t bufferSize = 128U;

    const DirectByteBuffer inDirectByteBuffer( bufferSize );
    const DirectByteBuffer outDirectByteBuffer( bufferSize );

    inDirectByteBuffer.prepareForWrite();

    std::string inString = "This string is passed to Java and returned back to C++ in sync callback one word at a time";
    inDirectByteBuffer.getBuffer() -> write( inString );

    javaBridge.dispatch( inDirectByteBuffer, outDirectByteBuffer );

    const std::string outString = bl::str::join( words, " " );
    UTF_REQUIRE_EQUAL( outString, str::to_upper_copy( inString ) );

    const auto& outBuffer = outDirectByteBuffer.getBuffer();

    std::string doneString;
    outBuffer -> read( &doneString );
    UTF_REQUIRE_EQUAL( doneString, "Done" );

    {
        /*
         * Repeat the test and pass the callback directly in the dispatch call.
         */

        words.clear();

        const JavaBridge javaBridge( javaClassName, javaClassNativeCallbackName );

        inDirectByteBuffer.prepareForWrite();
        inDirectByteBuffer.getBuffer() -> write( inString );

        javaBridge.dispatch( inDirectByteBuffer, outDirectByteBuffer, callback );

        const std::string outString = bl::str::join( words, " " );
        UTF_REQUIRE_EQUAL( outString, str::to_upper_copy( inString ) );

        const auto& outBuffer = outDirectByteBuffer.getBuffer();

        std::string doneString;
        outBuffer -> read( &doneString );
        UTF_REQUIRE_EQUAL( doneString, "Done" );
    }

    {
        /*
         * Throw std::runtime_error inside native callback.
         */

        words.clear();

        const std::string javaClassName = "org/swblocks/baselib/test/JavaBridgeCallbackException";
        const JavaBridge javaBridge( javaClassName, javaClassNativeCallbackName );

        std::string inString = "Throw C++ exception 2 times - std::runtime_error std::runtime_error";
        inDirectByteBuffer.prepareForWrite();
        inDirectByteBuffer.getBuffer() -> write( inString );

        javaBridge.dispatch( inDirectByteBuffer, outDirectByteBuffer, callback );

        const std::string outString = bl::str::join( words, " " );
        UTF_REQUIRE_EQUAL( outString, str::to_upper_copy( bl::str::replace_all_copy( inString, " std::runtime_error", "" ) ) );

        const auto& outBuffer = outDirectByteBuffer.getBuffer();

        std::string doneString;
        outBuffer -> read( &doneString );
        UTF_REQUIRE_EQUAL( doneString, "Done" );
    }
}

UTF_AUTO_TEST_CASE( Jni_JavaBridgeRestHelper )
{
    using namespace bl;

    bool nativeCallbackCalled = false;

    const auto nativeCallback = [ &nativeCallbackCalled ](
        SAA_in      const jni::DirectByteBuffer&            input,
        SAA_out     jni::DirectByteBuffer&                  output
        )
    {
        BL_UNUSED( output );

        const auto& buffer = input.getBuffer();

        std::string jsonPayload;
        std::string jsonContext;

        buffer -> read( &jsonPayload );

        if( buffer -> offset1() < buffer -> size() )
        {
            /*
             * The context object is optional - e.g. the shutdown
             * command does not provide context object
             */

            buffer -> read( &jsonContext );
        }

        BL_LOG_MULTILINE(
            Logging::debug(),
            BL_MSG()
                << "Output size: "
                << buffer -> size()
                << "\nPayload:\n"
                << json::saveToString( json::readFromString( jsonPayload ), true /* prettyPrint */ )
            );

        if( ! jsonContext.empty() )
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\nContext:\n"
                    << json::saveToString( json::readFromString( jsonContext ), true /* prettyPrint */ )
                );
        }

        nativeCallbackCalled = true;
    };

    const auto restServerClassName = "org/swblocks/baselib/test/JavaBridgeRestTestServer";
    const auto restServerNativeCallbackName = "nativeCallback";

    const auto engine = jni::JavaBridgeRestHelper::createInstance(
        nativeCallback,
        restServerClassName,
        restServerNativeCallbackName
        );

    {
        BL_SCOPE_EXIT( engine -> shutdown(); );

        const auto executeCallback = [ & ]() -> void
        {
            const std::string payloadJson = "{ \"data\" : { }  }";

            utils::ExecutionTimer timer(
                "JavaBridgeRestHelper::execute: " + payloadJson,
                Logging::debug()
                );

            const auto payload = dm::DataModelUtils::loadFromJsonText< dm::Payload >( payloadJson );

            const auto context = dm::FunctionContext::createInstance();
            context -> securityPrincipalLvalue() = dm::messaging::SecurityPrincipal::createInstance();

            context -> securityPrincipal() -> sid( "sid1234" );
            context -> securityPrincipal() -> givenName( "First" );
            context -> securityPrincipal() -> familyName( "Last" );
            context -> securityPrincipal() -> email( "user@host.com" );

            const auto output = data::DataBlock::createInstance( 1024 * 1024 /* capacity 1 MB */ );

            engine -> execute(
                context,
                dm::DataModelUtils::getDocAsPackedJsonString( payload ),
                output
                );

            const auto size = output -> size();

            std::string jsonPayload;
            std::string jsonContext;

            output -> read( &jsonPayload );
            output -> read( &jsonContext );

            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "Output size: "
                    << size
                    << "\nPayload:\n"
                    << json::saveToString( json::readFromString( jsonPayload ), true /* prettyPrint */ )
                    << "\nContext:\n"
                    << json::saveToString( json::readFromString( jsonContext ), true /* prettyPrint */ )
                );
        };

        UTF_REQUIRE( ! nativeCallbackCalled );
        executeCallback();
        UTF_REQUIRE( nativeCallbackCalled );
    }
}

UTF_AUTO_TEST_CASE( Jni_JvmHelpers )
{
    using namespace bl;

    fs::TmpDir tmpDir;

    const auto& rootDir = tmpDir.path();

    const auto libsDir = rootDir / "lib";

    const auto mainLibName = "foo.jar";
    const auto depLibName1 = "dep1.jar";
    const auto depLibName2 = "dep2.jar";

    encoding::writeTextFile( rootDir / mainLibName, "test-content" );
    encoding::writeTextFile( libsDir / depLibName1, "test-content" );
    encoding::writeTextFile( libsDir / depLibName2, "test-content" );

    const auto classPath = jni::JvmHelpers::buildClassPath( rootDir, mainLibName );

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Class path: "
            << str::quoteString( classPath )
        );

    const std::string pathVarSeparator( 1U /* count */, os::pathVarSeparator );

    const auto list = str::splitString( classPath, pathVarSeparator );

    UTF_REQUIRE_EQUAL( list.size(), 3U );

    /*
     * The main library / JAR should always be first
     */

    UTF_REQUIRE_EQUAL( list[ 0 ], fs::normalizePathCliParameter( ( rootDir / mainLibName ).string() ) );

    /*
     * Just require that both dependencies are on the path, but don't
     * require specific order
     */

    std::unordered_set< std::string > deps;

    deps.emplace( list[ 1 ] );
    deps.emplace( list[ 2 ] );

    UTF_REQUIRE( deps.find( fs::normalizePathCliParameter( ( libsDir / depLibName1 ).string() ) ) != deps.end() );
    UTF_REQUIRE( deps.find( fs::normalizePathCliParameter( ( libsDir / depLibName2 ).string() ) ) != deps.end() );
}
