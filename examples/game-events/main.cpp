// game-events — A game-like scenario with typed channels, components, and partial arg matching.
// Try adding new event types, changing priorities, or creating new entity classes.

#include <edict/Channel.h>
#include <iostream>
#include <string>

enum class DamageType { Fire, Ice, Physical };

std::string_view to_string(DamageType t) {
    switch (t) {
        case DamageType::Fire:     return "fire";
        case DamageType::Ice:      return "ice";
        case DamageType::Physical: return "physical";
    }
    return "unknown";
}

// Global typed channels — zero-cost, compile-time safe
edict::Channel<int, DamageType> on_damage;
edict::Channel<int>             on_heal;
edict::Channel<std::string>     on_chat;

class Player {
    edict::SubscriptionGroup subs;
    std::string name;
    int hp;
public:
    Player(std::string n, int max_hp) : name(std::move(n)), hp(max_hp) {
        // Full args — receives both amount and type
        subs += on_damage.subscribe([this](int amount, DamageType type) {
            hp -= amount;
            std::cout << name << " took " << amount << " " << to_string(type)
                      << " damage! HP: " << hp << "\n";
        });

        // Partial args — just the amount, ignores type
        subs += on_heal.subscribe([this](int amount) {
            hp = std::min(hp + amount, 100);
            std::cout << name << " healed " << amount << "! HP: " << hp << "\n";
        });

        // Zero-arg watcher on damage — just counts hits
        subs += on_damage.subscribe([this]() {
            if (hp <= 0)
                std::cout << "  ** " << name << " has been defeated! **\n";
        }, {.priority = -10}); // low priority — runs after the damage handler
    }

    int health() const { return hp; }
};

class DamageLogger {
    edict::SubscriptionGroup subs;
    int total = 0;
public:
    DamageLogger() {
        // High priority — logs before anything else processes
        subs += on_damage.subscribe([this](int amount) {
            total += amount;
            std::cout << "  [log] " << amount << " damage dealt (total: " << total << ")\n";
        }, {.priority = 100});
    }
};

int main() {
    Player hero("Hero", 100);
    Player sidekick("Sidekick", 60);
    DamageLogger logger;

    std::cout << "=== Combat begins ===\n\n";

    on_damage.publish(25, DamageType::Fire);
    std::cout << "\n";

    on_heal.publish(10);
    std::cout << "\n";

    on_damage.publish(40, DamageType::Ice);
    std::cout << "\n";

    on_damage.publish(50, DamageType::Physical);
    std::cout << "\n";

    on_chat.publish("GG");

    std::cout << "\n=== Final HP ===\n";
    std::cout << "Hero: " << hero.health() << "\n";
    std::cout << "Sidekick: " << sidekick.health() << "\n";
}
