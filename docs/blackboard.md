---
layout: default
title: Blackboard
nav_order: 5
---

# Blackboard
{: .no_toc }

Typed key-value state store with change observation.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

A Blackboard stores typed values by string key. When a value changes, observers are notified with the old and new values. Unlike the Broadcaster (which fires events that are immediately forgotten), the Blackboard *remembers* — any component can read the current state at any time.

```cpp
#include <edict/Blackboard.h>

edict::Blackboard<> board;

board.set("player/health", 100);

auto hp = board.get<int>("player/health");
if (hp) std::cout << "Health: " << *hp << "\n";  // Health: 100
```

## Setting values

`set()` stores a value and notifies observers:

```cpp
board.set("player/health", 100);       // int
board.set("player/name", std::string("Hero"));  // string
board.set("player/pos", Vec3{1, 2, 3});          // any copyable type
```

Each key can hold any type. Calling `set()` with a different type than previously stored replaces the value.

## Reading values

`get<T>()` returns `std::expected<T, edict::Error>`:

```cpp
auto hp = board.get<int>("player/health");

if (hp) {
    std::cout << "Health: " << *hp << "\n";
} else if (hp.error() == edict::Error::KeyNotFound) {
    std::cout << "No health value set\n";
} else if (hp.error() == edict::Error::BadCast) {
    std::cout << "Health is not stored as int\n";
}
```

{: .important }
> `get<T>()` returns a **copy** of the value. For large types, cache the result rather than calling `get()` repeatedly.

## Observing changes

`observe<T>()` subscribes to changes on a key. The `T` parameter tells Edict what type you expect — it should match what you pass to `set()`.

### Full signature — old and new value

```cpp
auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val, int new_val) {
        if (!old_val)
            std::cout << "Health initialized to " << new_val << "\n";
        else
            std::cout << "Health: " << *old_val << " -> " << new_val << "\n";
    });
```

On the first `set()`, `old_val` is `std::nullopt`. On subsequent sets, it holds the previous value.

### Single arg — just the new value

```cpp
auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val) {
        // Receives only the first published arg (the old value)
    });
```

### Zero args — just notified

```cpp
auto obs = board.observe<int>("player/health", []() {
    std::cout << "Health changed!\n";
});
```

### RAII lifetime

Like all Edict subscriptions, `observe()` returns a `Subscription` that unsubscribes on destruction:

```cpp
{
    auto obs = board.observe<int>("key", handler);
    board.set("key", 1);  // handler fires
}
board.set("key", 2);      // handler does NOT fire
```

## Other operations

```cpp
board.has("player/health");  // true if key exists
board.erase("player/health"); // remove key
board.keys();                  // vector<string> of all stored keys
```

## When to use Blackboard vs Broadcaster

| Blackboard | Broadcaster |
|:---|:---|
| "What is the current health?" | "Player just took 42 damage" |
| State that persists between reads | Events that fire and are forgotten |
| Late joiners need current state | Late joiners missed it |
| AI reads world state | Systems react to events |

The two work well together. Use the Broadcaster for events ("damage happened") and the Blackboard for the resulting state ("current health is 58").
