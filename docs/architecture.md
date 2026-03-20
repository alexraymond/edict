---
layout: default
title: Architecture
nav_order: 4
---

# Architecture

This page describes Edict's internal design for contributors and anyone curious about the implementation.

---

## Layer diagram

```
                    ┌─────────────────────────────────┐
                    │         Global free functions     │  edict::publish(), edict::set(), etc.
                    │            (global.h)             │  Thin wrappers over static instances.
                    └──────────┬───────────┬───────────┘
                               │           │
                 ┌─────────────▼──┐   ┌────▼──────────────┐
                 │  Broadcaster   │   │    Blackboard      │  User-facing API.
                 │  <Policy>      │   │    <Policy>        │  Blackboard contains a Broadcaster.
                 └───────┬────────┘   └────────────────────┘
                         │
                 ┌───────▼────────┐
                 │  TopicRouter   │   Multiplexes exact, wildcard, and predicate matches.
                 └───────┬────────┘
                         │
                 ┌───────▼────────┐
                 │   TopicTree    │   Trie for * and ** wildcard matching.
                 └────────────────┘

  Separate:      ┌────────────────┐
                 │ Channel<Args…> │   Typed, zero-erasure. Independent of the above.
                 └────────────────┘
```

### What each layer does

- **TopicTree** -- A trie (prefix tree) keyed by `/`-separated segments. Handles `*` (single-segment wildcard) and `**` (multi-segment globstar, terminal only). Zero-allocation matching: walks the topic string segment by segment without pre-splitting into a vector.

- **TopicRouter** -- Combines three matching strategies: exact topic lookup (hash map), wildcard matching (TopicTree), and custom predicates (`vector<pair<function, Id>>`). Provides `match_static()` for lock-safe matches (exact + wildcard only, no user code invoked) and a separate `predicates_snapshot()` for lock-free predicate evaluation.

- **Broadcaster\<Policy\>** -- The main pub/sub engine. Type-erases published arguments into `std::vector<std::any>`. Manages subscription entries, message queuing, retention, and dispatch. Parameterized by threading policy.

- **Blackboard\<Policy\>** -- Typed key-value store (`unordered_map<string, any>`). Contains a `Broadcaster<Policy>` internally for change notifications. `set()` publishes `(optional<T>, T)` to the internal broadcaster.

- **Channel\<Args...\>** -- Fully typed, zero-erasure pub/sub. Completely independent of the Broadcaster/TopicRouter stack. Stores subscribers as `std::function<void(const Args&...)>`. Maintains sorted insertion order by priority.

- **Global API** -- Free functions in `global.h` that forward to static `SharedBroadcaster` and `SharedBlackboard` instances.

---

## Threading model

### Policy-based design

Threading is controlled by a template parameter satisfying the `ThreadingPolicy` concept:

```cpp
template <typename P>
concept ThreadingPolicy = requires {
    typename P::Mutex;
    typename P::SharedLock;
    typename P::UniqueLock;
};
```

Two policies are provided:

- **`SingleThreaded`** -- All lock operations are constexpr no-ops. Zero runtime cost. `shared_lock<Mutex>` and `unique_lock<Mutex>` compile to nothing.
- **`MultiThreaded`** -- Uses `std::shared_mutex` with `std::shared_lock` (reads) and `std::unique_lock` (writes).

### Lock-release-before-callbacks

The most important threading invariant: **the mutex is never held while invoking user callbacks**. This prevents deadlock when callbacks re-enter the Broadcaster (subscribe, publish, unsubscribe).

The pattern in `publish()`:

1. **Acquire shared lock** -- snapshot matching subscription entries + error handler.
2. **Release lock** -- before any callback invocation.
3. **Sort snapshot** by priority.
4. **Invoke callbacks** on the snapshot copy.

This means:
- Subscribers can safely call `publish()`, `subscribe()`, or `cancel()` from within a callback.
- Concurrent `subscribe()`/`cancel()` from other threads won't corrupt dispatch (it operates on a copy).
- Ordering across threads is not guaranteed for concurrent `set()` on the same Blackboard key.

### Predicate snapshot pattern

Custom predicates (`std::function<bool(string_view)>`) present a challenge: they are user code that could re-enter the Broadcaster or block. The solution:

1. **Under shared lock** -- collect exact + wildcard matches via `match_static()`. Also snapshot the predicates vector.
2. **Release lock.**
3. **Evaluate predicates** on the snapshot -- safe from data races AND safe if a predicate calls back into the Broadcaster.
4. For each matching predicate, **briefly re-acquire shared lock** to look up the subscription entry (it may have been removed concurrently -- that's correct behavior).

---

## Snapshot dispatch for reentrancy

Both `Broadcaster` and `Channel` copy the subscriber list before dispatching:

- **Broadcaster**: copies matching `SubscriptionEntry` structs into a `vector`.
- **Channel**: copies the entire `entries_` vector.

This means if a callback subscribes or unsubscribes, the current dispatch iteration is unaffected. The new subscription will only fire on subsequent publishes.

---

## Type erasure boundary

```
             Typed world                │              Erased world
                                        │
  Channel<int, float>                   │   Broadcaster
  ──────────────────                    │   ───────────
  subscribe(F handler)                  │   subscribe(topic, F handler)
    → std::function<void(int, float)>   │     → make_erased(handler)
                                        │       → std::function<void(vector<any>)>
  publish(42, 3.14f)                    │
    → direct call                       │   publish(topic, 42, 3.14f)
                                        │     → pack into vector<any>
                                        │     → unpack via any_cast in erased callable
```

**Broadcaster** pays for flexibility with type erasure:
- `publish()` packs arguments into `vector<any>` (one heap allocation for the vector, each `any` may allocate for large types).
- `make_erased()` wraps the user's handler in a lambda that unpacks via `any_cast`.
- If the subscriber accepts fewer args than published, only the needed prefix is unpacked.

**Channel** avoids all of this:
- Subscribers are stored as `std::function<void(const Args&...)>`.
- `publish()` calls each function directly with the arguments.
- Partial matching is resolved at subscribe time by wrapping in an adaptor lambda.

---

## Performance characteristics

| Operation | Broadcaster | Channel |
|---|---|---|
| Subscribe | O(1) amortized (hash insert) | O(N) (sorted insert) |
| Unsubscribe | O(1) (hash erase) | O(N) (linear scan + erase) |
| Publish (no subscribers) | O(1) fast path | O(1) early return |
| Publish (N subscribers) | O(N log N) sort + O(N) dispatch | O(N) dispatch (pre-sorted) |
| Wildcard match | O(D) trie walk, D = topic depth | N/A |
| Predicate match | O(P) where P = predicate count | N/A |
| Queue | O(1) amortized | N/A |
| Dispatch | O(M * N log N), M = queued messages | N/A |

### Memory

- **Broadcaster** state is held in a `shared_ptr<State>`, enabling move semantics. Subscription removers capture a `weak_ptr`, so they safely no-op if the Broadcaster is destroyed first.
- **Channel** is non-movable because removers capture `this`. It uses a `shared_ptr<bool>` sentinel for safe-after-destruction behavior.
- **Retention** stores `deque<vector<any>>` per topic, bounded by the configured count.

### Topic validation

Topics are validated on both subscribe and publish:
- No leading/trailing `/`
- No empty segments
- No `*` in publish topics
- Maximum depth of 64 segments (configurable via `detail::max_topic_depth`)
- Invalid subscribe topics throw `std::invalid_argument`
- Invalid publish topics silently no-op (return without dispatching)

### Transparent hashing

All internal hash maps use a custom `StringHash` with `is_transparent` and `std::equal_to<>`. This allows `find(string_view)` without allocating a `std::string`, eliminating unnecessary heap allocations on the lookup path.
