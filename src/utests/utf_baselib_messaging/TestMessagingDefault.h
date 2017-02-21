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

#include <baselib/core/BuildInfo.h>

#include <utests/baselib/TestMessagingUtils.h>
#include <utests/baselib/UtfCrypto.h>

namespace
{
    template
    <
        typename E = void
    >
    class TestHostServicesContextT : public bl::messaging::AsyncBlockDispatcher
    {
        BL_DECLARE_OBJECT_IMPL_ONEIFACE( TestHostServicesContextT, bl::messaging::AsyncBlockDispatcher )

    protected:

        typedef TestHostServicesContextT< E >                                   this_type;

        bl::cpp::ScalarTypeIniter< bool >                                       m_wasBlockDispatched;
        bl::uuid_t                                                              m_targetPeerId;
        bl::uuid_t                                                              m_resolvedTargetPeerId;

        TestHostServicesContextT() NOEXCEPT
            :
            m_targetPeerId( bl::uuids::nil() ),
            m_resolvedTargetPeerId( bl::uuids::nil() )
        {
        }

        void dispatchCallback( SAA_in const bl::uuid_t& targetPeerId )
        {
            if( targetPeerId != m_targetPeerId )
            {
                m_resolvedTargetPeerId = targetPeerId;
            }

            m_wasBlockDispatched = true;
        }

    public:

        void targetPeerId( SAA_in const bl::uuid_t& targetPeerId ) NOEXCEPT
        {
            m_targetPeerId = targetPeerId;
        }

        auto wasMessageForBackend() const NOEXCEPT -> bool
        {
            return ! m_wasBlockDispatched;
        }

        auto resolvedTargetPeerId() const NOEXCEPT -> const bl::uuid_t&
        {
            return m_resolvedTargetPeerId;
        }

        virtual auto getAllActiveQueuesIds() -> std::unordered_set< bl::uuid_t > OVERRIDE
        {
            return std::unordered_set< bl::uuid_t >();
        }

        virtual auto tryGetMessageBlockCompletionQueue( SAA_in const bl::uuid_t& targetPeerId )
            -> bl::om::ObjPtr< bl::messaging::MessageBlockCompletionQueue > OVERRIDE
        {
            BL_UNUSED( targetPeerId );

            return nullptr;
        }

        virtual auto createDispatchTask(
            SAA_in                  const bl::uuid_t&                                   targetPeerId,
            SAA_in                  const bl::om::ObjPtr< bl::data::DataBlock >&        data
            )
            -> bl::om::ObjPtr< bl::tasks::Task > OVERRIDE
        {
            BL_UNUSED( data );

            return bl::tasks::SimpleTaskImpl::createInstance< bl::tasks::Task >(
                bl::cpp::bind(
                    &this_type::dispatchCallback,
                    bl::om::ObjPtrCopyable< this_type >::acquireRef( this ),
                    targetPeerId
                    )
                );
        }
    };

    typedef bl::om::ObjectImpl< TestHostServicesContextT<> > TestHostServicesContext;
    typedef TestHostServicesContext context_t;

    auto createTestSecurityPrincipal() -> bl::om::ObjPtr< bl::messaging::SecurityPrincipal >
    {
        auto principal = bl::messaging::SecurityPrincipal::createInstance();

        principal -> sidLvalue() = "e123456";
        principal -> emailLvalue() = "user@host.com";
        principal -> givenNameLvalue() = "First";
        principal -> familyNameLvalue() = "Last";

        return principal;
    }

    auto createProtocolMessage( SAA_in_opt const std::string& cookiesText = bl::str::empty() )
        -> bl::om::ObjPtr< bl::messaging::BrokerProtocol >
    {
        using namespace bl::messaging;

        return utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            bl::uuids::create() /* conversationId */,
            cookiesText
            );
    }

    void testBackendProcessingTaskJson(
        SAA_in      const std::string&                                              testName,
        SAA_in      const bl::om::ObjPtr< bl::messaging::BackendProcessing >&       backendProcessing,
        SAA_in      const bl::om::ObjPtr< bl::messaging::BrokerProtocol >&          brokerProtocol,
        SAA_in      const std::string&                                              protocolDataString,
        SAA_in      const bl::om::ObjPtr< context_t >&                              context,
        SAA_in_opt  const bl::uuid_t                                                sourcePeerId = bl::uuids::create(),
        SAA_in_opt  const bl::uuid_t                                                targetPeerId = bl::uuids::create()
        )
    {
        using namespace bl::messaging;

        MessageType::Enum messageType;

        bool isAssociateDissociateMessage = false;

        if(
            MessageType::tryToEnum( brokerProtocol -> messageType(), messageType ) &&
            (
                MessageType::BackendAssociateTargetPeerId == messageType ||
                MessageType::BackendDissociateTargetPeerId == messageType
            )
            )
        {
            isAssociateDissociateMessage = true;
        }
        else
        {
            UTF_REQUIRE( brokerProtocol -> sourcePeerId().empty() );
            UTF_REQUIRE( brokerProtocol -> targetPeerId().empty() );
        }

        BL_LOG_MULTILINE(
            bl::Logging::debug(),
            BL_MSG()
                << "\n********** "
                << testName
                << " **********\n"
            );

        using BackendProcessing = bl::messaging::BackendProcessing;

        const auto operationId = BackendProcessing::OperationId::Put;
        const auto commandId = BackendProcessing::CommandId::None;

        const auto sessionId = bl::uuids::create();
        const auto chunkId = bl::uuids::create();

        const std::size_t payloadSize = 1024U;
        const std::size_t dataBlockSize = 4 * 1024U;

        const auto data = bl::data::DataBlock::createInstance( dataBlockSize );
        data -> setSize( payloadSize );

        UTF_REQUIRE( data -> capacity() >= payloadSize + protocolDataString.size() );

        data -> setOffset1( data -> size() );

        std::copy_n(
            protocolDataString.data(),
            protocolDataString.size(),
            data -> begin() + data -> offset1()
            );

        data -> setSize( data -> size() + protocolDataString.size() );

        const auto hostServices = bl::om::ProxyImpl::createInstance< bl::om::Proxy >( true /* strongRef */ );

        hostServices -> connect( context.get() );

        {
            BL_SCOPE_EXIT(
                {
                    hostServices -> disconnect();
                }
                );

            context -> targetPeerId( targetPeerId );
            backendProcessing -> setHostServices( bl::om::copy( hostServices ) );

            const auto task = backendProcessing -> createBackendProcessingTask(
                operationId,
                commandId,
                sessionId,
                chunkId,
                sourcePeerId,
                targetPeerId,
                data
                );

            bl::tasks::scheduleAndExecuteInParallel(
                [ & ]( SAA_in const bl::om::ObjPtr< bl::tasks::ExecutionQueue >& eq ) -> void
                {
                    eq -> push_back( task );
                }
                );

            backendProcessing -> setHostServices( nullptr );
        }

        const auto protocolDataOffset = data -> offset1();

        UTF_REQUIRE( data -> size() > protocolDataOffset );

        const std::string newProtocolData(
            data -> begin() + protocolDataOffset,
            data -> size() - protocolDataOffset
            );

        const auto newBrokerProtocol =
            bl::dm::DataModelUtils::loadFromJsonText< BrokerProtocol >( newProtocolData );

        UTF_REQUIRE_EQUAL( brokerProtocol -> messageType(), newBrokerProtocol -> messageType() );
        UTF_REQUIRE_EQUAL( brokerProtocol -> messageId(), newBrokerProtocol -> messageId() );
        UTF_REQUIRE_EQUAL( brokerProtocol -> conversationId(), newBrokerProtocol -> conversationId() );

        if( isAssociateDissociateMessage )
        {
            UTF_REQUIRE_EQUAL( brokerProtocol -> sourcePeerId(), newBrokerProtocol -> sourcePeerId() );
            UTF_REQUIRE_EQUAL( brokerProtocol -> targetPeerId(), newBrokerProtocol -> targetPeerId() );
        }
        else
        {
            UTF_REQUIRE_EQUAL( bl::uuids::uuid2string( sourcePeerId ), newBrokerProtocol -> sourcePeerId() );
            UTF_REQUIRE_EQUAL( bl::uuids::uuid2string( targetPeerId ), newBrokerProtocol -> targetPeerId() );
        }

        if( newBrokerProtocol -> principalIdentityInfo() )
        {
            UTF_REQUIRE( ! newBrokerProtocol -> principalIdentityInfo() -> authenticationToken() );
            UTF_REQUIRE( newBrokerProtocol -> principalIdentityInfo() -> securityPrincipal() );

            const auto& securityPrincipal =
                newBrokerProtocol -> principalIdentityInfo() -> securityPrincipal();

            const auto principalExpected = utest::DummyAuthorizationCache::getTestSecurityPrincipal();

            const auto sidLower = bl::str::to_lower_copy( securityPrincipal -> sid() );

            UTF_REQUIRE_EQUAL( sidLower, principalExpected -> secureIdentity() );
            UTF_REQUIRE_EQUAL( securityPrincipal -> givenName(), principalExpected -> givenName() );
            UTF_REQUIRE_EQUAL( securityPrincipal -> familyName(), principalExpected -> familyName() );
            UTF_REQUIRE_EQUAL( securityPrincipal -> email(), principalExpected -> email() );
        }
    }

    void testBackendProcessingTask(
        SAA_in      const std::string&                                              testName,
        SAA_in      const bl::om::ObjPtr< bl::messaging::BackendProcessing >&       backendProcessing,
        SAA_in      const bl::om::ObjPtr< bl::messaging::BrokerProtocol >&          brokerProtocol,
        SAA_in_opt  const bl::om::ObjPtr< context_t >&                              context = nullptr,
        SAA_in_opt  const bl::uuid_t                                                sourcePeerId = bl::uuids::create(),
        SAA_in_opt  const bl::uuid_t                                                targetPeerId = bl::uuids::create()
        )
    {
        const auto protocolDataString = bl::dm::DataModelUtils::getDocAsPackedJsonString( brokerProtocol );

        const auto contextNonNull = context ? bl::om::copy( context ) : context_t::createInstance();

        testBackendProcessingTaskJson(
            testName,
            backendProcessing,
            brokerProtocol,
            protocolDataString,
            contextNonNull,
            sourcePeerId,
            targetPeerId
            );

        if( ! context )
        {
            /*
             * If nullptr was passed as context then we pass a freshly created context and
             * expect that properties are set as wasMessageForBackend()=true and no target
             * peer id re-mapping
             */

            UTF_REQUIRE( ! contextNonNull -> wasMessageForBackend() );
            UTF_REQUIRE( bl::uuids::nil() == contextNonNull -> resolvedTargetPeerId() );
        }
    }

    void testBackendProcessingTask(
        SAA_in      const std::string&                                              testName,
        SAA_in      const bl::om::ObjPtr< bl::messaging::BackendProcessing >&       backendProcessing,
        SAA_in      const std::string&                                              cookiesText,
        SAA_in_opt  const bl::om::ObjPtr< context_t >&                              context = nullptr,
        SAA_in_opt  const bl::uuid_t                                                sourcePeerId = bl::uuids::create(),
        SAA_in_opt  const bl::uuid_t                                                targetPeerId = bl::uuids::create()
        )
    {
        testBackendProcessingTask(
            testName,
            backendProcessing,
            createProtocolMessage( cookiesText ),
            context,
            sourcePeerId,
            targetPeerId
            );
    }

    /*
     * Use this macro to enable the hook in the tests where necessary:
     * (and also uncomment the function currently commented with the if 0)
     *
     * BL_EXCEPTION_HOOKS_THROW_GUARD( &exceptionThrowHook1 )
     */

    #if 0
    void exceptionThrowHook1( SAA_in const bl::BaseException& exception ) NOEXCEPT
    {
        if( std::string( "bl::SystemException" ) == exception.fullTypeName() )
        {
            const auto* errorCode = exception.errorCode();

            if( errorCode && bl::asio::error::operation_aborted == *errorCode )
            {
                const auto throwFunctionName =
                    std::string( *bl::eh::get_error_info< bl::eh::throw_function >( exception ) );

                if(
                    std::string::npos !=
                        throwFunctionName.find( "bl::tasks::TaskBaseT<E>::scheduleNothrow" ) ||
                    std::string::npos !=
                        throwFunctionName.find( "bl::tasks::TaskBaseT<void>::scheduleNothrow" )
                    )
                {
                    BL_RIP_MSG( exception.details() );
                }
            }
        }
    }
    #endif

    void exceptionThrowHook2( SAA_in const bl::BaseException& exception ) NOEXCEPT
    {
        /*
         * This hooks is to catch the issue in the following JIRA (it happens on Windows only):
         *
         * https://issuetracking.jpmchase.net/jira9/browse/APPDEPLOY-5414
         *
         * std::exception::what: System error has occurred: The semaphore timeout period has expired
         * system:121
         */

        if( bl::os::onWindows() && std::string( "bl::SystemException" ) == exception.fullTypeName() )
        {
            const auto* errorCode = exception.errorCode();

            if( errorCode && bl::eh::error_code( 121, bl::eh::system_category() ) == *errorCode )
            {
                BL_RIP_MSG( exception.details() );
            }
        }
    }

    void exceptionThrowHook3( SAA_in const bl::BaseException& exception ) NOEXCEPT
    {
        /*
         * This hooks is to catch the issue in the following JIRA:
         *
         * https://issuetracking.jpmchase.net/jira9/browse/APPDEPLOY-5402
         *
         * std::exception::what: Server error has occurred: Cannot assign requested address
         * generic:99
         */

        if( std::string( "bl::ServerErrorException" ) == exception.fullTypeName() )
        {
            const auto* errorCode = exception.errorCode();

            if( errorCode && bl::eh::error_code( 99, bl::eh::generic_category() ) == *errorCode )
            {
                BL_RIP_MSG( exception.details() );
            }
        }
    }

} // __unnamed

UTF_AUTO_TEST_CASE( BackendTests )
{
    using namespace bl::messaging;

    const auto brokerBackendProcessing = utest::TestMessagingUtils::createTestMessagingBackend();

    /*
     * Test the expected context defaults
     */

    {
        const auto context = context_t::createInstance();

        UTF_REQUIRE( context -> wasMessageForBackend() );
        UTF_REQUIRE( bl::uuids::nil() == context -> resolvedTargetPeerId() );
    }

    /*
     * No cookies test
     */

    testBackendProcessingTask( "no cookies test", brokerBackendProcessing, "" /* cookiesText */ );

    /*
     * Fresh cookies test
     */

    const auto freshCookiesText = utest::TestMessagingUtils::getTokenData();

    testBackendProcessingTask( "fresh cookies test", brokerBackendProcessing, freshCookiesText );

    /*
     * Cached cookies test
     *
     * Verify that the cache was actually used by doing a small loop expecting these to be
     * processed very fast now
     */

    for( std::size_t i = 0U; i < 20; ++i )
    {
        testBackendProcessingTask( "cached cookies test", brokerBackendProcessing, freshCookiesText );
    }

    /*
     * Acknowledgment message test
     */

    {
        const auto brokerProtocol = createProtocolMessage();
        brokerProtocol -> messageType( MessageType::toString( MessageType::AsyncRpcAcknowledgment ) );
        UTF_REQUIRE( ! brokerProtocol -> principalIdentityInfo() );
        testBackendProcessingTask( "acknowledgment cookies test", brokerBackendProcessing, brokerProtocol );
    }

    {
        /*
         * Test associate and dissociate messages
         */

        const auto sourcePeerId = bl::uuids::create();
        const auto targetPeerId = bl::uuids::create();

        const auto brokerProtocol = createProtocolMessage();
        UTF_REQUIRE( ! brokerProtocol -> principalIdentityInfo() );

        /*
         * An attempt to associate it would fail because sourcePeerId and targetPeerId were not specified
         */

        try
        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::BackendAssociateTargetPeerId ) );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (invalid source & target peer id)",
                brokerBackendProcessing,
                brokerProtocol,
                context
                );

            UTF_FAIL( "This code must throw" );
        }
        catch( bl::ServerErrorException& e )
        {
            const auto* ec = bl::eh::get_error_info< bl::eh::errinfo_error_code >( e );

            UTF_REQUIRE( ec );
            UTF_REQUIRE_EQUAL( *ec, bl::eh::errc::make_error_code( BrokerErrorCodes::ProtocolValidationFailed ) );
        }

        /*
         * An associate message should be processed by the broker and it should succeed
         *
         * The wasMessageForBackend() in the context should return true, but the resolvedTargetPeerId
         * in the context should still be nil()
         */

        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::BackendAssociateTargetPeerId ) );

            brokerProtocol -> sourcePeerId( bl::uuids::uuid2string( sourcePeerId ) );
            brokerProtocol -> targetPeerId( bl::uuids::uuid2string( targetPeerId ) );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (success for associate)",
                brokerBackendProcessing,
                brokerProtocol,
                context
                );

            UTF_REQUIRE( context -> wasMessageForBackend() );
            UTF_REQUIRE( bl::uuids::nil() == context -> resolvedTargetPeerId() );
        }

        /*
         * Another attempt to associate it should succeed because an association message / command is
         * idempotent and overrides the association if one already exists
         */

        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::BackendAssociateTargetPeerId ) );

            brokerProtocol -> sourcePeerId( bl::uuids::uuid2string( sourcePeerId ) );
            brokerProtocol -> targetPeerId( bl::uuids::uuid2string( targetPeerId ) );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (already associated)",
                brokerBackendProcessing,
                brokerProtocol,
                context
                );

            UTF_REQUIRE( context -> wasMessageForBackend() );
            UTF_REQUIRE( bl::uuids::nil() == context -> resolvedTargetPeerId() );
        }

        /*
         * Now we will try to process a message for that targetPeerId and verify that it gets
         * resolved to the original sourcePeerId which it is associated with
         */

        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::AsyncRpcAcknowledgment ) );

            brokerProtocol -> sourcePeerId( "" );
            brokerProtocol -> targetPeerId( "" );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (resolve test for dispatch)",
                brokerBackendProcessing,
                brokerProtocol,
                context,
                bl::uuids::create()     /* sourcePeerId */,
                targetPeerId
                );

            UTF_REQUIRE( ! context -> wasMessageForBackend() );
            UTF_REQUIRE( sourcePeerId == context -> resolvedTargetPeerId() );
        }

        /*
         * An attempt to dissociate it would fail because targetPeerId was not specified
         */

        try
        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::BackendDissociateTargetPeerId ) );

            brokerProtocol -> sourcePeerId( "" );
            brokerProtocol -> targetPeerId( "" );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (invalid source & target peer id for dissociate)",
                brokerBackendProcessing,
                brokerProtocol,
                context
                );

            UTF_FAIL( "This code must throw" );
        }
        catch( bl::ServerErrorException& e )
        {
            const auto* ec = bl::eh::get_error_info< bl::eh::errinfo_error_code >( e );

            UTF_REQUIRE( ec );
            UTF_REQUIRE_EQUAL( *ec, bl::eh::errc::make_error_code( BrokerErrorCodes::ProtocolValidationFailed ) );
        }

        /*
         * An dissociate message should be processed by the broker and it should succeed
         *
         * The wasMessageForBackend() in the context should return true, but the resolvedTargetPeerId
         * in the context should still be nil()
         */

        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::BackendDissociateTargetPeerId ) );

            brokerProtocol -> sourcePeerId( "" );
            brokerProtocol -> targetPeerId( bl::uuids::uuid2string( targetPeerId ) );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (success for dissociate)",
                brokerBackendProcessing,
                brokerProtocol,
                context
                );

            UTF_REQUIRE( context -> wasMessageForBackend() );
            UTF_REQUIRE( bl::uuids::nil() == context -> resolvedTargetPeerId() );
        }

        /*
         * Another attempt to dissociate it should succeed because the dissociate command / message
         * is idempotent and does nothing if there is no association
         */

        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::BackendDissociateTargetPeerId ) );

            brokerProtocol -> sourcePeerId( "" );
            brokerProtocol -> targetPeerId( bl::uuids::uuid2string( targetPeerId ) );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (does not exist for dissociate)",
                brokerBackendProcessing,
                brokerProtocol,
                context
                );

            UTF_REQUIRE( context -> wasMessageForBackend() );
            UTF_REQUIRE( bl::uuids::nil() == context -> resolvedTargetPeerId() );
        }

        /*
         * Now we will try to process a message for that targetPeerId and verify that it gets
         * resolved to nil() as it is no longer associated
         */

        {
            brokerProtocol -> messageType( MessageType::toString( MessageType::AsyncRpcAcknowledgment ) );

            brokerProtocol -> sourcePeerId( "" );
            brokerProtocol -> targetPeerId( "" );

            const auto context = context_t::createInstance();

            testBackendProcessingTask(
                "broker only messages test (resolve for non-existing association)",
                brokerBackendProcessing,
                brokerProtocol,
                context,
                bl::uuids::create()     /* sourcePeerId */,
                targetPeerId
                );

            UTF_REQUIRE( ! context -> wasMessageForBackend() );
            UTF_REQUIRE( bl::uuids::nil() == context -> resolvedTargetPeerId() );
        }
    }

    const auto testPermissionDeniedFailure = [ & ](
        SAA_in          const std::string&                                                  testName,
        SAA_in          const std::string&                                                  cookiesText,
        SAA_in          const std::string&                                                  janusMessageText
        ) -> void
    {
        try
        {
            testBackendProcessingTask( testName, brokerBackendProcessing, cookiesText );
        }
        catch( bl::ServerErrorException& e )
        {
            const auto* ec = bl::eh::get_error_info< bl::eh::errinfo_error_code >( e );

            UTF_REQUIRE( ec );
            UTF_REQUIRE_EQUAL( *ec, bl::eh::errc::make_error_code( BrokerErrorCodes::AuthorizationFailed ) );

            const auto* nestedExceptionPtr = bl::eh::get_error_info< bl::eh::errinfo_nested_exception_ptr >( e );

            UTF_REQUIRE_THROW_MESSAGE(
                bl::cpp::safeRethrowException( *nestedExceptionPtr ),
                bl::SecurityException,
                janusMessageText
                );
        }
    };

    /*
     * Stale cookies test
     */

    {
        const std::string staleCookiesText =
            "pajpm1-test=7ncHAAmzS9tLAQAAAAAAAAAAAAAJs0vbSwEAAGQAbzlJ2g0x1527PIZg3yJxRD7+JOA=;"
            "pajpm2-test=7ncHAAmzS9tLAQAAAAAAAAAAAAAJs0vbSwEAAGQA3dHd0nzvjppNxTR3XPa/ZmZrrsc=;"
            "pajpm3-test=7ncHAAmzS9tLAQAAAAAAAAAAAAAJs0vbSwEAAGQANu5i9+Ainxb/GVb0gBj6IqIaJLQ=;";

        testPermissionDeniedFailure(
            "stale cookies test" /* testName */,
            staleCookiesText,
            "Janus authorization failed: User has not been authenticated" /* janusMessageText */
            );
    }

    /*
     * Bad cookies test
     */

    testPermissionDeniedFailure(
        "bad cookies test"                      /* testName */,
        "<cookies>"                             /* cookiesText */,
        "Invalid authentication token"          /* janusMessageText */
        );

    /*
     * Invalid parameter tests
     */

    try
    {
        const auto brokerProtocol = createProtocolMessage( freshCookiesText );

        const auto context = context_t::createInstance();

        testBackendProcessingTaskJson(
            "invalid json test",
            brokerBackendProcessing,
            brokerProtocol,
            "<invalid json>",
            context
            );
    }
    catch( bl::ServerErrorException& e )
    {
        const auto* ec = bl::eh::get_error_info< bl::eh::errinfo_error_code >( e );

        UTF_REQUIRE( ec );
        UTF_REQUIRE_EQUAL( *ec, bl::eh::errc::make_error_code( BrokerErrorCodes::ProtocolValidationFailed ) );
    }

    const auto testInvalidArgumentFailure = [ & ]( SAA_in const bl::om::ObjPtr< BrokerProtocol >& brokerProtocol )
        -> void
    {
        try
        {
            testBackendProcessingTask( "invalid argument test", brokerBackendProcessing, brokerProtocol );
        }
        catch( bl::ServerErrorException& e )
        {
            const auto* ec = bl::eh::get_error_info< bl::eh::errinfo_error_code >( e );

            UTF_REQUIRE( ec );
            UTF_REQUIRE_EQUAL( *ec, bl::eh::errc::make_error_code( BrokerErrorCodes::ProtocolValidationFailed ) );
        }
    };

    {
        const auto brokerProtocol = createProtocolMessage( freshCookiesText );
        brokerProtocol -> messageType( "foo" );

        testInvalidArgumentFailure( brokerProtocol );
    }

    {
        const auto brokerProtocol = createProtocolMessage( freshCookiesText );
        brokerProtocol -> messageId( "foo" );

        testInvalidArgumentFailure( brokerProtocol );
    }

    {
        const auto brokerProtocol = createProtocolMessage( freshCookiesText );
        brokerProtocol -> conversationId( "foo" );

        testInvalidArgumentFailure( brokerProtocol );
    }

    {
        const auto brokerProtocol = createProtocolMessage( freshCookiesText );
        brokerProtocol -> principalIdentityInfo() -> authenticationToken( nullptr );

        testInvalidArgumentFailure( brokerProtocol );
    }

    {
        const auto brokerProtocol = createProtocolMessage( freshCookiesText );
        brokerProtocol -> principalIdentityInfo() -> authenticationToken() -> type( "foo" );

        testInvalidArgumentFailure( brokerProtocol );
    }

    {
        /*
         * Cover the 4 main cases:
         *
         * -- both authentication token & security principal is invalid combination
         * -- no authentication token & security principal is invalid combination
         * -- no authentication token & no security principal is invalid combination
         * -- no principal identity info is valid combination for any message
         */

        const auto brokerProtocol = createProtocolMessage( freshCookiesText );
        UTF_REQUIRE( brokerProtocol -> principalIdentityInfo() -> authenticationToken() );

        brokerProtocol -> principalIdentityInfo() -> securityPrincipal( createTestSecurityPrincipal() );
        testInvalidArgumentFailure( brokerProtocol );

        brokerProtocol -> principalIdentityInfo() -> authenticationToken( nullptr );
        testInvalidArgumentFailure( brokerProtocol );

        brokerProtocol -> principalIdentityInfo() -> securityPrincipal( nullptr );
        testInvalidArgumentFailure( brokerProtocol );

        brokerProtocol -> principalIdentityInfo( nullptr );
        testBackendProcessingTask( "no principal identity info", brokerBackendProcessing, brokerProtocol );
    }
}

UTF_AUTO_TEST_CASE( BrokerFacadeTests )
{
    if( ! test::UtfArgsParser::isServer() )
    {
        return;
    }

    /*
     * This global lock needed to avoid conflicts with the default ports used below
     */

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */
        );
}

UTF_AUTO_TEST_CASE( ProxyBrokerFacadeTests )
{
    if( ! test::UtfArgsParser::isServer() )
    {
        return;
    }

    typedef utest::TestMessagingUtils utils_t;

    utils_t::startBrokerProxy( /* ... ; use the defaults */ );
}

UTF_AUTO_TEST_CASE( ProxyBrokerClientBasicTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    typedef utest::TestMessagingUtils utils_t;

    std::atomic< std::size_t > noOfMessagesDelivered( 0U );

    const auto incomingSink = om::lockDisposable(
        MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
            [ & ](
                SAA_in              const bl::uuid_t&                               targetPeerId,
                SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                )
                -> void
            {
                BL_UNUSED( targetPeerId );
                BL_UNUSED( brokerProtocol );
                BL_UNUSED( payload );

                ++noOfMessagesDelivered;
            }
            )
        );

    const om::ObjPtrCopyable< om::Proxy > clientSink =
        om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

    const auto incomingObjectChannel = om::lockDisposable(
        MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
            cpp::bind(
                &utest::TestMessagingUtils::dispatchCallback,
                clientSink,
                uuids::nil()    /* targetPeerIdExpected */,
                _1              /* targetPeerId */,
                _2              /* brokerProtocol */,
                _3              /* payload */
                )
            )
        );

    utils_t::executeMessagingTests(
        incomingObjectChannel,
        [ & ](
            SAA_in          const std::string&                                      cookiesText,
            SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
            SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
            SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
            SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
            ) -> void
        {
            BL_UNUSED( cookiesText );
            BL_UNUSED( dataBlocksPool );

            const auto conversationId = uuids::create();

            const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                MessageType::AsyncRpcDispatch,
                conversationId,
                cookiesText
                );

            const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                );

            /*
             * Create UtfArgsParser::connections() messaging clients backed by the same
             * async wrapper
             *
             * Note that these don't own the backend and the queue, so when they get
             * disposed they will not actually dispose the backend, but just tear down
             * the connections
             */

            const auto noOfConnections = test::UtfArgsParser::connections();

            auto connections = utils_t::createNoOfConnections(
                noOfConnections,
                test::UtfArgsParser::host()                     /* brokerHostName */,
                utils_t::getDefaultProxyInboundPort()           /* brokerInboundPort */
                );

            UTF_REQUIRE_EQUAL( connections.size(), noOfConnections );

            utils_t::clients_list_t clients;

            for( std::size_t i = 0; i < test::UtfArgsParser::connections(); ++i )
            {
                const auto clientPeerId = uuids::create();

                auto blockDispatch = om::lockDisposable(
                    utils_t::client_factory_t::createWithSmartDefaults(
                        om::copy( eq ),
                        clientPeerId,
                        om::copy( backend ),
                        om::copy( asyncWrapper ),
                        test::UtfArgsParser::host()                         /* host */,
                        utils_t::getDefaultProxyInboundPort()               /* inboundPort */,
                        utils_t::getDefaultProxyInboundPort() + 1U          /* outboundPort */,
                        std::move( connections[ i ].first )                 /* inboundConnection */,
                        std::move( connections[ i ].second )                /* outboundConnection */,
                        om::copy( dataBlocksPool )
                        )
                    );

                auto client = om::lockDisposable(
                    MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                        om::qi< MessagingClientBlockDispatch >( blockDispatch ),
                        dataBlocksPool
                        )
                    );

                blockDispatch.detachAsObjPtr();

                clients.emplace_back( std::make_pair( clientPeerId, std::move( client ) ) );
            }

            /*
             * Just execute a bunch of messages at random and then verify that they have arrived
             * and that all channels were used fairly
             */

            {
                BL_SCOPE_EXIT(
                    {
                        clientSink -> disconnect();
                    }
                    );

                clientSink -> connect( incomingSink.get() );

                scheduleAndExecuteInParallel(
                    [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eqLocal ) -> void
                    {
                        eqLocal -> setOptions( ExecutionQueue::OptionKeepAll );

                        const auto defaultDuration = time::seconds( 5L );

                        const auto timerCallback = [ & ]() -> time::time_duration
                        {
                            const auto task = eqLocal -> pop( false /* wait */ );

                            if( task )
                            {
                                if( task -> isFailed() )
                                {
                                    BL_LOG_MULTILINE(
                                        bl::Logging::debug(),
                                        BL_MSG()
                                            << "An exception occurred while trying to send a message:\n"
                                            << bl::eh::diagnostic_information( task -> exception() )
                                        );
                                }
                                else
                                {
                                    BL_LOG_MULTILINE(
                                        bl::Logging::debug(),
                                        BL_MSG()
                                            << "Last message was sent successfully!"
                                        );

                                    UTF_REQUIRE_EQUAL( noOfMessagesDelivered, 1U );
                                }
                            }

                            if( eqLocal -> isEmpty() )
                            {
                                const auto getRandomPos = [ & ]() -> std::size_t
                                {
                                    return random::getUniformRandomUnsignedValue< std::size_t >(
                                        clients.size() - 1
                                        );
                                };

                                const auto pos1 = getRandomPos();
                                const auto pos2 = getRandomPos();

                                const auto& sourceClient = clients[ pos1 ].second;
                                const auto& targetPeerId = clients[ pos2 ].first;

                                noOfMessagesDelivered = 0;

                                eqLocal -> push_back(
                                    ExternalCompletionTaskImpl::createInstance< Task >(
                                        cpp::bind(
                                            &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                            om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                sourceClient -> outgoingObjectChannel().get()
                                                ),
                                            targetPeerId,
                                            om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                            om::ObjPtrCopyable< Payload >( payload ),
                                            _1 /* onReady - the completion callback */
                                            )
                                        )
                                    );
                            }

                            return defaultDuration;
                        };

                        SimpleTimer timer( timerCallback, cpp::copy( defaultDuration ) );

                        utils_t::waitForKeyOrTimeout();

                        eqLocal -> forceFlushNoThrow();
                    }
                    );
            }
        }
        );
}

UTF_AUTO_TEST_CASE( BrokerClientTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    typedef utest::TestMessagingUtils utils_t;

    const auto incomingObjectChannel = om::lockDisposable(
        MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
            [](
                SAA_in              const bl::uuid_t&                               targetPeerId,
                SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                )
                -> void
            {
                BL_UNUSED( targetPeerId );
                BL_UNUSED( brokerProtocol );
                BL_UNUSED( payload );

                UTF_FAIL( "This should not be called from this test" );
            }
            )
        );

    utils_t::executeMessagingTests(
        incomingObjectChannel,
        [ & ](
            SAA_in          const std::string&                                      cookiesText,
            SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
            SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
            SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
            SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
            ) -> void
        {
            BL_UNUSED( cookiesText );
            BL_UNUSED( dataBlocksPool );

            /*
             * Create UtfArgsParser::connections() messaging clients backed by the same
             * async wrapper
             *
             * Note that these don't own the backend and the queue, so when they get
             * disposed they will not actually dispose the backend, but just tear down
             * the connections
             */

            auto connections = utils_t::createConnections();

            UTF_REQUIRE_EQUAL( connections.size(), test::UtfArgsParser::connections() );

            std::vector< om::ObjPtrDisposable< MessagingClientBlockDispatch > > clients;

            for( std::size_t i = 0; i < test::UtfArgsParser::connections(); ++i )
            {
                const auto peerId = uuids::create();

                clients.emplace_back(
                    om::lockDisposable(
                        MessagingClientFactorySsl::createWithSmartDefaults(
                            om::copy( eq ),
                            peerId,
                            om::copy( backend ),
                            om::copy( asyncWrapper ),
                            test::UtfArgsParser::host(),
                            test::UtfArgsParser::port()                             /* inboundPort */,
                            test::UtfArgsParser::port() + 1                         /* outboundPort */,
                            std::move( connections[ i ].first )                     /* inboundConnection */,
                            std::move( connections[ i ].second )                    /* outboundConnection */
                            )
                        )
                    );
            }

            utils_t::waitForKeyOrTimeout();
        }
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingUtilsTests )
{
    using namespace bl;
    using namespace bl::messaging;

    /*
     * Test message verification helpers
     */

    typedef bl::cpp::function < std::string& ( SAA_in BrokerProtocol& ) > string_lvalue_getter_t;

    const auto testInvalidPropertyValue = [](
        SAA_in              const string_lvalue_getter_t&                       getter,
        SAA_in              const char*                                         exceptionMessage,
        SAA_in              std::string&&                                       invalidValue
        ) -> void
    {
        const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            uuids::create()                 /* conversationId */,
            "<test cookies>"                /* cookiesText */
            );

        getter( *brokerProtocol ) = BL_PARAM_FWD( invalidValue );

        UTF_REQUIRE_THROW_MESSAGE(
            MessagingUtils::verifyBrokerProtocolMessage( brokerProtocol ),
            InvalidDataFormatException,
            exceptionMessage
            );
    };

    testInvalidPropertyValue(
        bl::cpp::mem_fn( &BrokerProtocol::messageTypeLvalue )           /* getter */,
        "The message type specified is invalid ''"                      /* exceptionMessage */,
        ""                                                              /* invalidValue */
        );

    testInvalidPropertyValue(
        bl::cpp::mem_fn( &BrokerProtocol::messageTypeLvalue )           /* getter */,
        "The message type specified is invalid 'foo'"                   /* exceptionMessage */,
        "foo"                                                           /* invalidValue */
        );

    testInvalidPropertyValue(
        bl::cpp::mem_fn( &BrokerProtocol::messageIdLvalue )             /* getter */,
        "The 'messageId' property cannot be empty"                      /* exceptionMessage */,
        ""                                                              /* invalidValue */
        );

    testInvalidPropertyValue(
        bl::cpp::mem_fn( &BrokerProtocol::conversationIdLvalue )        /* getter */,
        "The 'conversationId' property cannot be empty"                 /* exceptionMessage */,
        ""                                                              /* invalidValue */
        );

    {
        const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            uuids::create()                 /* conversationId */,
            "<test cookies>"                /* cookiesText */
            );

        brokerProtocol -> principalIdentityInfoLvalue() = nullptr;

        MessagingUtils::verifyBrokerProtocolMessage( brokerProtocol );
    }

    {
        const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            uuids::create()                 /* conversationId */,
            "<test cookies>"                /* cookiesText */
            );

        const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();
        UTF_REQUIRE( principalIdentityInfo );

        UTF_REQUIRE( principalIdentityInfo -> authenticationTokenLvalue() );
        principalIdentityInfo -> securityPrincipalLvalue() = createTestSecurityPrincipal();

        UTF_REQUIRE_THROW_MESSAGE(
            MessagingUtils::verifyBrokerProtocolMessage( brokerProtocol ),
            InvalidDataFormatException,
            "Principal identity info is invalid: either security principal or authentication token must be provided"
            );

        principalIdentityInfo -> authenticationTokenLvalue() = nullptr;
        principalIdentityInfo -> securityPrincipalLvalue() = nullptr;

        UTF_REQUIRE_THROW_MESSAGE(
            MessagingUtils::verifyBrokerProtocolMessage( brokerProtocol ),
            InvalidDataFormatException,
            "Principal identity info is invalid: either security principal or authentication token must be provided"
            );
    }

    typedef bl::cpp::function < std::string& ( SAA_in AuthenticationToken& ) > token_lvalue_getter_t;

    const auto testInvalidTokenPropertyValue = [](
        SAA_in              const token_lvalue_getter_t&                        getter,
        SAA_in              const char*                                         exceptionMessage,
        SAA_in              std::string&&                                       invalidValue
        ) -> void
    {
        const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            uuids::create()                 /* conversationId */,
            "<test cookies>"                /* cookiesText */
            );

        const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();
        UTF_REQUIRE( principalIdentityInfo );

        const auto& authenticationToken = principalIdentityInfo -> authenticationToken();
        UTF_REQUIRE( authenticationToken );

        getter( *authenticationToken ) = BL_PARAM_FWD( invalidValue );

        UTF_REQUIRE_THROW_MESSAGE(
            MessagingUtils::verifyBrokerProtocolMessage( brokerProtocol ),
            InvalidDataFormatException,
            exceptionMessage
            );
    };

    testInvalidTokenPropertyValue(
        bl::cpp::mem_fn( &AuthenticationToken::typeLvalue )          /* getter */,
        "The authentication token type specified is invalid"         /* exceptionMessage */,
        ""                                                           /* invalidValue */
        );

    testInvalidTokenPropertyValue(
        bl::cpp::mem_fn( &AuthenticationToken::dataLvalue )          /* getter */,
        "The authentication token data is invalid or unavailable"    /* exceptionMessage */,
        ""                                                           /* invalidValue */
        );

    typedef bl::cpp::function < std::string& ( SAA_in SecurityPrincipal& ) > principal_lvalue_getter_t;

    const auto testInvalidPrincipalPropertyValue = [](
        SAA_in              const principal_lvalue_getter_t&                    getter,
        SAA_in              const char*                                         exceptionMessage,
        SAA_in              std::string&&                                       invalidValue
        ) -> void
    {
        const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            uuids::create()                 /* conversationId */,
            "<test cookies>"                /* cookiesText */
            );

        const auto& principalIdentityInfo = brokerProtocol -> principalIdentityInfo();
        UTF_REQUIRE( principalIdentityInfo );
        principalIdentityInfo -> authenticationTokenLvalue() = nullptr;

        auto& securityPrincipal = principalIdentityInfo -> securityPrincipalLvalue();
        UTF_REQUIRE( ! securityPrincipal );

        securityPrincipal = createTestSecurityPrincipal();

        getter( *securityPrincipal ) = BL_PARAM_FWD( invalidValue );

        UTF_REQUIRE_THROW_MESSAGE(
            MessagingUtils::verifyBrokerProtocolMessage( brokerProtocol ),
            InvalidDataFormatException,
            exceptionMessage
            );
    };

    const char* exceptionMessageExpected =
        "The security principal information specified is invalid as one of the required fields is empty";

    testInvalidPrincipalPropertyValue(
        bl::cpp::mem_fn( &SecurityPrincipal::sidLvalue )            /* getter */,
        exceptionMessageExpected                                    /* exceptionMessage */,
        ""                                                          /* invalidValue */
        );

    /*
     * Test serialization & deserialization helpers
     */

    {
        const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

        const auto conversationId = uuids::create();
        const auto messageId = uuids::create();

        const std::string cookiesText( "<test cookies>" );

        const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            conversationId,
            cookiesText,
            messageId
            );

        UTF_REQUIRE_EQUAL( brokerProtocol -> messageType(), MessageType::toString( MessageType::AsyncRpcDispatch ) );
        UTF_REQUIRE_EQUAL( brokerProtocol -> messageId(), uuids::uuid2string( messageId ) );
        UTF_REQUIRE_EQUAL( brokerProtocol -> conversationId(), uuids::uuid2string( conversationId ) );

        UTF_REQUIRE( brokerProtocol -> principalIdentityInfo() );
        UTF_REQUIRE( brokerProtocol -> principalIdentityInfo() -> authenticationToken() );
        UTF_REQUIRE( ! brokerProtocol -> principalIdentityInfo() -> securityPrincipal() );

        const auto& authenticationToken = brokerProtocol -> principalIdentityInfo() -> authenticationToken();

        UTF_REQUIRE_EQUAL(
            authenticationToken -> type(),
            utest::DummyAuthorizationCache::dummyTokenType()
            );
        UTF_REQUIRE_EQUAL( authenticationToken -> data(), cookiesText );

        const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
            utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
            );

        const auto dataBlock = MessagingUtils::serializeObjectsToBlock( brokerProtocol, payload, dataBlocksPool );

        const auto dataBlockCopy = MessagingUtils::serializeObjectsToBlock( brokerProtocol, payload );

        UTF_REQUIRE_EQUAL( dataBlock -> size(), dataBlockCopy -> size() );
        UTF_REQUIRE_EQUAL( dataBlock -> offset1(), dataBlockCopy -> offset1() );

        UTF_REQUIRE_EQUAL(
            0,
            std::memcmp(
                dataBlock -> begin(),
                dataBlockCopy -> begin(),
                dataBlock -> size() )
            );

        const auto pair = MessagingUtils::deserializeBlockToObjects( dataBlock );

        UTF_REQUIRE( pair.second );

        utest::DataModelTestUtils::requireObjectsEqual( brokerProtocol, pair.first /* brokerProtocol */ );
        utest::DataModelTestUtils::requireObjectsEqual( payload, pair.second /* payload */ );
    }

    /*
     * Test the endpoints expansion helper
     */

    {
        std::vector< std::string > endpoints;

        /*
         * Test various scenarios when the input is even number (2)
         */

        endpoints.push_back( "ep1:1234" );
        endpoints.push_back( "ep2:5678" );

        const auto testExpandEndpoints = [ & ](
            SAA_in          const std::size_t                                       noOfRequestedEndpoints,
            SAA_in          const std::size_t                                       expectedMultiplier
            )
            -> void
        {
            const auto expandedEndpoints =
                MessagingUtils::expandEndpoints( noOfRequestedEndpoints, cpp::copy( endpoints ) );

            UTF_REQUIRE( expandedEndpoints.size() >= endpoints.size() );
            UTF_REQUIRE_EQUAL( expandedEndpoints.size(), expectedMultiplier * endpoints.size() );

            for( std::size_t i = 0U, count = expandedEndpoints.size(); i < count; ++i )
            {
                UTF_REQUIRE_EQUAL( expandedEndpoints[ i ], endpoints[ i % endpoints.size() ] );
            }
        };

        const auto testAllScenarios = [ & ]() -> void
        {
            /*
             * Test various scenarios
             */

            testExpandEndpoints( 0U /* noOfRequestedEndpoints */, 1U /* expectedMultiplier */ );
            testExpandEndpoints( 1U /* noOfRequestedEndpoints */, 1U /* expectedMultiplier */ );
            testExpandEndpoints( endpoints.size() /* noOfRequestedEndpoints */, 1U /* expectedMultiplier */ );
            testExpandEndpoints( endpoints.size() + 1U /* noOfRequestedEndpoints */, 2U /* expectedMultiplier */ );
            testExpandEndpoints( endpoints.size() * 2U /* noOfRequestedEndpoints */, 2U /* expectedMultiplier */ );
            testExpandEndpoints( endpoints.size() * 2U + 1 /* noOfRequestedEndpoints */, 3U /* expectedMultiplier */ );
        };

        /*
         * Test various scenarios when the input is even number (2)
         */

        testAllScenarios();

        /*
         * Test various scenarios when the input is odd number (3)
         */

        endpoints.push_back( "ep3:9012" );

        testAllScenarios();
    }
}

UTF_AUTO_TEST_CASE( IO_MessagingClientObjectDispatchLocalTests )
{
    using namespace bl;
    using namespace bl::messaging;

    std::atomic< std::size_t > callsCount( 0U );

    const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

    const auto targetPeerIdExpected = uuids::create();

    const auto brokerProtocolExpected = utest::TestMessagingUtils::createBrokerProtocolMessage(
        MessageType::AsyncRpcDispatch,
        uuids::create()                     /* conversationId */,
        "<test cookies>"                    /* cookiesText */
        );

    const auto payloadExpected = bl::dm::DataModelUtils::loadFromFile< Payload >(
        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
        );

    const auto incomingObjectChannel = bl::om::lockDisposable(
        MessagingClientObjectDispatchFromCallback::createInstance(
            [ & ](
                SAA_in              const bl::uuid_t&                               targetPeerId,
                SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                ) -> void
            {
                UTF_REQUIRE_EQUAL( targetPeerIdExpected, targetPeerId );
                ++callsCount;

                UTF_REQUIRE( payload );

                utest::DataModelTestUtils::requireObjectsEqual( brokerProtocol, brokerProtocolExpected );
                utest::DataModelTestUtils::requireObjectsEqual( payload, payloadExpected );
            }
            )
        );

    {
        const auto client = bl::om::lockDisposable(
            MessagingClientObjectFactory::createFromObjectDispatchLocal< MessagingClientBlockDispatchLocal >(
                om::qi< MessagingClientObjectDispatch >( incomingObjectChannel ),
                om::copy( dataBlocksPool )
                )
            );

        const auto& objectDispatcher = client -> outgoingObjectChannel();

        const auto& blockDispatcher = client -> outgoingBlockChannel();

        const std::size_t noOfBlocks = 1024U;

        for( std::size_t i = 0U; i < noOfBlocks; ++i )
        {
            objectDispatcher -> pushMessage( targetPeerIdExpected, brokerProtocolExpected, payloadExpected );
        }

        om::qi< MessagingClientBlockDispatchLocal >( blockDispatcher ) -> flush();

        UTF_REQUIRE_EQUAL( callsCount.load(), noOfBlocks );
    }
}

UTF_AUTO_TEST_CASE( IO_MessagingClientObjectDispatchTcpDispatcherTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto callbackTests = []() -> void
    {
        std::atomic< std::size_t > callsCount( 0U );

        const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

        const auto targetPeerIdExpected = uuids::create();
        const auto sourcePeerId = uuids::create();
        const auto conversationId = uuids::create();
        const auto messageId = uuids::create();

        /*
         * Obtain fresh cookies and create a valid broker protocol message
         */

        const auto cookiesText = utest::TestMessagingUtils::getTokenData();

        const auto brokerProtocolToSend = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            conversationId,
            cookiesText,
            messageId
            );

        const auto brokerProtocolExpected = utest::TestMessagingUtils::createBrokerProtocolMessage(
            MessageType::AsyncRpcDispatch,
            conversationId,
            "<fake cookies text>",
            messageId
            );

        const auto principal = utest::DummyAuthorizationCache::getTestSecurityPrincipal();

        BrokerBackendTask::authorizeProtocolMessage( brokerProtocolExpected, principal );

        brokerProtocolExpected -> sourcePeerId( uuids::uuid2string( sourcePeerId ) );
        brokerProtocolExpected -> targetPeerId( uuids::uuid2string( targetPeerIdExpected ) );

        const auto payloadExpected = bl::dm::DataModelUtils::loadFromFile< Payload >(
            utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
            );

        const auto incomingObjectChannel1 = bl::om::lockDisposable(
            MessagingClientObjectDispatchFromCallback::createInstance(
                [ & ](
                    SAA_in              const bl::uuid_t&                               targetPeerId,
                    SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                    SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                    ) -> void
                {
                    BL_UNUSED( targetPeerId );
                    BL_UNUSED( brokerProtocol );
                    BL_UNUSED( payload );

                    UTF_FAIL( "This one should not be called" );
                }
                )
            );

        const auto incomingObjectChannel2 = bl::om::lockDisposable(
            MessagingClientObjectDispatchFromCallback::createInstance(
                [ & ](
                    SAA_in              const bl::uuid_t&                               targetPeerId,
                    SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                    SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                    ) -> void
                {
                    UTF_REQUIRE_EQUAL( targetPeerIdExpected, targetPeerId );
                    UTF_REQUIRE_EQUAL( uuids::uuid2string( targetPeerId ), brokerProtocol -> targetPeerId() );
                    UTF_REQUIRE_EQUAL( uuids::uuid2string( sourcePeerId ), brokerProtocol -> sourcePeerId() );

                    ++callsCount;

                    UTF_REQUIRE( payload );

                    utest::DataModelTestUtils::requireObjectsEqual( brokerProtocol, brokerProtocolExpected );
                    utest::DataModelTestUtils::requireObjectsEqual( payload, payloadExpected );
                }
                )
            );

        {
            auto connections1 = MessagingClientFactorySsl::createEstablishedConnections(
                "localhost"                                             /* host */,
                test::UtfArgsParser::port()                             /* inboundPort */,
                test::UtfArgsParser::port() + 1                         /* outboundPort */
                );

            auto connections2 = MessagingClientFactorySsl::createEstablishedConnections(
                "localhost"                                             /* host */,
                test::UtfArgsParser::port()                             /* inboundPort */,
                test::UtfArgsParser::port() + 1                         /* outboundPort */
                );

            const auto client1 = bl::om::lockDisposable(
                MessagingClientObjectFactory::createFromObjectDispatchTcp(
                    om::qi< MessagingClientObjectDispatch >( incomingObjectChannel1 ),
                    dataBlocksPool,
                    sourcePeerId,
                    "localhost"                                         /* host */,
                    test::UtfArgsParser::port()                         /* inboundPort */,
                    test::UtfArgsParser::port() + 1                     /* outboundPort */,
                    std::move( connections1.first )                     /* inboundConnection */,
                    std::move( connections1.second )                    /* outboundConnection */
                    )
                );

            const auto client2 = bl::om::lockDisposable(
                MessagingClientObjectFactory::createFromObjectDispatchTcp(
                    om::qi< MessagingClientObjectDispatch >( incomingObjectChannel2 ),
                    dataBlocksPool,
                    targetPeerIdExpected,
                    "localhost"                                         /* host */,
                    test::UtfArgsParser::port()                         /* inboundPort */,
                    test::UtfArgsParser::port() + 1                     /* outboundPort */,
                    std::move( connections2.first )                     /* inboundConnection */,
                    std::move( connections2.second )                    /* outboundConnection */
                    )
                );

            const auto& objectDispatcher = client1 -> outgoingObjectChannel();

            const std::size_t noOfBlocks = 1024U;

            /*
             * Test the completion callback interface which allows us to wait for message
             * delivery and do proper error handling (as necessary)
             *
             * 1) First send messages in batches of less than half the queue size (
             *    utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2) which should always
             *    succeed
             *
             * 2) Send a quick storm of messages and expect many of these to fail with
             *    BrokerErrorCodes::TargetPeerQueueFull error code
             *
             * 3) Send invalid peer id and expect BrokerErrorCodes::TargetPeerNotFound
             *
             * 4) Send a message which is expected to be authenticated, but don't provide an
             *    authentication token (expect BrokerErrorCodes::AuthorizationFailed)
             *
             * 5) Send invalid message and expect BrokerErrorCodes::ProtocolValidationFailed
             *
             * 6) Send noOfBlocks messages in async mode and wait for all of them to finish
             */

            scheduleAndExecuteInParallel(
                [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
                {
                    typedef utest::TestMessagingUtils utils_t;

                    eq -> setOptions( ExecutionQueue::OptionKeepFailed );

                    /*
                     * Step #1 from the comment above...
                     */

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Step #1: testing sync execution of "
                            << noOfBlocks
                            << " calls"
                        );

                    eq -> setThrottleLimit( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2U );

                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                    {
                        eq -> push_back(
                            ExternalCompletionTaskImpl::createInstance< Task >(
                                cpp::bind(
                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                        objectDispatcher.get()
                                        ),
                                    targetPeerIdExpected,
                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocolToSend ),
                                    om::ObjPtrCopyable< Payload >( payloadExpected ),
                                    _1 /* onReady - the completion callback */
                                    )
                                )
                            );
                    }

                    utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eq );

                    /*
                     * Step #2 from the comment above...
                     */

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Step #2: testing sync execution of "
                            << noOfBlocks
                            << " calls without throttle (many should fail)"
                        );

                    eq -> setThrottleLimit( 0U /* no throttle limit */ );

                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                    {
                        eq -> push_back(
                            ExternalCompletionTaskImpl::createInstance< Task >(
                                cpp::bind(
                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                        objectDispatcher.get()
                                        ),
                                    targetPeerIdExpected,
                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocolToSend ),
                                    om::ObjPtrCopyable< Payload >( payloadExpected ),
                                    _1 /* onReady - the completion callback */
                                    )
                                )
                            );
                    }

                    std::size_t noOfFailedCalls = 0U;

                    BL_SCOPE_EXIT(
                        {
                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Number of failed calls is "
                                    << noOfFailedCalls
                                );
                        }
                        );

                    while( ! eq -> isEmpty() )
                    {
                        const auto task = eq -> pop();

                        /*
                         * Because of the inherent race condition with the isEmpty() check
                         * returning 'false' right before it becomes 'true' we need to check
                         * for the case where task is nullptr
                         */

                        if( ! task )
                        {
                            continue;
                        }

                        UTF_REQUIRE( task -> isFailed() );

                        try
                        {
                            cpp::safeRethrowException( task -> exception() );

                            UTF_FAIL( "This must throw" );
                        }
                        catch( ServerErrorException& e )
                        {
                            const auto* ec = e.errorCode();

                            UTF_REQUIRE(
                                ec && eh::errc::make_error_code( BrokerErrorCodes::TargetPeerQueueFull ) == *ec
                                );

                            ++noOfFailedCalls;
                        }
                    }

                    UTF_REQUIRE( noOfFailedCalls );

                    /*
                     * Step #3 from the comment above...
                     *
                     * Sending a message to a non-existing target (targetPeerId is random uuid)
                     */

                    eq -> forceFlushNoThrow();

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Step #3: testing sync execution of a message which is expected to fail with"
                            << " BrokerErrorCodes::TargetPeerNotFound"
                        );

                    eq -> push_back(
                        ExternalCompletionTaskImpl::createInstance< Task >(
                            cpp::bind(
                                &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                    objectDispatcher.get()
                                    ),
                                uuids::create() /* random non-existing targetPeerId */,
                                om::ObjPtrCopyable< BrokerProtocol >( brokerProtocolToSend ),
                                om::ObjPtrCopyable< Payload >( payloadExpected ),
                                _1 /* onReady - the completion callback */
                                )
                            )
                        );

                    noOfFailedCalls = 0U;

                    try
                    {
                        eq -> flush();

                        UTF_FAIL( "This must throw" );
                    }
                    catch( ServerErrorException& e )
                    {
                        const auto* ec = e.errorCode();

                        UTF_REQUIRE(
                            ec && eh::errc::make_error_code( BrokerErrorCodes::TargetPeerNotFound ) == *ec
                            );

                        ++noOfFailedCalls;
                    }

                    UTF_REQUIRE( noOfFailedCalls );

                    /*
                     * Step #4 from the comment above...
                     */

                    eq -> forceFlushNoThrow();

                    const auto brokerProtocolNoCookies = utest::TestMessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        "<throw>",
                        messageId
                        );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Step #4: testing sync execution of a message which is expected to fail with"
                            << " BrokerErrorCodes::AuthorizationFailed"
                        );

                    eq -> push_back(
                        ExternalCompletionTaskImpl::createInstance< Task >(
                            cpp::bind(
                                &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                    objectDispatcher.get()
                                    ),
                                targetPeerIdExpected,
                                om::ObjPtrCopyable< BrokerProtocol >( brokerProtocolNoCookies ),
                                om::ObjPtrCopyable< Payload >( payloadExpected ),
                                _1 /* onReady - the completion callback */
                                )
                            )
                        );

                    noOfFailedCalls = 0U;

                    try
                    {
                        eq -> flush();

                        UTF_FAIL( "This must throw" );
                    }
                    catch( ServerErrorException& e )
                    {
                        const auto* ec = e.errorCode();

                        UTF_REQUIRE(
                            ec && eh::errc::make_error_code( BrokerErrorCodes::AuthorizationFailed ) == *ec
                            );

                        ++noOfFailedCalls;
                    }

                    UTF_REQUIRE( noOfFailedCalls );

                    /*
                     * Step #5 from the comment above...
                     */

                    eq -> forceFlushNoThrow();

                    const auto brokerProtocolInvalid = utest::TestMessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        cookiesText,
                        messageId
                        );

                    brokerProtocolInvalid -> messageId( "<invalid uuid>" );

                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "Step #5: testing sync execution of a message which is expected to fail with"
                            << " BrokerErrorCodes::ProtocolValidationFailed"
                        );

                    eq -> push_back(
                        ExternalCompletionTaskImpl::createInstance< Task >(
                            cpp::bind(
                                &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                    objectDispatcher.get()
                                    ),
                                targetPeerIdExpected,
                                om::ObjPtrCopyable< BrokerProtocol >( brokerProtocolInvalid ),
                                om::ObjPtrCopyable< Payload >( payloadExpected ),
                                _1 /* onReady - the completion callback */
                                )
                            )
                        );

                    noOfFailedCalls = 0U;

                    try
                    {
                        eq -> flush();

                        UTF_FAIL( "This must throw" );
                    }
                    catch( ServerErrorException& e )
                    {
                        const auto* ec = e.errorCode();

                        UTF_REQUIRE(
                            ec && eh::errc::make_error_code( BrokerErrorCodes::ProtocolValidationFailed ) == *ec
                            );

                        ++noOfFailedCalls;
                    }

                    UTF_REQUIRE( noOfFailedCalls );
                }
                );

            /*
             * Step #6 from the comment above...
             */

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Step #6: testing async execution of "
                    << noOfBlocks
                    << " calls"
                );

            /*
             * Note that at this point we need to reset callsCount to satisfy the
             * wait loop below which expects all messages to be delivered and
             * callsCount to be exactly noOfBlocks
             *
             * We will also so a small sleep and check that callsCount is stable
             * as we don't expect any messages to be delivered at this point
             */

            callsCount = 0U;
            os::sleep( time::seconds( 1L ) );
            UTF_REQUIRE( callsCount.load() == 0U );

            for( std::size_t i = 0U; i < noOfBlocks; ++i )
            {
                for( ;; )
                {
                    try
                    {
                        objectDispatcher -> pushMessage(
                            targetPeerIdExpected,
                            brokerProtocolToSend,
                            payloadExpected
                            );

                        break;
                    }
                    catch( ServerErrorException& e )
                    {
                        const auto* ec = e.errorCode();

                        if( ec && eh::errc::make_error_code( BrokerErrorCodes::TargetPeerQueueFull ) == *ec )
                        {
                            os::sleep( time::milliseconds( 100 ) );

                            continue;
                        }

                        throw;
                    }
                }
            }

            std::size_t retries = 0U;
            const std::size_t retryCount = 180U;

            for( ;; )
            {
                if( retries >= retryCount )
                {
                    UTF_FAIL( "Messages were not delivered within 180 seconds" );
                }

                if( callsCount.load() == noOfBlocks )
                {
                    BL_LOG(
                        Logging::debug(),
                        BL_MSG()
                            << "All messages were delivered approximately within less than "
                            << retries
                            << " seconds"
                        );

                    break;
                }

                os::sleep( time::seconds( 1L ) );
                ++retries;
            }
        }
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

namespace utest
{
    namespace dm
    {
        /*
         * TestAsyncRequest
         */

        BL_DM_DEFINE_CLASS_BEGIN( TestAsyncRequest )

            BL_DM_DECLARE_STRING_PROPERTY               ( inputPath )
            BL_DM_DECLARE_BOOL_PROPERTY                 ( shouldStart )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( inputPath )
                BL_DM_IMPL_PROPERTY( shouldStart )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( TestAsyncRequest )

        BL_DM_DEFINE_PROPERTY( TestAsyncRequest, inputPath )
        BL_DM_DEFINE_PROPERTY( TestAsyncRequest, shouldStart )

        /*
         * TestAsyncResponse
         */

        BL_DM_DEFINE_CLASS_BEGIN( TestAsyncResponse )

            BL_DM_DECLARE_STRING_PROPERTY               ( finalPath )
            BL_DM_DECLARE_BOOL_PROPERTY                 ( hasStarted )

            BL_DM_PROPERTIES_IMPL_BEGIN()
                BL_DM_IMPL_PROPERTY( finalPath )
                BL_DM_IMPL_PROPERTY( hasStarted )
            BL_DM_PROPERTIES_IMPL_END()

        BL_DM_DEFINE_CLASS_END( TestAsyncResponse )

        BL_DM_DEFINE_PROPERTY( TestAsyncResponse, finalPath )
        BL_DM_DEFINE_PROPERTY( TestAsyncResponse, hasStarted )

    } // dm

} // utest

namespace
{
    class TestConversationProcessing : public bl::messaging::ConversationProcessingBaseImpl<>
    {
        BL_DECLARE_OBJECT_IMPL( TestConversationProcessing )

    protected:

        typedef bl::messaging::ConversationProcessingBaseImpl<>                 base_type;

        typedef base_type::payload_t                                            payload_t;
        typedef base_type::object_dispatch_t                                    object_dispatch_t;

        typedef bl::messaging::BrokerProtocol                                   BrokerProtocol;
        typedef bl::messaging::Payload                                          Payload;

        bl::cpp::ScalarTypeIniter< bool >                                       m_isSender;
        bl::cpp::ScalarTypeIniter< bool >                                       m_useRequestResponseProcessingWrappers;

        bl::cpp::ScalarTypeIniter< int >                                        m_ticks;

        TestConversationProcessing(
            SAA_in          const bool                                          isSender,
            SAA_in          const bl::uuid_t&                                   peerId,
            SAA_in          const bl::uuid_t&                                   targetPeerId,
            SAA_in          const bl::uuid_t&                                   conversationId,
            SAA_in          bl::om::ObjPtr< object_dispatch_t >&&               objectDispatcher,
            SAA_in_opt      std::string&&                                       authenticationCookies,
            SAA_in_opt      MessageInfo&&                                       seedMessage = MessageInfo(),
            SAA_in_opt      const bool                                          useRequestResponseProcessingWrappers = false
            )
            :
            base_type(
                peerId,
                targetPeerId,
                conversationId,
                BL_PARAM_FWD( objectDispatcher ),
                BL_PARAM_FWD( authenticationCookies ),
                BL_PARAM_FWD( seedMessage )
                ),
            m_isSender( isSender ),
            m_useRequestResponseProcessingWrappers( useRequestResponseProcessingWrappers )
        {
        }

        virtual void processCurrentMessage() OVERRIDE
        {
            using namespace bl;
            using namespace bl::tasks;
            using namespace bl::messaging;

            /*
             * The seed message will be sent automatically, so the sender is expected to
             * only receive a response message
             *
             * The receiver is expected to receive a request message then process it and
             * then send a response
             */

            if( m_isSender )
            {
                if( m_ticks )
                {
                    if( m_ticks == 3 )
                    {
                        BL_LOG(
                            bl::Logging::debug(),
                            "Sender finished processing response"
                            );

                        if( m_useRequestResponseProcessingWrappers )
                        {
                            base_type::defaultProcessResponse( "TestConversationProcessing" );
                        }
                        else
                        {
                            m_isFinished = true;
                        }
                    }
                    else
                    {
                        BL_LOG(
                            bl::Logging::debug(),
                            "Sender is processing response..."
                            );
                    }
                }
                else
                {
                    BL_LOG(
                        bl::Logging::debug(),
                        "Sender received response for processing"
                        );
                }

                ++m_ticks.lvalue();
            }
            else
            {
                if( m_ticks )
                {
                    if( m_ticks == 3 )
                    {
                        BL_LOG(
                            bl::Logging::debug(),
                            "Receiver finished processing request"
                            );

                        if( m_useRequestResponseProcessingWrappers )
                        {
                            base_type::defaultProcessRequest( "TestConversationProcessing" );
                        }
                        else
                        {
                            const auto brokerProtocol =
                                MessagingUtils::createResponseProtocolMessage( m_conversationId );

                            const auto payload = bl::dm::DataModelUtils::loadFromFile< payload_t >(
                                utest::TestUtils::resolveDataFilePath( "async_rpc_response.json" )
                                );

                            base_type::sendMessage( true /* isLastMessage */, brokerProtocol, payload );

                            m_currentMessage = MessageInfo();
                        }
                    }
                    else
                    {
                        BL_LOG(
                            bl::Logging::debug(),
                            BL_MSG()
                                << "Receiver is processing request..."
                            );
                    }
                }
                else
                {
                    BL_LOG(
                        bl::Logging::debug(),
                        BL_MSG()
                            << "Receiver received request for processing"
                        );
                }

                ++m_ticks.lvalue();
            }
        }

    public:

        auto getResponse() const -> bl::om::ObjPtr< utest::dm::TestAsyncResponse >
        {
            auto response = bl::dm::DataModelUtils::castTo< utest::dm::TestAsyncResponse >(
                base_type::getAsyncRpcResponseOrThrowIfError()
                );

            BL_CHK(
                false,
                ! response -> finalPath().empty() && response -> hasStarted(),
                BL_MSG()
                    << "The message returned by the remote host does not contain valid response:\n"
                    << bl::dm::DataModelUtils::getDocAsPrettyJsonString( response )
                );

            return bl::om::copy( response );
        }
    };

    typedef bl::om::ObjectImpl< TestConversationProcessing > TestConversationProcessingImpl;

    /*
     * Tester class for the timeouts...
     */

    class TestConversationProcessingTimeouts : public TestConversationProcessing
    {
        BL_DECLARE_OBJECT_IMPL( TestConversationProcessingTimeouts )

    protected:

        typedef TestConversationProcessing                                      base_type;
        typedef base_type::object_dispatch_t                                    object_dispatch_t;

        typedef bl::messaging::BrokerProtocol                                   BrokerProtocol;
        typedef bl::messaging::Payload                                          Payload;

        bl::cpp::ScalarTypeIniter< bool >                                       m_emulateAckTimeout;
        bl::cpp::ScalarTypeIniter< bool >                                       m_emulateMsgTimeout;

        TestConversationProcessingTimeouts(
            SAA_in          const bool                                          isSender,
            SAA_in          const bl::uuid_t&                                   peerId,
            SAA_in          const bl::uuid_t&                                   targetPeerId,
            SAA_in          const bl::uuid_t&                                   conversationId,
            SAA_in          bl::om::ObjPtr< object_dispatch_t >&&               objectDispatcher,
            SAA_in_opt      std::string&&                                       authenticationCookies,
            SAA_in_opt      MessageInfo&&                                       seedMessage = MessageInfo()
            )
            :
            base_type(
                isSender,
                peerId,
                targetPeerId,
                conversationId,
                BL_PARAM_FWD( objectDispatcher ),
                BL_PARAM_FWD( authenticationCookies ),
                BL_PARAM_FWD( seedMessage )
                )
        {
        }

    public:

        void emulateAckTimeout( SAA_in const bool emulateAckTimeout ) NOEXCEPT
        {
            m_emulateAckTimeout = emulateAckTimeout;
        }

        void emulateMsgTimeout( SAA_in const bool emulateMsgTimeout ) NOEXCEPT
        {
            m_emulateMsgTimeout = emulateMsgTimeout;
        }

        virtual void processCurrentMessage() OVERRIDE
        {
            if( ! base_type::m_isSender && m_emulateMsgTimeout )
            {
                BL_ASSERT( ! m_emulateAckTimeout );

                /*
                 * Just swallow the messages and don't respond to anything
                 */

                m_currentMessage = MessageInfo();

                m_isFinished = true;

                return;
            }

            base_type::processCurrentMessage();
        }

        void onProcessing()
        {
            if( ! base_type::m_isSender && m_emulateAckTimeout )
            {
                BL_ASSERT( ! m_emulateMsgTimeout );

                m_isFinished = true;

                return;
            }

            base_type::onProcessing();
        }

        void pushMessage(
            SAA_in              const bl::uuid_t&                                   targetPeerId,
            SAA_in              const bl::om::ObjPtr< BrokerProtocol >&             brokerProtocol,
            SAA_in_opt          const bl::om::ObjPtr< Payload >&                    payload
            )
        {
            if( m_emulateAckTimeout )
            {
                /*
                 * Just swallow the messages and don't respond with acknowledgment messages
                 */

                return;
            }

            base_type::pushMessage( targetPeerId, brokerProtocol, payload );
        }
    };

    typedef bl::om::ObjectImpl< TestConversationProcessingTimeouts > TestConversationProcessingTimeoutsImpl;

} // __unnamed

UTF_AUTO_TEST_CASE( IO_MessagingMessageProcessingTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    typedef bl::messaging::ConversationProcessingBaseImpl<>::payload_t payload_t;

    const auto callbackTests = []() -> void
    {
        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

                const auto targetPeerId1 = uuids::create();
                const auto targetPeerId2 = uuids::create();

                const auto cookiesText = utest::TestMessagingUtils::getTokenData();

                const om::ObjPtrCopyable< om::Proxy > client1Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel1 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client1Sink,
                            targetPeerId1 /* targetPeerIdExpected */,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                const om::ObjPtrCopyable< om::Proxy > client2Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel2 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client2Sink,
                            targetPeerId2 /* targetPeerIdExpected */,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                auto connections1 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                auto connections2 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                const auto client1 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel1 ),
                        dataBlocksPool,
                        targetPeerId1,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections1.first )                     /* inboundConnection */,
                        std::move( connections1.second )                    /* outboundConnection */
                        )
                    );

                const auto client2 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel2 ),
                        dataBlocksPool,
                        targetPeerId2,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections2.first )                     /* inboundConnection */,
                        std::move( connections2.second )                    /* outboundConnection */
                        )
                    );

                typedef TestConversationProcessingImpl::MessageInfo MessageInfo;

                /*
                 * Create and place an initial seed message to be passed by the sender
                 */

                const auto conversationId = uuids::create();

                MessageInfo seedMessage;

                seedMessage.brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    cookiesText
                    );

                seedMessage.payload = bl::dm::DataModelUtils::loadFromFile< payload_t >(
                    utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                    );

                /*
                 * First test the baseline / success scenario... (i.e. a real request /
                 * response conversation)
                 *
                 * task1 will be the logical sender / initiator task and task2 will be the
                 * logical receiver / processing task
                 */

                typedef om::ObjectImpl
                <
                    ConversationProcessingTaskT< TestConversationProcessingImpl >
                >
                processing_task_t;

                auto processor1 = TestConversationProcessingImpl::createInstance(
                    true /* isSender */,
                    targetPeerId1                                           /* peerId (self) */,
                    targetPeerId2                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client1 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    cpp::copy( cookiesText )                                /* authenticationCookies */,
                    cpp::copy( seedMessage )
                    );

                auto processor2 = TestConversationProcessingImpl::createInstance(
                    false /* isSender */,
                    targetPeerId2                                           /* peerId (self) */,
                    targetPeerId1                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client2 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    ""                                                      /* authenticationCookies */
                    );

                const auto task1 = processing_task_t::createInstance< Task >( bl::om::copy( processor1 ) );
                const auto task2 = processing_task_t::createInstance< Task >( std::move( processor2 ) );

                BL_SCOPE_EXIT(
                    {
                        client1Sink -> disconnect();
                        client2Sink -> disconnect();
                    }
                    );

                client1Sink -> connect( task1.get() );
                client2Sink -> connect( task2.get() );

                eq -> push_back( task1 );
                eq -> push_back( task2 );

                /*
                 * Now post an initial message to task1 and wait for the conversation
                 * to finish with the exchange of back and forth messages
                 */

                eq -> wait( task2 );
                eq -> waitForSuccess( task1 );

                const auto response = processor1 -> getResponse();

                UTF_REQUIRE( response );
                UTF_CHECK_EQUAL( response -> hasStarted(), true );

                const auto payload = bl::dm::DataModelUtils::loadFromFile< AsyncRpcPayload >(
                    utest::TestUtils::resolveDataFilePath( "async_rpc_response.json" )
                    );

                const auto expectedResponse =
                    bl::dm::DataModelUtils::castTo< utest::dm::TestAsyncResponse >(
                        payload -> asyncRpcResponse()
                        );

                UTF_CHECK_EQUAL( response -> finalPath(), expectedResponse -> finalPath() );
            }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingMessageProcessingTestWrappers )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    typedef bl::messaging::ConversationProcessingBaseImpl<>::payload_t payload_t;

    const auto callbackTests = []() -> void
    {
        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

                const auto targetPeerId1 = uuids::create();
                const auto targetPeerId2 = uuids::create();

                const auto cookiesText = utest::TestMessagingUtils::getTokenData();

                const om::ObjPtrCopyable< om::Proxy > client1Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel1 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client1Sink,
                            targetPeerId1,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                const om::ObjPtrCopyable< om::Proxy > client2Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel2 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client2Sink,
                            targetPeerId2,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                auto connections1 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                auto connections2 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                const auto client1 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel1 ),
                        dataBlocksPool,
                        targetPeerId1,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections1.first )                     /* inboundConnection */,
                        std::move( connections1.second )                    /* outboundConnection */
                        )
                    );

                const auto client2 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel2 ),
                        dataBlocksPool,
                        targetPeerId2,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections2.first )                     /* inboundConnection */,
                        std::move( connections2.second )                    /* outboundConnection */
                        )
                    );

                typedef TestConversationProcessingImpl::MessageInfo MessageInfo;

                /*
                 * Create and place an initial seed message to be passed by the sender
                 */

                const auto conversationId = uuids::create();

                MessageInfo seedMessage;

                seedMessage.brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    cookiesText
                    );

                seedMessage.payload = bl::dm::DataModelUtils::loadFromFile< payload_t >(
                    utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                    );

                /*
                 * Test the common request processing wrapper. THe 'processRequestImpl()' is not
                 * overridden so the default implementation will throw - the exception will be correctly
                 * transported and rethrown by the call to getResponse()
                 */

                bl::Logging::LineLoggerPusher pushLineLogger( &utest::warningToDebugLineLogger );

                typedef om::ObjectImpl
                <
                    ConversationProcessingTaskT< TestConversationProcessingImpl >
                >
                processing_task_t;

                auto processor1 = TestConversationProcessingImpl::createInstance(
                    true /* isSender */,
                    targetPeerId1                                           /* peerId (self) */,
                    targetPeerId2                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client1 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    cpp::copy( cookiesText )                                /* authenticationCookies */,
                    cpp::copy( seedMessage ),
                    true                                                    /* useProcessRequestWrapper */
                    );

                auto processor2 = TestConversationProcessingImpl::createInstance(
                    false /* isSender */,
                    targetPeerId2                                           /* peerId (self) */,
                    targetPeerId1                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client2 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    ""                                                      /* authenticationCookies */,
                    MessageInfo()                                           /* seedMessage */,
                    true                                                    /* useProcessRequestWrapper */
                    );

                const auto task1 = processing_task_t::createInstance< Task >( bl::om::copy( processor1 ) );
                const auto task2 = processing_task_t::createInstance< Task >( std::move( processor2 ) );

                BL_SCOPE_EXIT(
                    {
                        client1Sink -> disconnect();
                        client2Sink -> disconnect();
                    }
                    );

                client1Sink -> connect( task1.get() );
                client2Sink -> connect( task2.get() );

                eq -> push_back( task1 );
                eq -> push_back( task2 );

                /*
                 * Now post an initial message to task1 and wait for the conversation
                 * to finish with the exchange of back and forth messages
                 */

                eq -> wait( task2 );
                eq -> waitForSuccess( task1 );

                UTF_CHECK_THROW( processor1 -> getResponse(), bl::UnexpectedException );
                }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingMessageProcessingTestAckTimeout )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    typedef bl::messaging::ConversationProcessingBaseImpl<>::payload_t payload_t;

    const auto callbackTests = []() -> void
    {
        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

                const auto targetPeerId1 = uuids::create();
                const auto targetPeerId2 = uuids::create();

                const auto cookiesText = utest::TestMessagingUtils::getTokenData();

                const om::ObjPtrCopyable< om::Proxy > client1Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel1 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client1Sink,
                            targetPeerId1,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                const om::ObjPtrCopyable< om::Proxy > client2Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel2 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client2Sink,
                            targetPeerId2,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                auto connections1 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                auto connections2 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                const auto client1 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel1 ),
                        dataBlocksPool,
                        targetPeerId1,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections1.first )                     /* inboundConnection */,
                        std::move( connections1.second )                    /* outboundConnection */
                        )
                    );

                const auto client2 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel2 ),
                        dataBlocksPool,
                        targetPeerId2,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections2.first )                     /* inboundConnection */,
                        std::move( connections2.second )                    /* outboundConnection */
                        )
                    );

                typedef TestConversationProcessingImpl::MessageInfo MessageInfo;

                /*
                 * Create and place an initial seed message to be passed by the sender
                 */

                const auto conversationId = uuids::create();

                MessageInfo seedMessage;

                seedMessage.brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    cookiesText
                    );

                seedMessage.payload = bl::dm::DataModelUtils::loadFromFile< payload_t >(
                    utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                    );

                /*
                 * Now let's test the timeout scenarios...
                 *
                 * timeoutTask1 will be the logical sender / initiator task and timeoutTask2 will
                 * be the logical receiver / processing task
                 */

                typedef om::ObjectImpl
                <
                    ConversationProcessingTaskT< TestConversationProcessingTimeoutsImpl >
                >
                processing_task_t;

                /*
                 * Set the execution queue to not keep the tasks, so we can flush it
                 * safely even when some of the tasks fail
                 */

                eq -> setOptions( ExecutionQueue::OptionKeepNone );

                auto processor1 = TestConversationProcessingTimeoutsImpl::createInstance(
                    true /* isSender */,
                    targetPeerId1                                           /* peerId (self) */,
                    targetPeerId2                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client1 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    cpp::copy( cookiesText )                                /* authenticationCookies */,
                    cpp::copy( seedMessage )
                    );

                auto processor2 = TestConversationProcessingTimeoutsImpl::createInstance(
                    false /* isSender */,
                    targetPeerId2                                           /* peerId (self) */,
                    targetPeerId1                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client2 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    ""                                                      /* authenticationCookies */
                    );

                /*
                 * For the timeout we are going to emulate we set it to some small value
                 * (smallTimeoutInSeconds) and then we set all other timeouts for 5x this
                 * value to ensure they are normally never hit (unless there is a bug)
                 *
                 * We don't want to leave the defaults for ackTimeout() and msgTimeout()
                 * because they are too big, but 5 x smallTimeoutInSeconds is better
                 * (otherwise in the case of an error it would take too long for the test
                 * to complete)
                 */

                const auto smallTimeoutInSeconds = 3L;

                processor1 -> ackTimeout( time::seconds( smallTimeoutInSeconds ) );
                processor1 -> msgTimeout( time::seconds( 5 * smallTimeoutInSeconds ) );

                processor2 -> ackTimeout( time::seconds( 5 * smallTimeoutInSeconds ) );
                processor2 -> msgTimeout( time::seconds( 5 * smallTimeoutInSeconds ) );

                processor2 -> emulateAckTimeout( true );

                const auto task1 = processing_task_t::createInstance< Task >( std::move( processor1 ) );
                const auto task2 = processing_task_t::createInstance< Task >( std::move( processor2 ) );

                BL_SCOPE_EXIT(
                    {
                        client1Sink -> disconnect();
                        client2Sink -> disconnect();
                    }
                    );

                client1Sink -> connect( task1.get() );
                client2Sink -> connect( task2.get() );

                eq -> push_back( task1 );
                eq -> push_back( task2 );

                eq -> wait( task1 );
                eq -> wait( task2 );

                UTF_REQUIRE( task1 -> isFailed() );
                UTF_REQUIRE( ! task2 -> isFailed() );

                UTF_REQUIRE_THROW_MESSAGE(
                    cpp::safeRethrowException( task1 -> exception() ),
                    TimeoutException,
                    "Messaging client did not receive acknowledgment within the specified interval"
                    );
            }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingMessageProcessingTestMsgTimeout )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    typedef bl::messaging::ConversationProcessingBaseImpl<>::payload_t payload_t;

    const auto callbackTests = []() -> void
    {
        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eq ) -> void
            {
                const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

                const auto targetPeerId1 = uuids::create();
                const auto targetPeerId2 = uuids::create();

                const auto cookiesText = utest::TestMessagingUtils::getTokenData();

                const om::ObjPtrCopyable< om::Proxy > client1Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel1 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client1Sink,
                            targetPeerId1,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                const om::ObjPtrCopyable< om::Proxy > client2Sink =
                    om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

                const auto incomingObjectChannel2 = bl::om::lockDisposable(
                    MessagingClientObjectDispatchFromCallback::createInstance(
                        cpp::bind(
                            &utest::TestMessagingUtils::dispatchCallback,
                            client2Sink,
                            targetPeerId2,
                            _1,
                            _2,
                            _3
                            )
                        )
                    );

                auto connections1 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                auto connections2 = MessagingClientFactorySsl::createEstablishedConnections(
                    "localhost"                                             /* host */,
                    test::UtfArgsParser::port()                             /* inboundPort */,
                    test::UtfArgsParser::port() + 1                         /* outboundPort */
                    );

                const auto client1 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel1 ),
                        dataBlocksPool,
                        targetPeerId1,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections1.first )                     /* inboundConnection */,
                        std::move( connections1.second )                    /* outboundConnection */
                        )
                    );

                const auto client2 = bl::om::lockDisposable(
                    MessagingClientObjectFactory::createFromObjectDispatchTcp(
                        om::qi< MessagingClientObjectDispatch >( incomingObjectChannel2 ),
                        dataBlocksPool,
                        targetPeerId2,
                        "localhost"                                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections2.first )                     /* inboundConnection */,
                        std::move( connections2.second )                    /* outboundConnection */
                        )
                    );

                typedef TestConversationProcessingImpl::MessageInfo MessageInfo;

                /*
                 * Create and place an initial seed message to be passed by the sender
                 */

                const auto conversationId = uuids::create();

                MessageInfo seedMessage;

                seedMessage.brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    cookiesText
                    );

                seedMessage.payload = bl::dm::DataModelUtils::loadFromFile< payload_t >(
                    utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                    );

                /*
                 * Now let's test the timeout scenarios...
                 *
                 * timeoutTask1 will be the logical sender / initiator task and timeoutTask2 will
                 * be the logical receiver / processing task
                 */

                typedef om::ObjectImpl
                <
                    ConversationProcessingTaskT< TestConversationProcessingTimeoutsImpl >
                >
                processing_task_t;

                /*
                 * Set the execution queue to not keep the tasks, so we can flush it
                 * safely even when some of the tasks fail
                 */

                eq -> setOptions( ExecutionQueue::OptionKeepNone );

                auto processor1 = TestConversationProcessingTimeoutsImpl::createInstance(
                    true /* isSender */,
                    targetPeerId1                                           /* peerId (self) */,
                    targetPeerId2                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client1 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    cpp::copy( cookiesText )                                /* authenticationCookies */,
                    cpp::copy( seedMessage )
                    );

                auto processor2 = TestConversationProcessingTimeoutsImpl::createInstance(
                    false /* isSender */,
                    targetPeerId2                                           /* peerId (self) */,
                    targetPeerId1                                           /* targetPeerId (the target) */,
                    conversationId,
                    om::copy( client2 -> outgoingObjectChannel() )          /* objectDispatcher */,
                    ""                                                      /* authenticationCookies */
                    );

                /*
                 * For the timeout we are going to emulate we set it to some small value
                 * (smallTimeoutInSeconds) and then we set all other timeouts for 5x this
                 * value to ensure they are normally never hit (unless there is a bug)
                 *
                 * We don't want to leave the defaults for ackTimeout() and msgTimeout()
                 * because they are too big, but 5 x smallTimeoutInSeconds is better
                 * (otherwise in the case of an error it would take too long for the test
                 * to complete)
                 */

                const auto smallTimeoutInSeconds = 3L;

                processor1 -> ackTimeout( time::seconds( 5 * smallTimeoutInSeconds ) );
                processor1 -> msgTimeout( time::seconds( smallTimeoutInSeconds ) );

                processor2 -> ackTimeout( time::seconds( 5 * smallTimeoutInSeconds ) );
                processor2 -> msgTimeout( time::seconds( 5 * smallTimeoutInSeconds ) );

                processor2 -> emulateMsgTimeout( true );

                const auto task1 = processing_task_t::createInstance< Task >( std::move( processor1 ) );
                const auto task2 = processing_task_t::createInstance< Task >( std::move( processor2 ) );

                BL_SCOPE_EXIT(
                    {
                        client1Sink -> disconnect();
                        client2Sink -> disconnect();
                    }
                    );

                client1Sink -> connect( task1.get() );
                client2Sink -> connect( task2.get() );

                eq -> push_back( task1 );
                eq -> push_back( task2 );

                eq -> wait( task1 );
                eq -> wait( task2 );

                UTF_REQUIRE( task1 -> isFailed() );
                UTF_REQUIRE( ! task2 -> isFailed() );

                UTF_REQUIRE_THROW_MESSAGE(
                    cpp::safeRethrowException( task1 -> exception() ),
                    TimeoutException,
                    "Messaging client did not receive response within the specified interval"
                    );
            }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingPerfTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    if( ! test::UtfArgsParser::isClient() )
    {
        /*
         * This is a manual test
         */

        return;
    }

    typedef utest::TestMessagingUtils utils_t;

    std::atomic< int > noOfMessagesInFlight( 0 );
    std::atomic< std::size_t > noOfMessagesDelivered( 0U );

    const auto completionCallback = []( SAA_in_opt const std::exception_ptr& eptr ) NOEXCEPT -> void
    {
        if( eptr )
        {
            /*
             * In our specific test case scenario the broker error codes are expected,
             * so we should filter these out first before we invoke the default handler
             */

            const auto ec = eh::errorCodeFromExceptionPtr( eptr );

            if( BrokerErrorCodes::isExpectedErrorCode( ec ) )
            {
                return;
            }
        }

        utils_t::client_t::completionCallbackDefault( eptr );
    };

    const auto incomingSink = om::lockDisposable(
        MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
            [ & ](
                SAA_in              const bl::uuid_t&                               targetPeerId,
                SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                )
                -> void
            {
                BL_UNUSED( targetPeerId );
                BL_UNUSED( brokerProtocol );
                BL_UNUSED( payload );

                --noOfMessagesInFlight;
                ++noOfMessagesDelivered;
            }
            )
        );

    const om::ObjPtrCopyable< om::Proxy > clientSink =
        om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

    const auto incomingObjectChannel = om::lockDisposable(
        MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
            cpp::bind(
                &utest::TestMessagingUtils::dispatchCallback,
                clientSink,
                uuids::nil()    /* targetPeerIdExpected */,
                _1              /* targetPeerId */,
                _2              /* brokerProtocol */,
                _3              /* payload */
                )
            )
        );

    utils_t::executeMessagingTests(
        incomingObjectChannel,
        [ & ](
            SAA_in          const std::string&                                      cookiesText,
            SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
            SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
            SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
            SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
            ) -> void
        {
            const auto conversationId = uuids::create();

            const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                MessageType::AsyncRpcDispatch,
                conversationId,
                cookiesText
                );

            const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                );

            const auto block =
                MessagingUtils::serializeObjectsToBlock( brokerProtocol, payload, dataBlocksPool );

            const auto messageSize = block -> size();

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Messages size is "
                    << messageSize
                );

            /*
             * Create UtfArgsParser::connections() messaging clients backed by the same
             * async wrapper and then perform perf tests by sending and processing
             * messages
             *
             * Note that these don't own the backend and the queue, so when they get
             * disposed they will not actually dispose the backend, but just tear down
             * the connections
             */

            auto connections = utils_t::createConnections();

            UTF_REQUIRE_EQUAL( connections.size(), test::UtfArgsParser::connections() );
            UTF_REQUIRE( test::UtfArgsParser::connections() > 1 );

            utils_t::clients_list_t clients;

            clients.reserve( test::UtfArgsParser::connections() );

            for( std::size_t i = 0; i < test::UtfArgsParser::connections(); ++i )
            {
                const auto peerId = uuids::create();

                auto blockDispatch = om::lockDisposable(
                    utils_t::client_factory_t::createWithSmartDefaults(
                        om::copy( eq ),
                        peerId,
                        om::copy( backend ),
                        om::copy( asyncWrapper ),
                        test::UtfArgsParser::host()                         /* host */,
                        test::UtfArgsParser::port()                         /* inboundPort */,
                        test::UtfArgsParser::port() + 1                     /* outboundPort */,
                        std::move( connections[ i ].first )                 /* inboundConnection */,
                        std::move( connections[ i ].second )                /* outboundConnection */,
                        om::copy( dataBlocksPool )
                        )
                    );

                {
                    const auto clientImpl = om::qi< utils_t::client_t >( blockDispatch );

                    clientImpl -> completionCallback( cpp::copy( completionCallback ) );
                }

                auto client = om::lockDisposable(
                    MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                        om::qi< MessagingClientBlockDispatch >( blockDispatch ),
                        dataBlocksPool
                        )
                    );

                blockDispatch.detachAsObjPtr();

                clients.emplace_back( std::make_pair( peerId, std::move( client ) ) );
            }

            bool cancelRequested = false;

            const auto loadGeneratorTask = SimpleTaskImpl::createInstance< Task >(
                [ & ]() -> void
                {
                    const auto startTime = time::microsec_clock::universal_time();

                    const int maxMessagesInFlight = static_cast< int >(
                        clients.size() * ( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 3U )
                        );

                    const double messagesPerSecondDelta = std::min< double >( 0.05 * maxMessagesInFlight, 200.0 );
                    const double elapsedInSecondsDelta = 2.0;

                    double messagesPerSecondLast = 0.0;
                    double elapsedInSecondsLast = 0.0;

                    for( ;; )
                    {
                        if( cancelRequested )
                        {
                            break;
                        }

                        if( noOfMessagesInFlight >= maxMessagesInFlight )
                        {
                            os::sleep( time::milliseconds( 100L )  );
                            continue;
                        }

                        /*
                         * Choose two clients at random and send message between them
                         */

                        const auto pos1 = random::getUniformRandomUnsignedValue< std::size_t >( clients.size() - 1U );
                        const auto pos2 = random::getUniformRandomUnsignedValue< std::size_t >( clients.size() - 1U );

                        if( pos1 == pos2 )
                        {
                            /*
                             * The sender and the receiver can't be the same; try again...
                             */

                            continue;
                        }

                        const auto& client = clients[ pos1 ].second;
                        const auto& targetPeerId = clients[ pos2 ].first;

                        try
                        {
                            client -> outgoingObjectChannel() -> pushMessage(
                                targetPeerId,
                                brokerProtocol,
                                payload
                                );
                        }
                        catch( bl::ServerErrorException& e )
                        {
                            const auto* ec = e.errorCode();

                            if( ec && eh::errc::make_error_code( BrokerErrorCodes::TargetPeerQueueFull ) == *ec )
                            {
                                /*
                                 * The queue of this specific client is full
                                 *
                                 * Let's find another one...
                                 */

                                continue;
                            }

                            throw;
                        }

                        ++noOfMessagesInFlight;

                        const auto duration = time::microsec_clock::universal_time() - startTime;

                        const auto elapsedInSeconds = duration.total_milliseconds() / 1000.0;

                        if( std::abs( elapsedInSeconds - elapsedInSecondsLast ) < elapsedInSecondsDelta )
                        {
                            continue;
                        }

                        elapsedInSecondsLast = elapsedInSeconds;

                        const auto messagesPerSecond = noOfMessagesDelivered / elapsedInSeconds;

                        if( std::abs( messagesPerSecond - messagesPerSecondLast ) > messagesPerSecondDelta )
                        {
                            /*
                             * Only print if there is a meaningful delta relative to last time
                             */

                            const auto kbytesPerSecond =
                                ( noOfMessagesDelivered * messageSize ) / elapsedInSeconds / 1024.0;

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Messages per second is "
                                    << messagesPerSecond
                                    << "; kilobytes per second is "
                                    << kbytesPerSecond
                                );

                            messagesPerSecondLast = messagesPerSecond;
                        }
                    }
                }
                );

            {
                BL_SCOPE_EXIT(
                    {
                        clientSink -> disconnect();
                    }
                    );

                clientSink -> connect( incomingSink.get() );

                eq -> push_back( loadGeneratorTask );

                {
                    BL_SCOPE_EXIT(
                        {
                            cancelRequested = true;
                            waitForSuccessOrCancel( eq, loadGeneratorTask );
                        }
                        );

                    utils_t::waitForKeyOrTimeout();
                }
            }
        }
        );
}

UTF_AUTO_TEST_CASE( RotatingMessagingClientObjectDispatchTests )
{
    using namespace bl;
    using namespace bl::messaging;
    using namespace bl::dm::messaging;

    typedef RotatingMessagingClientObjectDispatch rotating_dispatcher_t;

    const std::size_t objectCount = 10;
    std::vector< std::size_t > usageCount( objectCount );

    const auto createDispatchers =
        [ & ]() -> rotating_dispatcher_t::DispatchList
        {
            rotating_dispatcher_t::DispatchList dispatchers;

            for( size_t n = 0; n < objectCount; ++n )
            {
                dispatchers.emplace_back(
                    MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                        [ &usageCount, n ](
                            SAA_in              const bl::uuid_t&                               targetPeerId,
                            SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                            SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                            ) -> void
                            {
                                BL_UNUSED( targetPeerId );
                                BL_UNUSED( brokerProtocol );
                                BL_UNUSED( payload );

                                ++usageCount[ n ];
                            }
                        )
                    );
            }

            return dispatchers;
        };

    /*
     * Check that all dispatchers are evenly used as each pushMesage should select the next one
     */

    auto rotatingDispatcher =
        rotating_dispatcher_t::createInstance< MessagingClientObjectDispatch >( createDispatchers() );

    const auto brokerProtocol = createProtocolMessage();

    for( std::size_t n = 0; n < objectCount * 2; ++n )
    {
        rotatingDispatcher -> pushMessage( uuids::nil() /* targetPeerId */, brokerProtocol, nullptr /* payload */ );
    }

    for( std::size_t n = 0; n < objectCount; ++n )
    {
        UTF_REQUIRE_EQUAL( 2U, usageCount[ n ] );
    }

    /*
     * Check that disposed dispatchers aren't used
     */

    usageCount = std::vector< size_t >( objectCount );

    auto dispatchers = createDispatchers();

    for( std::size_t n = 0; n < objectCount; n += 2 )
    {
        om::qi< MessagingClientObjectDispatchFromCallback >( dispatchers[ n ] ) -> dispose();
    }

    rotatingDispatcher =
        rotating_dispatcher_t::createInstance< MessagingClientObjectDispatch >( std::move( dispatchers ) );

    for( std::size_t n = 0; n < objectCount * 2; ++n )
    {
        rotatingDispatcher -> pushMessage( uuids::nil() /* targetPeerId */, brokerProtocol, nullptr /* payload */ );
    }

    for( std::size_t n = 0; n < objectCount; ++n )
    {
        UTF_REQUIRE_EQUAL( ( n % 2 == 0 ) ? 0U : 4U, usageCount[ n ] );
    }

    /*
     * Check that the initially selected dispatcher is random
     */

    const auto getUsedIndex =
        [ & ]() -> std::size_t
        {
            for( std::size_t n = 0; n < usageCount.size(); ++n )
            {
                if( usageCount[ n ] > 0 )
                {
                    return n;
                }
            }

            UTF_FAIL( "No dispatcher was found");

            return 0;
        };

    std::unordered_set< std::size_t > usedIndices;

    for( std::size_t n = 0; n < 100; ++n )
    {
        usageCount = std::vector< size_t >( objectCount );

        rotatingDispatcher =
            rotating_dispatcher_t::createInstance< MessagingClientObjectDispatch >( createDispatchers() );

        rotatingDispatcher -> pushMessage( uuids::nil() /* targetPeerId */, brokerProtocol, nullptr /* payload */ );

        usedIndices.emplace( getUsedIndex() );
    }

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << usedIndices.size()
            << " out of "
            << objectCount
            << " dispatchers selected randomly after 100 iterations"
        );

    UTF_REQUIRE( usedIndices.size() > 1 );
}

UTF_AUTO_TEST_CASE( RotatingMessagingClientBlockDispatchTests )
{
    using namespace bl;
    using namespace bl::messaging;
    using namespace bl::dm::messaging;

    typedef RotatingMessagingClientBlockDispatch rotating_dispatcher_t;

    const std::size_t objectCount = 10;
    std::vector< std::size_t > usageCount( objectCount );

    const auto createDispatchers =
        [ & ]() -> rotating_dispatcher_t::DispatchList
        {
            rotating_dispatcher_t::DispatchList dispatchers;

            for( size_t n = 0; n < objectCount; ++n )
            {
                dispatchers.emplace_back(
                    MessagingClientBlockDispatchFromCallback::createInstance< MessagingClientBlockDispatch >(
                        [ &usageCount, n ](
                            SAA_in              const bl::uuid_t&                               targetPeerId,
                            SAA_in              const bl::om::ObjPtr< bl::data::DataBlock >&    dataBlock
                            ) -> void
                            {
                                BL_UNUSED( targetPeerId );
                                BL_UNUSED( dataBlock );

                                ++usageCount[ n ];
                            }
                        )
                    );
            }

            return dispatchers;
        };

    /*
     * Check that all dispatchers are evenly used as each pushMesage should select the next one
     */

    auto rotatingDispatcher =
        rotating_dispatcher_t::createInstance< MessagingClientBlockDispatch >( createDispatchers() );

    const auto brokerProtocol = createProtocolMessage();

    const auto dataBlock = MessagingUtils::serializeObjectsToBlock( brokerProtocol, nullptr /* payload */ );

    for( std::size_t n = 0; n < objectCount * 2; ++n )
    {
        rotatingDispatcher -> pushBlock( uuids::nil() /* targetPeerId */, dataBlock );
    }

    for( std::size_t n = 0; n < objectCount; ++n )
    {
        UTF_REQUIRE_EQUAL( 2U, usageCount[ n ] );
    }

    /*
     * Check that disposed dispatchers aren't used
     */

    usageCount = std::vector< size_t >( objectCount );

    auto dispatchers = createDispatchers();

    for( std::size_t n = 0; n < objectCount; n += 2 )
    {
        om::qi< MessagingClientBlockDispatchFromCallback >( dispatchers[ n ] ) -> dispose();
    }

    rotatingDispatcher =
        rotating_dispatcher_t::createInstance< MessagingClientBlockDispatch >( std::move( dispatchers ) );

    for( std::size_t n = 0; n < objectCount * 2; ++n )
    {
        rotatingDispatcher -> pushBlock( uuids::nil() /* targetPeerId */, dataBlock );
    }

    for( std::size_t n = 0; n < objectCount; ++n )
    {
        UTF_REQUIRE_EQUAL( ( n % 2 == 0 ) ? 0U : 4U, usageCount[ n ] );
    }

    /*
     * Check that the initially selected dispatcher is random
     */

    const auto getUsedIndex =
        [ & ]() -> std::size_t
        {
            for( std::size_t n = 0; n < usageCount.size(); ++n )
            {
                if( usageCount[ n ] > 0 )
                {
                    return n;
                }
            }

            UTF_FAIL( "No dispatcher was found");

            return 0;
        };

    std::unordered_set< std::size_t > usedIndices;

    for( std::size_t n = 0; n < 100; ++n )
    {
        usageCount = std::vector< size_t >( objectCount );

        rotatingDispatcher =
            rotating_dispatcher_t::createInstance< MessagingClientBlockDispatch >( createDispatchers() );

        rotatingDispatcher -> pushBlock( uuids::nil() /* targetPeerId */, dataBlock );

        usedIndices.emplace( getUsedIndex() );
    }

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << usedIndices.size()
            << " out of "
            << objectCount
            << " dispatchers selected randomly after 100 iterations"
        );

    UTF_REQUIRE( usedIndices.size() > 1 );
}

UTF_AUTO_TEST_CASE( IO_MessagingDemultiplexingTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto callbackTests = []() -> void
    {
        typedef utest::TestMessagingUtils utils_t;

        std::atomic< std::size_t > noOfMessagesDelivered( 0U );

        const auto incomingSink = om::lockDisposable(
            MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                [ & ](
                    SAA_in              const bl::uuid_t&                               targetPeerId,
                    SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                    SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                    )
                    -> void
                {
                    BL_UNUSED( targetPeerId );
                    BL_UNUSED( brokerProtocol );
                    BL_UNUSED( payload );

                    ++noOfMessagesDelivered;
                }
                )
            );

        const om::ObjPtrCopyable< om::Proxy > clientSink =
            om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

        const auto incomingObjectChannel = om::lockDisposable(
            MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                cpp::bind(
                    &utest::TestMessagingUtils::dispatchCallback,
                    clientSink,
                    uuids::nil()    /* targetPeerIdExpected */,
                    _1              /* targetPeerId */,
                    _2              /* brokerProtocol */,
                    _3              /* payload */
                    )
                )
            );

        utils_t::executeMessagingTests(
            incomingObjectChannel,
            [ & ](
                SAA_in          const std::string&                                      cookiesText,
                SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
                SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
                SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
                SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
                ) -> void
            {
                const auto peerId1 = uuids::create();
                const auto peerId2 = uuids::create();

                const auto conversationId = uuids::create();

                const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                    MessageType::AsyncRpcDispatch,
                    conversationId,
                    cookiesText
                    );

                const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                    utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                    );

                /*
                 * Create UtfArgsParser::connections() messaging clients backed by the same
                 * async wrapper and then perform perf tests by sending and processing
                 * messages
                 *
                 * Note that these don't own the backend and the queue, so when they get
                 * disposed they will not actually dispose the backend, but just tear down
                 * the connections
                 */

                auto connections = utils_t::createConnections();

                UTF_REQUIRE_EQUAL( connections.size(), test::UtfArgsParser::connections() );
                UTF_REQUIRE( test::UtfArgsParser::connections() > 1 );

                utils_t::clients_list_t clients;

                clients.reserve( test::UtfArgsParser::connections() );

                for( std::size_t i = 0; i < test::UtfArgsParser::connections(); ++i )
                {
                    /*
                     * Choose the peer id based on if it is odd  vs even index
                     */

                    const auto& peerId = ( 0 == i % 2 ) ? peerId1 : peerId2;

                    auto blockDispatch = om::lockDisposable(
                        utils_t::client_factory_t::createWithSmartDefaults(
                            om::copy( eq ),
                            peerId,
                            om::copy( backend ),
                            om::copy( asyncWrapper ),
                            test::UtfArgsParser::host()                         /* host */,
                            test::UtfArgsParser::port()                         /* inboundPort */,
                            test::UtfArgsParser::port() + 1                     /* outboundPort */,
                            std::move( connections[ i ].first )                 /* inboundConnection */,
                            std::move( connections[ i ].second )                /* outboundConnection */,
                            om::copy( dataBlocksPool )
                            )
                        );

                    auto client = om::lockDisposable(
                        MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                            om::qi< MessagingClientBlockDispatch >( blockDispatch ),
                            dataBlocksPool
                            )
                        );

                    blockDispatch.detachAsObjPtr();

                    clients.emplace_back( std::make_pair( peerId, std::move( client ) ) );
                }

                /*
                 * Just execute a bunch of messages at random and then verify that they have arrived
                 * and that all channels were used fairly
                 */

                {
                    BL_SCOPE_EXIT(
                        {
                            clientSink -> disconnect();
                        }
                        );

                    clientSink -> connect( incomingSink.get() );

                    scheduleAndExecuteInParallel(
                        [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eqLocal ) -> void
                        {
                            eqLocal -> setOptions( ExecutionQueue::OptionKeepFailed );
                            eqLocal -> setThrottleLimit( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2U );

                            const std::size_t noOfBlocks = 20 * clients.size();

                            for( std::size_t i = 0U; i < noOfBlocks; ++i )
                            {
                                const auto pos1 = i % clients.size();
                                const auto pos2 = ( i + 1 ) % clients.size();

                                const auto& client = clients[ pos1 ].second;
                                const auto& targetPeerId = clients[ pos2 ].first;

                                eqLocal -> push_back(
                                    ExternalCompletionTaskImpl::createInstance< Task >(
                                        cpp::bind(
                                            &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                            om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                client -> outgoingObjectChannel().get()
                                                ),
                                            targetPeerId,
                                            om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                            om::ObjPtrCopyable< Payload >( payload ),
                                            _1 /* onReady - the completion callback */
                                            )
                                        )
                                    );

                                /*
                                 * Send at least one request successfully before we start parallelizing
                                 * the rest of the requests to avoid unnecessary request to Janus
                                 */

                                if( 0U == i )
                                {
                                    eqLocal -> flush();
                                }
                            }

                            eqLocal -> flush();

                            /*
                             * Verify that all messages were delivered successfully and the
                             * message distribution over the channels was uniform
                             */

                            UTF_REQUIRE_EQUAL( noOfMessagesDelivered, noOfBlocks );

                            utils_t::verifyUniformMessageDistribution( clients );
                        }
                        );
                }
            }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingMultiplexingTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto callbackTests = []() -> void
    {
        typedef utest::TestMessagingUtils utils_t;

        const auto executeTests = [](
            SAA_in          const std::size_t                                           noOfConnections,
            SAA_in_opt      const bl::uuid_t                                            peerId
            )
            -> void
        {
            UTF_REQUIRE( noOfConnections >= 1 );

            std::atomic< std::size_t > noOfMessagesDelivered( 0U );

            const auto incomingSink = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    [ & ](
                        SAA_in              const bl::uuid_t&                               targetPeerId,
                        SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                        SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                        )
                        -> void
                    {
                        BL_UNUSED( targetPeerId );
                        BL_UNUSED( brokerProtocol );
                        BL_UNUSED( payload );

                        ++noOfMessagesDelivered;
                    }
                    )
                );

            const om::ObjPtrCopyable< om::Proxy > clientSink =
                om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

            const auto incomingObjectChannel = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    cpp::bind(
                        &utest::TestMessagingUtils::dispatchCallback,
                        clientSink,
                        uuids::nil()    /* targetPeerIdExpected */,
                        _1              /* targetPeerId */,
                        _2              /* brokerProtocol */,
                        _3              /* payload */
                        )
                    )
                );

            utils_t::executeMessagingTests(
                incomingObjectChannel,
                [ & ](
                    SAA_in          const std::string&                                      cookiesText,
                    SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
                    SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
                    SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
                    SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
                    ) -> void
                {
                    const auto conversationId = uuids::create();

                    const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        cookiesText
                        );

                    const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                        );

                    /*
                     * Create noOfConnections messaging clients backed by the same async wrapper
                     *
                     * Note that these don't own the backend and the queue, so when they get
                     * disposed they will not actually dispose the backend, but just tear down
                     * the connections
                     */

                    auto connections = utils_t::createNoOfConnections( noOfConnections );
                    UTF_REQUIRE_EQUAL( connections.size(), noOfConnections );

                    utils_t::clients_list_t clients;

                    clients.reserve( noOfConnections );

                    for( std::size_t i = 0; i < noOfConnections; ++i )
                    {
                        const auto clientPeerId = uuids::nil() == peerId ? uuids::create() : peerId;

                        /*
                         * The peer id is either fixed or a unique one is generated for each connection
                         */

                        auto blockDispatch = om::lockDisposable(
                            utils_t::client_factory_t::createWithSmartDefaults(
                                om::copy( eq ),
                                clientPeerId,
                                om::copy( backend ),
                                om::copy( asyncWrapper ),
                                test::UtfArgsParser::host()                         /* host */,
                                test::UtfArgsParser::port()                         /* inboundPort */,
                                test::UtfArgsParser::port() + 1                     /* outboundPort */,
                                std::move( connections[ i ].first )                 /* inboundConnection */,
                                std::move( connections[ i ].second )                /* outboundConnection */,
                                om::copy( dataBlocksPool )
                                )
                            );

                        auto client = om::lockDisposable(
                            MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                om::qi< MessagingClientBlockDispatch >( blockDispatch ),
                                dataBlocksPool
                                )
                            );

                        blockDispatch.detachAsObjPtr();

                        clients.emplace_back( std::make_pair( clientPeerId, std::move( client ) ) );
                    }

                    /*
                     * Just execute a bunch of messages at random and then verify that they have arrived
                     * and that all channels were used fairly
                     */

                    {
                        BL_SCOPE_EXIT(
                            {
                                clientSink -> disconnect();
                            }
                            );

                        clientSink -> connect( incomingSink.get() );

                        scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eqLocal ) -> void
                            {
                                eqLocal -> setOptions( ExecutionQueue::OptionKeepFailed );
                                eqLocal -> setThrottleLimit( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2U );

                                /*
                                 * Create a number of logical peer ids an register them with the backend
                                 */

                                const std::size_t noOfLogicalPeerIds = 5 * clients.size();

                                std::vector< std::pair< bl::uuid_t, bl::uuid_t > > logicalPeerIds;
                                logicalPeerIds.reserve( noOfLogicalPeerIds );

                                for( std::size_t i = 0; i < noOfLogicalPeerIds; ++i )
                                {
                                    const auto logicalPeerId = uuids::create();

                                    const auto pos = i % clients.size();

                                    const auto& client = clients[ pos ].second;
                                    const auto& physicalTargetPeerId = clients[ pos ].first;

                                    const auto associateMessage = utest::TestMessagingUtils::createBrokerProtocolMessage(
                                        MessageType::BackendAssociateTargetPeerId,
                                        bl::uuids::create() /* conversationId */,
                                        "" /* cookiesText */
                                        );

                                    associateMessage -> sourcePeerId( bl::uuids::uuid2string( physicalTargetPeerId ) );
                                    associateMessage -> targetPeerId( bl::uuids::uuid2string( logicalPeerId ) );

                                    eqLocal -> push_back(
                                        ExternalCompletionTaskImpl::createInstance< Task >(
                                            cpp::bind(
                                                &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                    client -> outgoingObjectChannel().get()
                                                    ),
                                                uuids::create() /* targetPeerId */,
                                                om::ObjPtrCopyable< BrokerProtocol >( associateMessage ),
                                                om::ObjPtrCopyable< Payload >(),
                                                _1 /* onReady - the completion callback */
                                                )
                                            )
                                        );

                                    logicalPeerIds.push_back( std::make_pair( logicalPeerId, physicalTargetPeerId ) );
                                }

                                eqLocal -> flush();

                                /*
                                 * The number of delivered messages is expected to be zero as these
                                 * messages are handled by the broker and never dispatched to the client
                                 */

                                UTF_REQUIRE_EQUAL( noOfMessagesDelivered, 0U );

                                {
                                    /*
                                     * Send a bunch of messages to the logical peer ids and verify that they
                                     * arrived correctly
                                     */

                                    const std::size_t noOfBlocks = 10 * noOfLogicalPeerIds;

                                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                                    {
                                        const auto pos1 = i % clients.size();
                                        const auto pos2 = i % noOfLogicalPeerIds;

                                        const auto& client = clients[ pos1 ].second;
                                        const auto& targetPeerId = logicalPeerIds[ pos2 ].first;

                                        eqLocal -> push_back(
                                            ExternalCompletionTaskImpl::createInstance< Task >(
                                                cpp::bind(
                                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                        client -> outgoingObjectChannel().get()
                                                        ),
                                                    targetPeerId,
                                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                                    om::ObjPtrCopyable< Payload >( payload ),
                                                    _1 /* onReady - the completion callback */
                                                    )
                                                )
                                            );

                                        /*
                                         * Send at least one request successfully before we start parallelizing
                                         * the rest of the requests to avoid unnecessary request to Janus
                                         */

                                        if( 0U == i )
                                        {
                                            eqLocal -> flush();
                                        }
                                    }

                                    eqLocal -> flush();

                                    /*
                                     * Verify that all messages were delivered successfully and the
                                     * message distribution over the channels was uniform
                                     */

                                    UTF_REQUIRE_EQUAL( noOfMessagesDelivered, noOfBlocks );

                                    utils_t::verifyUniformMessageDistribution( clients );
                                }

                                {
                                    /*
                                     * Send some messages to the physical client peer ids and verify
                                     * that these are also delivered
                                     */

                                    noOfMessagesDelivered = 0;

                                    const std::size_t noOfBlocks = 10 * clients.size();

                                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                                    {
                                        const auto pos1 = i % clients.size();
                                        const auto pos2 = ( i + 1 ) % clients.size();

                                        const auto& client = clients[ pos1 ].second;
                                        const auto& targetPeerId = clients[ pos2 ].first;

                                        eqLocal -> push_back(
                                            ExternalCompletionTaskImpl::createInstance< Task >(
                                                cpp::bind(
                                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                        client -> outgoingObjectChannel().get()
                                                        ),
                                                    targetPeerId,
                                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                                    om::ObjPtrCopyable< Payload >( payload ),
                                                    _1 /* onReady - the completion callback */
                                                    )
                                                )
                                            );
                                    }

                                    eqLocal -> flush();

                                    /*
                                     * Verify that all messages were delivered successfully and the
                                     * message distribution over the channels was uniform
                                     */

                                    UTF_REQUIRE_EQUAL( noOfMessagesDelivered, noOfBlocks );
                                }

                                /*
                                 * Dissociate all logical peer ids
                                 */

                                noOfMessagesDelivered = 0;

                                for( std::size_t i = 0; i < noOfLogicalPeerIds; ++i )
                                {
                                    const auto& logicalPeerId = logicalPeerIds[ i ].first;
                                    const auto& physicalTargetPeerId = logicalPeerIds[ i ].second;

                                    const auto pos = i % clients.size();
                                    const auto& client = clients[ pos ].second;

                                    const auto associateMessage = utest::TestMessagingUtils::createBrokerProtocolMessage(
                                        MessageType::BackendDissociateTargetPeerId,
                                        bl::uuids::create() /* conversationId */,
                                        "" /* cookiesText */
                                        );

                                    associateMessage -> sourcePeerId( bl::uuids::uuid2string( physicalTargetPeerId ) );
                                    associateMessage -> targetPeerId( bl::uuids::uuid2string( logicalPeerId ) );

                                    eqLocal -> push_back(
                                        ExternalCompletionTaskImpl::createInstance< Task >(
                                            cpp::bind(
                                                &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                    client -> outgoingObjectChannel().get()
                                                    ),
                                                uuids::create() /* targetPeerId */,
                                                om::ObjPtrCopyable< BrokerProtocol >( associateMessage ),
                                                om::ObjPtrCopyable< Payload >(),
                                                _1 /* onReady - the completion callback */
                                                )
                                            )
                                        );
                                }

                                eqLocal -> flush();

                                UTF_REQUIRE_EQUAL( noOfMessagesDelivered, 0U );

                                {
                                    /*
                                     * Send a bunch of messages to the logical peer ids and verify that they
                                     * all fail now
                                     */

                                    const std::size_t noOfBlocks = 10 * noOfLogicalPeerIds;

                                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                                    {
                                        const auto pos1 = i % clients.size();
                                        const auto pos2 = i % noOfLogicalPeerIds;

                                        const auto& client = clients[ pos1 ].second;
                                        const auto& targetPeerId = logicalPeerIds[ pos2 ].first;

                                        eqLocal -> push_back(
                                            ExternalCompletionTaskImpl::createInstance< Task >(
                                                cpp::bind(
                                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                        client -> outgoingObjectChannel().get()
                                                        ),
                                                    targetPeerId,
                                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                                    om::ObjPtrCopyable< Payload >( payload ),
                                                    _1 /* onReady - the completion callback */
                                                    )
                                                )
                                            );
                                    }

                                    eqLocal -> flushNoThrowIfFailed();

                                    UTF_REQUIRE_EQUAL( noOfMessagesDelivered, 0U );

                                    UTF_REQUIRE_EQUAL( eqLocal -> size(), noOfBlocks );

                                    while( ! eqLocal -> isEmpty() )
                                    {
                                        const auto task = eqLocal -> pop();

                                        if( task )
                                        {
                                            UTF_REQUIRE_EQUAL( task -> isFailed(), true );

                                            try
                                            {
                                                cpp::safeRethrowException( task -> exception() );
                                            }
                                            catch( ServerErrorException& e )
                                            {
                                                const auto* ec =
                                                    eh::get_error_info< eh::errinfo_error_code >( e );

                                                UTF_REQUIRE( ec );

                                                UTF_REQUIRE_EQUAL(
                                                    *ec,
                                                    eh::errc::make_error_code( BrokerErrorCodes::TargetPeerNotFound )
                                                    );
                                            }
                                        }
                                    }
                                }
                            }
                            );
                    }
                }
                );
        };

        executeTests( 1U /* noOfConnections */, uuids::create() /* peerId */ );
        executeTests( 2U /* noOfConnections */, uuids::create() /* peerId */ );
        executeTests( 3U /* noOfConnections */, uuids::create() /* peerId */ );

        /*
         * Execute the tests with peerId=nil() which will generate unique physical peerId
         * for each connection
         */

        executeTests( 3U /* noOfConnections */, uuids::nil() /* peerId */ );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = bl::om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    bl::messaging::BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests
        );
}

UTF_AUTO_TEST_CASE( IO_MessagingProxyBackendTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    BL_EXCEPTION_HOOKS_THROW_GUARD( &exceptionThrowHook2 )

    typedef utest::TestMessagingUtils utils_t;

    const auto brokerInboundPort = test::UtfArgsParser::port() + 2U;

    const auto heartbeatInterval =  time::seconds( 3L );

    om::Object* proxyBackendRef = nullptr;

    const auto callbackTests = [ & ]() -> void
    {
        const auto sendSingleMessageTests = [ brokerInboundPort ](
            SAA_in          const unsigned short                                        port1,
            SAA_in          const unsigned short                                        port2,
            SAA_in          const bl::uuid_t&                                           peerId1,
            SAA_in          const bl::uuid_t&                                           peerId2
            )
            -> void
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n************************* sendSingleMessageTests [begin] *************************\n"
                    << "\n*** port1: "
                    << port1
                    << "\n*** port2: "
                    << port2
                    << "\n*** peerId1: "
                    << peerId1
                    << "\n*** peerId2: "
                    << peerId2
                );

            BL_SCOPE_EXIT(
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "\n************************* sendSingleMessageTests [end] *************************\n"
                            << "*** Exited with exception: "
                            << ( std::current_exception() ? "true" : "false" )
                            << "\n"
                        );
                }
                );

            const auto incomingSink1 = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    [ = ](
                        SAA_in              const bl::uuid_t&                               targetPeerId,
                        SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                        SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                        )
                        -> void
                    {
                        BL_UNUSED( payload );

                        UTF_REQUIRE_EQUAL( peerId1, uuids::string2uuid( brokerProtocol -> sourcePeerId() ) );
                        UTF_REQUIRE_EQUAL( peerId2, uuids::string2uuid( brokerProtocol -> targetPeerId() ) );
                        UTF_REQUIRE_EQUAL( peerId2, targetPeerId );
                    }
                    )
                );

            const auto incomingSink2 = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    [ = ](
                        SAA_in              const bl::uuid_t&                               targetPeerId,
                        SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                        SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                        )
                        -> void
                    {
                        BL_UNUSED( payload );

                        UTF_REQUIRE_EQUAL( peerId2, uuids::string2uuid( brokerProtocol -> sourcePeerId() ) );
                        UTF_REQUIRE_EQUAL( peerId1, uuids::string2uuid( brokerProtocol -> targetPeerId() ) );
                        UTF_REQUIRE_EQUAL( peerId1, targetPeerId );
                    }
                    )
                );

            const om::ObjPtrCopyable< om::Proxy > clientSink =
                om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

            const auto incomingObjectChannel = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    cpp::bind(
                        &utest::TestMessagingUtils::dispatchCallback,
                        clientSink,
                        uuids::nil()    /* targetPeerIdExpected */,
                        _1              /* targetPeerId */,
                        _2              /* brokerProtocol */,
                        _3              /* payload */
                        )
                    )
                );

            utils_t::executeMessagingTests(
                incomingObjectChannel,
                [ & ](
                    SAA_in          const std::string&                                      cookiesText,
                    SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
                    SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
                    SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
                    SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
                    ) -> void
                {
                    const auto conversationId = uuids::create();

                    const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        cookiesText
                        );

                    const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                        );

                    /*
                     * First create a client for the proxy and then create such client for the
                     * real broker
                     *
                     * The peer id is either fixed or a unique one is generated for each connection
                     */

                    om::ObjPtrDisposable< MessagingClientObject > client1;
                    om::ObjPtrDisposable< MessagingClientObject > client2;

                    {
                        BL_EXCEPTION_HOOKS_THROW_GUARD( &exceptionThrowHook3 )

                        /*
                         * Create two messaging clients - one to the real broker and one to the proxy -
                         * send messages between them and verify that the source and target peer ids
                         * are preserved correctly in the broker protocol message
                         *
                         * Note that these don't own the backend and the queue, so when they get
                         * disposed they will not actually dispose the backend, but just tear down
                         * the connections
                         */

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Creating connections for client1..."
                            );

                        auto connections1 = utils_t::createNoOfConnections(
                            1U /* noOfConnections */,
                            test::UtfArgsParser::host(),
                            port1
                            );
                        UTF_REQUIRE_EQUAL( connections1.size(), 1U /* noOfConnections */ );

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Creating connections for client2..."
                            );

                        auto connections2 = utils_t::createNoOfConnections(
                            1U /* noOfConnections */,
                            test::UtfArgsParser::host(),
                            port2
                            );
                        UTF_REQUIRE_EQUAL( connections2.size(), 1U /* noOfConnections */ );

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Creating client1..."
                            );

                        auto blockDispatch1 = om::lockDisposable(
                            utils_t::client_factory_t::createWithSmartDefaults(
                                om::copy( eq ),
                                peerId1,
                                om::copy( backend ),
                                om::copy( asyncWrapper ),
                                test::UtfArgsParser::host()                         /* host */,
                                port1                                               /* inboundPort */,
                                port1 + 1U                                          /* outboundPort */,
                                std::move( connections1[ 0U ].first )               /* inboundConnection */,
                                std::move( connections1[ 0U ].second )              /* outboundConnection */,
                                om::copy( dataBlocksPool )
                                )
                            );

                        client1 = om::lockDisposable(
                            MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                om::qi< MessagingClientBlockDispatch >( blockDispatch1 ),
                                dataBlocksPool
                                )
                            );

                        blockDispatch1.detachAsObjPtr();

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Creating client2..."
                            );

                        auto blockDispatch2 = om::lockDisposable(
                            utils_t::client_factory_t::createWithSmartDefaults(
                                om::copy( eq ),
                                peerId2,
                                om::copy( backend ),
                                om::copy( asyncWrapper ),
                                test::UtfArgsParser::host()                         /* host */,
                                port2                                               /* inboundPort */,
                                port2 + 1U                                          /* outboundPort */,
                                std::move( connections2[ 0U ].first )               /* inboundConnection */,
                                std::move( connections2[ 0U ].second )              /* outboundConnection */,
                                om::copy( dataBlocksPool )
                                )
                            );

                        client2 = om::lockDisposable(
                            MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                om::qi< MessagingClientBlockDispatch >( blockDispatch2 ),
                                dataBlocksPool
                                )
                            );

                        blockDispatch2.detachAsObjPtr();
                    }

                    /*
                     * Just execute a bunch of messages at random and then verify that they have arrived
                     * and that all channels were used fairly
                     */

                    {
                        BL_SCOPE_EXIT(
                            {
                                clientSink -> disconnect();
                            }
                            );

                        scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eqLocal ) -> void
                            {
                                eqLocal -> setOptions( ExecutionQueue::OptionKeepFailed );
                                eqLocal -> setThrottleLimit( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2U );

                                /*
                                 * Send two messages - first client1 -> client2 and then client2 -> client1
                                 * and before sending each message we connect the respective sink that knows
                                 * how to validate the message
                                 *
                                 * Note also that the ordering is important and that we always have to send
                                 * client1 -> client2 message first as if we are sending messages between
                                 * clients connected to proxy and real broker we need to make sure that the
                                 * proxy -> broker message is sent first to ensure the proxy client has
                                 * registered and will receive the reverse message (otherwise we have to do
                                 * an arbitrary wait as we don't know how long it would take for the proxy
                                 * client to register)
                                 */

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Message was being scheduled to be sent to incomingSink1..."
                                    );

                                clientSink -> connect( incomingSink1.get() );

                                eqLocal -> push_back(
                                    ExternalCompletionTaskImpl::createInstance< Task >(
                                        cpp::bind(
                                            &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                            om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                client1 -> outgoingObjectChannel().get()
                                                ),
                                            peerId2,
                                            om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                            om::ObjPtrCopyable< Payload >( payload ),
                                            _1 /* onReady - the completion callback */
                                            )
                                        )
                                    );

                                utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Message send to incomingSink1 successfully"
                                    );

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Message was being scheduled to be sent to incomingSink2..."
                                    );


                                clientSink -> connect( incomingSink2.get() );

                                eqLocal -> push_back(
                                    ExternalCompletionTaskImpl::createInstance< Task >(
                                        cpp::bind(
                                            &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                            om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                client2 -> outgoingObjectChannel().get()
                                                ),
                                            peerId1,
                                            om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                            om::ObjPtrCopyable< Payload >( payload ),
                                            _1 /* onReady - the completion callback */
                                            )
                                        )
                                    );

                                utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Message send to incomingSink2 successfully"
                                    );

                                clientSink -> disconnect();
                            }
                            );
                    }
                }
                );
        };

        const auto executeTests = [ brokerInboundPort ](
            SAA_in          const std::size_t                                           noOfConnections,
            SAA_in_opt      const bl::uuid_t                                            peerId
            )
            -> void
        {
            BL_LOG_MULTILINE(
                Logging::debug(),
                BL_MSG()
                    << "\n************************* executeTests [begin] *************************\n"
                    << "\n*** noOfConnections: "
                    << noOfConnections
                    << "\n*** peerId: "
                    << peerId
                );

            BL_SCOPE_EXIT(
                {
                    BL_LOG_MULTILINE(
                        Logging::debug(),
                        BL_MSG()
                            << "\n************************* executeTests [end] *************************\n"
                            << "*** Exited with exception: "
                            << ( std::current_exception() ? "true" : "false" )
                            << "\n"
                        );
                }
                );

            UTF_REQUIRE( noOfConnections >= 1 );

            std::atomic< std::size_t > noOfMessagesDelivered( 0U );

            const auto incomingSink = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    [ & ](
                        SAA_in              const bl::uuid_t&                               targetPeerId,
                        SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                        SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                        )
                        -> void
                    {
                        BL_UNUSED( targetPeerId );
                        BL_UNUSED( brokerProtocol );
                        BL_UNUSED( payload );

                        ++noOfMessagesDelivered;
                    }
                    )
                );

            const om::ObjPtrCopyable< om::Proxy > clientSink =
                om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

            const auto incomingObjectChannel = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    cpp::bind(
                        &utest::TestMessagingUtils::dispatchCallback,
                        clientSink,
                        uuids::nil()    /* targetPeerIdExpected */,
                        _1              /* targetPeerId */,
                        _2              /* brokerProtocol */,
                        _3              /* payload */
                        )
                    )
                );

            utils_t::executeMessagingTests(
                incomingObjectChannel,
                [ & ](
                    SAA_in          const std::string&                                      cookiesText,
                    SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
                    SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
                    SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
                    SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
                    ) -> void
                {
                    const auto conversationId = uuids::create();

                    const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        cookiesText
                        );

                    const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                        );

                    /*
                     * Create noOfConnections messaging clients backed by the same async wrapper
                     *
                     * Note that these don't own the backend and the queue, so when they get
                     * disposed they will not actually dispose the backend, but just tear down
                     * the connections
                     */

                    auto proxyConnections = utils_t::createNoOfConnections( noOfConnections );
                    UTF_REQUIRE_EQUAL( proxyConnections.size(), noOfConnections );

                    auto brokerConnections = utils_t::createNoOfConnections(
                        noOfConnections,
                        test::UtfArgsParser::host(),
                        brokerInboundPort
                        );
                    UTF_REQUIRE_EQUAL( brokerConnections.size(), noOfConnections );

                    utils_t::clients_list_t proxyClients;
                    utils_t::clients_list_t brokerClients;

                    proxyClients.reserve( noOfConnections );
                    brokerClients.reserve( noOfConnections );

                    for( std::size_t i = 0; i < noOfConnections; ++i )
                    {
                        /*
                         * First create a client for the proxy and then create such client for the
                         * real broker
                         *
                         * The peer id is either fixed or a unique one is generated for each connection
                         */

                        {
                            const auto clientPeerId = uuids::nil() == peerId ? uuids::create() : peerId;

                            auto blockDispatch = om::lockDisposable(
                                utils_t::client_factory_t::createWithSmartDefaults(
                                    om::copy( eq ),
                                    clientPeerId,
                                    om::copy( backend ),
                                    om::copy( asyncWrapper ),
                                    test::UtfArgsParser::host()                         /* host */,
                                    test::UtfArgsParser::port()                         /* inboundPort */,
                                    test::UtfArgsParser::port() + 1U                    /* outboundPort */,
                                    std::move( proxyConnections[ i ].first )            /* inboundConnection */,
                                    std::move( proxyConnections[ i ].second )           /* outboundConnection */,
                                    om::copy( dataBlocksPool )
                                    )
                                );

                            auto client = om::lockDisposable(
                                MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                    om::qi< MessagingClientBlockDispatch >( blockDispatch ),
                                    dataBlocksPool
                                    )
                                );

                            blockDispatch.detachAsObjPtr();

                            proxyClients.emplace_back( std::make_pair( clientPeerId, std::move( client ) ) );
                        }

                        {
                            const auto clientPeerId = uuids::nil() == peerId ? uuids::create() : peerId;

                            auto blockDispatch = om::lockDisposable(
                                utils_t::client_factory_t::createWithSmartDefaults(
                                    om::copy( eq ),
                                    clientPeerId,
                                    om::copy( backend ),
                                    om::copy( asyncWrapper ),
                                    test::UtfArgsParser::host()                         /* host */,
                                    brokerInboundPort                                   /* inboundPort */,
                                    brokerInboundPort + 1U                              /* outboundPort */,
                                    std::move( brokerConnections[ i ].first )           /* inboundConnection */,
                                    std::move( brokerConnections[ i ].second )          /* outboundConnection */,
                                    om::copy( dataBlocksPool )
                                    )
                                );

                            auto client = om::lockDisposable(
                                MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                    om::qi< MessagingClientBlockDispatch >( blockDispatch ),
                                    dataBlocksPool
                                    )
                                );

                            blockDispatch.detachAsObjPtr();

                            brokerClients.emplace_back( std::make_pair( clientPeerId, std::move( client ) ) );
                        }
                    }

                    /*
                     * Just execute a bunch of messages at random and then verify that they have arrived
                     * and that all channels were used fairly
                     */

                    {
                        BL_SCOPE_EXIT(
                            {
                                clientSink -> disconnect();
                            }
                            );

                        clientSink -> connect( incomingSink.get() );

                        scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eqLocal ) -> void
                            {
                                eqLocal -> setOptions( ExecutionQueue::OptionKeepFailed );
                                eqLocal -> setThrottleLimit( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2U );

                                UTF_REQUIRE_EQUAL( proxyClients.size(), brokerClients.size() );

                                const auto vectorsSize = proxyClients.size();

                                {
                                    BL_LOG(
                                        Logging::debug(),
                                        BL_MSG()
                                            << "\n**** Send some messages to the clients .... [begin]\n"
                                        );

                                    BL_SCOPE_EXIT(
                                        {
                                            BL_LOG(
                                                Logging::debug(),
                                                BL_MSG()
                                                    << "\n**** Send some messages to the clients .... [end]\n"
                                                    << "*** Exited with exception: "
                                                    << ( std::current_exception() ? "true" : "false" )
                                                    << "\n"
                                                );
                                        }
                                        );
                                    /*
                                     * Send some messages to the clients and verify that these are also delivered
                                     */

                                    noOfMessagesDelivered = 0;

                                    const std::size_t noOfBlocks = 10 * vectorsSize;

                                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                                    {
                                        const auto pos1 = i % vectorsSize;
                                        const auto pos2 = ( i + 1 ) % vectorsSize;

                                        const auto& sourceClient = proxyClients[ pos1 ].second;
                                        const auto& targetPeerId = proxyClients[ pos2 ].first;

                                        eqLocal -> push_back(
                                            ExternalCompletionTaskImpl::createInstance< Task >(
                                                cpp::bind(
                                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                        sourceClient -> outgoingObjectChannel().get()
                                                        ),
                                                    targetPeerId,
                                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                                    om::ObjPtrCopyable< Payload >( payload ),
                                                    _1 /* onReady - the completion callback */
                                                    )
                                                )
                                            );

                                        /*
                                         * Send at least one request successfully before we start parallelizing
                                         * the rest of the requests to avoid unnecessary request to Janus
                                         */

                                        if( 0U == i )
                                        {
                                            utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );
                                        }
                                    }

                                    utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );

                                    /*
                                     * Verify that all messages were delivered successfully and the
                                     * message distribution over the channels was uniform
                                     */

                                    UTF_REQUIRE_EQUAL( noOfMessagesDelivered, noOfBlocks );
                                }

                                {
                                    BL_LOG(
                                        Logging::debug(),
                                        BL_MSG()
                                            << "\n**** Send messages from proxy to real brokers.... [begin]\n"
                                        );

                                    BL_SCOPE_EXIT(
                                        {
                                            BL_LOG(
                                                Logging::debug(),
                                                BL_MSG()
                                                    << "\n**** Send messages from proxy to real brokers.... [end]\n"
                                                    << "*** Exited with exception: "
                                                    << ( std::current_exception() ? "true" : "false" )
                                                    << "\n"
                                                );
                                        }
                                        );

                                    /*
                                     * Send some messages to between the proxy clients and the real broker
                                     * clients and vice versa
                                     */

                                    const auto getRandomPos = [ vectorsSize ]() -> std::size_t
                                    {
                                        return random::getUniformRandomUnsignedValue< std::size_t >(
                                            vectorsSize - 1
                                            );
                                    };

                                    const auto getCoinToss = []() -> bool
                                    {
                                        return 0 == random::getUniformRandomUnsignedValue< int >( 1 );
                                    };

                                    noOfMessagesDelivered = 0;

                                    const std::size_t noOfBlocks = 10 * vectorsSize;

                                    for( std::size_t i = 0U; i < noOfBlocks; ++i )
                                    {
                                        /*
                                         * Indexes of the clients are chosen at random and also the source
                                         * vs. the target are chosen to be from proxy to broker client or
                                         * vice versa (again at random)
                                         */

                                        const auto pos1 = getRandomPos();
                                        const auto pos2 = getRandomPos();

                                        const auto coinToss = getCoinToss();

                                        const auto& sourceClient =
                                            coinToss ? proxyClients[ pos1 ].second : brokerClients[ pos1 ].second ;

                                        const auto& targetPeerId =
                                            coinToss ? brokerClients[ pos2 ].first : proxyClients[ pos2 ].first;

                                        eqLocal -> push_back(
                                            ExternalCompletionTaskImpl::createInstance< Task >(
                                                cpp::bind(
                                                    &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                                    om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                        sourceClient -> outgoingObjectChannel().get()
                                                        ),
                                                    targetPeerId,
                                                    om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                                    om::ObjPtrCopyable< Payload >( payload ),
                                                    _1 /* onReady - the completion callback */
                                                    )
                                                )
                                            );

                                        /*
                                         * Send at least one request successfully before we start parallelizing
                                         * the rest of the requests to avoid unnecessary request to Janus
                                         */

                                        if( 0U == i )
                                        {
                                            utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );
                                        }
                                    }

                                    utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );

                                    /*
                                     * Verify that all messages were delivered successfully and the
                                     * message distribution over the channels was uniform
                                     */

                                    UTF_REQUIRE_EQUAL( noOfMessagesDelivered, noOfBlocks );
                                }
                            }
                            );
                    }
                }
                );
        };

        {
            sendSingleMessageTests(
                test::UtfArgsParser::port()             /* port1 */,
                brokerInboundPort                       /* port2 */,
                uuids::create()                         /* peerId1 */,
                uuids::create()                         /* peerId2 */
                );

            sendSingleMessageTests(
                test::UtfArgsParser::port()             /* port1 */,
                test::UtfArgsParser::port()             /* port2 */,
                uuids::create()                         /* peerId1 */,
                uuids::create()                         /* peerId2 */
                );

            sendSingleMessageTests(
                brokerInboundPort                       /* port1 */,
                brokerInboundPort                       /* port2 */,
                uuids::create()                         /* peerId1 */,
                uuids::create()                         /* peerId2 */
                );

            const auto stickyPeerId1 = uuids::create();
            const auto stickyPeerId2 = uuids::create();

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Sticky peer id 1 is "
                    << str::quoteString( uuids::uuid2string( stickyPeerId1 ) )
                );

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Sticky peer id 2 is "
                    << str::quoteString( uuids::uuid2string( stickyPeerId2 ) )
                );

            /*
             * A peer id that has been connected to the proxy (stickyPeerId1) can disconnect and
             * then immediately connect to the broker directly (that scenario should work)
             *
             * However a peer id that was connected to the broker directly (stickyPeerId2)
             * cannot disconnect and then immediately connect to the proxy because the broker will
             * detect the connection was torn down only after the heartbeatInterval
             *
             * Therefore we need to wait for at least heartbeatInterval before we try to reuse
             * the peer id that was connected to the broker directly (stickyPeerId2) with the
             * proxy
             */

            sendSingleMessageTests(
                test::UtfArgsParser::port()             /* port1 */,
                brokerInboundPort                       /* port2 */,
                stickyPeerId1                           /* peerId1 */,
                stickyPeerId2                           /* peerId2 */
                );

            sendSingleMessageTests(
                test::UtfArgsParser::port()             /* port1 */,
                brokerInboundPort                       /* port2 */,
                uuids::create()                         /* peerId1 */,
                stickyPeerId1                           /* peerId2 */
                );

            /*
             * Sleep is necessary here before we do the next two tests - see comment above
             */

            os::sleep( heartbeatInterval + time::seconds( 2L ) );

            sendSingleMessageTests(
                test::UtfArgsParser::port()             /* port1 */,
                brokerInboundPort                       /* port2 */,
                stickyPeerId2                           /* peerId1 */,
                stickyPeerId1                           /* peerId2 */
                );

            sendSingleMessageTests(
                test::UtfArgsParser::port()             /* port1 */,
                brokerInboundPort                       /* port2 */,
                stickyPeerId2                           /* peerId1 */,
                uuids::create()                         /* peerId2 */
                );
        }

        {
            const auto peerId = uuids::create();

            executeTests( 1U /* noOfConnections */, peerId );
        }

        {
            const auto peerId = uuids::create();

            executeTests( 21U /* noOfConnections */, peerId );
        }

        /*
         * Execute the tests with peerId=nil() which will generate unique physical peerId
         * for each connection
         */

        executeTests( 24 /* noOfConnections */, uuids::nil() /* peerId */ );

        /*
         * Now test if the client pruning logic works correctly
         */

        UTF_REQUIRE( proxyBackendRef );

        const auto proxyBackend =
            om::qi< ProxyBrokerBackendProcessingFactorySsl::proxy_backend_t >( proxyBackendRef );

        {
            std::unordered_set< bl::uuid_t > activeClients;
            std::unordered_set< bl::uuid_t > pendingPrune;

            proxyBackend -> getCurrentState( &activeClients, &pendingPrune );

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Active clients count is "
                    << activeClients.size()
                    << "; client pending prune is "
                    << pendingPrune.size()
                );

            UTF_REQUIRE( activeClients.size() );
        }

        proxyBackend -> setClientsPruneIntervals(
            time::seconds( 3L )         /* clientsPruneCheckInterval */,
            time::seconds( 12L )        /* clientsPruneInterval */
            );

        const std::size_t maxRetries = 60U;
        std::size_t retries = 0U;

        for( ;; )
        {
            if( retries > maxRetries )
            {
                UTF_FAIL( "All clients did not go in pending prune mode in 2 minutes" );

                break;
            }

            os::sleep( time::seconds( 2L ) );

            std::unordered_set< bl::uuid_t > activeClients;
            std::unordered_set< bl::uuid_t > pendingPrune;

            proxyBackend -> getCurrentState( &activeClients, &pendingPrune );

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Active clients count is "
                    << activeClients.size()
                    << "; client pending prune is "
                    << pendingPrune.size()
                );

            for( const auto& peerId : pendingPrune )
            {
                activeClients.erase( peerId );
            }

            if( activeClients.size() )
            {
                ++retries;
                continue;
            }

            UTF_REQUIRE_EQUAL( 0U, activeClients.size() );
            UTF_REQUIRE( pendingPrune.size() );

            break;
        }

        retries = 0U;

        for( ;; )
        {
            if( retries > maxRetries )
            {
                UTF_FAIL( "All clients did get pruned after in pending prune mode for 2 minutes" );

                break;
            }

            os::sleep( time::seconds( 2L ) );

            std::unordered_set< bl::uuid_t > activeClients;
            std::unordered_set< bl::uuid_t > pendingPrune;

            proxyBackend -> getCurrentState( &activeClients, &pendingPrune );

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Active clients count is "
                    << activeClients.size()
                    << "; client pending prune is "
                    << pendingPrune.size()
                );

            if( activeClients.size() || pendingPrune.size() )
            {
                ++retries;
                continue;
            }

            UTF_REQUIRE_EQUAL( 0U, activeClients.size() );
            UTF_REQUIRE_EQUAL( 0U, pendingPrune.size() );

            break;
        }
    };

    test::MachineGlobalTestLock lock;

    const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()                      /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()              /* certificatePem */,
        brokerInboundPort                                           /* inboundPort */,
        brokerInboundPort + 1U                                      /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                          /* maxConcurrentTasks */,
        cpp::bind(
            &utils_t::startBrokerProxy,
            om::ObjPtrCopyable< TaskControlTokenRW >( controlToken ),
            callbackTests,
            test::UtfArgsParser::port()                             /* proxyInboundPort */,
            test::UtfArgsParser::connections()                      /* noOfConnections */,
            test::UtfArgsParser::host()                             /* brokerHostName */,
            brokerInboundPort,
            om::ObjPtrCopyable< data::datablocks_pool_type >( dataBlocksPool ),
            heartbeatInterval,
            &proxyBackendRef
            ),
        om::copy( controlToken ),
        dataBlocksPool,
        cpp::copy( heartbeatInterval )
        );
}

UTF_AUTO_TEST_CASE( IO_ConnectionEstablisherHangTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    BL_LOG(
        Logging::debug(),
        BL_MSG()
            << "Executing hang test on the following platform "
            << str::quoteString( bl::BuildInfo::platform )
            << " ..."
        );

    const auto callbackTests = [ & ]() -> void
    {
        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
            {
                eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                typedef ProxyBrokerBackendProcessingFactorySsl::connection_establisher_t connection_establisher_t;

                const std::size_t noOfConnections = 256U;

                const std::size_t randomInMiddle =
                    ( 2U * noOfConnections ) / 3U +
                    random::getUniformRandomUnsignedValue< std::size_t >( noOfConnections / 3U - 1U );

                for( std::size_t i = 0U; i < noOfConnections; ++i )
                {
                    if( randomInMiddle == i )
                    {
                        controlToken -> requestCancel();
                    }

                    const auto inboundConnection =
                        connection_establisher_t::template createInstance< Task >(
                            cpp::copy( test::UtfArgsParser::host() ),
                            test::UtfArgsParser::port()                     /* inboundPort */,
                            false                                           /* logExceptions */
                            );

                    const auto outboundConnection =
                        connection_establisher_t::template createInstance< Task >(
                            cpp::copy( test::UtfArgsParser::host() ),
                            test::UtfArgsParser::port() + 1U                /* outboundPort */,
                            false                                           /* logExceptions */
                            );

                    eq -> push_back( inboundConnection );
                    eq -> push_back( outboundConnection );
                }

                const auto startTime = time::second_clock::universal_time();

                for( ;; )
                {
                    os::sleep( time::seconds( 2 ) );

                    if( eq -> isEmpty() )
                    {
                        break;
                    }

                    if( test::UtfArgsParser::isClient() )
                    {
                        continue;
                    }

                    const auto elapsed = time::second_clock::universal_time() - startTime;

                    /*
                     * Note: the checks below are to handle an apparently OS issue with connect
                     * operations hanging occasionally on rhel5 & rhel6 only
                     */

                    if( elapsed > time::minutes( 5 ) && bl::BuildInfo::platform == "linux-rhel5" )
                    {
                        eq -> cancelAll( false /* wait */ );
                    }

                    if( elapsed > time::minutes( 15 ) && bl::BuildInfo::platform == "linux-rhel5" )
                    {
                        BL_RIP_MSG( "Connection tasks are hung more than 15 minutes" );
                    }

                    if( elapsed > time::minutes( 5 ) && bl::BuildInfo::platform == "linux-rhel6" )
                    {
                        eq -> cancelAll( false /* wait */ );
                    }

                    if( elapsed > time::minutes( 15 ) && bl::BuildInfo::platform == "linux-rhel6" )
                    {
                        BL_RIP_MSG( "Connection tasks are hung more than 15 minutes" );
                    }
                }
            }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests,
        om::copy( controlToken )
        );
}

namespace utest
{
    template
    <
        typename STREAM,
        typename ASYNCWRAPPER,
        typename SERVERPOLICY = bl::tasks::TcpServerPolicyDefault
    >
    class TestTcpBlockServerT :
        public bl::tasks::TcpBlockServerT< STREAM, ASYNCWRAPPER, SERVERPOLICY >
    {
        BL_DECLARE_OBJECT_IMPL( TestTcpBlockServerT )

    public:

        typedef bl::tasks::TcpBlockServerT< STREAM, ASYNCWRAPPER, SERVERPOLICY >            base_type;

    protected:

        TestTcpBlockServerT(
            SAA_in              const bl::om::ObjPtr< bl::tasks::TaskControlTokenRW >&      controlToken,
            SAA_in              const bl::om::ObjPtr< bl::data::datablocks_pool_type >&     dataBlocksPool,
            SAA_in              std::string&&                                               host,
            SAA_in              const unsigned short                                        port,
            SAA_in              const std::string&                                          privateKeyPem,
            SAA_in              const std::string&                                          certificatePem,
            SAA_in              const bl::om::ObjPtr< ASYNCWRAPPER >&                       asyncWrapper,
            SAA_in_opt          const bl::uuid_t&                                           peerId = bl::uuids::nil()
            )
            :
            base_type(
                controlToken,
                dataBlocksPool,
                BL_PARAM_FWD( host ),
                port,
                privateKeyPem,
                certificatePem,
                asyncWrapper,
                peerId
                )
        {
        }

        virtual auto createConnection( SAA_inout typename STREAM::stream_ref&& connectedStream )
            -> bl::om::ObjPtr< bl::tasks::Task > OVERRIDE
        {
            auto connection = base_type::createConnection( BL_PARAM_FWD( connectedStream ) );

            /*
             * For testing purpose force the connection to be cancelled before it is returned
             */

            connection -> requestCancel();

            return connection;
        }

        virtual auto createProtocolHandshakeTask( SAA_inout typename STREAM::stream_ref&& connectedStream )
            -> bl::om::ObjPtr< bl::tasks::Task > OVERRIDE
        {
            auto task = base_type::createProtocolHandshakeTask( BL_PARAM_FWD( connectedStream ) );

            /*
             * For testing purpose force the connection to be cancelled before it is returned
             */

            task -> requestCancel();

            return task;
        }
    };

} // utest

UTF_AUTO_TEST_CASE( IO_EarlyCancelIssueTests )
{
    using namespace bl;
    using namespace bl::tasks;
    using namespace bl::messaging;

    typedef om::ObjectImpl
    <
        utest::TestTcpBlockServerT
        <
            TcpSslSocketAsyncBase /* STREAM */,
            AsyncMessageDispatcherWrapper,
            TcpServerPolicySmooth
        >
    >
    acceptor_t;

    std::vector< om::ObjPtr< Task > > connections;

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto callbackTests = [ & ]() -> void
    {
        scheduleAndExecuteInParallel(
            [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
            {
                eq -> setOptions( tasks::ExecutionQueue::OptionKeepNone );

                /*
                 * Note that the connection establisher is chosen to be non-SSL on purpose to
                 * simulate the bug of a task which is canceled before even started is allowed
                 * to be scheduled
                 *
                 * The non-SSL connection establisher is connected to SSL backend which will
                 * cause the handshake task to actually hang while trying to read on the socket
                 * (which is the big because it should never be scheduled if it is cancelled)
                 */

                typedef TcpConnectionEstablisherConnectorImpl< TcpSocketAsyncBase /* STREAM */ >
                    connection_establisher_t;

                const std::size_t noOfConnections = 256U;

                for( std::size_t i = 0U; i < noOfConnections; ++i )
                {
                    const auto inboundConnection =
                        connection_establisher_t::template createInstance< Task >(
                            cpp::copy( test::UtfArgsParser::host() ),
                            test::UtfArgsParser::port()                     /* inboundPort */,
                            false                                           /* logExceptions */
                            );

                    connections.push_back( om::copy( inboundConnection ) );

                    eq -> push_back( inboundConnection );
                }

                eq -> flushNoThrowIfFailed();

                /*
                 * Wait for 2 seconds for the server tasks to start and then try
                 * to cancel the acceptor and unwind everything
                 */

                os::sleep( time::seconds( 2L ) );

                controlToken -> requestCancel();
            }
            );
    };

    test::MachineGlobalTestLock lock;

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute
    <
        SslBrokerDispatchingBackendProcessingImpl           /* BACKEND */,
        AsyncMessageDispatcherWrapper                       /* ASYNCWRAPPER */,
        acceptor_t                                          /* ACCEPTOR */
    >
    (
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()              /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()      /* certificatePem */,
        test::UtfArgsParser::port()                         /* inboundPort */,
        test::UtfArgsParser::port() + 1                     /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                  /* maxConcurrentTasks */,
        callbackTests,
        om::copy( controlToken )
    );
}

UTF_AUTO_TEST_CASE( IO_ConnectionEstablisherBasicTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    if( ! test::UtfArgsParser::isClient() )
    {
        return;
    }

    typedef utest::TestMessagingUtils                                               utils_t;
    typedef ProxyBrokerBackendProcessingFactorySsl::connection_establisher_t        connection_establisher_t;

    scheduleAndExecuteInParallel(
        [ & ]( SAA_in const om::ObjPtr< tasks::ExecutionQueue >& eq ) -> void
        {
            eq -> setOptions( tasks::ExecutionQueue::OptionKeepAll );

            const std::size_t noOfConnections = test::UtfArgsParser::connections();

            for( std::size_t i = 0U; i < noOfConnections; ++i )
            {
                const auto connection =
                    connection_establisher_t::template createInstance< Task >(
                        cpp::copy( test::UtfArgsParser::host() ),
                        test::UtfArgsParser::port()
                        );


                eq -> push_back( connection );
            }

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "Establishing "
                    << noOfConnections
                    << " connections to endpoint: "
                    << net::formatEndpointId( test::UtfArgsParser::host(), test::UtfArgsParser::port() )
                );

            eq -> flush();

            BL_LOG(
                Logging::debug(),
                BL_MSG()
                    << "All "
                    << noOfConnections
                    << " connections to endpoint "
                    << net::formatEndpointId( test::UtfArgsParser::host(), test::UtfArgsParser::port() )
                    << " were established successfully"
                );

            utils_t::waitForKeyOrTimeout();
        }
        );
}

UTF_AUTO_TEST_CASE( IO_FlushQueueWithRetriesOnTargetPeerNotFoundTests )
{
    using namespace bl;
    using namespace bl::data;
    using namespace bl::tasks;
    using namespace bl::messaging;

    typedef utest::TestMessagingUtils utils_t;

    const auto heartbeatInterval =  time::seconds( 3L );

    const auto callbackTests = []() -> void
    {
        const auto singleMessageWithRetriesTests = [](
            SAA_in          const unsigned short                                        port1,
            SAA_in          const unsigned short                                        port2,
            SAA_in          const bl::uuid_t&                                           peerId1,
            SAA_in          const bl::uuid_t&                                           peerId2
            )
            -> void
        {
            const auto incomingSink = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    [ = ](
                        SAA_in              const bl::uuid_t&                               targetPeerId,
                        SAA_in              const bl::om::ObjPtr< BrokerProtocol >&         brokerProtocol,
                        SAA_in_opt          const bl::om::ObjPtr< Payload >&                payload
                        )
                        -> void
                    {
                        BL_UNUSED( payload );

                        UTF_REQUIRE_EQUAL( peerId1, uuids::string2uuid( brokerProtocol -> sourcePeerId() ) );
                        UTF_REQUIRE_EQUAL( peerId2, uuids::string2uuid( brokerProtocol -> targetPeerId() ) );
                        UTF_REQUIRE_EQUAL( peerId2, targetPeerId );
                    }
                    )
                );

            const om::ObjPtrCopyable< om::Proxy > clientSink =
                om::ProxyImpl::createInstance< om::Proxy >( true /* strongRef */ );

            const auto incomingObjectChannel = om::lockDisposable(
                MessagingClientObjectDispatchFromCallback::createInstance< MessagingClientObjectDispatch >(
                    cpp::bind(
                        &utest::TestMessagingUtils::dispatchCallback,
                        clientSink,
                        uuids::nil()    /* targetPeerIdExpected */,
                        _1              /* targetPeerId */,
                        _2              /* brokerProtocol */,
                        _3              /* payload */
                        )
                    )
                );

            utils_t::executeMessagingTests(
                incomingObjectChannel,
                [ & ](
                    SAA_in          const std::string&                                      cookiesText,
                    SAA_in          const bl::om::ObjPtr< datablocks_pool_type >&           dataBlocksPool,
                    SAA_in          const bl::om::ObjPtr< ExecutionQueue >&                 eq,
                    SAA_in          const bl::om::ObjPtr< BackendProcessing >&              backend,
                    SAA_in          const bl::om::ObjPtr< utils_t::async_wrapper_t >&       asyncWrapper
                    ) -> void
                {
                    const auto conversationId = uuids::create();

                    const auto brokerProtocol = utest::TestMessagingUtils::createBrokerProtocolMessage(
                        MessageType::AsyncRpcDispatch,
                        conversationId,
                        cookiesText
                        );

                    const auto payload = bl::dm::DataModelUtils::loadFromFile< Payload >(
                        utest::TestUtils::resolveDataFilePath( "async_rpc_request.json" )
                        );

                    /*
                     * First create a client for the proxy and then create such client for the
                     * real broker
                     *
                     * The peer id is either fixed or a unique one is generated for each connection
                     */

                    om::ObjPtrDisposable< MessagingClientObject > client1;
                    om::ObjPtrDisposable< MessagingClientObject > client2;

                    {
                        /*
                         * Create two messaging clients - one to the real broker and one to the proxy -
                         * send messages between them and verify that the source and target peer ids
                         * are preserved correctly in the broker protocol message
                         *
                         * Note that these don't own the backend and the queue, so when they get
                         * disposed they will not actually dispose the backend, but just tear down
                         * the connections
                         */

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Creating connections for client1..."
                            );

                        auto connections1 = utils_t::createNoOfConnections(
                            1U /* noOfConnections */,
                            test::UtfArgsParser::host(),
                            port1
                            );
                        UTF_REQUIRE_EQUAL( connections1.size(), 1U /* noOfConnections */ );

                        BL_LOG(
                            Logging::debug(),
                            BL_MSG()
                                << "Creating client1..."
                            );

                        auto blockDispatch1 = om::lockDisposable(
                            utils_t::client_factory_t::createWithSmartDefaults(
                                om::copy( eq ),
                                peerId1,
                                om::copy( backend ),
                                om::copy( asyncWrapper ),
                                test::UtfArgsParser::host()                         /* host */,
                                port1                                               /* inboundPort */,
                                port1 + 1U                                          /* outboundPort */,
                                std::move( connections1[ 0U ].first )               /* inboundConnection */,
                                std::move( connections1[ 0U ].second )              /* outboundConnection */,
                                om::copy( dataBlocksPool )
                                )
                            );

                        client1 = om::lockDisposable(
                            MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                om::qi< MessagingClientBlockDispatch >( blockDispatch1 ),
                                dataBlocksPool
                                )
                            );

                        blockDispatch1.detachAsObjPtr();
                    }

                    /*
                     * Create a simple timer which will connect client2 with some delay (e.g. 2 seconds)
                     */

                    const auto defaultDuration = time::seconds( 4L );

                    SimpleTimer timer(
                        [ & ]() -> time::time_duration
                        {
                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Creating connections for client2..."
                                );

                            auto connections2 = utils_t::createNoOfConnections(
                                1U /* noOfConnections */,
                                test::UtfArgsParser::host(),
                                port2
                                );
                            UTF_REQUIRE_EQUAL( connections2.size(), 1U /* noOfConnections */ );

                            BL_LOG(
                                Logging::debug(),
                                BL_MSG()
                                    << "Creating client2..."
                                );

                            auto blockDispatch2 = om::lockDisposable(
                                utils_t::client_factory_t::createWithSmartDefaults(
                                    om::copy( eq ),
                                    peerId2,
                                    om::copy( backend ),
                                    om::copy( asyncWrapper ),
                                    test::UtfArgsParser::host()                         /* host */,
                                    port2                                               /* inboundPort */,
                                    port2 + 1U                                          /* outboundPort */,
                                    std::move( connections2[ 0U ].first )               /* inboundConnection */,
                                    std::move( connections2[ 0U ].second )              /* outboundConnection */,
                                    om::copy( dataBlocksPool )
                                    )
                                );

                            client2 = om::lockDisposable(
                                MessagingClientObjectImplDefault::createInstance< MessagingClientObject >(
                                    om::qi< MessagingClientBlockDispatch >( blockDispatch2 ),
                                    dataBlocksPool
                                    )
                                );

                            blockDispatch2.detachAsObjPtr();

                            return defaultDuration;
                        },
                        cpp::copy( defaultDuration ),
                        cpp::copy( defaultDuration )       /* initDelay (same as defaultDuration) */
                        );

                    {
                        BL_SCOPE_EXIT(
                            {
                                clientSink -> disconnect();
                            }
                            );

                        scheduleAndExecuteInParallel(
                            [ & ]( SAA_in const om::ObjPtr< ExecutionQueue >& eqLocal ) -> void
                            {
                                eqLocal -> setOptions( ExecutionQueue::OptionKeepFailed );
                                eqLocal -> setThrottleLimit( utils_t::sender_connection_t::BLOCK_QUEUE_SIZE / 2U );

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Message was being scheduled to be sent to incomingSink..."
                                    );

                                clientSink -> connect( incomingSink.get() );

                                eqLocal -> push_back(
                                    ExternalCompletionTaskImpl::createInstance< Task >(
                                        cpp::bind(
                                            &MessagingClientObjectDispatch::pushMessageCopyCallback,
                                            om::ObjPtrCopyable< MessagingClientObjectDispatch >::acquireRef(
                                                client1 -> outgoingObjectChannel().get()
                                                ),
                                            peerId2,
                                            om::ObjPtrCopyable< BrokerProtocol >( brokerProtocol ),
                                            om::ObjPtrCopyable< Payload >( payload ),
                                            _1 /* onReady - the completion callback */
                                            )
                                        )
                                    );

                                utils_t::flushQueueWithRetriesOnTargetPeerNotFound( eqLocal );

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << "Message send to incomingSink successfully"
                                    );

                                clientSink -> disconnect();
                            }
                            );
                    }
                }
                );
        };

        singleMessageWithRetriesTests(
            test::UtfArgsParser::port()             /* port1 */,
            test::UtfArgsParser::port()             /* port2 */,
            uuids::create()                         /* peerId1 */,
            uuids::create()                         /* peerId2 */
            );
    };

    test::MachineGlobalTestLock lock;

    const auto dataBlocksPool = data::datablocks_pool_type::createInstance();

    const auto controlToken = SimpleTaskControlTokenImpl::createInstance< TaskControlTokenRW >();

    const auto processingBackend = om::lockDisposable(
        utest::TestMessagingUtils::createTestMessagingBackend()
        );

    BrokerFacade::execute(
        processingBackend,
        test::UtfCrypto::getDefaultServerKey()                      /* privateKeyPem */,
        test::UtfCrypto::getDefaultServerCertificate()              /* certificatePem */,
        test::UtfArgsParser::port()                                 /* inboundPort */,
        test::UtfArgsParser::port() + 1U                            /* outboundPort */,
        test::UtfArgsParser::threadsCount(),
        0U                                                          /* maxConcurrentTasks */,
        callbackTests,
        om::copy( controlToken ),
        dataBlocksPool,
        cpp::copy( heartbeatInterval )
        );
}
