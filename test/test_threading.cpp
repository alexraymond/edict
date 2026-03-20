#include <doctest/doctest.h>
#include <edict/Blackboard.h>
#include <edict/Broadcaster.h>
#include <atomic>
#include <thread>
#include <vector>
#include <latch>

TEST_CASE("SharedBroadcaster: concurrent publish") {
    edict::Broadcaster<edict::MultiThreaded> bus;
    std::atomic<int> count{0};
    auto sub = bus.subscribe("test", [&]() { count.fetch_add(1, std::memory_order_relaxed); });

    constexpr int num_threads = 8;
    constexpr int publishes_per_thread = 100;
    std::latch start(num_threads);
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&] {
            start.arrive_and_wait(); // synchronize start
            for (int j = 0; j < publishes_per_thread; ++j)
                bus.publish("test");
        });
    }

    for (auto& t : threads) t.join();
    CHECK(count == num_threads * publishes_per_thread);
}

TEST_CASE("SharedBroadcaster: concurrent subscribe/unsubscribe") {
    edict::Broadcaster<edict::MultiThreaded> bus;
    std::atomic<int> count{0};

    constexpr int num_threads = 8;
    std::latch start(num_threads);
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&] {
            start.arrive_and_wait();
            for (int j = 0; j < 50; ++j) {
                auto sub = bus.subscribe("test", [&]() {
                    count.fetch_add(1, std::memory_order_relaxed);
                });
                bus.publish("test");
            }
        });
    }

    for (auto& t : threads) t.join();
    CHECK(count > 0);
    CHECK_FALSE(bus.has_subscribers("test")); // all subs destroyed
}

TEST_CASE("SharedBroadcaster: reentrant publish does not deadlock") {
    edict::Broadcaster<edict::MultiThreaded> bus;
    std::vector<int> received;

    auto sub = bus.subscribe("test", [&](int v) {
        received.push_back(v);
        if (v == 1) bus.publish("test", 2); // reentrant — must not deadlock
    });

    bus.publish("test", 1);
    CHECK(received.size() == 2);
    CHECK(received[0] == 1);
    CHECK(received[1] == 2);
}

TEST_CASE("SharedBroadcaster: self-cancel during dispatch does not deadlock") {
    edict::Broadcaster<edict::MultiThreaded> bus;
    int count = 0;
    edict::Subscription sub;

    sub = bus.subscribe("test", [&]() {
        ++count;
        sub.cancel(); // reentrant cancel — must not deadlock
    });

    bus.publish("test");
    CHECK(count == 1);
    bus.publish("test");
    CHECK(count == 1); // cancelled
}

TEST_CASE("SharedBroadcaster: concurrent predicate subscribe/publish") {
    edict::Broadcaster<edict::MultiThreaded> bus;
    std::atomic<int> count{0};
    std::atomic<bool> running{true};

    // Thread that continuously adds/removes predicate subscriptions
    std::thread modifier([&] {
        while (running.load(std::memory_order_relaxed)) {
            auto sub = bus.subscribe(
                [](std::string_view t) { return t.starts_with("test"); },
                [&]() { count.fetch_add(1, std::memory_order_relaxed); });
            // sub immediately destroyed — unsubscribes
        }
    });

    // Concurrent publishes
    for (int i = 0; i < 200; ++i)
        bus.publish("test/data", i);

    running.store(false, std::memory_order_relaxed);
    modifier.join();
    // No crash, no TSan error = success
    CHECK(true);
}

TEST_CASE("SharedBlackboard: concurrent set and get") {
    edict::Blackboard<edict::MultiThreaded> bb;
    std::atomic<int> observations{0};

    auto obs = bb.observe<int>("counter", [&]() {
        observations.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int num_threads = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            for (int j = 0; j < 100; ++j) {
                bb.set("counter", i * 100 + j);
                auto val = bb.get<int>("counter");
                CHECK(val.has_value());
            }
        });
    }
    for (auto& t : threads) t.join();
    CHECK(observations == num_threads * 100);
}

TEST_CASE("SharedBlackboard: concurrent observe + erase") {
    edict::Blackboard<edict::MultiThreaded> bb;
    std::atomic<int> notifications{0};

    auto obs = bb.observe<int>("key", [&]() {
        notifications.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int rounds = 50;
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&, i] {
            for (int j = 0; j < rounds; ++j) {
                bb.set("key", i * rounds + j);
                bb.erase("key");
            }
        });
    }
    for (auto& t : threads) t.join();
    // Each set fires the observer, each erase fires zero-arg observers
    CHECK(notifications > 0);
}

TEST_CASE("SharedBroadcaster: concurrent dispatch") {
    edict::Broadcaster<edict::MultiThreaded> bus;
    std::atomic<int> count{0};
    auto sub = bus.subscribe("tick", [&]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    // Queue from main thread
    for (int i = 0; i < 100; ++i)
        bus.queue("tick");

    // Dispatch from multiple threads simultaneously
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
        threads.emplace_back([&] { bus.dispatch(); });
    for (auto& t : threads) t.join();

    // All 100 queued messages should be dispatched exactly once
    CHECK(count == 100);
}
