// hello-world — The simplest possible Edict example.
// Try changing the channel type, adding more subscribers, or publishing different values.

#include <edict/Channel.h>
#include <iostream>
#include <string>

void on_greet(const std::string& name) {
    std::cout << "Hello, " << name << "!\n";
}

int main() {
    // A typed channel — publish and subscribe with compile-time type safety.
    edict::Channel<std::string> greet;

    // Subscribe a free function
    auto s1 = greet.subscribe(on_greet);

    // Subscribe a lambda
    auto s2 = greet.subscribe([](const std::string& name) {
        std::cout << "  (whispers) hey " << name << "...\n";
    });

    // Subscribe a zero-arg watcher — just gets notified, doesn't need the data
    auto s3 = greet.subscribe([]() {
        std::cout << "  [someone was greeted]\n";
    });

    greet.publish("World");
    std::cout << "\n";
    greet.publish("Edict");

    std::cout << "\n--- Unsubscribing s2 ---\n\n";
    s2.cancel();

    greet.publish("Again");
}
