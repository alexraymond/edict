# Edict

A header-only C++23 pub/sub and blackboard library. Zero dependencies. Maximum power, minimum ceremony.

## Quick Start

```cpp
#include <edict/Channel.h>

edict::Channel<int, std::string> events("damage");

auto sub = events.subscribe([](int amount, const std::string& type) {
    std::cout << amount << " " << type << " damage!\n";
});

events.publish(42, "fire");
```

## Features

- **Typed channels** — `Channel<Args...>` with zero type erasure on the publish path
- **String-topic broadcaster** — `Broadcaster<Policy>` for dynamic topic routing
- **Partial argument matching** — zero-arg watchers, partial subscribers, full subscribers
- **Hierarchical wildcards** — `*` (single level) and `**` (multi-level) topic patterns
- **Custom predicates** — subscribe with arbitrary topic matching functions
- **RAII subscriptions** — automatic cleanup on scope exit, no manual unsubscribe
- **Subscription groups** — one object manages multiple subscription lifetimes
- **Priority ordering** — control which subscribers fire first
- **Queue/dispatch** — deferred message delivery for game loops
- **Exception isolation** — one subscriber throwing doesn't break others
- **Filtered subscriptions** — conditional dispatch based on published data
- **Message retention/replay** — late subscribers receive recent history
- **Blackboard** — typed key-value state store with change observation
- **Threading policies** — zero-cost single-threaded or `shared_mutex` multi-threaded
- **Header-only** — copy the headers and go. No build step, no dependencies.

## Installation

**Copy headers:**
```
cp -r include/edict /your/project/include/
```

**CMake add_subdirectory:**
```cmake
add_subdirectory(vendor/edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

**CMake FetchContent:**
```cmake
include(FetchContent)
FetchContent_Declare(edict GIT_REPOSITORY https://github.com/alexraymond/edict.git GIT_TAG v1.0.0)
FetchContent_MakeAvailable(edict)
target_link_libraries(myapp PRIVATE edict::edict)
```

## API

### Typed Channels

The primary, zero-cost path. No type erasure. Compile-time type safety.

```cpp
#include <edict/Channel.h>

edict::Channel<int, float> sensor;

// Full args
auto s1 = sensor.subscribe([](int id, float temp) { /* ... */ });

// Partial — just the first arg
auto s2 = sensor.subscribe([](int id) { /* ... */ });

// Zero-arg watcher — just notified
auto s3 = sensor.subscribe([]() { /* ... */ });

sensor.publish(1, 23.5f);  // all three fire
```

### Member Functions

```cpp
class Player {
    edict::SubscriptionGroup subs;
public:
    Player(edict::Channel<int>& damage) {
        subs += damage.subscribe(this, &Player::on_damage);
    }
    void on_damage(int amount) { hp -= amount; }
};
```

### String-Topic Broadcaster

For dynamic topics determined at runtime. Uses `std::any` internally.

```cpp
#include <edict/Broadcaster.h>

edict::Broadcaster<> bus;

auto sub = bus.subscribe("player/damage", [](int amount) {
    std::cout << "Took " << amount << " damage\n";
});

bus.publish("player/damage", 42);
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

### Priority

Higher priority fires first. Default is 0.

```cpp
auto s1 = ch.subscribe(handler_a, {.priority = 10});  // fires first
auto s2 = ch.subscribe(handler_b, {.priority = 0});   // fires second
auto s3 = ch.subscribe(handler_c, {.priority = -5});   // fires last
```

### Queue / Dispatch

```cpp
bus.queue("damage", 42);
bus.queue("damage", 10);
// Nothing delivered yet

bus.dispatch();  // both messages delivered now
```

### RAII Subscriptions

```cpp
{
    auto sub = ch.subscribe(handler);
    ch.publish(42);  // handler called
}
// sub destroyed — automatically unsubscribed

ch.publish(42);  // handler NOT called
```

### Subscription Groups

```cpp
class Entity {
    edict::SubscriptionGroup subs;
public:
    Entity(edict::Broadcaster<>& bus) {
        subs += bus.subscribe("damage", [this](int a) { /* ... */ });
        subs += bus.subscribe("heal",   [this](int a) { /* ... */ });
        subs += bus.subscribe("death",  [this]()      { /* ... */ });
    }
    // ~Entity: all three subscriptions automatically cancelled
};
```

### Filtered Subscriptions

```cpp
#include <edict/Error.h>  // for edict::filter

auto sub = bus.subscribe("damage",
    [](int amount) { apply_screen_shake(); },
    edict::filter([](int amount) { return amount > 50; }));

bus.publish("damage", 10);   // filter rejects — handler NOT called
bus.publish("damage", 100);  // filter passes — handler called
```

### Message Retention / Replay

```cpp
bus.retain("sensor/temp", 5);  // keep last 5 messages

bus.publish("sensor/temp", 20.0);
bus.publish("sensor/temp", 21.0);
bus.publish("sensor/temp", 22.0);

// Late subscriber receives retained history immediately
auto sub = bus.subscribe("sensor/temp",
    [](double t) { std::cout << t << "\n"; },
    {.replay = true});
// Prints: 20, 21, 22
```

### Blackboard — Typed State Store

```cpp
#include <edict/Blackboard.h>

edict::Blackboard<> board;

board.set("player/health", 100);

auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val, int new_val) {
        std::cout << "Health: " << new_val << "\n";
    });

board.set("player/health", 75);  // observer fires

auto hp = board.get<int>("player/health");  // returns std::expected<int, Error>
```

### Threading

```cpp
// Zero-cost single-threaded (default)
edict::Broadcaster<> bus;

// Thread-safe with shared_mutex
edict::SharedBroadcaster bus;
```

### Global Convenience API (opt-in)

```cpp
#include <edict/global.h>

auto sub = edict::subscribe("topic", [](int v) { /* ... */ });
edict::publish("topic", 42);

edict::set("key", 100);
auto val = edict::get<int>("key");
```

## Examples

| Example | Demonstrates |
|---------|-------------|
| [hello-world](examples/hello-world/) | Simplest possible — typed channel, subscribe, publish |
| [game-events](examples/game-events/) | Typed channels, SubscriptionGroup, partial arg matching |
| [broadcaster](examples/broadcaster/) | String topics, wildcards, predicates, queue/dispatch |
| [wildcards](examples/wildcards/) | Hierarchical topics with `*` and `**` patterns |
| [blackboard](examples/blackboard/) | Typed state store with change observation |
| [threaded](examples/threaded/) | SharedBroadcaster with concurrent publishers |
| [queued-dispatch](examples/queued-dispatch/) | Game-loop style batched delivery |

## Requirements

- **C++23** (`-std=c++23`)
- GCC 14+, Clang 18+, or MSVC 17.6+
- CMake 3.25+ (for building tests/examples)

## License

MIT — see [LICENCE](LICENCE).
