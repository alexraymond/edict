#pragma once

#include <edict/Broadcaster.h>
#include <edict/Error.h>
#include <edict/Policy.h>
#include <edict/Subscription.h>
#include <edict/detail/Traits.h>

#include <algorithm>
#include <any>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace edict {

/// Typed key-value state store with change observation.
/// Uses an internal Broadcaster for change notifications.
/// Observers support partial arg matching:
///   (std::optional<T>, T) — old and new value
///   (T)                   — new value only
///   ()                    — just notified
template <typename Policy = SingleThreaded>
    requires ThreadingPolicy<Policy>
class Blackboard {
public:
    Blackboard() : state_(std::make_shared<State>()) {}

    Blackboard(const Blackboard&) = delete;
    Blackboard& operator=(const Blackboard&) = delete;
    Blackboard(Blackboard&&) = default;
    Blackboard& operator=(Blackboard&&) = default;

    /// Store a value under a key. Fires observers with (old, new). Lock released before callbacks.
    template <typename T>
    void set(std::string_view key, T&& value) {
        using V = std::decay_t<T>;
        std::optional<V> old_val;
        V new_copy(std::forward<T>(value));
        auto key_str = std::string(key);

        {
            typename Policy::UniqueLock lock(state_->mutex);
            auto it = state_->store.find(key_str);
            if (it != state_->store.end()) {
                try {
                    old_val = std::any_cast<V>(it->second);
                } catch (const std::bad_any_cast&) {
                    // Type changed: old value treated as absent (nullopt).
                    // Existing observers of the old type will receive bad_any_cast
                    // during dispatch, routed to the error handler if set.
                }
                it->second = new_copy;
            } else {
                state_->store.emplace(key_str, new_copy);
            }
        }
        // Lock released before notifying observers (prevents deadlock)

        state_->broadcaster.publish(key_str, old_val, new_copy);
    }

    /// Read a value. Returns std::expected<T, Error>.
    template <typename T>
    [[nodiscard]] std::expected<T, Error> get(std::string_view key) const {
        typename Policy::SharedLock lock(state_->mutex);
        auto it = state_->store.find(std::string(key));
        if (it == state_->store.end()) {
            return std::unexpected(Error::KeyNotFound);
        }
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::unexpected(Error::BadCast);
        }
    }

    /// Observe changes to a key. T must match the type passed to set().
    /// Handler can take (optional<T>, T), (optional<T>), or () via partial matching.
    template <typename T, typename F>
        requires detail::Subscribable<F, std::optional<T>, T>
    [[nodiscard]] Subscription observe(std::string_view key, F&& handler,
                                        SubscribeOptions opts = {}) {
        return state_->broadcaster.subscribe(std::string(key),
            std::forward<F>(handler), opts);
    }

    /// Check if a key exists.
    [[nodiscard]] bool has(std::string_view key) const {
        typename Policy::SharedLock lock(state_->mutex);
        return state_->store.contains(std::string(key));
    }

    /// Remove a key. Does NOT fire observers (erase has no knowledge of the stored type).
    void erase(std::string_view key) {
        typename Policy::UniqueLock lock(state_->mutex);
        state_->store.erase(std::string(key));
    }

    /// Set callback for observer exceptions (propagated to internal broadcaster).
    void set_error_handler(ErrorHandler handler) {
        state_->broadcaster.set_error_handler(std::move(handler));
    }

    /// List all keys.
    [[nodiscard]] std::vector<std::string> keys() const {
        typename Policy::SharedLock lock(state_->mutex);
        std::vector<std::string> result;
        result.reserve(state_->store.size());
        for (const auto& [k, _] : state_->store) {
            result.push_back(k);
        }
        return result;
    }

private:
    struct State {
        mutable typename Policy::Mutex mutex;
        std::unordered_map<std::string, std::any, detail::StringHash, std::equal_to<>> store;
        Broadcaster<Policy> broadcaster;
    };

    std::shared_ptr<State> state_;
};

using SharedBlackboard = Blackboard<MultiThreaded>;

} // namespace edict
