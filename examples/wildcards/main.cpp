// wildcards — Hierarchical topic matching with * and ** patterns.
// Try publishing to different topic paths to see which subscribers fire.

#include <edict/Broadcaster.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster<> bus;

    // Exact match — only fires for this specific topic
    auto exact = bus.subscribe("home/kitchen/temperature", [](double val) {
        std::cout << "[exact]    kitchen temp: " << val << " C\n";
    });

    // * matches exactly ONE segment at that position
    auto single = bus.subscribe_pattern("home/*/temperature", [](double val) {
        std::cout << "[*]        some room temp: " << val << " C\n";
    });

    // ** matches ZERO OR MORE segments (must be at the end)
    auto multi = bus.subscribe_pattern("home/**", []() {
        std::cout << "[**]       home event detected\n";
    });

    // * in the middle
    auto mid_wild = bus.subscribe_pattern("home/*/sensor/*", [](double val) {
        std::cout << "[*/*/]     room sensor reading: " << val << "\n";
    });

    std::cout << "=== Publishing to various topics ===\n\n";

    std::cout << "--- home/kitchen/temperature ---\n";
    bus.publish("home/kitchen/temperature", 22.0);
    // Matches: exact, *, **

    std::cout << "\n--- home/bedroom/temperature ---\n";
    bus.publish("home/bedroom/temperature", 19.5);
    // Matches: *, ** (not exact — different room)

    std::cout << "\n--- home/garage/sensor/humidity ---\n";
    bus.publish("home/garage/sensor/humidity", 65.0);
    // Matches: **, mid_wild

    std::cout << "\n--- home/kitchen/sensor/co2 ---\n";
    bus.publish("home/kitchen/sensor/co2", 412.0);
    // Matches: **, mid_wild

    std::cout << "\n--- home ---\n";
    bus.publish("home");
    // Matches: ** only (zero segments after home)

    std::cout << "\n--- home/kitchen/oven/temperature/internal ---\n";
    bus.publish("home/kitchen/oven/temperature/internal", 180.0);
    // Matches: ** only (too deep for * patterns)

    std::cout << "\n--- office/temperature ---\n";
    bus.publish("office/temperature", 21.0);
    // Matches: nothing (different root)

    std::cout << "\n=== Summary ===\n";
    std::cout << "Subscribers on home/kitchen/temperature: "
              << bus.subscriber_count("home/kitchen/temperature") << "\n";
    std::cout << "Has subscribers on home/bedroom/temperature: "
              << (bus.has_subscribers("home/bedroom/temperature") ? "yes" : "no") << "\n";
    std::cout << "Has subscribers on office/temperature: "
              << (bus.has_subscribers("office/temperature") ? "yes" : "no") << "\n";
}
