---
layout: default
title: User Guide
nav_order: 3
---

# User Guide

This guide walks through every Edict feature, from the basics to advanced patterns. Each section builds on the previous one, with complete runnable examples.

---

## 1. Basic pub/sub with Broadcaster

`Broadcaster` is the core of Edict. Subscribe with a string topic and any callable. Publish sends data to all matching subscribers.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    auto sub = bus.subscribe("damage", [](std::string target, int amount) {
        std::cout << target << " took " << amount << " damage\n";
    });

    bus.publish("damage", std::string("Goblin"), 25);
    bus.publish("damage", std::string("Dragon"), 100);
}
```

Output:
```
Goblin took 25 damage
Dragon took 100 damage
```

Topics are `/`-separated strings like `"player/health"` or `"ui/button/click"`. No leading or trailing slashes. No empty segments.

---

## 2. Partial argument matching

Subscribers don't have to accept all published arguments. They can accept fewer (matched left-to-right) or none at all.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    // Full args: (string, int)
    auto full = bus.subscribe("score", [](std::string player, int points) {
        std::cout << player << " scored " << points << "\n";
    });

    // Partial: just the first arg
    auto partial = bus.subscribe("score", [](std::string player) {
        std::cout << player << " scored!\n";
    });

    // Zero-arg watcher: just knows something happened
    auto watcher = bus.subscribe("score", []() {
        std::cout << "A score event occurred\n";
    });

    bus.publish("score", std::string("Alice"), 42);
}
```

Output:
```
Alice scored 42
Alice scored!
A score event occurred
```

This works because Edict inspects the callable's signature at compile time and adapts the dispatch accordingly.

---

## 3. Member functions and SubscriptionGroup

Subscribe member functions directly without `std::bind`:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

class HUD {
public:
    void on_health(int hp) { std::cout << "HUD health: " << hp << "\n"; }
    void on_mana(int mp) { std::cout << "HUD mana: " << mp << "\n"; }
};

int main() {
    edict::Broadcaster bus;
    HUD hud;

    edict::SubscriptionGroup subs;
    subs += bus.subscribe("health", &hud, &HUD::on_health);
    subs += bus.subscribe("mana", &hud, &HUD::on_mana);

    bus.publish("health", 75);
    bus.publish("mana", 30);

    // subs destructor cancels all subscriptions
}
```

Output:
```
HUD health: 75
HUD mana: 30
```

{: .warning }
The object pointer must outlive the subscription. Edict does not take ownership of `&hud`.

`SubscriptionGroup` collects subscriptions and cancels them all when destroyed or when `cancel_all()` is called. This is the recommended pattern for class-based subscribers.

---

## 4. Wildcards: * and **

Use `subscribe_pattern()` for wildcard subscriptions.

- `*` matches exactly one segment
- `**` matches zero or more remaining segments (must be the last segment)

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    // * matches one segment
    auto star = bus.subscribe_pattern("player/*/damage", []() {
        std::cout << "Some player took damage\n";
    });

    // ** matches everything under player/
    auto globstar = bus.subscribe_pattern("player/**", []() {
        std::cout << "Something happened to a player\n";
    });

    bus.publish("player/1/damage", 50);
    std::cout << "---\n";
    bus.publish("player/2/heal", 20);
    std::cout << "---\n";
    bus.publish("player/3/inventory/drop", std::string("sword"));
}
```

Output:
```
Some player took damage
Something happened to a player
---
Something happened to a player
---
Something happened to a player
```

`"player/*/damage"` matches `"player/1/damage"` but not `"player/1/inventory/damage"`. `"player/**"` matches anything starting with `"player/"`.

{: .note }
Replay (`{.replay = true}`) is not supported for pattern subscriptions.

---

## 5. Custom predicates

For routing logic beyond wildcards, subscribe with any `bool(std::string_view)` callable:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <regex>
#include <string>

int main() {
    edict::Broadcaster bus;

    // Subscribe using a lambda predicate
    auto sub = bus.subscribe(
        [](std::string_view topic) { return topic.starts_with("debug"); },
        [](std::string msg) { std::cout << "[DEBUG] " << msg << "\n"; }
    );

    // Subscribe using a regex
    std::regex pattern(R"(error/\d+)");
    auto regex_sub = bus.subscribe(
        [&pattern](std::string_view topic) {
            return std::regex_match(topic.begin(), topic.end(), pattern);
        },
        [](std::string msg) { std::cout << "[ERROR] " << msg << "\n"; }
    );

    bus.publish("debug/physics", std::string("collision detected"));
    bus.publish("error/404", std::string("not found"));
    bus.publish("info/startup", std::string("ignored"));
}
```

Output:
```
[DEBUG] collision detected
[ERROR] not found
```

The predicate is evaluated for every published topic, so keep it fast. Predicates are evaluated outside the internal lock, so they are safe to use even with `SharedBroadcaster`.

{: .note }
Replay (`{.replay = true}`) is not supported for predicate subscriptions.

---

## 6. Filtered subscriptions

Filters inspect the published *data* (not the topic) and skip delivery when the predicate returns false. Use `edict::filter()` to wrap a predicate:

```cpp
#include <edict/edict.h>
#include <iostream>

int main() {
    edict::Broadcaster bus;

    // Only receive damage events where amount > 50
    auto sub = bus.subscribe(
        "damage",
        [](int amount) { std::cout << "Big hit: " << amount << "\n"; },
        edict::filter([](int amount) { return amount > 50; })
    );

    bus.publish("damage", 10);   // filtered out
    bus.publish("damage", 75);   // delivered
    bus.publish("damage", 200);  // delivered
}
```

Output:
```
Big hit: 75
Big hit: 200
```

Filters work with both exact subscriptions and pattern subscriptions:

```cpp
#include <edict/edict.h>
#include <iostream>

int main() {
    edict::Broadcaster bus;

    auto sub = bus.subscribe_pattern(
        "sensor/*",
        [](double value) { std::cout << "High reading: " << value << "\n"; },
        edict::filter([](double value) { return value > 100.0; })
    );

    bus.publish("sensor/temp", 50.0);   // filtered out
    bus.publish("sensor/temp", 150.0);  // delivered
    bus.publish("sensor/pressure", 200.0);  // delivered
}
```

Output:
```
High reading: 150
High reading: 200
```

The filter predicate uses the same partial argument matching as handlers -- it can inspect fewer arguments than published.

---

## 7. Priority ordering

Higher priority subscribers fire first. Default priority is 0.

```cpp
#include <edict/edict.h>
#include <iostream>

int main() {
    edict::Broadcaster bus;

    auto low = bus.subscribe("event", []() {
        std::cout << "Low priority (0)\n";
    });

    auto high = bus.subscribe("event", []() {
        std::cout << "High priority (10)\n";
    }, {.priority = 10});

    auto medium = bus.subscribe("event", []() {
        std::cout << "Medium priority (5)\n";
    }, {.priority = 5});

    bus.publish("event");
}
```

Output:
```
High priority (10)
Medium priority (5)
Low priority (0)
```

Use `SubscribeOptions` with designated initializers: `{.priority = 10}`.

---

## 8. Queue and dispatch (game loop pattern)

Instead of firing subscribers immediately, buffer messages with `queue()` and flush them with `dispatch()`:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Broadcaster bus;

    auto sub = bus.subscribe("event", [](std::string msg) {
        std::cout << "Processing: " << msg << "\n";
    });

    // Queue messages (no callbacks fire yet)
    bus.queue("event", std::string("physics_tick"));
    bus.queue("event", std::string("ai_update"));
    bus.queue("event", std::string("render"));

    std::cout << "Before dispatch\n";

    // Flush all queued messages
    bus.dispatch();

    std::cout << "After dispatch\n";
}
```

Output:
```
Before dispatch
Processing: physics_tick
Processing: ai_update
Processing: render
After dispatch
```

This is essential for game loops where you want deterministic ordering: gather events during the frame, then process them all at a well-defined point.

---

## 9. Message retention and replay

Retain messages on a topic so that late-joining subscribers can replay them:

```cpp
#include <edict/edict.h>
#include <iostream>

int main() {
    edict::Broadcaster bus;

    // Retain the last 3 messages on "log"
    bus.retain("log", 3);

    // Publish 4 messages (only last 3 are kept)
    bus.publish("log", 1);
    bus.publish("log", 2);
    bus.publish("log", 3);
    bus.publish("log", 4);

    // Late subscriber with replay enabled
    auto sub = bus.subscribe("log", [](int n) {
        std::cout << "Replayed: " << n << "\n";
    }, {.replay = true});

    // New messages still arrive normally
    bus.publish("log", 5);
}
```

Output:
```
Replayed: 2
Replayed: 3
Replayed: 4
Replayed: 5
```

The handler is the same for both replay and normal dispatch, so all five lines use the same format. The first three are replayed retained messages (message `1` was evicted because only 3 are kept). The last line is from the normal `publish()` call.

`retain(topic, count)` sets how many messages to keep. Pass 0 to disable retention and clear stored messages. Replay is triggered immediately when `subscribe()` is called with `{.replay = true}`.

{: .note }
Replay is only supported for exact topic subscriptions, not for pattern or predicate subscriptions.

---

## 10. Blackboard (typed state store)

`Blackboard` is a typed key-value store with change observers. Think of it as a shared data dictionary where any component can read/write values and get notified of changes.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <optional>
#include <string>

int main() {
    edict::Blackboard board;

    // Observe changes to "health" -- receives (old_value, new_value)
    auto sub = board.observe<int>("health", [](std::optional<int> old_val, int new_val) {
        if (old_val)
            std::cout << "Health: " << *old_val << " -> " << new_val << "\n";
        else
            std::cout << "Health set to " << new_val << "\n";
    });

    board.set("health", 100);
    board.set("health", 75);

    // Read values back
    auto hp = board.get<int>("health");
    if (hp)
        std::cout << "Current health: " << *hp << "\n";
}
```

Output:
```
Health set to 100
Health: 100 -> 75
Current health: 75
```

### Observer signatures

Observers use the same partial matching as Broadcaster:

```cpp
edict::Blackboard board;

// Full: old value (optional) and new value
auto full = board.observe<int>("score", [](std::optional<int> old_val, int new_val) {
    std::cout << "Full: " << new_val << "\n";
});

// Partial: just the old value
auto partial = board.observe<int>("score", [](std::optional<int> old_val) {
    std::cout << "Partial\n";
});

// Zero-arg: just notified that something changed
auto watcher = board.observe<int>("score", []() {
    std::cout << "Changed!\n";
});

board.set("score", 42);
```

Output:
```
Full: 42
Partial
Changed!
```

### Other operations

```cpp
board.has("health");     // true if key exists
board.erase("health");   // remove key, fires zero-arg observers
board.keys();            // returns std::vector<std::string> of all keys
```

`get()` returns `std::expected<T, edict::Error>`. On failure, the error is either `Error::KeyNotFound` or `Error::BadCast` (if the stored type doesn't match).

---

## 11. Threading (SharedBroadcaster / SharedBlackboard)

By default, `Broadcaster` and `Blackboard` use a zero-cost `SingleThreaded` policy. For multithreaded use, use the thread-safe aliases:

```cpp
#include <edict/edict.h>
#include <iostream>
#include <thread>

int main() {
    edict::SharedBroadcaster bus;  // = Broadcaster<MultiThreaded>

    auto sub = bus.subscribe("ping", [](int n) {
        std::cout << "Received: " << n << "\n";
    });

    std::thread sender([&bus]() {
        for (int i = 0; i < 3; ++i)
            bus.publish("ping", i);
    });

    sender.join();
}
```

Output (order may vary):
```
Received: 0
Received: 1
Received: 2
```

`SharedBroadcaster` uses `std::shared_mutex` internally. The lock is released before invoking callbacks, so subscribers can safely call back into the broadcaster (subscribe, publish, unsubscribe) without deadlock.

`SharedBlackboard` works the same way:

```cpp
edict::SharedBlackboard board;  // = Blackboard<MultiThreaded>
```

{: .warning }
With `MultiThreaded` policy, concurrent `set()` calls on the same Blackboard key may deliver observer notifications out of order. The lock is released before callbacks to prevent deadlock, but this means ordering is not guaranteed across threads.

---

## 12. Error handling

By default, if a subscriber throws, the exception is silently swallowed and remaining subscribers still fire. Set an error handler to catch these:

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
            std::cout << "Error on '" << topic << "': " << e.what() << "\n";
        }
    });

    auto sub = bus.subscribe("risky", []() {
        throw std::runtime_error("something went wrong");
    });

    auto safe = bus.subscribe("risky", []() {
        std::cout << "I still fire after the error\n";
    });

    bus.publish("risky");
}
```

Output:
```
Error on 'risky': something went wrong
I still fire after the error
```

The error handler receives the `std::exception_ptr` and the topic string. If the error handler itself throws, that exception is silently swallowed.

Blackboard uses the same mechanism:

```cpp
edict::Blackboard board;
board.set_error_handler([](std::exception_ptr ep, std::string_view key) {
    // handle observer errors
});
```

---

## 13. Global convenience API

For quick prototyping or simple programs, Edict provides free functions that use a global `SharedBroadcaster` and `SharedBlackboard`:

```cpp
#include <edict/global.h>
#include <iostream>
#include <string>

int main() {
    auto sub = edict::subscribe("chat", [](std::string msg) {
        std::cout << msg << "\n";
    });

    edict::publish("chat", std::string("hello from global API"));

    // Blackboard
    edict::set("score", 100);
    auto val = edict::get<int>("score");
    if (val)
        std::cout << "Score: " << *val << "\n";

    // Reset all global state (useful in tests)
    edict::reset();
}
```

Output:
```
hello from global API
Score: 100
```

Available global functions:

| Function | Description |
|---|---|
| `edict::subscribe(topic, handler, opts)` | Subscribe to a topic |
| `edict::subscribe(topic, obj, method, opts)` | Subscribe a member function |
| `edict::subscribe_pattern(pattern, handler, opts)` | Subscribe with wildcards |
| `edict::publish(topic, args...)` | Publish a message |
| `edict::queue(topic, args...)` | Queue a message |
| `edict::dispatch()` | Flush queued messages |
| `edict::set(key, value)` | Set a blackboard value |
| `edict::get<T>(key)` | Read a blackboard value |
| `edict::observe<T>(key, handler, opts)` | Observe blackboard changes |
| `edict::has(key)` | Check if a blackboard key exists |
| `edict::erase(key)` | Remove a blackboard key |
| `edict::reset()` | Reset all global state |

{: .warning }
`edict::reset()` is NOT thread-safe. Call it only from single-threaded test setup/teardown with no concurrent activity.

---

## 14. Typed channels (Channel\<Args...\>)

For hot paths where you know the message type at compile time, `Channel<Args...>` provides zero type-erasure dispatch. No `std::any`, no packing/unpacking.

```cpp
#include <edict/edict.h>
#include <iostream>
#include <string>

int main() {
    edict::Channel<std::string, int> damage_channel("damage");

    auto sub = damage_channel.subscribe([](std::string target, int amount) {
        std::cout << target << " took " << amount << " damage\n";
    });

    // Zero-arg watcher works here too
    auto counter = 0;
    auto count_sub = damage_channel.subscribe([&counter]() {
        ++counter;
    });

    damage_channel.publish("Goblin", 25);
    damage_channel.publish("Dragon", 100);

    std::cout << "Total hits: " << counter << "\n";
}
```

Output:
```
Goblin took 25 damage
Dragon took 100 damage
Total hits: 2
```

### Channel vs Broadcaster

| | Broadcaster | Channel\<Args...\> |
|---|---|---|
| Topic routing | String matching, wildcards, predicates | None (single channel) |
| Type safety | Runtime (`std::any_cast`) | Compile-time |
| Overhead | Packs args into `vector<any>` | Direct `std::function` call |
| Threading | `SingleThreaded` or `MultiThreaded` policy | Single-threaded only |
| Queue/dispatch | Yes | No |
| Retention/replay | Yes | No |
| Use case | Flexible event routing | Hot-path, known-type events |

Channel supports partial argument matching, member function binding, priority ordering, and error handlers -- just like Broadcaster.

```cpp
edict::Channel<double, double, double> position_channel;

position_channel.subscribe([](double x, double y, double z) {
    // full args
});

position_channel.subscribe([](double x, double y) {
    // partial: just x and y
});

position_channel.subscribe([]() {
    // zero-arg: just notified
});
```

{: .warning }
`Channel` is non-movable because subscription removers capture `this`. Declare it as a class member or local variable -- don't try to move it.

### Introspection

```cpp
damage_channel.subscriber_count();  // number of active subscribers
damage_channel.has_subscribers();   // true if any subscribers exist
damage_channel.topic();             // the topic string passed to constructor
```
