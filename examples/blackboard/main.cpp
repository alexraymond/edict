// blackboard — Typed key-value state store with change observation.
// Try adding new keys, reading values at different times, or changing observer signatures.

#include <edict/Blackboard.h>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main() {
    edict::Blackboard<> board;

    // --- Full observer: sees old and new value ---
    auto health_obs = board.observe<int>("player/health",
        [](std::optional<int> old_val, int new_val) {
            if (!old_val)
                std::cout << "Health initialized: " << new_val << "\n";
            else
                std::cout << "Health: " << *old_val << " -> " << new_val
                          << " (" << (new_val > *old_val ? "+" : "") << (new_val - *old_val) << ")\n";
        });

    // --- Zero-arg watcher: just knows something changed ---
    auto any_health = board.observe<int>("player/health", []() {
        std::cout << "  [health updated]\n";
    });

    std::cout << "=== Setting values ===\n\n";

    board.set("player/health", 100);
    board.set("player/health", 75);
    board.set("player/health", 90);

    // Read current state at any time
    if (auto hp = board.get<int>("player/health"))
        std::cout << "\nCurrent health: " << *hp << "\n";

    // --- Store different types ---
    std::cout << "\n=== Mixed types ===\n\n";

    board.set("player/name", std::string("Alex"));
    board.set("player/level", 5);
    board.set("player/inventory", std::vector<std::string>{"sword", "shield", "potion"});

    if (auto name = board.get<std::string>("player/name"))
        std::cout << "Name: " << *name << "\n";

    if (auto level = board.get<int>("player/level"))
        std::cout << "Level: " << *level << "\n";

    if (auto inv = board.get<std::vector<std::string>>("player/inventory")) {
        std::cout << "Inventory: ";
        for (const auto& item : *inv)
            std::cout << item << " ";
        std::cout << "\n";
    }

    // --- Error handling: wrong type ---
    auto result = board.get<double>("player/level"); // level is int, not double
    if (!result)
        std::cout << "\nget<double>(\"player/level\"): " << edict::error_message(result.error()) << "\n";

    auto missing = board.get<int>("nonexistent");
    if (!missing)
        std::cout << "get<int>(\"nonexistent\"): " << edict::error_message(missing.error()) << "\n";

    // --- Keys and cleanup ---
    std::cout << "\nAll keys: ";
    for (const auto& k : board.keys())
        std::cout << k << " ";
    std::cout << "\n";

    board.erase("player/inventory");
    std::cout << "After erase: has inventory? " << (board.has("player/inventory") ? "yes" : "no") << "\n";
}
