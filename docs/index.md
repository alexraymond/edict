---
layout: default
title: Home
nav_order: 1
---

# Edict

A header-only C++23 pub/sub and blackboard library. Zero dependencies. Maximum power, minimum ceremony.
{: .fs-6 .fw-300 }

[Get Started](guide){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/alexraymond/edict){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## Quick Start

```cpp
#include <edict/Channel.h>

edict::Channel<int, std::string> events("damage");

auto sub = events.subscribe([](int amount, const std::string& type) {
    std::cout << amount << " " << type << " damage!\n";
});

events.publish(42, "fire");
```

## Two Layers, One API

**Typed Channels** — zero-cost, compile-time safe, no type erasure:

```cpp
edict::Channel<int, DamageType> damage;
auto sub = damage.subscribe([](int amount, DamageType type) { /* ... */ });
damage.publish(42, DamageType::Fire);
```

**String-Topic Broadcaster** — dynamic routing with wildcards:

```cpp
edict::Broadcaster<> bus;
auto sub = bus.subscribe("player/damage", [](int amount) { /* ... */ });
bus.publish("player/damage", 42);
```

Both support partial argument matching, priority ordering, RAII subscriptions, exception isolation, and threading policies.

## Features

| Feature | Description |
|:--------|:------------|
| **Typed Channels** | `Channel<Args...>` with zero type erasure on publish |
| **String Topics** | `Broadcaster<Policy>` with dynamic routing |
| **Partial Args** | Zero-arg watchers, partial subscribers, full handlers |
| **Wildcards** | `*` (one segment) and `**` (multi-level) patterns |
| **Predicates** | Custom topic matching functions |
| **RAII Subscriptions** | Automatic cleanup, no manual unsubscribe |
| **Subscription Groups** | One object manages multiple lifetimes |
| **Priority** | Control callback firing order |
| **Queue/Dispatch** | Deferred delivery for game loops |
| **Filters** | Conditional dispatch based on published data |
| **Retention/Replay** | Late subscribers receive recent history |
| **Blackboard** | Typed key-value state store with observation |
| **Threading** | Zero-cost single-threaded or shared_mutex multi-threaded |
| **Header-Only** | Copy and go. Zero dependencies. |

## Installation

Copy the headers:

```bash
cp -r include/edict /your/project/include/
```

Or use CMake FetchContent:

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
- GCC 14+, Clang 18+, or MSVC 17.6+
