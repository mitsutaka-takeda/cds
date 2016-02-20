#pragma once

#include <functional>
#include <list>
#include <thread>

#include "rxcpp/rx-includes.hpp"
#include "task-system.hpp"

namespace rxcpp {

    namespace schedulers {
        struct Unit{};

        struct thread_pool : public scheduler_interface {
        private:
            using this_type = thread_pool;

            thread_pool(const this_type&);

            mutable thread_factory factory;

            struct new_worker : public worker_interface {
            private:
                using this_type = new_worker;
                using queue_type = detail::action_queue;

                new_worker(const this_type&);

                struct new_worker_state : public std::enable_shared_from_this<new_worker_state> {
                    // A task with scheduled time. The scheduled time will determine when
                    // the task should be executed.
                    using queue_item_time = detail::schedulable_queue<typename clock_type::time_point>;
                    using item_type       = queue_item_time::item_type;

                    virtual ~new_worker_state()
                        {
                            std::unique_lock<std::mutex> guard(lock);
                            if (work_thread.joinable() && work_thread.get_id() != std::this_thread::get_id()) {
                                lifetime.unsubscribe();
                                guard.unlock();
                                work_thread.join();
                            }
                            else {
                                lifetime.unsubscribe();
                                work_thread.detach();
                            }
                        }

                    explicit new_worker_state(composite_subscription cs)
                        : lifetime(cs)
                        {
                        }

                    composite_subscription lifetime;
                    mutable std::mutex lock;
                    mutable std::condition_variable wake;
                    // what is queue_item_time?
                    mutable queue_item_time q;
                    std::thread work_thread;
                    recursion r;
                    std::list<future<Unit>> tasks;
                }; // struct new_worker_state

                std::shared_ptr<new_worker_state> state;

            public:

                explicit new_worker(std::shared_ptr<new_worker_state> ws)
                    : state(std::move(ws))
                    {
                    }

                new_worker(composite_subscription cs, thread_factory& tf)
                    : state(std::make_shared<new_worker_state>(cs))
                    {
                        auto keepAlive = state;

                        // Add an unsubscribe function.
                        state->lifetime.add([keepAlive](){
                                std::lock_guard<std::mutex> guard(keepAlive->lock);
                                auto expired = std::move(keepAlive->q);
                                if (!keepAlive->q.empty()) abort();
                                keepAlive->wake.notify_one();
                            });
                        // Create a worker thread
                        state->work_thread = tf([keepAlive](){

                                // take ownership ???
                                // queue is a thread local queue of type action_queue.
                                queue_type::ensure(std::make_shared<new_worker>(keepAlive));
                                // release ownership
                                RXCPP_UNWIND_AUTO([]{
                                        queue_type::destroy();
                                    });

                                for(;;) {
                                    std::unique_lock<std::mutex> guard(keepAlive->lock);

                                    if (keepAlive->q.empty()) {
                                        // wait until an item is pushed onto the queue
                                        // or the observer unsubscribes.
                                        keepAlive->wake.wait(guard, [keepAlive](){
                                                return !keepAlive->lifetime.is_subscribed() || !keepAlive->q.empty();
                                            });
                                    }
                                    if (!keepAlive->lifetime.is_subscribed()) {
                                        break;
                                    }
                                    // peek the next task(schedulable). The peek is the task(what) and scheduled time(when).
                                    auto& peek = keepAlive->q.top();
                                    if (!peek.what.is_subscribed()) {
                                        // if the next task is already unsubscribed, then pop.
                                        keepAlive->q.pop();
                                        continue;
                                    }
                                    // if the task is supposed to be triggered in the future
                                    // then wait for the moment.
                                    if (clock_type::now() < peek.when) {
                                        keepAlive->wake.wait_until(guard, peek.when);
                                        continue;
                                    }
                                    auto what = peek.what;
                                    keepAlive->q.pop();
                                    keepAlive->r.reset(keepAlive->q.empty());
                                    guard.unlock();
                                    for(auto i = 0; i < 4; ++i) {
                                        auto f = async([what, keepAlive]{ 
                                                what(keepAlive->r.get_recurse());
                                                return Unit{};
                                            });
                                        auto pos = keepAlive->tasks.insert(keepAlive->tasks.end(), f);
                                        auto g = f.then(
                                            [keepAlive, pos = keepAlive->tasks.insert(keepAlive->tasks.end(), f)]
                                            (Unit)
                                            {
                                                std::lock_guard<std::mutex> g(keepAlive->lock);
                                                keepAlive->tasks.erase(pos);
                                                return Unit{};
                                            });
                                        *pos = g;
                                    }
                                }
                            });
                    }

                virtual clock_type::time_point now() const {
                    return clock_type::now();
                }

                virtual void schedule(const schedulable& scbl) const {
                    schedule(now(), scbl);
                }

                virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
                    if (scbl.is_subscribed()) {
                        std::unique_lock<std::mutex> guard(state->lock);
                        state->q.push(new_worker_state::item_type(when, scbl));
                        state->r.reset(false);
                    }
                    state->wake.notify_one();
                }
            }; // class worker end

        public:
            thread_pool()
                : factory([](std::function<void()> start){
                        return std::thread(std::move(start));
                    })
                {
                }
            explicit thread_pool(thread_factory tf)
                : factory(tf)
                {
                }

            virtual clock_type::time_point now() const {
                return clock_type::now();
            }

            virtual worker create_worker(composite_subscription cs) const {
                return worker(cs, std::make_shared<new_worker>(cs, factory));
            }
        };

        inline
        scheduler 
        make_thread_pool(){
            return make_scheduler<rxcpp::schedulers::thread_pool>();
        }

    } // namespace schedulers

    namespace operators {

        class observe_on_multiple_worker : public rxcpp::coordination_base
        {
            rxsc::scheduler scheduler_;

            class input_type
            {
                rxsc::worker controller;
                rxsc::scheduler factory;
                identity_one_worker coordination;
            public:
                explicit input_type(rxsc::worker w)
                    : controller(w)
                    , factory(rxsc::make_same_worker(w))
                    , coordination(factory)
                    {
                    }
                inline rxsc::worker get_worker() const {
                    return controller;
                }
                inline rxsc::scheduler get_scheduler() const {
                    return factory;
                }
                inline rxsc::scheduler::clock_type::time_point now() const {
                    return factory.now();
                }
                template<class Observable>
                auto in(Observable o) const
                    -> decltype(o.observe_on(coordination)) {
                    return      o.observe_on(coordination);
                }
                template<class Subscriber>
                auto out(Subscriber s) const
                    -> Subscriber {
                    return s;
                }
                template<class F>
                auto act(F f) const
                    -> F {
                    return f;
                }
            };

        public:

            explicit observe_on_multiple_worker(rxsc::scheduler sc) : scheduler_(sc) {}

            using coordinator_type = coordinator<input_type>;

            inline rxsc::scheduler::clock_type::time_point now() const {
                return scheduler_.now();
            }

            inline coordinator_type create_coordinator(composite_subscription cs = composite_subscription()) const {
                auto w = scheduler_.create_worker(std::move(cs));
                return coordinator_type(input_type(std::move(w)));
            }
        };
        
        inline
        observe_on_multiple_worker observe_on_thread_pool(){
            return observe_on_multiple_worker(rxcpp::schedulers::make_thread_pool());
        }
    } // namespace operators
} // namespace rxcpp
