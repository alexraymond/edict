---
layout: default
title: Threading
nav_order: 6
---

# Threading
{: .no_toc }

Zero-cost single-threaded by default. One-word upgrade to thread-safe.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Threading policies

Edict uses policy-based design for threading. The policy is a template parameter on `Broadcaster` and `Blackboard`:

```cpp
edict::Broadcaster<>                          bus;   // SingleThreaded (default)
edict::Broadcaster<edict::SingleThreaded>     bus;   // same — explicit
edict::Broadcaster<edict::MultiThreaded>      bus;   // thread-safe
edict::SharedBroadcaster                      bus;   // alias for Broadcaster<MultiThreaded>
```

### SingleThreaded (default)

All locking compiles away to nothing. Zero overhead. The mutex type is an empty struct with no-op `lock()`/`unlock()`. Use this when:
- Your Broadcaster is accessed from a single thread only
- You're in a game loop or UI thread
- Performance matters and you control the threading model

### MultiThreaded

Uses `std::shared_mutex` internally. Subscribe/unsubscribe acquire exclusive locks. Publish acquires locks for matching, then dispatches callbacks outside the lock. Use this when:
- Multiple threads publish or subscribe
- You need concurrent publish from worker threads
- Components on different threads share a Broadcaster

## Concurrent usage

```cpp
edict::SharedBroadcaster bus;
std::atomic<int> total{0};

auto sub = bus.subscribe("work", [&](int value) {
    total.fetch_add(value, std::memory_order_relaxed);
});

// Safe: multiple threads publishing concurrently
std::vector<std::thread> workers;
for (int i = 0; i < 4; ++i) {
    workers.emplace_back([&, i] {
        for (int j = 0; j < 100; ++j)
            bus.publish("work", i + 1);
    });
}

for (auto& t : workers) t.join();
std::cout << "Total: " << total << "\n";  // deterministic: 1000
```

## Reentrancy safety

Publishing from within a subscriber callback is safe and will not deadlock, even with `MultiThreaded`:

```cpp
edict::SharedBroadcaster bus;

auto sub = bus.subscribe("ping", [&]() {
    bus.publish("pong");  // reentrant publish — safe!
});

auto pong = bus.subscribe("pong", []() {
    std::cout << "Pong!\n";
});

bus.publish("ping");  // prints: Pong!
```

Internally, Edict tracks the dispatching thread via `std::atomic<std::thread::id>`. Reentrant calls skip lock acquisition since the current thread already holds it. This applies to `publish()`, `dispatch()`, and `cancel()` within callbacks.

## Self-unsubscribe during dispatch

Cancelling your own subscription from within a callback is safe:

```cpp
edict::Subscription sub;
sub = bus.subscribe("oneshot", [&]() {
    std::cout << "Fired once!\n";
    sub.cancel();  // safe — removes self from future dispatches
});

bus.publish("oneshot");  // prints: Fired once!
bus.publish("oneshot");  // nothing — already cancelled
```

## Channel threading

`Channel<Args...>` does not have a threading policy — it's designed for single-threaded use. If you need thread-safe typed pub/sub, wrap the publish/subscribe calls in your own mutex, or use a `Broadcaster<MultiThreaded>` with typed wrappers.

## Lifetime safety

A `Subscription` can outlive its `Broadcaster` safely. The subscription's remover lambda captures a `weak_ptr` to the Broadcaster's internal state. If the Broadcaster is destroyed first, `cancel()` becomes a no-op:

```cpp
edict::Subscription sub;
{
    edict::SharedBroadcaster bus;
    sub = bus.subscribe("topic", handler);
}
// bus is destroyed — sub.cancel() is safe (no-op)
sub.cancel();  // does nothing, no crash
```
