---
layout: default
title: Home
nav_order: 1
---

# Edict

**Header-only C++23 pub/sub and blackboard library.**
Maximum power, minimum ceremony.

```cpp
#include <edict/edict.h>

edict::Broadcaster bus;
auto sub = bus.subscribe("chat", [](std::string msg) { std::cout << msg << "\n"; });
bus.publish("chat", std::string("hello"));
```

Output:
```
hello
```

That's it. Three lines to wire up a fully type-safe publish/subscribe system.

## Why Edict?

| Feature | What it means |
|---|---|
| **Header-only** | Drop the `include/` folder into your project. Done. |
| **Zero dependencies** | Pure C++23 standard library. No Boost, no frameworks. |
| **String topics** | `"player/damage"`, `"ui/button/click"` -- human-readable routing. |
| **Wildcard matching** | `"player/*"` and `"player/**"` for broad subscriptions. |
| **Custom predicates** | Subscribe with regex, lambdas, or any `bool(string_view)` callable. |
| **Partial argument matching** | Subscribe with fewer args than published. Zero-arg watchers just work. |
| **Filtered subscriptions** | Receive only messages where a data predicate returns true. |
| **Priority ordering** | Control which subscribers fire first. |
| **Queue/dispatch** | Buffer messages, flush them in your game loop. |
| **Message retention** | Late-joining subscribers can replay missed messages. |
| **Typed channels** | `Channel<int, float>` for zero-overhead hot paths. |
| **Blackboard** | Typed key-value store with change observers. |
| **Thread-safe variants** | `SharedBroadcaster` and `SharedBlackboard` for multithreaded code. |
| **RAII subscriptions** | `[[nodiscard]]` handles that auto-unsubscribe on destruction. |
| **Global convenience API** | Free functions for quick prototyping. |

## Installation

### Option 1: Copy the headers

Copy the `include/edict/` directory into your project's include path.

### Option 2: CMake subdirectory

```cmake
add_subdirectory(edict)
target_link_libraries(your_target PRIVATE edict::edict)
```

### Option 3: CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    edict
    GIT_REPOSITORY https://github.com/alexraymond/edict.git
    GIT_TAG master
)
FetchContent_MakeAvailable(edict)
target_link_libraries(your_target PRIVATE edict::edict)
```

### Requirements

- C++23 compiler (GCC 13+, Clang 17+, MSVC 19.37+)
- No external dependencies

## Quick links

- [Getting Started](getting-started) -- your first program
- [User Guide](guide) -- the full tutorial
- [Architecture](architecture) -- how it works inside
- [How-To Recipes](how-to) -- copy-paste solutions for common patterns
