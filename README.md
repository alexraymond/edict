# Edict

A header-only C++23 pub/sub and blackboard library. Zero dependencies. Maximum power, minimum ceremony.

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

## Features

- **Simple pub/sub** — subscribe to a topic, publish to it. One line each.
- **Partial argument matching** — zero-arg watchers, partial subscribers, full subscribers
- **Hierarchical wildcards** — `*` (single level) and `**` (multi-level) topic patterns
- **Custom predicates** — subscribe with arbitrary topic matching functions
- **RAII subscriptions** — automatic cleanup on scope exit, no manual unsubscribe
- **Subscription groups** — one object manages multiple subscription lifetimes
- **Priority ordering** — control which subscribers fire first
- **Queue/dispatch** — deferred message delivery for game loops
- **Filtered subscriptions** — conditional dispatch based on published data
- **Message retention/replay** — late subscribers receive recent history
- **Blackboard** — typed key-value state store with change observation
- **Exception isolation** — one subscriber throwing doesn't break others
- **Threading policies** — zero-cost single-threaded or `shared_mutex` multi-threaded
- **Header-only** — copy the headers and go. No build step, no dependencies.

## Installation

**Copy headers:**
```
cp -r include/edict /your/project/include/
```

**CMake FetchContent:**
```cmake
include(FetchContent)
FetchContent_Declare(edict GIT_REPOSITORY https://github.com/alexraymond/edict.git GIT_TAG master)
FetchContent_MakeAvailable(edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

**CMake add_subdirectory:**
```cmake
add_subdirectory(vendor/edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

## API

### Subscribe and Publish

```cpp
edict::Broadcaster<> bus;

// Any callable works — lambdas, free functions, member functions
auto sub = bus.subscribe("sensor/temp", [](double val) {
    std::cout << "Temperature: " << val << "\n";
});

bus.publish("sensor/temp", 23.5);
```

### Partial Argument Matching

Subscribers can take fewer args than published. Zero-arg subscribers are watchers — just notified.

```cpp
// publish sends (int, float, string)
// but subscribers can take any prefix:
auto full    = bus.subscribe("event", [](int a, float b, const std::string& c) { });
auto partial = bus.subscribe("event", [](int a) { });
auto watcher = bus.subscribe("event", []() { });

bus.publish("event", 42, 3.14f, std::string("hello"));  // all three fire
```

### Member Functions

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

### Wildcards

```cpp
// * matches one segment
auto s1 = bus.subscribe_pattern("sensor/*/temp", [](double v) { /* ... */ });

// ** matches zero or more segments (terminal only)
auto s2 = bus.subscribe_pattern("sensor/**", []() { /* any sensor event */ });

bus.publish("sensor/kitchen/temp", 22.0);  // triggers s1 and s2
bus.publish("sensor/garage/door", 1);      // triggers s2 only
```

### Predicates

```cpp
auto sub = bus.subscribe(
    [](std::string_view topic) { return topic.starts_with("alert/"); },
    []() { std::cout << "ALERT!\n"; });
```

### Filtered Subscriptions

```cpp
auto sub = bus.subscribe("damage",
    [](int amount) { apply_screen_shake(); },
    edict::filter([](int amount) { return amount > 50; }));
```

### Priority

```cpp
auto s1 = bus.subscribe("event", handler_a, {.priority = 10});  // fires first
auto s2 = bus.subscribe("event", handler_b, {.priority = 0});   // fires second
auto s3 = bus.subscribe("event", handler_c, {.priority = -5});  // fires last
```

### Queue / Dispatch

```cpp
bus.queue("damage", 42);
bus.queue("damage", 10);
bus.dispatch();  // both delivered now
```

### Message Retention

```cpp
bus.retain("sensor/temp", 5);  // keep last 5
bus.publish("sensor/temp", 20.0);
bus.publish("sensor/temp", 21.0);

// Late subscriber gets history immediately
auto sub = bus.subscribe("sensor/temp", handler, {.replay = true});
```

### Blackboard — Typed State Store

```cpp
#include <edict/Blackboard.h>

edict::Blackboard<> board;
board.set("player/health", 100);

auto hp = board.get<int>("player/health");  // std::expected<int, Error>

auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val, int new_val) {
        std::cout << "Health: " << new_val << "\n";
    });
```

### Threading

```cpp
edict::Broadcaster<> bus;              // single-threaded, zero overhead
edict::SharedBroadcaster bus;          // thread-safe with shared_mutex
```

### Global Convenience API

```cpp
#include <edict/global.h>

auto sub = edict::subscribe("topic", [](int v) { /* ... */ });
edict::publish("topic", 42);
```

## Advanced: Typed Channels

For performance-critical hot paths, `Channel<Args...>` provides zero-cost typed dispatch with no `std::any` overhead:

```cpp
#include <edict/Channel.h>

edict::Channel<int, float> sensor;
auto sub = sensor.subscribe([](int id, float val) { /* ... */ });
sensor.publish(1, 23.5f);  // direct typed call, no type erasure
```

## Examples

| Example | Demonstrates |
|---------|-------------|
| [hello-world](examples/hello-world/) | Simplest possible — subscribe, publish, unsubscribe |
| [game-events](examples/game-events/) | Components, SubscriptionGroup, partial arg matching |
| [broadcaster](examples/broadcaster/) | Wildcards, predicates, filters, queue/dispatch |
| [wildcards](examples/wildcards/) | Hierarchical `*` and `**` patterns |
| [blackboard](examples/blackboard/) | Typed state store with change observation |
| [threaded](examples/threaded/) | SharedBroadcaster with concurrent publishers |
| [queued-dispatch](examples/queued-dispatch/) | Game-loop style batched delivery |

## Requirements

- **C++23** (`-std=c++23`)
- GCC 14+, Clang 18+, or MSVC 17.6+
- CMake 3.25+ (for building tests/examples)

## License

MIT — see [LICENCE](LICENCE).
