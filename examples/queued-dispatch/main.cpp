// queued-dispatch — Game-loop style batched message delivery.
// Messages are queued during the frame, then dispatched all at once.
// Try changing the frame count, adding more event types, or nesting dispatches.

#include <edict/Broadcaster.h>
#include <iostream>
#include <string>

struct GameState {
    int score = 0;
    int enemies_killed = 0;
    int damage_taken = 0;
};

int main() {
    edict::Broadcaster<> bus;
    GameState state;

    // Subscribers process events when dispatch() is called
    auto score_sub = bus.subscribe("game/score", [&](int points) {
        state.score += points;
        std::cout << "  +Score: " << points << " (total: " << state.score << ")\n";
    });

    auto kill_sub = bus.subscribe("game/kill", [&](const std::string& enemy) {
        state.enemies_killed++;
        std::cout << "  Killed: " << enemy << " (#" << state.enemies_killed << ")\n";
    });

    auto damage_sub = bus.subscribe("game/damage", [&](int amount) {
        state.damage_taken += amount;
        std::cout << "  Ouch! " << amount << " damage (total: " << state.damage_taken << ")\n";
    });

    // Simulate 3 game frames
    for (int frame = 1; frame <= 3; ++frame) {
        std::cout << "=== Frame " << frame << " ===\n";

        // --- Simulation phase: queue events, nothing delivered yet ---
        switch (frame) {
            case 1:
                bus.queue("game/kill", std::string("Goblin"));
                bus.queue("game/score", 100);
                bus.queue("game/damage", 15);
                break;
            case 2:
                bus.queue("game/kill", std::string("Orc"));
                bus.queue("game/kill", std::string("Skeleton"));
                bus.queue("game/score", 250);
                break;
            case 3:
                bus.queue("game/damage", 40);
                bus.queue("game/kill", std::string("Dragon"));
                bus.queue("game/score", 1000);
                break;
        }

        std::cout << "(events queued, processing...)\n";

        // --- Dispatch phase: all events fire at once ---
        bus.dispatch();

        std::cout << "\n";
    }

    std::cout << "=== Game Over ===\n";
    std::cout << "Final score:    " << state.score << "\n";
    std::cout << "Enemies killed: " << state.enemies_killed << "\n";
    std::cout << "Damage taken:   " << state.damage_taken << "\n";
}
