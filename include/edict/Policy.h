#pragma once

#include <concepts>
#include <mutex>
#include <shared_mutex>

namespace edict {

/// Zero-cost threading policy. All synchronization compiles away to nothing.
struct SingleThreaded {
    struct Mutex {
        constexpr void lock() noexcept {}
        constexpr void unlock() noexcept {}
        constexpr void lock_shared() noexcept {}
        constexpr void unlock_shared() noexcept {}
    };
    using SharedLock = std::shared_lock<Mutex>;
    using UniqueLock = std::unique_lock<Mutex>;
};

/// Thread-safe policy using std::shared_mutex (read-many, write-few).
struct MultiThreaded {
    using Mutex = std::shared_mutex;
    using SharedLock = std::shared_lock<std::shared_mutex>;
    using UniqueLock = std::unique_lock<std::shared_mutex>;
};

/// Concept for threading policies.
template <typename P>
concept ThreadingPolicy = requires {
    typename P::Mutex;
    typename P::SharedLock;
    typename P::UniqueLock;
};

} // namespace edict
