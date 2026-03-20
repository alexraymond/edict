#include <edict/Broadcaster.h>
#include <iostream>

int main() {
    edict::Broadcaster<> bus;

    auto sub = bus.subscribe("tick", [](int frame) {
        std::cout << "Processing frame " << frame << "\n";
    });

    // Simulate a game loop: queue events, dispatch once per frame
    for (int frame = 1; frame <= 3; ++frame) {
        // Queue events during the frame
        bus.queue("tick", frame);
        bus.queue("tick", frame);

        // Dispatch all at once (e.g., end of frame)
        std::cout << "--- Frame " << frame << " ---\n";
        bus.dispatch();
    }
}
