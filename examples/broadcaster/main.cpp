#include <edict/Broadcaster.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster<> bus;

    auto s1 = bus.subscribe("sensor/temperature", [](double val) {
        std::cout << "Temperature: " << val << " C\n";
    });

    auto s2 = bus.subscribe_pattern("sensor/*", []() {
        std::cout << "  [sensor activity]\n";
    });

    auto s3 = bus.subscribe(
        [](std::string_view t) { return t.starts_with("alert/"); },
        []() { std::cout << "ALERT!\n"; });

    bus.publish("sensor/temperature", 23.5);
    bus.publish("sensor/humidity", 65.0);
    bus.publish("alert/fire");

    std::cout << "\nQueued dispatch:\n";
    bus.queue("sensor/temperature", 24.1);
    bus.queue("sensor/temperature", 24.8);
    bus.dispatch();
}
