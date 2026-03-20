#pragma once

#include <edict/edict.h>

namespace edict {

namespace detail {
inline SharedBroadcaster& global_broadcaster() {
    static SharedBroadcaster instance;
    return instance;
}
inline SharedBlackboard& global_blackboard() {
    static SharedBlackboard instance;
    return instance;
}
} // namespace detail

/// Reset global state. NOT thread-safe — call only from single-threaded
/// test setup/teardown with no concurrent activity.
inline void reset() {
    detail::global_broadcaster() = SharedBroadcaster{};
    detail::global_blackboard() = SharedBlackboard{};
}

// ── Pub/sub free functions ──────────────────────────────────────────────────

template <typename F>
[[nodiscard]] Subscription subscribe(std::string_view topic, F&& handler,
                                      SubscribeOptions opts = {}) {
    return detail::global_broadcaster().subscribe(topic,
        std::forward<F>(handler), opts);
}

template <typename T, typename MF>
[[nodiscard]] Subscription subscribe(std::string_view topic, T* obj, MF method,
                                      SubscribeOptions opts = {}) {
    return detail::global_broadcaster().subscribe(topic, obj, method, opts);
}

template <typename... Args>
void publish(std::string_view topic, const Args&... args) {
    detail::global_broadcaster().publish(topic, args...);
}

template <typename... Args>
void queue(std::string_view topic, Args&&... args) {
    detail::global_broadcaster().queue(topic, std::forward<Args>(args)...);
}

inline void dispatch() { detail::global_broadcaster().dispatch(); }

// ── Blackboard free functions ───────────────────────────────────────────────

template <typename T>
void set(std::string_view key, T&& value) {
    detail::global_blackboard().set(key, std::forward<T>(value));
}

template <typename T>
[[nodiscard]] auto get(std::string_view key) {
    return detail::global_blackboard().template get<T>(key);
}

template <typename T, typename F>
[[nodiscard]] Subscription observe(std::string_view key, F&& handler,
                                    SubscribeOptions opts = {}) {
    return detail::global_blackboard().template observe<T>(key,
        std::forward<F>(handler), opts);
}

inline bool has(std::string_view key) { return detail::global_blackboard().has(key); }
inline void erase(std::string_view key) { detail::global_blackboard().erase(key); }

} // namespace edict
