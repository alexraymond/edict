#include <edict/Broadcaster.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    edict::SharedBroadcaster bus;
    std::atomic<int> total{0};

    auto sub = bus.subscribe("work", [&](int amount) {
        total.fetch_add(amount, std::memory_order_relaxed);
    });

    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&, i] {
            for (int j = 0; j < 100; ++j)
                bus.publish("work", i + 1);
        });
    }

    for (auto& t : workers) t.join();
    std::cout << "Total work: " << total << "\n";
    std::cout << "Expected:   " << (1 + 2 + 3 + 4) * 100 << "\n";
}
