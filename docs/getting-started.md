---
layout: default
title: Getting Started
nav_order: 2
---

# Getting Started

This page walks you through your first Edict program: subscribe, publish, and unsubscribe.

## Your first program

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    // Subscribe to "greet" -- receives a std::string
    auto sub = bus.subscribe("greet", [](std::string name) {
        std::cout << "Hello, " << name << "!\n";
    });

    // Publish a message
    bus.publish("greet", std::string("World"));
    bus.publish("greet", std::string("Edict"));

    // sub goes out of scope here and automatically unsubscribes
    return 0;
}
```

Output:
```
Hello, World!
Hello, Edict!
```

## What just happened?

1. **`bus.subscribe("greet", ...)`** registers a lambda that listens on the `"greet"` topic. It returns a `Subscription` handle.
2. **`bus.publish("greet", std::string("World"))`** sends a message to all subscribers on `"greet"`. The lambda fires immediately.
3. When `sub` is destroyed at the end of `main()`, it automatically unsubscribes. No manual cleanup needed.

## The [[nodiscard]] warning

`subscribe()` is marked `[[nodiscard]]`. If you ignore the return value, the subscription is destroyed immediately and your handler never fires.

{: .warning }
This is the most common mistake with Edict. Always store the returned `Subscription`.

```cpp
// WRONG: subscription immediately destroyed, handler never called
bus.subscribe("topic", []() { std::cout << "never prints\n"; });

// RIGHT: subscription kept alive
auto sub = bus.subscribe("topic", []() { std::cout << "prints!\n"; });
```

## Early unsubscribe

Call `cancel()` to unsubscribe before the handle is destroyed:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    auto sub = bus.subscribe("ping", [](std::string msg) {
        std::cout << "Got: " << msg << "\n";
    });

    bus.publish("ping", std::string("first"));

    sub.cancel();  // unsubscribe now

    bus.publish("ping", std::string("second"));  // no output

    return 0;
}
```

Output:
```
Got: first
```

## Multiple subscribers

Multiple subscribers on the same topic all receive every message:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    auto logger = bus.subscribe("event", [](std::string msg) {
        std::cout << "[LOG] " << msg << "\n";
    });

    auto counter_value = 0;
    auto counter = bus.subscribe("event", [&counter_value]() {
        ++counter_value;
    });

    bus.publish("event", std::string("player_joined"));
    bus.publish("event", std::string("player_left"));

    std::cout << "Total events: " << counter_value << "\n";

    return 0;
}
```

Output:
```
[LOG] player_joined
[LOG] player_left
Total events: 2
```

Notice the counter lambda takes zero arguments even though the publisher sends a `std::string`. This is partial argument matching -- subscribers can accept fewer arguments than published.

## Managing multiple subscriptions

Use `SubscriptionGroup` to manage several subscriptions together:

```cpp
#include <edict/edict.h>
#include <iostream>

int main() {
    edict::Broadcaster bus;

    edict::SubscriptionGroup group;
    group += bus.subscribe("health", [](int hp) {
        std::cout << "HP: " << hp << "\n";
    });
    group += bus.subscribe("mana", [](int mp) {
        std::cout << "MP: " << mp << "\n";
    });

    bus.publish("health", 100);
    bus.publish("mana", 50);

    group.cancel_all();  // unsubscribe everything at once

    bus.publish("health", 0);  // no output
    bus.publish("mana", 0);    // no output

    return 0;
}
```

Output:
```
HP: 100
MP: 50
```

## Next steps

The [User Guide](guide) covers every feature in depth: wildcards, priorities, queuing, blackboards, threading, and more.
