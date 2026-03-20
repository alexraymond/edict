// broadcaster — Dynamic string-topic pub/sub with wildcards, predicates, and filters.
// Try publishing to different topics, adding new predicates, or changing filter thresholds.

#include <edict/Broadcaster.h>
#include <edict/Error.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster<> bus;

    // --- Exact topic subscription ---
    auto temp_sub = bus.subscribe("sensor/kitchen/temperature", [](double val) {
        std::cout << "Kitchen temp: " << val << " C\n";
    });

    // --- Wildcard: * matches one segment ---
    auto any_sensor = bus.subscribe_pattern("sensor/*/temperature", [](double val) {
        std::cout << "  [some room] temp: " << val << " C\n";
    });

    // --- Wildcard: ** matches everything under a prefix ---
    auto all_sensors = bus.subscribe_pattern("sensor/**", []() {
        std::cout << "  [sensor activity detected]\n";
    });

    // --- Predicate: custom matching logic ---
    auto alerts = bus.subscribe(
        [](std::string_view topic) { return topic.starts_with("alert/"); },
        [](const std::string& msg) {
            std::cout << "!! ALERT: " << msg << " !!\n";
        });

    // --- Filtered subscription: only fires when condition is met ---
    auto high_temp = bus.subscribe("sensor/kitchen/temperature",
        [](double val) {
            std::cout << "  >>> HIGH TEMP WARNING: " << val << " C <<<\n";
        },
        edict::filter([](double val) { return val > 30.0; }));

    std::cout << "=== Publishing sensor data ===\n\n";

    bus.publish("sensor/kitchen/temperature", 22.5);
    std::cout << "\n";

    bus.publish("sensor/bedroom/temperature", 19.0);
    std::cout << "\n";

    bus.publish("sensor/kitchen/temperature", 35.0);  // triggers the filter!
    std::cout << "\n";

    bus.publish("sensor/garage/humidity", 65.0);
    std::cout << "\n";

    bus.publish("alert/fire", std::string("Smoke detected in garage"));
    std::cout << "\n";

    // --- Queue and dispatch ---
    std::cout << "=== Queuing messages ===\n";
    bus.queue("sensor/kitchen/temperature", 23.1);
    bus.queue("sensor/kitchen/temperature", 23.5);
    bus.queue("alert/system", std::string("Battery low"));
    std::cout << "(nothing delivered yet)\n\n";

    std::cout << "=== Dispatching ===\n\n";
    bus.dispatch();

    // --- Introspection ---
    std::cout << "Active exact topics: ";
    for (const auto& t : bus.active_topics())
        std::cout << t << " ";
    std::cout << "\n";
    std::cout << "Subscribers on sensor/kitchen/temperature: "
              << bus.subscriber_count("sensor/kitchen/temperature") << "\n";
}
