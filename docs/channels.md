---
layout: default
title: Typed Channels
nav_order: 3
---

# Typed Channels
{: .no_toc }

Zero-cost, compile-time safe pub/sub.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

`Channel<Args...>` is Edict's primary interface. It's a typed callback list — when you publish `(int, float)`, every subscriber receives `(int, float)`. No strings. No type erasure. No runtime overhead beyond the function calls themselves.

```cpp
edict::Channel<int, float> sensor;
```

This declares a channel that publishes `int` and `float` values. The compiler enforces that:
- `publish()` receives exactly `(int, float)`
- `subscribe()` accepts any callable that takes a prefix of `(int, float)`

## Partial argument matching

Subscribers don't need to accept all published arguments. They can take fewer — the extras are silently discarded:

```cpp
edict::Channel<int, float, std::string> telemetry;

// Full args — receives everything
auto s1 = telemetry.subscribe([](int id, float val, const std::string& unit) {
    std::cout << "Sensor " << id << ": " << val << " " << unit << "\n";
});

// Partial — takes first two, ignores the string
auto s2 = telemetry.subscribe([](int id, float val) {
    log_reading(id, val);
});

// Partial — takes just the first
auto s3 = telemetry.subscribe([](int id) {
    mark_active(id);
});

// Zero-arg watcher — just wants to know something happened
auto s4 = telemetry.subscribe([]() {
    ++event_count;
});

telemetry.publish(42, 23.5f, "celsius");  // all four fire
```

This is checked at compile time. If your callable needs *more* arguments than published, it won't compile:

```cpp
// Won't compile: channel publishes (int, float), handler needs 3 args
auto bad = telemetry.subscribe([](int, float, std::string, double) { });
```

## Member functions

Bind an object and its member function directly — no `std::bind` or lambda wrapper needed:

```cpp
class Logger {
public:
    void on_reading(int id, float val) {
        std::cout << "[Logger] " << id << ": " << val << "\n";
    }
};

Logger logger;
auto sub = sensor.subscribe(&logger, &Logger::on_reading);
```

{: .warning }
> The object must outlive the subscription. Edict stores a raw pointer — if the object is destroyed while the subscription is active, you'll get a dangling pointer. Use `SubscriptionGroup` tied to the object's lifetime (see below).

## Priority

Control the order callbacks fire with `SubscribeOptions`:

```cpp
auto s1 = ch.subscribe(handler_a, {.priority = 10});  // fires first
auto s2 = ch.subscribe(handler_b, {.priority = 0});   // fires second (default)
auto s3 = ch.subscribe(handler_c, {.priority = -5});   // fires last
```

Same-priority subscribers fire in subscription order. Priorities are stored sorted at subscribe time — no per-publish sort.

## Subscription groups

When a class subscribes to multiple channels, use `SubscriptionGroup` to manage all lifetimes at once:

```cpp
class Player {
    edict::SubscriptionGroup subs;
    int hp = 100;
public:
    Player(edict::Channel<int, DamageType>& damage,
           edict::Channel<int>& heal,
           edict::Channel<>& death) {
        subs += damage.subscribe([this](int amount, DamageType type) {
            hp -= amount;
        });
        subs += heal.subscribe([this](int amount) {
            hp = std::min(hp + amount, 100);
        });
        subs += death.subscribe([this]() {
            std::cout << "Game over\n";
        });
    }
    // ~Player: all three subscriptions cancelled automatically
};
```

`SubscriptionGroup::operator+=` takes a `Subscription` by value (moved in). `cancel_all()` clears them all early.

## Exception isolation

If a subscriber throws, the exception is caught and remaining subscribers still fire:

```cpp
auto s1 = ch.subscribe([]() { throw std::runtime_error("oops"); });
auto s2 = ch.subscribe([]() { std::cout << "I still run!\n"; });

ch.publish();  // s1 throws, s2 still fires
```

## Introspection

```cpp
ch.subscriber_count();  // number of active subscribers
ch.has_subscribers();    // true if at least one
ch.topic();              // the topic string (if provided at construction)
```

## When to use Channel vs Broadcaster

| Use Channel when... | Use Broadcaster when... |
|:---|:---|
| You know the message types at compile time | Topics are dynamic (config files, user input) |
| You want zero overhead | You need wildcard matching |
| Components can share a `Channel&` | Components only share string topic names |
| This is a hot path (thousands of publishes/sec) | This is a setup/config path |
