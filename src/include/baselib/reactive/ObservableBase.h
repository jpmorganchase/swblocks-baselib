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

#ifndef __BL_REACTIVE_OBSERVABLEBASE_H_
#define __BL_REACTIVE_OBSERVABLEBASE_H_

#include <baselib/reactive/Observable.h>
#include <baselib/reactive/ReactiveIncludes.h>

#include <baselib/tasks/Task.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueImpl.h>

#include <baselib/core/ErrorDispatcher.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/BaseIncludes.h>

#include <unordered_map>

namespace bl
{
    namespace reactive
    {
        template
        <
            typename E = void
        >
        class ObservableBaseT;

        typedef ObservableBaseT<> ObservableBase;

        namespace detail
        {
            enum
            {
                DEFAULT_WAIT_TIME_IN_MILLISECONDS = 50,
            };

            /**
             * @brief class ObserverDisposer
             */

            template
            <
                typename E = void
            >
            class ObserverDisposerT : public om::Disposable
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE_NO_DESTRUCTOR( ObserverDisposerT, om::Disposable )

            protected:

                uuid_t                                                                      m_subscriptionId;
                std::weak_ptr< ObservableBase >                                             m_observable;

                ObserverDisposerT(
                    SAA_in                  const uuid_t&                                   subscriptionId,
                    SAA_in                  const std::shared_ptr< ObservableBase >&        observable
                    )
                    :
                    m_subscriptionId( subscriptionId ),
                    m_observable( observable )
                {
                }

                ~ObserverDisposerT() NOEXCEPT
                {
                    BL_NOEXCEPT_BEGIN()

                    dispose();

                    BL_NOEXCEPT_END()
                }

            public:

                /*
                 * This method will be declared out of line since
                 * it has to use ObservableBase type which is
                 * incomplete at this point
                 */

                virtual void  dispose() OVERRIDE;
            };

            typedef om::ObjectImpl< ObserverDisposerT<> > ObserverDisposerImpl;

            /**
             * @brief class ObserverDisposer
             */

            template
            <
                typename E = void
            >
            class ObserverNextInvokerT :
                public tasks::TimerTaskBase
            {
                BL_DECLARE_OBJECT_IMPL( ObserverNextInvokerT )

            protected:

                const om::ObjPtr< Observer >                                                        m_observer;
                const std::shared_ptr< cpp::any >                                                   m_value;

                ObserverNextInvokerT(
                    SAA_in              const om::ObjPtr< Observer >&                               observer,
                    SAA_in              const std::shared_ptr< cpp::any >&                          value
                    )
                    :
                    m_observer( om::copy( observer ) ),
                    m_value( value )
                {
                }

            public:

                virtual time::time_duration run() OVERRIDE
                {
                    if( isCanceled() || m_observer -> onNext( *m_value ) )
                    {
                        /*
                         * The observer has accepted the value; we're done.
                         */

                        return time::neg_infin;
                    }

                    /*
                     * If we're here that means the observer has not accepted the value.
                     * Wait for a bit and retry.
                     */

                    return time::milliseconds( DEFAULT_WAIT_TIME_IN_MILLISECONDS );
                }
            };

            typedef om::ObjectImpl< ObserverNextInvokerT<> > ObserverNextInvokerImpl;

            /**
             * @brief A helper to facilitate single execution of onCompleted
             */

            class SingletonOnCompleteExecute : public om::ObjectDefaultBase
            {
                BL_DECLARE_OBJECT_IMPL_ONEIFACE( SingletonOnCompleteExecute, Object )

            protected:

                om::ObjPtr< Observer >                                              m_observer;
                cpp::ScalarTypeIniter< bool >                                       m_onCompletedCalled;
                os::mutex                                                           m_lock;

                SingletonOnCompleteExecute( SAA_in const om::ObjPtr< Observer >& observer )
                    :
                    m_observer( om::copy( observer ) )
                {
                }

            public:

                void callOnCompleted()
                {
                    BL_MUTEX_GUARD( m_lock );

                    if( ! m_onCompletedCalled )
                    {
                        m_observer -> onCompleted();
                        m_onCompletedCalled = true;
                    }
                }
            };

            typedef om::ObjectImpl< SingletonOnCompleteExecute > SingletonOnCompleteExecuteImpl;

            /**
             * @brief The subscription info holder
             */

            class SubscriptionInfo
            {
                BL_CTR_DEFAULT( SubscriptionInfo, public )
                BL_CTR_COPY_DELETE( SubscriptionInfo )

            public:

                /*
                 * The observer for this subscription registration
                 */

                uuid_t                                                              subscriptionId;
                om::ObjPtr< Observer >                                              observer;
                om::ObjPtr< detail::SingletonOnCompleteExecuteImpl >                singleOnComplete;
                cpp::ScalarTypeIniter< bool >                                       disposing;
                cpp::ScalarTypeIniter< bool >                                       completedScheduled;

                /*
                 * The events queue for this observer
                 *
                 * This queue will be stranded, so all events are queued and always
                 * executed serially
                 */

                om::ObjPtrDisposable< tasks::ExecutionQueue >                       eventsQueue;
            };

        } // detail

        /**
         * @brief class ObservableBase
         */
        template
        <
            typename E
        >
        class ObservableBaseT :
            public Observable,
            public om::Disposable,
            public ErrorDispatcher,
            public tasks::TimerTaskBase
        {
            BL_DECLARE_OBJECT_IMPL_NO_DESTRUCTOR( ObservableBaseT )

            BL_QITBL_BEGIN()
                BL_QITBL_ENTRY( Observable )
                BL_QITBL_ENTRY( om::Disposable )
                BL_QITBL_ENTRY( ErrorDispatcher )
                BL_QITBL_ENTRY( tasks::Task )
            BL_QITBL_END( Observable )

        public:

            enum
            {
                DEFAULT_WAIT_TIME_IN_MILLISECONDS = detail::DEFAULT_WAIT_TIME_IN_MILLISECONDS,
            };

            enum
            {
                DEFAULT_THROTTLE_LIMIT = 1024,
            };

            typedef ObservableBaseT< E >                                            this_type;

            typedef om::ObjPtrCopyable< this_type, Observable >                     copyable_this_type_t;
            typedef om::ObjPtrCopyable< detail::SingletonOnCompleteExecuteImpl >    copyable_oncomplete_t;

        protected:

            typedef cpp::function<
                void ( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& )
                >
                callback_subscription_t;

            typedef std::unordered_map<
                uuid_t,
                cpp::SafeUniquePtr< detail::SubscriptionInfo >
                >
                subscriptions_map_t;

            subscriptions_map_t                                                     m_subscriptions;
            cpp::ScalarTypeIniter< bool >                                           m_stopRequested;
            cpp::ScalarTypeIniter< bool >                                           m_disposed;
            cpp::ScalarTypeIniter< bool >                                           m_completed;
            cpp::ScalarTypeIniter< bool >                                           m_notifyCompleteOnError;
            cpp::ScalarTypeIniter< bool >                                           m_ignoreSubscribersFailures;
            cpp::ScalarTypeIniter< bool >                                           m_allowNoSubscribers;
            std::size_t                                                             m_maxPendingEvents;
            std::exception_ptr                                                      m_dispatchedException;

        private:

            /*
             * Observable objects are implemented as timer tasks, but they can't be cancelled as typical
             * tasks / timer tasks can be because they have to unwind correctly (possibly stop sub-tasks
             * etc), so they can only be stopped or rather requested to stop and then wait for them to
             * stop / unwind as fast as they can
             *
             * To enforce this design invariant we need to override isCanceled(), requestCancel(),
             * cancelTask() and requestCancelInternal() and make them private, so they can't be
             * overridden or even used in the derived classes and make the public interface
             * requestCancel() method to simply call requestStop()
             *
             * Then we are going to define tryStopObservable() pure virtual method here which will be
             * responsible to try to stop the observable as soon as possible when called, but this call
             * should not be blocking in general
             *
             * If the observable logic is too complex, so it can't be stopped once it has started then
             * the only option is to implement tryStopObservable() to do nothing and just let it run its
             * course (more or less)
             *
             * At the end when the observable stops generating events if it was stopped deliberately the
             * task associated with the observable will fail with asio::error::operation_aborted, so this
             * will not go unnoticed anyway
             */

            bool isCanceled() const NOEXCEPT
            {
                return tasks::TimerTaskBase::isCanceled();
            }

            virtual void cancelTask() OVERRIDE
            {
                tasks::TimerTaskBase::cancelTask();
            }

            void requestCancelInternal() NOEXCEPT
            {
                tasks::TimerTaskBase::requestCancelInternal();
            }

            virtual void requestCancel() NOEXCEPT OVERRIDE
            {
                /*
                 * Observable objects cannot be cancelled as tasks, but they can only be stopped --
                 * see long comments above
                 */

                BL_MUTEX_GUARD( m_lock );

                requestStop();
            }

            void requestStop()
            {
                m_stopRequested = true;

                tryStopObservable();
            }

        protected:

            ObservableBaseT()
                :
                m_maxPendingEvents( DEFAULT_THROTTLE_LIMIT )
            {
            }

            ~ObservableBaseT() NOEXCEPT
            {
                BL_NOEXCEPT_BEGIN()

                BL_VERIFY( tryDisposeInternal( false /* calledExternally */ ) );

                BL_NOEXCEPT_END()
            }

            virtual void tryStopObservable() = 0;

            /**
             * @brief Checks if there are subscribers which have failed.
             *
             * Failed will be unsubscribed and if m_ignoreSubscribersFailures
             * is set to true the exception won't be propagated in the
             * current observer. Otherwise we will re-throw the exception
             * in the current observer which will disconnect everyone
             * and fail the task
             */

            void checkSubscribersQueues()
            {
                /*
                 * Check the subscriber queues for errors and handle appropriately:
                 *
                 *  1) if m_ignoreSubscribersFailures=true we will just silently
                 *     disconnect the subscriber
                 *
                 *  2) if m_ignoreSubscribersFailures=false we will disconnect
                 *     the subscriber, but also raise an error in the current
                 *     task / observable
                 */

                om::ObjPtr< tasks::Task > failedTaskSave;
                std::vector< uuid_t > unregisterIds;

                const auto eptr = tryForEachSubscriptionNothrow(
                    [ &unregisterIds, &failedTaskSave ]( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& subscription ) -> void
                    {
                        if( subscription -> eventsQueue && subscription -> eventsQueue -> hasReady() )
                        {
                            /*
                             * We have a failed task
                             */

                            const auto failedTask = subscription -> eventsQueue -> pop( false /* wait */ );
                            BL_ASSERT( failedTask && failedTask -> isFailed() );

                            unregisterIds.push_back( subscription -> subscriptionId );

                            try
                            {
                                cpp::safeRethrowException( failedTask -> exception() );
                            }
                            catch( ObjectDisconnectedException& e )
                            {
                                /*
                                 * Object has disconnected; we should unregister it,
                                 * but no need to fail the observable
                                 */

                                BL_LOG(
                                    Logging::debug(),
                                    BL_MSG()
                                        << e.what()
                                    );
                            }
                            catch( std::exception& )
                            {
                                if( ! failedTaskSave )
                                {
                                    failedTaskSave = om::copy( failedTask );
                                }
                            }
                        }
                    }
                    );

                /*
                 * Before we check if we should throw we first want to
                 * unsubscribe the failed subscriptions
                 */

                for( const auto& subscriptionId : unregisterIds )
                {
                    unsubscribeInternal( subscriptionId, false /* wait */ );
                }

                /*
                 * Check if we should rethrow exceptions with eptr
                 * taking precedence over eptrSubscription
                 */

                if( eptr )
                {
                    cpp::safeRethrowException( eptr );
                }

                if( failedTaskSave && ! m_ignoreSubscribersFailures )
                {
                    cpp::safeRethrowException( failedTaskSave -> exception() );
                }
            }

            /**
             * @brief Tries executing a callback for each subscription
             *
             * On failure the first exception is saved and returned
             * All other exceptions are just reported
             */

            std::exception_ptr tryForEachSubscriptionNothrow( SAA_in const callback_subscription_t& cb ) NOEXCEPT
            {
                std::exception_ptr eptr = nullptr;

                for( auto& subscription : m_subscriptions )
                {
                    utils::tryCatchLog(
                        BL_MSG_CB(
                            BL_MSG()
                                << "Observable "
                                << this
                                << " failed to execute subscription event"
                            ),
                        [ & ]() -> void
                        {
                            cb( subscription.second );
                        },
                        [ & ]() -> void
                        {
                            if( nullptr == eptr )
                            {
                                eptr = std::current_exception();
                            }
                        }
                        );
                }

                return eptr;
            }

            /**
             * @brief Tries executing a callback for each subscription
             *
             * On failure the first exception is thrown
             * All other exceptions are just reported
             */

            void forEachSubscription( SAA_in const callback_subscription_t& cb )
            {
                std::exception_ptr eptr = tryForEachSubscriptionNothrow( cb );

                if( eptr )
                {
                    cpp::safeRethrowException( eptr );
                }
            }

            void notifyObserverComplete( SAA_in const copyable_oncomplete_t& onCompleteSingleton ) NOEXCEPT
            {
                BL_WARN_NOEXCEPT_BEGIN()

                onCompleteSingleton -> callOnCompleted();

                BL_WARN_NOEXCEPT_END( "ObservableBase::notifyObserverComplete" )
            }

            bool tryFlushObserversEventQueues() NOEXCEPT
            {
                bool flushed = true;

                /*
                 * For each observer cancel all events in the queue and send onCompleted()
                 *
                 * Note also that we can't really handle exceptions from this call, so
                 * if an exception is thrown our only option is to abort the app
                 */

                BL_NOEXCEPT_BEGIN()

                forEachSubscription(
                    [ this, &flushed ]( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& subscription ) -> void
                    {
                        if( subscription -> eventsQueue )
                        {
                            if( ! subscription -> disposing )
                            {
                                subscription -> eventsQueue -> forceFlushNoThrow( false /* wait */ );

                                subscription -> eventsQueue -> push_back(
                                    cpp::bind(
                                        &this_type::notifyObserverComplete,
                                        copyable_this_type_t::acquireRef( this ),
                                        copyable_oncomplete_t::acquireRef( subscription -> singleOnComplete.get() )
                                        )
                                    );

                                subscription -> completedScheduled = true;

                                subscription -> disposing = true;
                            }

                            if( flushed && false == subscription -> eventsQueue -> isEmpty() )
                            {
                                flushed = false;
                            }

                            if( flushed )
                            {
                                subscription -> eventsQueue -> dispose();
                                subscription -> eventsQueue.reset();
                            }
                        }
                    }
                    );

                BL_NOEXCEPT_END()

                return flushed;
            }

            virtual bool tryDisposeInternal( SAA_in const bool calledExternally ) NOEXCEPT
            {
                if( m_disposed )
                {
                    BL_ASSERT( m_subscriptions.empty() );
                    return true;
                }

                if( calledExternally )
                {
                    requestStop();

                    m_disposed = tryFlushObserversEventQueues();

                    if( m_disposed )
                    {
                        m_subscriptions.clear();
                    }
                }
                else
                {
                    BL_ASSERT( Running != m_state );
                    m_disposed = true;
                }

                return m_disposed;
            }

            void chkNotDisposed()
            {
                BL_CHK(
                    true,
                    m_disposed,
                    BL_MSG()
                        << "Attempt to subscribe to observable which has been disposed"
                    );
            }

            bool chk2DisposeExecutionQueue(
                SAA_in          const cpp::SafeUniquePtr< detail::SubscriptionInfo >&               info,
                SAA_in          const bool                                                          wait
                )
            {
                if( ! info -> eventsQueue )
                {
                    return true;
                }

                info -> eventsQueue -> forceFlushNoThrow( wait );

                if( wait || info -> eventsQueue -> isEmpty() )
                {
                    BL_ASSERT( info -> eventsQueue -> isEmpty() );

                    info -> eventsQueue -> dispose();
                    info -> eventsQueue.reset();

                    return true;
                }

                return false;
            }

            void chk2InitExecutionQueue( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& info )
            {
                /*
                 * This can only be called when the task is active
                 */

                BL_ASSERT( m_eq );

                if( ! info -> eventsQueue )
                {
                    /*
                     * Create a stranded events queue for the observer - i.e. only one event will be
                     * executing at a time
                     */

                    info -> eventsQueue = tasks::ExecutionQueueImpl::createInstance< tasks::ExecutionQueue >(
                         tasks::ExecutionQueue::OptionKeepFailed,
                         1          /* maxExecuting */
                         );
                }
            }

            bool notifyOnNext( SAA_in cpp::any&& value )
            {
                /*
                 * Before we push out the new event check the subscriber queues for errors
                 *
                 * Then we check if we need to throttle the events
                 */

                if( ! m_allowNoSubscribers )
                {
                    BL_CHK(
                        0,
                        m_subscriptions.size(),
                        BL_MSG()
                            << "Observable cannot have empty subscribers list"
                        );
                }

                checkSubscribersQueues();

                for( const auto& entry : m_subscriptions )
                {
                    const auto& subscription = entry.second;

                    if( ! subscription -> eventsQueue )
                    {
                        continue;
                    }

                    if( m_maxPendingEvents && subscription -> eventsQueue -> size() >= m_maxPendingEvents )
                    {
                        /*
                         * We have a throttle limit and some of the event queues are full.
                         * Reject the request and notify the caller it should wait and retry
                         */

                        return false;
                    }

                    if( subscription -> completedScheduled )
                    {
                        /*
                         * onCompleted() event has already been scheduled
                         *
                         * We can't schedule onError() event at this point
                         */

                        return false;
                    }
                }

                const auto sharedValue = std::make_shared< cpp::any >();
                sharedValue -> swap( value );

                forEachSubscription(
                    [ &sharedValue, this ]( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& subscription ) -> void
                    {
                        chk2InitExecutionQueue( subscription );

                        const auto nextInvoker = detail::ObserverNextInvokerImpl::createInstance< Task >(
                            subscription -> observer,
                            sharedValue
                            );

                        subscription -> eventsQueue -> push_back( nextInvoker );
                    }
                    );

                return true;
            }

            void notifyObserverError(
                SAA_in              const om::ObjPtrCopyable< Observer >&                       observer,
                SAA_in              const std::exception_ptr&                                   eptr
                ) NOEXCEPT
            {
                BL_WARN_NOEXCEPT_BEGIN()

                observer -> onError( eptr );

                BL_WARN_NOEXCEPT_END( "ObservableBase::notifyObserverError" )
            }

            /**
             * @brief Nothrow on error notify helper
             */

            void notifyOnErrorNothrow( SAA_in const std::exception_ptr& eptr ) NOEXCEPT
            {
                /*
                 * For each observer cancel all events in the queue and send onCompleted()
                 */

                ( void ) tryForEachSubscriptionNothrow(
                    [ &eptr, this ]( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& subscription ) -> void
                    {
                        if( subscription -> completedScheduled )
                        {
                            /*
                             * onCompleted() event has already been scheduled
                             *
                             * We can't schedule onError() event at this point
                             */

                            return;
                        }

                        chk2InitExecutionQueue( subscription );

                        /*
                         * Discard all pending events since the error event must be
                         * delivered as soon as possible
                         */

                        subscription -> eventsQueue -> forceFlushNoThrow( false /* wait */ );

                        subscription -> eventsQueue -> push_back(
                            cpp::bind(
                                &this_type::notifyObserverError,
                                copyable_this_type_t::acquireRef( this ),
                                om::ObjPtrCopyable< Observer >( subscription -> observer ),
                                eptr
                                )
                            );
                    }
                    );
            }

            /**
             * @brief Notify subscribers we're done
             *
             * By default it throws
             */

            void notifyOnCompleteInternal( SAA_in const bool nothrow = false )
            {
                if( m_completed )
                {
                    return;
                }

                m_completed = true;

                /*
                 * For each observer cancel all events in the queue and send onCompleted()
                 */

                const auto eptr = tryForEachSubscriptionNothrow(
                    [ this ]( SAA_in const cpp::SafeUniquePtr< detail::SubscriptionInfo >& subscription ) -> void
                    {
                        chk2InitExecutionQueue( subscription );

                        subscription -> eventsQueue -> push_back(
                            cpp::bind(
                                &this_type::notifyObserverComplete,
                                copyable_this_type_t::acquireRef( this ),
                                copyable_oncomplete_t::acquireRef( subscription -> singleOnComplete.get() )
                                )
                            );

                        subscription -> completedScheduled = true;
                    }
                    );

                if( eptr && false == nothrow )
                {
                    cpp::safeRethrowException( eptr );
                }
            }

            void notifyOnComplete()
            {
                notifyOnCompleteInternal( false /* nothrow */ );
            }

            void notifyOnCompleteNothrow() NOEXCEPT
            {
                notifyOnCompleteInternal( true /* nothrow */ );
            }

            bool areAllEventsFlushed() const NOEXCEPT
            {
                for( auto& entry : m_subscriptions )
                {
                    auto& subscription = entry.second;

                    if( ! subscription -> eventsQueue )
                    {
                        continue;
                    }

                    if( ! subscription -> eventsQueue -> isEmpty() )
                    {
                        return false;
                    }
                }

                return true;
            }

            void chk2ThrowIfStopped() const
            {
                if( m_stopRequested )
                {
                    BL_CHK_EC(
                        asio::error::operation_aborted,
                        "An observable task was canceled"
                        );
                }
            }

            time::time_duration chk2WaitAllEvents2Flush()
            {
                const auto duration = chk2LoopUntilShutdownFinished();

                if( ! duration.is_special() )
                {
                    /*
                     * Shutdown is still pending ...
                     */

                    return duration;
                }

                /*
                 * First we check if some subscriber queues have
                 * failed and if so remember the exception.
                 *
                 * The subscriber queues which have failed
                 * executions will eventually be unsubscribed
                 */

                std::exception_ptr eptr;

                try
                {
                    checkSubscribersQueues();
                }
                catch( std::exception& )
                {
                    eptr = std::current_exception();
                }

                if( eptr )
                {
                    if( nullptr == m_exception )
                    {
                        m_exception = eptr;
                    }
                }

                if( areAllEventsFlushed() )
                {
                    if( m_exception )
                    {
                        cpp::safeRethrowException( m_exception );
                    }

                    chk2ThrowIfStopped();

                    return time::neg_infin;
                }

                /*
                 * There are still events in the event queue to be flushed.
                 * Wait a little bit...
                 */

                return time::milliseconds( DEFAULT_WAIT_TIME_IN_MILLISECONDS );
            }

            virtual time::time_duration chk2LoopUntilShutdownFinished() NOEXCEPT
            {
                /*
                 * By default finish the shutdown immediately
                 */

                return time::neg_infin;
            }

            virtual time::time_duration chk2LoopUntilFinished()
            {
                /*
                 * By default finish immediately
                 */

                return time::neg_infin;
            }

            virtual time::time_duration run() OVERRIDE
            {
                if( m_completed )
                {
                    /*
                     * The observable has completed, but not all events are flushed yet.
                     * Wait until all events are flushed through the event queues
                     */

                    return chk2WaitAllEvents2Flush();
                }

                if( m_notifyCompleteOnError )
                {
                    notifyOnCompleteNothrow();

                    return time::milliseconds( 0 );
                }

                std::exception_ptr eptr;

                try
                {
                    /*
                     * If we have an externally dispatched exception let's throw it
                     */

                    if( m_dispatchedException )
                    {
                        const auto dispatchedException = m_dispatchedException;
                        m_dispatchedException = nullptr;

                        cpp::safeRethrowException( dispatchedException );
                    }

                    /*
                     * Before we go to the main loop we want to check the
                     * subscriber queues for failures.
                     */

                    checkSubscribersQueues();

                    const auto duration = chk2LoopUntilFinished();

                    if( ! duration.is_special() )
                    {
                        /*
                         * We're not finished yet
                         */

                        return duration;
                    }

                    notifyOnComplete();
                }
                catch( std::exception& )
                {
                    eptr = std::current_exception();
                }

                if( eptr )
                {
                    /*
                     * An error has occurred, so we initiate shutdown immediately
                     *
                     * Note: both calls below are nothrow operations
                     *
                     * Note: we save the exception in m_exception, so when we're
                     * done and ready to exit the task we will fail with the
                     * appropriate exception
                     */

                    m_exception = eptr;

                    notifyOnErrorNothrow( m_exception );

                    requestStop();

                    m_notifyCompleteOnError = true;
                }

                /*
                 * If we're here that means we either failed or we completed
                 * gracefully. Now we just enter the wait for everything to
                 * shut down gracefully.
                 */

                return time::milliseconds( 0 );
            }

            bool unsubscribeInternal( SAA_in const uuid_t& subscriptionId, SAA_in const bool wait )
            {
                const auto pos = m_subscriptions.find( subscriptionId );

                if( pos != m_subscriptions.end() && chk2DisposeExecutionQueue( pos -> second, wait ) )
                {
                    m_subscriptions.erase( pos );

                    return true;
                }

                return false;
            }

        public:

            virtual void  dispose() OVERRIDE
            {
                BL_NOEXCEPT_BEGIN()

                /*
                 * Just keep trying to dispose until it is done
                 */

                for ( ;; )
                {
                    {
                        BL_MUTEX_GUARD( m_lock );

                        if( tryDisposeInternal( true /* calledExternally */ ) )
                        {
                            break;
                        }
                    }

                    os::sleep( time::milliseconds( DEFAULT_WAIT_TIME_IN_MILLISECONDS ) );
                }

                BL_NOEXCEPT_END()
            }

            virtual void dispatchException( SAA_in const std::exception_ptr eptr ) NOEXCEPT OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                if( ! m_dispatchedException )
                {
                    m_dispatchedException = eptr;
                }
            }

            virtual om::ObjPtr< om::Disposable > subscribe( SAA_in const om::ObjPtr< Observer >& observer ) OVERRIDE
            {
                BL_MUTEX_GUARD( m_lock );

                chkNotDisposed();

                const auto subscriptionId = uuids::create();
                auto& info = m_subscriptions[ subscriptionId ];
                info.reset( new detail::SubscriptionInfo );
                info -> observer = om::copy( observer );
                info -> singleOnComplete =
                        detail::SingletonOnCompleteExecuteImpl::createInstance( observer );
                info -> subscriptionId = subscriptionId;

                const auto sharedThis = std::shared_ptr< ObservableBase >( om::getSharedPtr< Observable >( this ), this );

                return detail::ObserverDisposerImpl::createInstance< om::Disposable >( subscriptionId, sharedThis );
            }

            void setThrottleLimit( SAA_in const std::size_t maxPendingEvents = 0 )
            {
                BL_MUTEX_GUARD( m_lock );

                m_maxPendingEvents = maxPendingEvents;
            }

            bool unsubscribe( SAA_in const uuid_t& subscriptionId )
            {
                BL_MUTEX_GUARD( m_lock );

                return unsubscribeInternal( subscriptionId, true /* wait */ );
            }

            bool allowNoSubscribers() const NOEXCEPT
            {
                return m_allowNoSubscribers;
            }

            void allowNoSubscribers( SAA_in const bool allowNoSubscribers ) NOEXCEPT
            {
                m_allowNoSubscribers = allowNoSubscribers;
            }
        };

        namespace detail
        {
            /*
             * Methods of ObserverDisposerT< E > which need to be defined out of line
             */

            template
            <
                typename E
            >
            void
            ObserverDisposerT< E >::dispose()
            {
                const auto observable = m_observable.lock();

                if( observable )
                {
                    observable -> unsubscribe( m_subscriptionId );
                }

                m_observable.reset();
            }

        } // detail

    } // reactive

} // bl

#endif /* __BL_REACTIVE_OBSERVABLEBASE_H_ */
