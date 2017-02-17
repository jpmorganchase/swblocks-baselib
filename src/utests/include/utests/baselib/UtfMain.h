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

#if !defined(UTF_TEST_MODULE)
#error UTF_TEST_MODULE macro should be defined before this header is included; it should also be included only once
#endif

#include <baselib/crypto/TrustedRoots.h>

#include <baselib/http/SimpleHttpTask.h>

#include <baselib/core/AppInitDone.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/MessageBuffer.h>
#include <baselib/core/Logging.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/OS.h>
#include <baselib/core/BaseIncludes.h>

#include <string>
#include <sstream>

#include <utests/baselib/UtfArgsParser.h>
#include <utests/baselib/UtfCrypto.h>
#include <utests/baselib/Utf.h>

#include <baselib/core/detail/BoostIncludeGuardPush.h>
#include <boost/test/unit_test_monitor.hpp>
#include <baselib/core/detail/BoostIncludeGuardPop.h>

/**
 * @brief The logging config
 */

template
<
    typename E = void
>
class DefaultUtfConfigT
{
private:

    bl::cpp::SafeUniquePtr< bl::AppInitDoneDefault >                m_appInit;
    bl::Logging::LineLoggerPusher                                   m_pushLineLogger;

    bl::fs::path                                                    m_file1;
    bl::fs::path                                                    m_file2;
    bl::fs::path                                                    m_resultFile;

    static bl::Logging::line_logger_t                               g_defaultLogger;

    static std::string getUmdhPath()
    {
        return "\"C:\\dev\\3rd\\PF\\Windows Kits\\8.0\\Debuggers\\x86\\umdh.exe\"";
    }

    static std::string getUmdhPathAdmin()
    {
        return "\"C:\\Program Files (x86)\\Windows Kits\\8.0\\Debuggers\\x86\\umdh.exe\"";
    }

    static std::string getUmdhPathAdminNoQuotes()
    {
        return "C:\\Program Files (x86)\\Windows Kits\\8.0\\Debuggers\\x86\\umdh.exe";
    }

    static void utfLineLogger(
        SAA_in      const std::string&                prefix,
        SAA_in      const std::string&                text,
        SAA_in      const bool                        enableTimestamp,
        SAA_in      const bl::Logging::Level          level
        )
    {
        /*
         * There were some API changes made in Boost.Test framework in 1.59 which we need
         * to accommodate - e.g. is_initialized() API was replaced with test_in_progress()
         */

#if BOOST_VERSION < 105900
        if( boost::unit_test::framework::is_initialized() )
#else
        if( boost::unit_test::framework::test_in_progress() )
#endif
        {
            bl::cpp::SafeOutputStringStream os;

            bl::Logging::defaultLineLoggerNoLock(
                prefix,
                text,
                true /* enableTimestamp */,
                level,
                false /* addNewLine */,
                os
                );

            os.flush();
            const auto msg = os.str();

            BL_STDIO_TEXT(
                {
                    switch( level )
                    {
                        case bl::Logging::LL_ERROR:
                            UTF_ERROR_MESSAGE( msg );
                            break;

                        case bl::Logging::LL_WARNING:
                            UTF_ERROR_MESSAGE( msg );
                            break;

                        default:
                            UTF_MESSAGE( msg );
                            break;
                    }
                }
                );
        }
        else
        {
            /*
             * The UTF isn't initialized yet; we can only use the default logger
             */

            g_defaultLogger( prefix, text, enableTimestamp, level );
        }
    }

    static void translateException( const std::exception& ex )
    {
        UTF_ERROR_MESSAGE( "Unhandled exception was caught:\n" + bl::eh::diagnostic_information( ex ) );
    }

public:

    typedef DefaultUtfConfigT< E > this_type;

    DefaultUtfConfigT()
        :
        m_pushLineLogger( &this_type::utfLineLogger )
    {
        /*
         * The default abstract priority for unit tests should be 'Normal'
         */

        bl::os::setAbstractPriorityDefault( bl::os::AbstractPriority::Normal );

        bl::Logging::setLevel( bl::Logging::LL_DEBUG, true /* default global logging level */ );

        using namespace bl;

        auto& monitor = boost::unit_test::unit_test_monitor;
        monitor.register_exception_translator< std::exception >( &this_type::translateException );

        const auto& mts = boost::unit_test::framework::master_test_suite();
        BL_ASSERT( mts.argc >= 0 );

        /*
         * The UTF_TEST_APP_INIT_UTF_ARGS_PARSER macro can be used to override the UTF argument parser
         * class in some unit tests which have non-generic (custom / specialized) command line arguments
         */

        #ifndef UTF_TEST_APP_INIT_UTF_ARGS_PARSER
        #define UTF_TEST_APP_INIT_UTF_ARGS_PARSER test::UtfArgsParser
        #endif // UTF_TEST_APP_INIT_UTF_ARGS_PARSER

        UTF_TEST_APP_INIT_UTF_ARGS_PARSER::init( ( std::size_t )( mts.argc ), ( const char* const* )( mts.argv ) );

        const auto timeoutInSeconds = ( long )( test::UtfArgsParser::timeoutInSeconds() );

        if( timeoutInSeconds )
        {
            bl::http::Parameters::timeoutInSecondsGet( timeoutInSeconds );
            bl::http::Parameters::timeoutInSecondsOther( timeoutInSeconds );
        }

        bl::Logging::setLevel( ( int ) test::UtfArgsParser::loggingLevel(), true /* default global logging level */ );

        m_appInit.reset(
            new bl::AppInitDoneDefault(
                ( int ) test::UtfArgsParser::loggingLevel() /* default global logging level */,
                test::UtfArgsParser::threadsCount()         /* the default # of threads in the TP */,
                nullptr                                     /* sharedThreadPool */,
                nullptr                                     /* sharedNonBlockingThreadPool */
#ifdef UTF_TEST_APP_INIT_DEACTIVATE_THREAD_POOLS
                , UTF_TEST_APP_INIT_DEACTIVATE_THREAD_POOLS
#endif
                )
            );

        /*
         * Make sure we initialize OpenSSL and add our DEV root certificate to
         * the list of the global trusted roots
         */

        bl::crypto::registerTrustedRoot( test::UtfCrypto::getDevRootCA() /* certificatePemText */ );

#ifdef UTF_TEST_APP_INIT_PHASE1_INIT
       UTF_TEST_APP_INIT_PHASE1_INIT
#endif // UTF_TEST_APP_INIT_PHASE1_INIT

        bl::crypto::CryptoBase::init();

        BL_CHK( false, 0L == om::outstandingObjectRefs(), BL_MSG() << "Objects leaked!" );

        if( os::onWindows() && test::UtfArgsParser::isUmdhModeEnabled() )
        {
            const fs::path temp( os::getEnvironmentVariable( "TEMP" ) );

            m_file1 = temp;
            m_file1 /= fs::path( uuids::uuid2string( uuids::create() ) + ".umdh" );

            m_file2 = temp;
            m_file2 /= fs::path( uuids::uuid2string( uuids::create() ) + ".umdh" );

            m_resultFile = temp;
            m_resultFile /= fs::path( uuids::uuid2string( uuids::create() ) + ".umdh" );

            const auto umdhPath = fs::exists( getUmdhPathAdminNoQuotes() ) ? getUmdhPathAdmin() : getUmdhPath();

            bl::cpp::SafeOutputStringStream ss;
            ss << umdhPath;
            ss << " -p:";
            ss << os::getPid();
            ss << " -f:";
            ss << m_file1.string();

            const auto cmdLine = ss.str();
            const auto processRef = os::createProcess( cmdLine );

            BL_CHK(
                false,
                0 == os::tryAwaitTermination( processRef ),
                BL_MSG()
                    << "Failed to execute: "
                    << cmdLine
                );
        }
    }

    ~DefaultUtfConfigT() NOEXCEPT
    {
        long outstandingRefs = 0L;

        try
        {
            using namespace bl;

            if( m_appInit )
            {
                /*
                 * Dispose the global thread pools
                 */

                m_appInit -> dispose();

                BL_ASSERT( ! bl::ThreadPoolDefault::getDefault( ThreadPoolId::GeneralPurpose ) );
                BL_ASSERT( ! bl::ThreadPoolDefault::getDefault( ThreadPoolId::NonBlocking ) );
            }

            if( os::onWindows() && test::UtfArgsParser::isUmdhModeEnabled() )
            {
                const auto umdhPath = fs::exists( getUmdhPathAdminNoQuotes() ) ? getUmdhPathAdmin() : getUmdhPath();

                {
                    bl::cpp::SafeOutputStringStream ss;
                    ss << umdhPath;
                    ss << " -p:";
                    ss << os::getPid();
                    ss << " -f:";
                    ss << m_file2.string();

                    const auto cmdLine = ss.str();
                    const auto processRef = os::createProcess( cmdLine );

                    BL_CHK(
                        false,
                        0 == os::tryAwaitTermination( processRef ),
                        BL_MSG()
                            << "Failed to execute: "
                            << cmdLine
                        );
                }

                {
                    bl::cpp::SafeOutputStringStream ss;
                    ss << umdhPath;
                    ss << " ";
                    ss << m_file1.string();
                    ss << " ";
                    ss << m_file2.string();
                    ss << " -f:";
                    ss << m_resultFile.string();

                    const auto cmdLine = ss.str();
                    const auto processRef = os::createProcess( cmdLine );

                    BL_CHK(
                        false,
                        0 == os::tryAwaitTermination( processRef ),
                        BL_MSG()
                            << "Failed to execute: "
                            << cmdLine
                        );
                }

                /*
                 * Using the logger here may not be safe since it maybe
                 * still wired to the UTF logger (which cannot be used during globals unwind)
                 */

                bl::Logging::LineLoggerPusher pushLineLogger( bl::Logging::getDefaultLineLogger() );

                BL_LOG_MULTILINE(
                    bl::Logging::info(),
                    BL_MSG()
                        << "The result UMDH file is:\n"
                        << m_resultFile.string()
                    );
            }

            outstandingRefs = bl::om::outstandingObjectRefs();
            BL_CHK( false, 0L == outstandingRefs, BL_MSG() << "Objects leaked!" );
        }
        catch( std::exception& e )
        {
            /*
             * Using the logger here may not be safe since it maybe
             * still wired to the UTF logger (which cannot be used during globals unwind)
             */

            bl::Logging::LineLoggerPusher pushLineLogger( bl::Logging::getDefaultLineLogger() );

            BL_LOG_MULTILINE(
                bl::Logging::error(),
                BL_MSG()
                    << "DefaultUtfConfig()::~DefaultUtfConfig() failed with the following exception: "
                    << bl::eh::diagnostic_information( e )
                    << "\nOutstanding object references are "
                    << outstandingRefs
                );
        }
    }
};

template
<
    typename E
>
bl::Logging::line_logger_t
DefaultUtfConfigT< E >::g_defaultLogger = bl::Logging::getDefaultLineLogger();

typedef DefaultUtfConfigT<> DefaultUtfConfig;

UTF_GLOBAL_FIXTURE( DefaultUtfConfig )

