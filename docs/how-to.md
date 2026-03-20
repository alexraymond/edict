---
layout: default
title: How-To Recipes
nav_order: 6
---

# How-To Recipes
{: .no_toc }

Common patterns and solutions.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Decouple game components

**Problem:** Your Player, Enemy, and UI classes need to communicate but shouldn't include each other's headers.

**Solution:** Use typed channels as the shared contract.

```cpp
// events.h — shared by all components, includes nothing else
#pragma once
#include <edict/Channel.h>
#include <string>

inline edict::Channel<int, std::string> on_damage;
inline edict::Channel<int>              on_heal;
inline edict::Channel<std::string>      on_chat;
```

```cpp
// player.cpp
#include "events.h"

class Player {
    edict::SubscriptionGroup subs;
    int hp = 100;
public:
    Player() {
        subs += on_damage.subscribe([this](int amount, const std::string& source) {
            hp -= amount;
            if (hp <= 0) on_chat.publish("Player defeated by " + source);
        });
        subs += on_heal.subscribe([this](int amount) {
            hp = std::min(hp + amount, 100);
        });
    }
};
```

```cpp
// enemy.cpp — knows nothing about Player
#include "events.h"

void attack() {
    on_damage.publish(25, "Goblin");
}
```

```cpp
// ui.cpp — knows nothing about Player or Enemy
#include "events.h"

class UI {
    edict::SubscriptionGroup subs;
public:
    UI() {
        subs += on_chat.subscribe([](const std::string& msg) {
            render_chat_bubble(msg);
        });
        subs += on_damage.subscribe([]() {  // zero-arg watcher
            flash_screen_red();
        });
    }
};
```

## Build a game loop with queued dispatch

**Problem:** You want events to be processed at a specific point in the frame, not immediately when published.

**Solution:** Use `queue()` during the frame and `dispatch()` at the end.

```cpp
edict::Broadcaster<> bus;

// Systems subscribe during initialization
auto physics = bus.subscribe("collision", [](int a, int b) { resolve(a, b); });
auto audio   = bus.subscribe("collision", []() { play_sound("bonk.wav"); });

// Game loop
while (running) {
    // --- Simulation phase: detect events, queue them ---
    for (auto& pair : detect_collisions())
        bus.queue("collision", pair.first, pair.second);

    for (auto& spawn : pending_spawns)
        bus.queue("entity/spawn", spawn.id);

    // --- Dispatch phase: all events fire here, in order ---
    bus.dispatch();

    // --- Render phase: state is consistent ---
    render();
}
```

## Use the Blackboard for AI decision-making

**Problem:** Your AI system needs to read the current world state without subscribing to every event.

**Solution:** Use the Blackboard as a shared world model. Systems write state, AI reads it.

```cpp
edict::Blackboard<> world;

// Health system writes current health
world.set("player/health", 100);
world.set("player/position", Vec3{10, 0, 5});
world.set("enemy/count", 3);

// AI reads the blackboard to make decisions
void ai_think() {
    auto hp = world.get<int>("player/health");
    auto enemies = world.get<int>("enemy/count");

    if (hp && enemies) {
        if (*hp < 20 && *enemies > 2)
            flee();
        else if (*hp > 80)
            attack();
        else
            defend();
    }
}

// Optional: react to specific changes
auto low_health_alert = world.observe<int>("player/health",
    [](std::optional<int>, int new_hp) {
        if (new_hp < 20)
            std::cout << "Warning: low health!\n";
    });
```

## Handle late-joining subscribers

**Problem:** A UI panel opens after data has already been published. It needs to show the current state.

**Solution:** Use message retention and replay.

```cpp
edict::Broadcaster<> bus;

// Configure retention: keep last 10 temperature readings
bus.retain("sensor/temperature", 10);

// Readings come in over time...
bus.publish("sensor/temperature", 20.0);
bus.publish("sensor/temperature", 21.5);
bus.publish("sensor/temperature", 22.0);

// Later, a chart UI opens and needs the history:
auto chart = bus.subscribe("sensor/temperature",
    [](double val) { add_to_chart(val); },
    {.replay = true});
// Immediately receives: 20.0, 21.5, 22.0
```

## Filter noisy data

**Problem:** You're subscribed to a high-frequency topic but only care about certain values.

**Solution:** Use filtered subscriptions. The filter runs before the handler — if it returns false, the handler is skipped entirely.

```cpp
// Only process temperature readings above 30 degrees
auto alarm = bus.subscribe("sensor/temperature",
    [](double val) {
        std::cout << "OVERHEATING: " << val << "\n";
        trigger_alarm();
    },
    edict::filter([](double val) { return val > 30.0; }));

bus.publish("sensor/temperature", 22.0);  // filtered out
bus.publish("sensor/temperature", 35.0);  // alarm fires
bus.publish("sensor/temperature", 28.0);  // filtered out
bus.publish("sensor/temperature", 41.0);  // alarm fires
```

## Build a logging/auditing system

**Problem:** You want to log all messages on certain topics without modifying the publishers.

**Solution:** Use wildcard subscriptions with high priority.

```cpp
// Log everything under "game/**" — fires before any other subscriber
auto audit_log = bus.subscribe_pattern("game/**", []() {
    // Zero-arg watcher — receives no data, just knows something happened
    log_event("game activity at " + timestamp());
}, {.priority = 1000});

// Log specific data for damage events
auto damage_log = bus.subscribe("game/damage", [](int amount) {
    log_event("damage: " + std::to_string(amount));
}, {.priority = 999});
```

## Use the global API for prototyping

**Problem:** You want the simplest possible setup — no object creation, no passing references.

**Solution:** Include `edict/global.h` for free functions over a default global instance.

```cpp
#include <edict/global.h>

// Anywhere in your code:
auto sub = edict::subscribe("events/click", [](int x, int y) {
    std::cout << "Click at " << x << ", " << y << "\n";
});

// Anywhere else:
edict::publish("events/click", 100, 200);

// Blackboard too:
edict::set("score", 0);
auto score = edict::get<int>("score");

// For tests — reset global state:
edict::reset();
```

{: .note }
> The global API uses `SharedBroadcaster` (thread-safe) internally. For maximum performance in single-threaded code, create your own `Broadcaster<>` or `Channel<Args...>` instance instead.

## Handle errors gracefully

**Problem:** A subscriber might throw, and you want to log it without crashing.

**Solution:** Install an error handler on the Broadcaster.

```cpp
bus.set_error_handler([](std::exception_ptr ep, std::string_view topic) {
    try {
        std::rethrow_exception(ep);
    } catch (const std::exception& e) {
        std::cerr << "[" << topic << "] subscriber error: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[" << topic << "] unknown subscriber error\n";
    }
});
```

Without a handler, exceptions are silently swallowed. Either way, remaining subscribers always fire — one bad subscriber never blocks others.
