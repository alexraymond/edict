---
layout: default
title: User Guide
nav_order: 2
---

# User Guide
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Typed Channels

`Channel<Args...>` is the primary, zero-cost path. No type erasure. Compile-time type safety.

```cpp
#include <edict/Channel.h>

edict::Channel<int, float> sensor;

// Full args
auto s1 = sensor.subscribe([](int id, float temp) {
    std::cout << "Sensor " << id << ": " << temp << "\n";
});

// Partial — just the first arg
auto s2 = sensor.subscribe([](int id) {
    std::cout << "Activity on sensor " << id << "\n";
});

// Zero-arg watcher — just notified
auto s3 = sensor.subscribe([]() {
    std::cout << "Something happened\n";
});

sensor.publish(1, 23.5f);  // all three fire
```

### Member Functions

```cpp
class Player {
    edict::SubscriptionGroup subs;
    int hp = 100;
public:
    Player(edict::Channel<int>& damage) {
        subs += damage.subscribe(this, &Player::on_damage);
    }
    void on_damage(int amount) { hp -= amount; }
};
```

### Priority

Higher priority fires first. Default is 0.

```cpp
auto s1 = ch.subscribe(handler_a, {.priority = 10});  // fires first
auto s2 = ch.subscribe(handler_b, {.priority = 0});   // fires second
auto s3 = ch.subscribe(handler_c, {.priority = -5});  // fires last
```

---

## String-Topic Broadcaster

For dynamic topics determined at runtime.

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
// * matches exactly one segment
auto s1 = bus.subscribe_pattern("sensor/*/temp", [](double v) { /* ... */ });

// ** matches zero or more segments (terminal only)
auto s2 = bus.subscribe_pattern("sensor/**", []() { /* any sensor event */ });

bus.publish("sensor/kitchen/temp", 22.0);  // triggers s1 and s2
bus.publish("sensor/garage/door", 1);      // triggers s2 only
```

### Custom Predicates

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

bus.publish("damage", 10);   // filter rejects
bus.publish("damage", 100);  // filter passes — handler called
```

### Queue / Dispatch

```cpp
bus.queue("tick", frame_number);
bus.queue("tick", frame_number);

// Nothing delivered yet — dispatch at end of frame
bus.dispatch();
```

### Message Retention

```cpp
bus.retain("sensor/temp", 5);  // keep last 5 messages

bus.publish("sensor/temp", 20.0);
bus.publish("sensor/temp", 21.0);

// Late subscriber gets history immediately
auto sub = bus.subscribe("sensor/temp",
    [](double t) { std::cout << t << "\n"; },
    {.replay = true});
```

---

## Blackboard

Typed key-value state store with change observation.

```cpp
#include <edict/Blackboard.h>

edict::Blackboard<> board;

// Store values
board.set("player/health", 100);
board.set("player/name", std::string("Hero"));

// Read values
auto hp = board.get<int>("player/health");  // std::expected<int, Error>
if (hp) std::cout << "HP: " << *hp << "\n";

// Observe changes
auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val, int new_val) {
        std::cout << "Health changed to " << new_val << "\n";
    });

board.set("player/health", 75);  // observer fires
```

Observer signatures support partial arg matching:

| Signature | Receives |
|:----------|:---------|
| `(std::optional<T>, T)` | Old value (nullopt on first set) and new value |
| `(std::optional<T>)` | Old value only |
| `()` | Just notified |

---

## Subscriptions

All `subscribe` and `observe` methods return a move-only `Subscription`.

### RAII Cleanup

```cpp
{
    auto sub = ch.subscribe(handler);
    ch.publish(42);  // handler called
}
// sub destroyed — automatically unsubscribed
ch.publish(42);  // handler NOT called
```

### Manual Cancel

```cpp
auto sub = ch.subscribe(handler);
sub.cancel();  // unsubscribe early
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
    // ~Entity: all three subscriptions cancelled automatically
};
```

---

## Threading

```cpp
// Zero-cost single-threaded (default)
edict::Broadcaster<> bus;
edict::Channel<int> ch;

// Thread-safe
edict::SharedBroadcaster bus;            // = Broadcaster<MultiThreaded>
edict::Broadcaster<edict::MultiThreaded> bus2;
```

Reentrant-safe: publishing from within a subscriber callback is allowed and does not deadlock.

---

## Global Convenience API

Opt-in via a separate header:

```cpp
#include <edict/global.h>

auto sub = edict::subscribe("topic", [](int v) { /* ... */ });
edict::publish("topic", 42);

edict::set("key", 100);
auto val = edict::get<int>("key");

// Reset between tests
edict::reset();
```

---

## Error Handling

The library never throws. Subscriber exceptions are caught and isolated — remaining subscribers still fire.

```cpp
bus.set_error_handler([](std::exception_ptr ep, std::string_view topic) {
    try { std::rethrow_exception(ep); }
    catch (const std::exception& e) {
        std::cerr << "Error on " << topic << ": " << e.what() << "\n";
    }
});
```

`Blackboard::get<T>()` returns `std::expected<T, edict::Error>`:

```cpp
auto val = board.get<int>("missing_key");
if (!val) {
    // val.error() == edict::Error::KeyNotFound
}
```
