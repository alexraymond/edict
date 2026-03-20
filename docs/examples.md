---
layout: default
title: Examples
nav_order: 3
---

# Examples

All examples are in the [`examples/`](https://github.com/alexraymond/edict/tree/master/examples) directory. Build with:

```bash
cmake -S . -B build -DEDICT_BUILD_EXAMPLES=ON
cmake --build build
```

---

## hello-world

The simplest possible usage — a typed channel with one subscriber.

```cpp
#include <edict/Channel.h>
#include <iostream>
#include <string>

int main() {
    edict::Channel<std::string> greet("greet");

    auto sub = greet.subscribe([](const std::string& name) {
        std::cout << "Hello, " << name << "!\n";
    });

    greet.publish("World");
    greet.publish("Edict");
}
```

Output:
```
Hello, World!
Hello, Edict!
```

---

## game-events

Typed channels with `SubscriptionGroup`, partial arg matching, and member functions.

```cpp
edict::Channel<int, DamageType> damage("player/damage");
edict::Channel<int> heal("player/heal");
edict::Channel<> death("player/death");

class Player {
    edict::SubscriptionGroup subs;
    int hp = 100;
public:
    Player() {
        subs += damage.subscribe([this](int amount, DamageType) {
            hp -= amount;
        });
        subs += heal.subscribe([this](int amount) { hp += amount; });
        subs += death.subscribe([this]() { /* handle death */ });
    }
};
```

---

## broadcaster

String-topic Broadcaster with wildcards, predicates, and queue/dispatch.

```cpp
edict::Broadcaster<> bus;

auto s1 = bus.subscribe("sensor/temperature", [](double val) { /* ... */ });
auto s2 = bus.subscribe_pattern("sensor/*", []() { /* any sensor */ });
auto s3 = bus.subscribe(
    [](std::string_view t) { return t.starts_with("alert/"); },
    []() { /* alert handler */ });

bus.publish("sensor/temperature", 23.5);
bus.queue("sensor/temperature", 24.1);
bus.dispatch();
```

---

## blackboard

Typed state store with change observation.

```cpp
edict::Blackboard<> board;

auto obs = board.observe<int>("player/health",
    [](std::optional<int> old_val, int new_val) {
        // old_val is nullopt on first set
    });

board.set("player/health", 100);
auto hp = board.get<int>("player/health"); // std::expected<int, Error>
```

---

## threaded

`SharedBroadcaster` with concurrent publishers from multiple threads.

```cpp
edict::SharedBroadcaster bus;
std::atomic<int> total{0};

auto sub = bus.subscribe("work", [&](int amount) {
    total.fetch_add(amount, std::memory_order_relaxed);
});

std::vector<std::thread> workers;
for (int i = 0; i < 4; ++i)
    workers.emplace_back([&, i] {
        for (int j = 0; j < 100; ++j) bus.publish("work", i + 1);
    });
```

---

## queued-dispatch

Game-loop style batched delivery.

```cpp
edict::Broadcaster<> bus;
auto sub = bus.subscribe("tick", [](int frame) { /* ... */ });

for (int frame = 1; frame <= 60; ++frame) {
    bus.queue("tick", frame);
    bus.dispatch(); // fire at end of frame
}
```
