#include <edict/Broadcaster.h>
#include <iostream>

int main() {
    edict::Broadcaster<> bus;

    // * matches exactly one segment
    auto s1 = bus.subscribe_pattern("home/*/temperature", [](double val) {
        std::cout << "Room temp: " << val << "\n";
    });

    // ** matches zero or more segments
    auto s2 = bus.subscribe_pattern("home/**", []() {
        std::cout << "  [home event]\n";
    });

    auto s3 = bus.subscribe("home/kitchen/temperature", [](double val) {
        std::cout << "Kitchen specifically: " << val << "\n";
    });

    bus.publish("home/kitchen/temperature", 22.0);
    bus.publish("home/bedroom/temperature", 19.5);
    bus.publish("home/garage/door", 1);
}
