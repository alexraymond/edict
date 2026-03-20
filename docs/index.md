---
layout: default
title: Home
nav_order: 1
---

# Edict

A header-only C++23 pub/sub and blackboard library. Zero dependencies. Maximum power, minimum ceremony.
{: .fs-6 .fw-300 }

[Get Started](getting-started){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/alexraymond/edict){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## Quick Start

```cpp
#include <edict/Broadcaster.h>

edict::Broadcaster<> bus;

auto sub = bus.subscribe("player/damage", [](int amount) {
    std::cout << "Took " << amount << " damage!\n";
});

bus.publish("player/damage", 42);
```

Subscribe. Publish. That's it.

## What can it do?

```cpp
// Any callable works — lambdas, free functions, member functions
auto s1 = bus.subscribe("sensor/temp", [](double val) { log(val); });

// Partial args — don't need all the data? Take less.
auto s2 = bus.subscribe("sensor/temp", []() { ++event_count; });

// Wildcards — match topic patterns
auto s3 = bus.subscribe_pattern("sensor/**", []() { /* any sensor */ });

// Predicates — custom matching logic
auto s4 = bus.subscribe(
    [](std::string_view t) { return t.starts_with("alert/"); },
    []() { sound_alarm(); });

// Filters — only fire when data meets a condition
auto s5 = bus.subscribe("sensor/temp",
    [](double v) { warn("hot!"); },
    edict::filter([](double v) { return v > 30.0; }));

// Priority — control firing order
auto s6 = bus.subscribe("event", handler, {.priority = 10});

// Queue for later
bus.queue("sensor/temp", 22.5);
bus.dispatch();  // fire all queued messages
```

When a `Subscription` goes out of scope, it automatically unsubscribes. No cleanup code needed. For classes with multiple subscriptions, use `SubscriptionGroup`:

```cpp
class Player {
    edict::SubscriptionGroup subs;
public:
    Player(edict::Broadcaster<>& bus) {
        subs += bus.subscribe("damage", this, &Player::on_damage);
        subs += bus.subscribe("heal",   this, &Player::on_heal);
    }
    void on_damage(int amount) { hp -= amount; }
    void on_heal(int amount)   { hp += amount; }
    // ~Player: all subscriptions cancelled automatically
};
```

## Beyond Pub/Sub

**Blackboard** — a typed key-value store where you can read the current state at any time, not just when events fire:

```cpp
edict::Blackboard<> board;
board.set("player/health", 100);

auto hp = board.get<int>("player/health");  // returns std::expected<int, Error>

auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val, int new_val) {
        if (new_val < 20) warn("low health!");
    });
```

**Threading** — zero-cost single-threaded by default. One word to go thread-safe:

```cpp
edict::SharedBroadcaster bus;  // thread-safe, reentrant-safe
```

## Features

| Feature | Description |
|:--------|:------------|
| **Simple pub/sub** | Subscribe to a topic, publish to it. One line each. |
| **Partial args** | Zero-arg watchers, partial subscribers, full handlers |
| **Wildcards** | `*` (one segment) and `**` (multi-level) patterns |
| **Predicates** | Custom topic matching functions |
| **RAII subscriptions** | Automatic cleanup, no manual unsubscribe |
| **Subscription groups** | One object manages multiple lifetimes |
| **Priority** | Control callback firing order |
| **Queue/dispatch** | Deferred delivery for game loops |
| **Filters** | Conditional dispatch based on published data |
| **Retention/replay** | Late subscribers receive recent history |
| **Blackboard** | Typed key-value state store with observation |
| **Threading** | Zero-cost single-threaded or shared_mutex multi-threaded |
| **Header-only** | Copy and go. Zero dependencies. |

## Installation

Copy the headers:

```bash
cp -r include/edict your_project/include/
```

Or use CMake:

```cmake
include(FetchContent)
FetchContent_Declare(edict
    GIT_REPOSITORY https://github.com/alexraymond/edict.git
    GIT_TAG master)
FetchContent_MakeAvailable(edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

## Requirements

- C++23 (`-std=c++23`)
- GCC 14+, Clang 18+ (with libc++), or MSVC 17.6+
