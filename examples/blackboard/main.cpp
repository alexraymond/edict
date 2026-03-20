#include <edict/Blackboard.h>
#include <iostream>
#include <optional>
#include <string>

int main() {
    edict::Blackboard<> board;

    // Observe health changes — full signature (old, new)
    auto obs = board.observe<int>("player/health",
        [](std::optional<int> old_val, int new_val) {
            if (!old_val)
                std::cout << "Health initialized to " << new_val << "\n";
            else
                std::cout << "Health: " << *old_val << " -> " << new_val << "\n";
        });

    // Zero-arg watcher — just notified of any change
    auto watcher = board.observe<int>("player/health",
        []() { std::cout << "  [health changed]\n"; });

    board.set("player/health", 100);
    board.set("player/health", 75);
    board.set("player/health", 30);

    // Read current state anytime
    auto hp = board.get<int>("player/health");
    if (hp)
        std::cout << "Current health: " << *hp << "\n";

    // Store different types
    board.set("player/name", std::string("Hero"));
    board.set("player/level", 5);

    std::cout << "Keys: ";
    for (const auto& k : board.keys())
        std::cout << k << " ";
    std::cout << "\n";
}
