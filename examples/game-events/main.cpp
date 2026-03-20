#include <edict/Channel.h>
#include <iostream>
#include <string>

enum class DamageType { Fire, Ice, Physical };

edict::Channel<int, DamageType> damage("player/damage");
edict::Channel<int> heal("player/heal");
edict::Channel<> death("player/death");

class Player {
    edict::SubscriptionGroup subs;
    int hp = 100;
public:
    Player() {
        subs += damage.subscribe([this](int amount, DamageType) {
            hp -= amount;
            std::cout << "Took " << amount << " damage! HP: " << hp << "\n";
        });
        subs += heal.subscribe([this](int amount) {
            hp += amount;
            std::cout << "Healed " << amount << "! HP: " << hp << "\n";
        });
        subs += death.subscribe([this]() {
            std::cout << "Player died! Final HP: " << hp << "\n";
        });
    }
    int health() const { return hp; }
};

int main() {
    Player player;

    damage.publish(30, DamageType::Fire);
    heal.publish(10);
    damage.publish(85, DamageType::Ice);

    if (player.health() <= 0)
        death.publish();
}
