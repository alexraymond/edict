#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace edict {

/**
 * Move-only RAII subscription handle. Automatically unsubscribes on
 * destruction. Call cancel() to unsubscribe early. Safe to outlive
 * the Channel/Broadcaster that created it (no-op on cancel).
 */
class Subscription {
public:
    using Id = std::uint64_t;
    // Remover must be noexcept-invocable. When std::move_only_function<void() noexcept>
    // is available, switch to it for compile-time enforcement.
    using Remover = std::function<void()>;

    Subscription() = default;

    explicit Subscription(Id id, Remover remover)
        : id_{id}, remover_{std::move(remover)} {}

    ~Subscription() { cancel(); }

    // Move-only
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : id_{std::exchange(other.id_, 0)}
        , remover_{std::exchange(other.remover_, nullptr)} {}

    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            cancel();
            id_ = std::exchange(other.id_, 0);
            remover_ = std::exchange(other.remover_, nullptr);
        }
        return *this;
    }

    void cancel() noexcept {
        if (auto fn = std::exchange(remover_, nullptr)) {
            try { fn(); } catch (...) {} // remover contract: must not throw
        }
        id_ = 0;
    }

    [[nodiscard]] bool active() const noexcept {
        return static_cast<bool>(remover_);
    }

    [[nodiscard]] Id id() const noexcept { return id_; }

private:
    Id id_{0};
    Remover remover_;
};

static_assert(!std::is_copy_constructible_v<Subscription>);
static_assert(!std::is_copy_assignable_v<Subscription>);
static_assert(std::is_move_constructible_v<Subscription>);
static_assert(std::is_move_assignable_v<Subscription>);


/**
 * Container for multiple Subscriptions. All are cancelled when the
 * group is destroyed or cancel_all() is called.
 */
class SubscriptionGroup {
public:
    SubscriptionGroup() = default;

    // Move-only
    SubscriptionGroup(const SubscriptionGroup&) = delete;
    SubscriptionGroup& operator=(const SubscriptionGroup&) = delete;
    SubscriptionGroup(SubscriptionGroup&&) noexcept = default;
    SubscriptionGroup& operator=(SubscriptionGroup&&) noexcept = default;

    /// Add a subscription. Takes by value (Meyers Item 41).
    SubscriptionGroup& operator+=(Subscription sub) {
        subs_.push_back(std::move(sub));
        return *this;
    }

    void cancel_all() { subs_.clear(); }

    [[nodiscard]] std::size_t size() const noexcept { return subs_.size(); }

private:
    std::vector<Subscription> subs_;
};

static_assert(!std::is_copy_constructible_v<SubscriptionGroup>);
static_assert(!std::is_copy_assignable_v<SubscriptionGroup>);

} // namespace edict
