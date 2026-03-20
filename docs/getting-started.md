---
layout: default
title: Getting Started
nav_order: 2
---

# Getting Started
{: .no_toc }

Build your first Edict program in under 5 minutes.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Step 1: Include the header

```cpp
#include <edict/Broadcaster.h>
```

That's the only header you need for pub/sub. (`edict/edict.h` includes everything if you prefer.)

## Step 2: Create a Broadcaster

```cpp
edict::Broadcaster<> bus;
```

A Broadcaster routes messages by topic string. The `<>` means single-threaded (zero overhead). For multi-threaded use, write `edict::SharedBroadcaster`.

## Step 3: Subscribe

```cpp
auto sub = bus.subscribe("greet", [](const std::string& name) {
    std::cout << "Hello, " << name << "!\n";
});
```

`subscribe()` returns a `Subscription` — a move-only RAII handle. **You must store it.** If you don't, the subscription is immediately destroyed and the handler never fires.

{: .warning }
> `subscribe()` is `[[nodiscard]]`. Ignoring the return value unsubscribes immediately.
> ```cpp
> bus.subscribe("greet", handler);  // BUG: subscription dies instantly
> auto sub = bus.subscribe("greet", handler);  // correct
> ```

## Step 4: Publish

```cpp
bus.publish("greet", std::string("World"));   // prints: Hello, World!
bus.publish("greet", std::string("Edict"));   // prints: Hello, Edict!
```

Every subscriber fires synchronously, in priority order, inside the `publish()` call.

## Step 5: Unsubscribe

Just let the subscription go out of scope:

```cpp
{
    auto sub = bus.subscribe("greet", handler);
    bus.publish("greet", std::string("yes"));   // handler fires
}
bus.publish("greet", std::string("no"));        // handler does NOT fire — sub is dead
```

Or cancel explicitly:

```cpp
sub.cancel();
```

---

## Complete example

```cpp
#include <edict/Broadcaster.h>
#include <iostream>
#include <string>

void on_greet(const std::string& name) {
    std::cout << "Hello, " << name << "!\n";
}

int main() {
    edict::Broadcaster<> bus;

    // Free function
    auto s1 = bus.subscribe("greet", on_greet);

    // Lambda
    auto s2 = bus.subscribe("greet", [](const std::string& name) {
        std::cout << "  (whispers) hey " << name << "...\n";
    });

    // Zero-arg watcher — doesn't need the data, just wants notification
    auto s3 = bus.subscribe("greet", []() {
        std::cout << "  [someone was greeted]\n";
    });

    bus.publish("greet", std::string("World"));
    // Hello, World!
    //   (whispers) hey World...
    //   [someone was greeted]

    s2.cancel();  // unsubscribe the whisperer

    bus.publish("greet", std::string("Again"));
    // Hello, Again!
    //   [someone was greeted]
}
```

## What's next?

- [Broadcaster](broadcaster) — wildcards, predicates, filters, queue/dispatch
- [Blackboard](blackboard) — typed state store
- [Threading](threading) — multi-threaded usage
- [How-To Recipes](how-to) — common patterns
- [Typed Channels](channels) — advanced: zero-cost typed dispatch for hot paths
