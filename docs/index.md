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

## What is Edict?

Edict lets C++ components communicate without knowing about each other. A sensor publishes temperature readings. A display subscribes and shows them. Neither has a pointer to the other. Neither includes the other's header. They're connected only by a shared topic.

```cpp
#include <edict/Channel.h>
#include <iostream>

// A typed channel — the publisher and subscriber agree on (int, std::string)
edict::Channel<int, std::string> events;

// Subscribe: any callable that takes a prefix of the channel's types
auto sub = events.subscribe([](int amount, const std::string& type) {
    std::cout << amount << " " << type << " damage!\n";
});

events.publish(42, "fire");  // prints: 42 fire damage!
```

That's the entire API. Subscribe returns a handle. When the handle dies, you're unsubscribed. When you publish, every subscriber fires.

## Why Edict?

**Two paths, zero compromises:**

| | Typed Channel | String-Topic Broadcaster |
|:--|:--|:--|
| **Type safety** | Compile-time | Runtime (`std::any`) |
| **Overhead** | Zero — direct function call | Small — any_cast per arg |
| **Topics** | Implicit (one channel = one topic) | Dynamic strings with wildcards |
| **Best for** | Hot paths, game systems, known types | Config-driven routing, plugins |

Most libraries force you to choose. Edict gives you both, and they share the same subscription model.

**What you get:**

- **Partial argument matching** — a subscriber taking `(int)` works on a channel publishing `(int, float, string)`. A zero-arg `()` subscriber works on any channel — it's just notified.
- **RAII subscriptions** — no manual cleanup. Subscription dies? You're unsubscribed.
- **Priority ordering** — control which subscribers fire first.
- **Wildcards** — `sensor/*` matches `sensor/kitchen`, `sensor/**` matches everything beneath.
- **Filters** — skip the callback unless the data meets a condition.
- **Queue/dispatch** — batch messages and deliver them all at once (game-loop pattern).
- **Retention/replay** — new subscribers receive the last N messages on a topic.
- **Blackboard** — a typed key-value store with change observation.
- **Threading** — zero-cost single-threaded by default. One-word upgrade to thread-safe.
- **Zero dependencies** — pure C++23 standard library. Header-only. Copy and go.

## Installation

**Copy the headers** into your project:

```bash
cp -r include/edict your_project/include/
```

**Or use CMake** (any of these methods):

```cmake
# FetchContent (recommended)
include(FetchContent)
FetchContent_Declare(edict
    GIT_REPOSITORY https://github.com/alexraymond/edict.git
    GIT_TAG master)
FetchContent_MakeAvailable(edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

```cmake
# add_subdirectory (if vendored)
add_subdirectory(vendor/edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

## Requirements

- **C++23** — compile with `-std=c++23`
- **GCC 14+**, **Clang 18+** (with libc++), or **MSVC 17.6+**
- **CMake 3.25+** for building tests and examples
