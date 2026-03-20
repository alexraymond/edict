---
layout: default
title: Broadcaster
nav_order: 3
---

# Broadcaster
{: .no_toc }

String-topic pub/sub with wildcards, predicates, and filters.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

`Broadcaster<Policy>` is Edict's dynamic-topic interface. Topics are strings. Matching supports exact, wildcard, and predicate-based routing. Arguments are type-erased via `std::any` internally.

```cpp
#include <edict/Broadcaster.h>

edict::Broadcaster<> bus;

auto sub = bus.subscribe("sensor/temperature", [](double val) {
    std::cout << "Temperature: " << val << "\n";
});

bus.publish("sensor/temperature", 23.5);
```

The default `Broadcaster<>` is single-threaded (zero locking overhead). For multi-threaded use, see [Threading](threading).

## Topic syntax

Topics are `/`-separated hierarchical paths:

```
sensor/kitchen/temperature
player/damage/fire
alert/system/battery
```

Rules:
- No leading or trailing `/`
- No empty segments (`sensor//temp` is invalid)
- Wildcard characters (`*`, `**`) are only valid in subscription patterns, not publish topics

## Wildcard patterns

### `*` — matches exactly one segment

```cpp
auto sub = bus.subscribe_pattern("sensor/*/temperature", [](double val) {
    std::cout << "Room temp: " << val << "\n";
});

bus.publish("sensor/kitchen/temperature", 22.0);   // matches
bus.publish("sensor/bedroom/temperature", 19.5);   // matches
bus.publish("sensor/temperature", 22.0);            // does NOT match (no middle segment)
bus.publish("sensor/a/b/temperature", 22.0);        // does NOT match (* is one segment only)
```

### `**` — matches zero or more segments (terminal only)

```cpp
auto sub = bus.subscribe_pattern("sensor/**", []() {
    std::cout << "Sensor event\n";
});

bus.publish("sensor");                         // matches (zero segments after sensor)
bus.publish("sensor/temperature");             // matches (one segment)
bus.publish("sensor/kitchen/temperature");     // matches (two segments)
bus.publish("sensor/a/b/c/d");                 // matches (four segments)
bus.publish("alert/fire");                     // does NOT match (different root)
```

`**` must be the last segment in the pattern. `sensor/**/temperature` is invalid.

### Combining exact, wildcard, and predicate

All matching types work together. A single publish can trigger exact-match, wildcard, and predicate subscribers:

```cpp
bus.subscribe("sensor/kitchen/temp", handler_a);                      // exact
bus.subscribe_pattern("sensor/*/temp", handler_b);                     // wildcard
bus.subscribe_pattern("sensor/**", handler_c);                         // globstar
bus.subscribe([](std::string_view t) { return t.size() < 30; }, handler_d); // predicate

bus.publish("sensor/kitchen/temp", 22.0);  // all four fire
```

## Custom predicates

Subscribe with an arbitrary function that receives the topic string:

```cpp
auto sub = bus.subscribe(
    [](std::string_view topic) { return topic.starts_with("alert/"); },
    [](const std::string& msg) {
        std::cout << "ALERT: " << msg << "\n";
    });

bus.publish("alert/fire", std::string("Smoke detected"));   // fires
bus.publish("log/info", std::string("All clear"));           // does not fire
```

## Filtered subscriptions

Filters are predicates on the *published data* (not the topic). The subscriber only fires when the filter returns true:

```cpp
auto sub = bus.subscribe("sensor/temperature",
    [](double val) {
        std::cout << "HIGH TEMP: " << val << "\n";
    },
    edict::filter([](double val) { return val > 30.0; }));

bus.publish("sensor/temperature", 22.0);   // filter rejects — handler NOT called
bus.publish("sensor/temperature", 35.0);   // filter passes — prints HIGH TEMP: 35
```

Filters support partial argument matching too — a filter taking `(int)` works on a topic publishing `(int, string)`.

## Queue and dispatch

For game-loop or frame-based architectures, queue messages during the frame and dispatch them all at once:

```cpp
// During the frame — queue events
bus.queue("game/score", 100);
bus.queue("game/kill", std::string("Goblin"));
bus.queue("game/damage", 15);

// End of frame — deliver all at once
bus.dispatch();
```

Messages are delivered in FIFO order. Messages queued *during* dispatch go to the next `dispatch()` call.

## Message retention and replay

Store the last N messages on a topic. New subscribers with `{.replay = true}` receive the retained history immediately:

```cpp
bus.retain("sensor/temperature", 5);  // keep last 5

bus.publish("sensor/temperature", 20.0);
bus.publish("sensor/temperature", 21.0);
bus.publish("sensor/temperature", 22.0);

// This subscriber gets all three immediately upon subscribing:
auto late = bus.subscribe("sensor/temperature",
    [](double t) { std::cout << t << "\n"; },
    {.replay = true});
// prints: 20, 21, 22
```

Pass `retain("topic", 0)` to disable retention and clear history.

## Error handling

Install a custom error handler for subscriber exceptions:

```cpp
bus.set_error_handler([](std::exception_ptr ep, std::string_view topic) {
    try { std::rethrow_exception(ep); }
    catch (const std::exception& e) {
        std::cerr << "[" << topic << "] error: " << e.what() << "\n";
    }
});
```

Without a handler, exceptions are silently caught and swallowed. Remaining subscribers always fire regardless.

## Introspection

```cpp
bus.subscriber_count("sensor/temp");   // exact + wildcard + predicate matches
bus.has_subscribers("sensor/temp");     // true if any would fire
bus.active_topics();                    // list of exact-match topics (not wildcards)
```

## Member functions

```cpp
struct Handler {
    void on_event(int value) { /* ... */ }
};

Handler h;
auto sub = bus.subscribe("events/click", &h, &Handler::on_event);
```
