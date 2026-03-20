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

You only need one header for most use cases:

```cpp
#include <edict/Channel.h>    // typed pub/sub
// or
#include <edict/Broadcaster.h> // string-topic pub/sub
// or
#include <edict/edict.h>       // everything
```

## Step 2: Create a channel

A `Channel<Args...>` declares what type of data flows through it:

```cpp
edict::Channel<std::string> greet;
```

This channel carries `std::string` messages. Publishers send strings, subscribers receive strings.

## Step 3: Subscribe

```cpp
auto sub = greet.subscribe([](const std::string& name) {
    std::cout << "Hello, " << name << "!\n";
});
```

`subscribe()` returns a `Subscription` — a move-only RAII handle. **You must store it.** If you don't, the subscription is immediately destroyed and the handler never fires.

{: .warning }
> `subscribe()` is `[[nodiscard]]`. Ignoring the return value unsubscribes immediately.
> ```cpp
> greet.subscribe(handler);  // BUG: subscription dies instantly
> auto sub = greet.subscribe(handler);  // correct
> ```

## Step 4: Publish

```cpp
greet.publish("World");   // prints: Hello, World!
greet.publish("Edict");   // prints: Hello, Edict!
```

Every subscriber fires synchronously, in priority order, inside the `publish()` call.

## Step 5: Unsubscribe

Just let the subscription go out of scope:

```cpp
{
    auto sub = greet.subscribe(handler);
    greet.publish("yes");   // handler fires
}
greet.publish("no");        // handler does NOT fire — sub is dead
```

Or cancel explicitly:

```cpp
sub.cancel();
```

---

## Complete example

```cpp
#include <edict/Channel.h>
#include <iostream>
#include <string>

void on_greet(const std::string& name) {
    std::cout << "Hello, " << name << "!\n";
}

int main() {
    edict::Channel<std::string> greet;

    // Free function
    auto s1 = greet.subscribe(on_greet);

    // Lambda
    auto s2 = greet.subscribe([](const std::string& name) {
        std::cout << "  (whispers) hey " << name << "...\n";
    });

    // Zero-arg watcher — doesn't need the data, just wants notification
    auto s3 = greet.subscribe([]() {
        std::cout << "  [someone was greeted]\n";
    });

    greet.publish("World");
    // Hello, World!
    //   (whispers) hey World...
    //   [someone was greeted]

    s2.cancel();  // unsubscribe the whisperer

    greet.publish("Again");
    // Hello, Again!
    //   [someone was greeted]
}
```

## What's next?

- [Typed Channels](channels) — the zero-cost path
- [Broadcaster](broadcaster) — string topics with wildcards
- [Blackboard](blackboard) — typed state store
- [Threading](threading) — multi-threaded usage
- [How-To Recipes](how-to) — common patterns
