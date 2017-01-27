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

#ifndef __UTESTS_SHARED_UTFARGSPARSER_H_
#define __UTESTS_SHARED_UTFARGSPARSER_H_

#include <baselib/core/OS.h>
#include <baselib/core/Logging.h>
#include <baselib/core/ProgramOptions.h>
#include <baselib/core/BaseIncludes.h>
#include <baselib/core/ThreadPoolImpl.h>

#include <cstddef>

namespace test
{
    /**
     * @brief The argument parser common base class (for the generic / baselib library test code)
     */

    template
    <
        typename E = void
    >
    class UtfArgsParserBaseT
    {
        BL_NO_CREATE( UtfArgsParserBaseT )

    public:

        enum : unsigned short
        {
            PORT_DEFAULT        = 28100U,
        };

        static const std::string                            g_localHost;

    protected:

        static bool             g_optionsParsed;

        static std::string      g_argv0;
        static std::string      g_host;
        static unsigned short   g_port;
        static bool             g_isVerboseMode;
        static bool             g_isServer;
        static bool             g_isClient;
        static std::size_t      g_loggingLevel;
        static std::size_t      g_connections;
        static std::size_t      g_dataSizeInMB;
        static std::size_t      g_threadsCount;
        static std::size_t      g_objectsCount;
        static std::size_t      g_timeoutInSeconds;
        static std::size_t      g_pid;
        static std::string      g_path;
        static std::string      g_outputPath;
        static std::string      g_userId;
        static std::string      g_password;
        static std::string      g_licenseKey;
        static bool             g_isRelaxedScanMode;
        static bool             g_isUmdhModeEnabled;
        static bool             g_isAnalysisEnabled;
        static bool             g_isUseLocalBlobServer;
        static bool             g_isVerifyOnly;
        static bool             g_isEnableSessions;
        static bool             g_isWaitForCleanup;
        static std::string      g_uniqueId;

        template
        <
            typename DERIVED
        >
        static void parseOptions( SAA_in const std::size_t argc, SAA_in_ecount( argc ) const char* const* argv )
        {
            if( g_optionsParsed )
            {
                /*
                 * This is the guard for the case where parseOptions is called multiple
                 * times from different UTF config fixtures
                 */

                return;
            }

            bl::po::options_description desc("UTF options");

            DERIVED::addOptions( desc );

            bl::po::variables_map vm;
            g_argv0.assign( argv[ 0 ] );
            bl::po::store( bl::po::parse_command_line( ( int ) argc, argv, desc ), vm );
            bl::po::notify( vm );

            if( vm.count( "utf-help" ) )
            {
                BL_LOG_MULTILINE(
                    bl::Logging::debug(),
                    BL_MSG()
                        << desc
                    );
            }

            DERIVED::loadOptions( vm );

            DERIVED::dumpOptions();

            g_optionsParsed = true;
        }

        template
        <
            typename DERIVED
        >
        static void initInternal( SAA_in const std::size_t argc, SAA_in_ecount( argc ) const char* const* argv )
        {
            BL_LOG(
                bl::Logging::debug(),
                BL_MSG()
                    << "ARGPARSE: UTF module argument count is "
                    << argc
                );

            for( std::size_t i = 0; i < argc; ++i )
            {
                BL_LOG(
                    bl::Logging::debug(),
                    BL_MSG()
                        << "ARGPARSE: argv["
                        << i
                        << "] is '"
                        << argv[ i ]
                        << "'"
                    );
            }

            parseOptions< DERIVED >( argc, argv );
        }

    public:

        static const std::string& argv0() NOEXCEPT
        {
            return g_argv0;
        }

        static const std::string& host() NOEXCEPT
        {
            return g_host;
        }

        static unsigned short port() NOEXCEPT
        {
            return g_port;
        }

        static bool isVerboseMode() NOEXCEPT
        {
            return g_isVerboseMode;
        }

        static bool isServer() NOEXCEPT
        {
            return g_isServer;
        }

        static bool isClient() NOEXCEPT
        {
            return g_isClient;
        }

        static std::size_t loggingLevel() NOEXCEPT
        {
            return g_loggingLevel;
        }

        static std::size_t connections() NOEXCEPT
        {
            return g_connections;
        }

        static std::size_t dataSizeInMB() NOEXCEPT
        {
            return g_dataSizeInMB;
        }

        static std::size_t threadsCount() NOEXCEPT
        {
            return g_threadsCount;
        }

        static std::size_t objectsCount() NOEXCEPT
        {
            return g_objectsCount;
        }

        static std::size_t timeoutInSeconds() NOEXCEPT
        {
            return g_timeoutInSeconds;
        }

        static std::size_t pid() NOEXCEPT
        {
            return g_pid;
        }

        static const std::string& path() NOEXCEPT
        {
            return g_path;
        }

        static const std::string& outputPath() NOEXCEPT
        {
            return g_outputPath;
        }

        static const std::string& userId() NOEXCEPT
        {
            return g_userId;
        }

        static const std::string& password() NOEXCEPT
        {
            return g_password;
        }

        static const std::string& licenseKey() NOEXCEPT
        {
            return g_licenseKey;
        }

        static bool isRelaxedScanMode() NOEXCEPT
        {
            return g_isRelaxedScanMode;
        }

        static bool isUmdhModeEnabled() NOEXCEPT
        {
            return g_isUmdhModeEnabled;
        }

        static bool isAnalysisEnabled() NOEXCEPT
        {
            return g_isAnalysisEnabled;
        }

        static bool isUseLocalBlobServer() NOEXCEPT
        {
            return g_isUseLocalBlobServer;
        }

        static bool isVerifyOnly() NOEXCEPT
        {
            return g_isVerifyOnly;
        }

        static bool isEnableSessions() NOEXCEPT
        {
            return g_isEnableSessions;
        }

        static bool isWaitForCleanup() NOEXCEPT
        {
            return g_isWaitForCleanup;
        }

        static const std::string& uniqueId() NOEXCEPT
        {
            return g_uniqueId;
        }

        static void addOptions( SAA_inout bl::po::options_description& desc )
        {
            desc.add_options()
                ( "utf-help", "Show the available UTF arguments" )
                ( "host,h", bl::po::value< std::string >() -> default_value( g_localHost ), "The host name" )
                ( "port,p", bl::po::value< unsigned short >() -> default_value( PORT_DEFAULT ), "The port" )
                ( "verbose-mode,v", "Run in most verbose mode" )
                ( "is-server", "Is this test running in server mode" )
                ( "is-client", "Is this test running in client mode" )
                ( "bl-logging-level", bl::po::value< std::size_t >() -> default_value( bl::Logging::LL_DEBUG ), "The logging level (INFO=4, DEBUG=5, TRACE=6)" )
                ( "connections", bl::po::value< std::size_t >() -> default_value( 16U ), "The number of client connections (max 64K)" )
                ( "data-size", bl::po::value< std::size_t >() -> default_value( 200U ), "The data size in MB (max 2048)" )
                ( "objects-count", bl::po::value< std::size_t >() -> default_value( 1000U ), "The number of test objects to be created" )
                ( "threads-count", bl::po::value< std::size_t >() -> default_value( bl::ThreadPoolImpl::THREADS_COUNT_DEFAULT ), "The number of threads in the default thread pool (max 512)" )
                ( "timeout-in-seconds", bl::po::value< std::size_t >() -> default_value( 0U ), "Timeout value in seconds" )
                ( "pid", bl::po::value< std::size_t >() -> default_value( 0U ), "Process id" )
                ( "path", bl::po::value< std::string >() -> default_value( "" ), "The path to process" )
                ( "output-path", bl::po::value< std::string >() -> default_value( "" ), "Output path to save results" )
                ( "userid", bl::po::value< std::string >() -> default_value( "" ), "The user id (if required)" )
                ( "password", bl::po::value< std::string >() -> default_value( "" ), "The user password (if required)" )
                ( "license-key", bl::po::value< std::string >() -> default_value( "" ), "The license key (if required)" )
                ( "relaxed-scan-mode", "Relaxed scan mode is enabled" )
                ( "umdh-mode-enabled", "UMDH mode is enabled (on Windows only)" )
                ( "analysis-enabled", "Runtime analysis, i.e. appverif, is enabled (on Windows only)" )
                ( "use-local-blob-server", "Use blob server from localhost" )
                ( "verify-only", "Verify only (don't make stateful changes)" )
                ( "enable-sessions", "Enable sessions on upload" )
                ( "wait-for-cleanup", "Wait for [Enter] input before state clean-up on exit" )
                ( "unique-id", bl::po::value< std::string >() -> default_value( "" ), "Unique identifier (to sync between unit and scenario tests)" )
            ;
        }

        static void loadOptions( SAA_inout bl::po::variables_map& vm )
        {
            if( vm.count( "host" ) )
            {
                g_host = vm[ "host" ].as< std::string >();
            }

            if( vm.count( "port" ) )
            {
                g_port = vm[ "port" ].as< unsigned short >();
            }

            if( vm.count( "verbose-mode" ) )
            {
                g_isVerboseMode = true;
            }

            if( vm.count( "is-server" ) )
            {
                g_isServer = true;
            }

            if( vm.count( "is-client" ) )
            {
                g_isClient = true;
            }

            if( vm.count( "bl-logging-level" ) )
            {
                g_loggingLevel = std::min< std::size_t >( vm[ "bl-logging-level" ].as< std::size_t >(), bl::Logging::LL_TRACE );
            }

            if( vm.count( "connections" ) )
            {
                g_connections = std::min< std::size_t >( vm[ "connections" ].as< std::size_t >(), 64 * 1024U );
            }

            if( vm.count( "data-size" ) )
            {
                g_dataSizeInMB = std::min< std::size_t >( vm[ "data-size" ].as< std::size_t >(), 2048U );
            }

            if( vm.count( "threads-count" ) )
            {
                g_threadsCount = std::min< std::size_t >( vm[ "threads-count" ].as< std::size_t >(), 512U );
            }

            if( vm.count( "objects-count" ) )
            {
                g_objectsCount = vm[ "objects-count" ].as< std::size_t >();
            }

            if( vm.count( "timeout-in-seconds" ) )
            {
                g_timeoutInSeconds = vm[ "timeout-in-seconds" ].as< std::size_t >();
            }

            if( vm.count( "pid" ) )
            {
                g_pid = vm[ "pid" ].as< std::size_t >();
            }

            if( vm.count( "path" ) )
            {
                g_path = vm[ "path" ].as< std::string >();
            }

            if( vm.count( "output-path" ) )
            {
                g_outputPath = vm[ "output-path" ].as< std::string >();
            }

            if( vm.count( "userid" ) )
            {
                g_userId = vm[ "userid" ].as< std::string >();
            }

            if( vm.count( "password" ) )
            {
                g_password = vm[ "password" ].as< std::string >();
            }

            if( vm.count( "license-key" ) )
            {
                g_licenseKey = vm[ "license-key" ].as< std::string >();
            }

            if( vm.count( "relaxed-scan-mode" ) )
            {
                g_isRelaxedScanMode = true;
            }

            if( vm.count( "umdh-mode-enabled" ) )
            {
                g_isUmdhModeEnabled = true;
            }

            if( vm.count( "analysis-enabled" )
                || bl::os::tryGetEnvironmentVariable( "BL_ANALYSIS_TESTING" ) )
            {
                g_isAnalysisEnabled = true;
            }

            if( vm.count( "use-local-blob-server" ) )
            {
                g_isUseLocalBlobServer = true;
            }

            if( vm.count( "verify-only" ) )
            {
                g_isVerifyOnly = true;
            }

            if( vm.count( "enable-sessions" ) )
            {
                g_isEnableSessions = true;
            }

            if( vm.count( "wait-for-cleanup" ) )
            {
                g_isWaitForCleanup = true;
            }

            if( vm.count( "unique-id" ) )
            {
                g_uniqueId = vm[ "unique-id" ].as< std::string >();
            }
        }

        static void dumpOptions()
        {
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'host' is " << g_host );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'port' is " << g_port );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'verbose-mode' is " << g_isVerboseMode );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'is-server' is " << g_isServer );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'is-client' is " << g_isClient );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'loggingLevel' is " << g_loggingLevel );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'connections' is " << g_connections );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'dataSizeInMB' is " << g_dataSizeInMB );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'threadsCount' is " << g_threadsCount );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'objectsCount' is " << g_objectsCount );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'timeoutInSeconds' is " << g_timeoutInSeconds );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'pid' is " << g_pid );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'path' is " << g_path );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'output path' is " << g_outputPath );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'userId' is " << g_userId );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isRelaxedScanMode' is " << g_isRelaxedScanMode );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isUmdhModeEnabled' is " << g_isUmdhModeEnabled );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isAnalysisEnabled' is " << g_isAnalysisEnabled );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isUseLocalBlobServer' is " << g_isUseLocalBlobServer );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isVerifyOnly' is " << g_isVerifyOnly );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isEnableSessions' is " << g_isEnableSessions );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'isWaitForCleanup' is " << g_isWaitForCleanup );
            BL_LOG( bl::Logging::debug(), BL_MSG() << "ARGPARSE: UTF argument 'uniqueId' is " << g_uniqueId );
        }
    };

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_optionsParsed ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_argv0 );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, unsigned short, g_port ) = 0U;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_host );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isVerboseMode ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isServer ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isClient ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_loggingLevel ) = bl::Logging::LL_DEBUG;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_connections ) = 16U;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_dataSizeInMB ) = 200U;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_threadsCount ) = bl::ThreadPoolImpl::THREADS_COUNT_DEFAULT;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_objectsCount ) = 1000U;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_timeoutInSeconds ) = 0U;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::size_t, g_pid ) = 0U;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_path );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_outputPath );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_userId );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_password );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_licenseKey );

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isRelaxedScanMode ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isUmdhModeEnabled ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isAnalysisEnabled ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isUseLocalBlobServer ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isVerifyOnly ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isEnableSessions ) = false;

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, bool, g_isWaitForCleanup ) = false;

    BL_DEFINE_STATIC_CONST_STRING( UtfArgsParserBaseT, g_localHost ) = "localhost";

    BL_DEFINE_STATIC_MEMBER( UtfArgsParserBaseT, std::string, g_uniqueId );

    typedef UtfArgsParserBaseT<> UtfArgsParserBase;

    /**
     * @brief The argument parser instance for the baselib / generic code
     */

    template
    <
        typename E = void
    >
    class UtfArgsParserT : public UtfArgsParserBase
    {
        BL_NO_CREATE( UtfArgsParserT )

    public:

        static void init( SAA_in const std::size_t argc, SAA_in_ecount( argc ) const char* const* argv )
        {
            UtfArgsParserBase::initInternal< UtfArgsParserBase >( argc, argv );
        }
    };

    typedef UtfArgsParserT<> UtfArgsParser;

    /**
     * @brief The argument parser base template implementation (to be derived from other implementations)
     */

    template
    <
        typename DERIVED
    >
    class UtfArgsParserImpl : public UtfArgsParserBase
    {
        BL_NO_CREATE( UtfArgsParserImpl )

    public:

        static void init( SAA_in const std::size_t argc, SAA_in_ecount( argc ) const char* const* argv )
        {
            UtfArgsParserBase::initInternal< DERIVED >( argc, argv );
        }
    };

} // test

#endif /* __UTESTS_SHARED_UTFARGSPARSER_H_ */
