---
layout: default
title: How-To Recipes
nav_order: 5
---

# How-To Recipes

Practical, copy-paste solutions for common patterns.

---

## Decouple game components

Use a shared `Broadcaster` as an event bus so components don't reference each other directly.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

class CombatSystem {
public:
    explicit CombatSystem(edict::Broadcaster& bus) : bus_(bus) {}

    void attack(const std::string& target, int damage) {
        std::cout << "Attacking " << target << " for " << damage << "\n";
        bus_.publish("combat/damage", target, damage);
    }

private:
    edict::Broadcaster& bus_;
};

class AudioSystem {
public:
    explicit AudioSystem(edict::Broadcaster& bus) {
        subs_ += bus.subscribe("combat/damage", [](std::string target, int amount) {
            std::cout << "  [Audio] Playing hit sound (damage=" << amount << ")\n";
        });
    }

private:
    edict::SubscriptionGroup subs_;
};

class UISystem {
public:
    explicit UISystem(edict::Broadcaster& bus) {
        subs_ += bus.subscribe("combat/damage", [](std::string target, int amount) {
            std::cout << "  [UI] Showing damage number: -" << amount << " on " << target << "\n";
        });
    }

private:
    edict::SubscriptionGroup subs_;
};

int main() {
    edict::Broadcaster bus;

    AudioSystem audio(bus);
    UISystem ui(bus);
    CombatSystem combat(bus);

    combat.attack("Goblin", 50);
    combat.attack("Dragon", 200);
}
```

Output:
```
Attacking Goblin for 50
  [Audio] Playing hit sound (damage=50)
  [UI] Showing damage number: -50 on Goblin
Attacking Dragon for 200
  [Audio] Playing hit sound (damage=200)
  [UI] Showing damage number: -200 on Dragon
```

`CombatSystem` knows nothing about `AudioSystem` or `UISystem`. Adding new systems means zero changes to existing code.

---

## Game loop with queued dispatch

Buffer events during the frame, flush them at a controlled point.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    auto sub = bus.subscribe("frame/event", [](std::string event) {
        std::cout << "  Processing: " << event << "\n";
    });

    for (int frame = 1; frame <= 3; ++frame) {
        std::cout << "Frame " << frame << " - gathering events\n";

        // Simulate systems queuing events
        bus.queue("frame/event", std::string("physics_step"));
        bus.queue("frame/event", std::string("ai_tick"));
        bus.queue("frame/event", std::string("animation_update"));

        std::cout << "Frame " << frame << " - dispatching\n";
        bus.dispatch();
    }
}
```

Output:
```
Frame 1 - gathering events
Frame 1 - dispatching
  Processing: physics_step
  Processing: ai_tick
  Processing: animation_update
Frame 2 - gathering events
Frame 2 - dispatching
  Processing: physics_step
  Processing: ai_tick
  Processing: animation_update
Frame 3 - gathering events
Frame 3 - dispatching
  Processing: physics_step
  Processing: ai_tick
  Processing: animation_update
```

`queue()` stores messages without firing any callbacks. `dispatch()` atomically swaps out the queue and delivers all messages. This gives you deterministic processing order within each frame.

---

## AI blackboard pattern

Use `Blackboard` as a shared knowledge store for AI decision-making.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <optional>
#include <string>

int main() {
    edict::Blackboard board;

    // AI system reacts to threat level changes
    auto threat_sub = board.observe<int>("threat_level",
        [](std::optional<int> old_val, int new_val) {
            if (new_val >= 80)
                std::cout << "AI: FLEE! Threat level " << new_val << "\n";
            else if (new_val >= 50)
                std::cout << "AI: Defend. Threat level " << new_val << "\n";
            else
                std::cout << "AI: Patrol. Threat level " << new_val << "\n";
        });

    // Perception system writes to the blackboard
    board.set("threat_level", 20);
    board.set("threat_level", 60);
    board.set("threat_level", 90);

    // Decision system reads current state
    auto threat = board.get<int>("threat_level");
    if (threat)
        std::cout << "Current threat: " << *threat << "\n";
}
```

Output:
```
AI: Patrol. Threat level 20
AI: Defend. Threat level 60
AI: FLEE! Threat level 90
Current threat: 90
```

Multiple AI subsystems can observe different keys independently. The blackboard decouples writers (perception, combat, world state) from readers (behavior trees, planners, animation).

---

## Late-joining subscribers with retention

Use retention to ensure subscribers don't miss important messages, even if they subscribe after the message was published.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    // Keep the last message on "config/difficulty"
    bus.retain("config/difficulty", 1);

    // Config is published at startup
    bus.publish("config/difficulty", std::string("hard"));

    // A UI panel subscribes later and immediately gets the current value
    auto sub = bus.subscribe("config/difficulty", [](std::string difficulty) {
        std::cout << "Difficulty is: " << difficulty << "\n";
    }, {.replay = true});

    // Future changes also arrive normally
    bus.publish("config/difficulty", std::string("easy"));
}
```

Output:
```
Difficulty is: hard
Difficulty is: easy
```

The first line is replayed from the retained message. The second is a normal publish.

### Retaining multiple messages

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    bus.retain("chat", 5);  // keep last 5 chat messages

    bus.publish("chat", std::string("Alice: hello"));
    bus.publish("chat", std::string("Bob: hi there"));
    bus.publish("chat", std::string("Alice: how are you?"));

    // New player joins and sees chat history
    auto sub = bus.subscribe("chat", [](std::string msg) {
        std::cout << msg << "\n";
    }, {.replay = true});
}
```

Output:
```
Alice: hello
Bob: hi there
Alice: how are you?
```

Call `bus.retain("chat", 0)` to disable retention and clear stored messages.

---

## Logging and audit system

Use wildcards to build a system-wide logger.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    // Log everything
    auto logger = bus.subscribe_pattern("**", []() {
        std::cout << "[AUDIT] event fired\n";
    });

    // Log all player events with details
    auto player_log = bus.subscribe_pattern("player/*", [](std::string detail) {
        std::cout << "[PLAYER] " << detail << "\n";
    });

    bus.publish("player/join", std::string("Alice connected"));
    std::cout << "---\n";
    bus.publish("player/leave", std::string("Bob disconnected"));
    std::cout << "---\n";
    bus.publish("system/shutdown", std::string("server stopping"));
}
```

Output:
```
[AUDIT] event fired
[PLAYER] Alice connected
---
[AUDIT] event fired
[PLAYER] Bob disconnected
---
[AUDIT] event fired
```

The `**` pattern matches every topic. The `player/*` pattern matches any single segment under `player/`.

### Priority-based logging

Use priority to ensure the logger always fires first, even before normal subscribers:

```cpp
#include <edict/edict.h>
#include <iostream>

int main() {
    edict::Broadcaster bus;

    auto handler = bus.subscribe("event", []() {
        std::cout << "Handler runs\n";
    });

    auto logger = bus.subscribe("event", []() {
        std::cout << "Logger runs first\n";
    }, {.priority = 100});

    bus.publish("event");
}
```

Output:
```
Logger runs first
Handler runs
```

---

## Error handling

Catch and log subscriber errors without crashing.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    bus.set_error_handler([](std::exception_ptr ep, std::string_view topic) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            std::cout << "[ERROR] " << topic << ": " << e.what() << "\n";
        }
    });

    auto bad = bus.subscribe("data", [](int value) {
        if (value < 0)
            throw std::runtime_error("negative value not allowed");
        std::cout << "Got: " << value << "\n";
    });

    auto safe = bus.subscribe("data", [](int value) {
        std::cout << "Also got: " << value << "\n";
    });

    bus.publish("data", 42);
    std::cout << "---\n";
    bus.publish("data", -1);
}
```

Output:
```
Got: 42
Also got: 42
---
[ERROR] data: negative value not allowed
Also got: -1
```

Key behaviors:
- If a subscriber throws, the error handler is called and dispatch continues to the next subscriber.
- If no error handler is set, subscriber exceptions are silently swallowed.
- If the error handler itself throws, that exception is silently swallowed.
- The same `set_error_handler()` method exists on `Broadcaster`, `Channel`, and `Blackboard`.

### Checking blackboard errors

`Blackboard::get<T>()` returns `std::expected<T, edict::Error>` instead of throwing:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Blackboard board;

    // Key doesn't exist
    auto result = board.get<int>("missing");
    if (!result)
        std::cout << "Error: " << edict::error_message(result.error()) << "\n";

    // Type mismatch
    board.set("name", std::string("Alice"));
    auto wrong = board.get<int>("name");
    if (!wrong)
        std::cout << "Error: " << edict::error_message(wrong.error()) << "\n";

    // Success
    auto right = board.get<std::string>("name");
    if (right)
        std::cout << "Name: " << *right << "\n";
}
```

Output:
```
Error: key not found in blackboard
Error: requested type does not match stored type
Name: Alice
```

Error codes:

| Error | Meaning |
|---|---|
| `Error::KeyNotFound` | The key does not exist in the blackboard |
| `Error::BadCast` | The stored type does not match the requested type |
| `Error::InvalidTopic` | The topic string is malformed |
