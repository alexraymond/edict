#include <edict/Channel.h>
#include <iostream>
#include <string>

int main() {
    edict::Channel<std::string> greet("greet");

    auto sub = greet.subscribe([](const std::string& name) {
        std::cout << "Hello, " << name << "!\n";
    });

    greet.publish("World");
    greet.publish("Edict");
}
