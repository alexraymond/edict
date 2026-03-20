// threaded — SharedBroadcaster with concurrent publishers and subscribers.
// Try changing the thread count, adding subscribe/unsubscribe during publish, etc.

#include <edict/Broadcaster.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

int main() {
    edict::SharedBroadcaster bus;
    std::mutex cout_mtx; // protect std::cout from interleaved output

    std::atomic<int> total_messages{0};
    std::atomic<int> total_value{0};

    // Subscriber that aggregates values from all threads
    auto aggregator = bus.subscribe("work/result", [&](int value) {
        total_value.fetch_add(value, std::memory_order_relaxed);
        total_messages.fetch_add(1, std::memory_order_relaxed);
    });

    // Subscriber that logs high-value results
    auto logger = bus.subscribe("work/result",
        [&](int value) {
            std::lock_guard lk(cout_mtx);
            std::cout << "  [high value] thread " << std::this_thread::get_id()
                      << " produced " << value << "\n";
        },
        edict::filter([](int value) { return value > 90; }));

    // Completion listener
    auto done_sub = bus.subscribe("work/done", [&](const std::string& worker) {
        std::lock_guard lk(cout_mtx);
        std::cout << worker << " finished\n";
    });

    constexpr int num_workers = 4;
    constexpr int work_per_thread = 50;

    std::cout << "=== Launching " << num_workers << " worker threads ===\n\n";
    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i) {
        workers.emplace_back([&, i] {
            for (int j = 0; j < work_per_thread; ++j) {
                int value = (i * work_per_thread + j) % 100;
                bus.publish("work/result", value);
            }
            bus.publish("work/done", std::string("Worker-" + std::to_string(i)));
        });
    }

    for (auto& t : workers) t.join();

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    std::cout << "\n=== Results ===\n";
    std::cout << "Total messages: " << total_messages << "\n";
    std::cout << "Total value:    " << total_value << "\n";
    std::cout << "Time:           " << ms << " us\n";
    auto safe_ms = ms > 0 ? ms : 1;
    std::cout << "Throughput:     " << (total_messages.load() * 1000000 / safe_ms) << " msg/s\n";

    // Demonstrate reentrant safety: publish from within a subscriber callback
    std::cout << "\n=== Reentrancy test ===\n";
    auto reentrant = bus.subscribe("ping", [&]() {
        bus.publish("pong");  // publish from within a subscriber — safe, no deadlock
    });
    auto pong_sub = bus.subscribe("pong", [&]() {
        std::cout << "Pong received! (reentrant publish worked)\n";
    });
    bus.publish("ping");
}
