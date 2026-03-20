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

### Threading

```cpp
// Zero-cost single-threaded (default)
edict::Broadcaster<> bus;

// Thread-safe with shared_mutex
edict::Broadcaster<edict::MultiThreaded> bus;
// or: edict::SharedBroadcaster bus;
```

## Examples

| Example | Demonstrates |
|---------|-------------|
| [hello-world](examples/hello-world/) | Simplest possible — typed channel, subscribe, publish |
| [game-events](examples/game-events/) | Typed channels, SubscriptionGroup, partial arg matching |
| [broadcaster](examples/broadcaster/) | String topics, wildcards, predicates, queue/dispatch |
| [wildcards](examples/wildcards/) | Hierarchical topics with `*` and `**` patterns |

## Requirements

- **C++23** (`-std=c++23`)
- GCC 14+, Clang 18+, or MSVC 17.6+
- CMake 3.25+ (for building tests/examples)

## License

MIT — see [LICENCE](LICENCE).
