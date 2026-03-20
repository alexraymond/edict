#pragma once

#include <edict/Error.h>
#include <edict/Policy.h>
#include <edict/Subscription.h>
#include <edict/TopicRouter.h>
#include <edict/detail/Traits.h>

#include <algorithm>
#include <any>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace edict {

/// String-topic pub/sub engine with dynamic topic routing.
/// Uses std::any for type erasure on the convenience path.
/// For zero-cost typed dispatch, use Channel<Args...> instead.
/// Thread-safety controlled by Policy template parameter.
template <typename Policy = SingleThreaded>
    requires ThreadingPolicy<Policy>
class Broadcaster {
public:
    Broadcaster() : state_(std::make_shared<State>()) {}

    Broadcaster(const Broadcaster&) = delete;
    Broadcaster& operator=(const Broadcaster&) = delete;
    Broadcaster(Broadcaster&&) = default;
    Broadcaster& operator=(Broadcaster&&) = default;

    // ── Subscribe: exact topic + callable ────────────────────────────────

    /// Subscribe to an exact topic. Partial arg matching supported.
    template <typename F>
        requires detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe(std::string_view topic, F&& handler,
                                          SubscribeOptions opts = {}) {
        auto id = allocate_id();
        auto erased = make_erased(std::forward<F>(handler));

        {
            typename Policy::UniqueLock lock(state_->mutex);
            state_->router.add_exact(std::string(topic), id);
            state_->entries.emplace(id, SubscriptionEntry{
                std::move(erased), opts.priority, std::string(topic), {}});
        }

        replay_retained(id, topic, erased, opts);
        return make_subscription(id);
    }

    // ── Subscribe: exact topic + member function ─────────────────────────

    /// Subscribe a member function to an exact topic.
    template <typename T, typename MF>
    [[nodiscard]] Subscription subscribe(std::string_view topic, T* obj, MF method,
                                          SubscribeOptions opts = {}) {
        return subscribe(topic, detail::bind_member(obj, method), opts);
    }

    // ── Subscribe: wildcard pattern ──────────────────────────────────────

    /// Subscribe to a wildcard pattern (* and **).
    template <typename F>
        requires detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe_pattern(std::string_view pattern, F&& handler,
                                                  SubscribeOptions opts = {}) {
        auto id = allocate_id();
        auto erased = make_erased(std::forward<F>(handler));

        {
            typename Policy::UniqueLock lock(state_->mutex);
            state_->router.add_pattern(pattern, id);
            state_->entries.emplace(id, SubscriptionEntry{
                std::move(erased), opts.priority, std::string(pattern), {}});
        }

        return make_subscription(id);
    }

    // ── Subscribe: predicate-based topic matching ────────────────────────

    /// Subscribe with a custom topic predicate.
    template <typename Pred, typename F>
        requires std::invocable<Pred, std::string_view> && detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe(Pred&& predicate, F&& handler,
                                          SubscribeOptions opts = {}) {
        auto id = allocate_id();
        auto erased = make_erased(std::forward<F>(handler));

        {
            typename Policy::UniqueLock lock(state_->mutex);
            state_->router.add_predicate(std::forward<Pred>(predicate), id);
            state_->entries.emplace(id, SubscriptionEntry{
                std::move(erased), opts.priority, {}, {}});
        }

        return make_subscription(id);
    }

    // ── Subscribe: exact topic + callable + filter ────────────────────────

    /// Subscribe with a data filter. Handler only called when filter passes.
    template <typename F, typename Pred>
        requires detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe(std::string_view topic, F&& handler,
                                          Filter<Pred> filt, SubscribeOptions opts = {}) {
        auto id = allocate_id();
        auto erased = make_erased(std::forward<F>(handler));
        auto erased_filter = make_erased_filter(std::move(filt.predicate));

        {
            typename Policy::UniqueLock lock(state_->mutex);
            state_->router.add_exact(std::string(topic), id);
            state_->entries.emplace(id, SubscriptionEntry{
                std::move(erased), opts.priority, std::string(topic),
                std::move(erased_filter)});
        }

        replay_retained(id, topic, erased, opts);
        return make_subscription(id);
    }

    // ── Retain ──────────────────────────────────────────────────────────

    /// Enable message retention. Last count messages stored per topic. Pass 0 to disable.
    void retain(std::string_view topic, std::size_t count) {
        typename Policy::UniqueLock lock(state_->mutex);
        if (count == 0) {
            state_->retention_config.erase(std::string(topic));
            state_->retained_messages.erase(std::string(topic));
        } else {
            state_->retention_config[std::string(topic)] = count;
        }
    }

    // ── Publish ──────────────────────────────────────────────────────────

    /// Publish to all matching subscribers. Reentrant-safe.
    template <typename... Args>
    void publish(std::string_view topic, const Args&... args) {
        std::vector<std::any> packed;
        if constexpr (sizeof...(Args) > 0) {
            packed.reserve(sizeof...(Args));
            (packed.emplace_back(args), ...);
        }

        const auto this_thread = std::this_thread::get_id();
        const bool reentrant = (state_->dispatching_thread.load() == this_thread);

        std::vector<SubscriptionEntry> snapshot;
        {
            std::optional<typename Policy::UniqueLock> lock;
            if (!reentrant) {
                lock.emplace(state_->mutex);
                state_->dispatching_thread.store(this_thread);
            }
            state_->router.match(topic, [&](TopicRouter::Id id) {
                if (auto it = state_->entries.find(id); it != state_->entries.end())
                    snapshot.push_back(it->second);
            });
        }

        std::stable_sort(snapshot.begin(), snapshot.end(),
            [](const SubscriptionEntry& a, const SubscriptionEntry& b) {
                return a.priority > b.priority;
            });

        auto topic_str = std::string(topic);
        for (const auto& entry : snapshot) {
            if (entry.filter && !entry.filter(packed))
                continue;
            try {
                entry.callable(packed);
            } catch (...) {
                try {
                    if (state_->error_handler)
                        state_->error_handler(std::current_exception(), topic_str);
                } catch (...) {}
            }
        }

        // Store retained message
        {
            std::optional<typename Policy::UniqueLock> lock;
            if (!reentrant) lock.emplace(state_->mutex);
            auto rit = state_->retention_config.find(topic_str);
            if (rit != state_->retention_config.end()) {
                auto& msgs = state_->retained_messages[topic_str];
                msgs.push_back(RetainedMessage{packed});
                while (msgs.size() > rit->second)
                    msgs.pop_front();
            }
        }

        if (!reentrant)
            state_->dispatching_thread.store(std::thread::id{});
    }

    // ── Queue / Dispatch ─────────────────────────────────────────────────

    /// Enqueue a message for deferred delivery.
    template <typename... Args>
    void queue(std::string_view topic, Args&&... args) {
        std::vector<std::any> packed;
        if constexpr (sizeof...(Args) > 0) {
            packed.reserve(sizeof...(Args));
            (packed.emplace_back(std::forward<Args>(args)), ...);
        }

        typename Policy::UniqueLock lock(state_->mutex);
        state_->message_queue.emplace_back(
            QueuedMessage{std::string(topic), std::move(packed)});
    }

    /// Deliver all queued messages in FIFO order.
    void dispatch() {
        const auto this_thread = std::this_thread::get_id();
        const bool reentrant = (state_->dispatching_thread.load() == this_thread);

        std::vector<QueuedMessage> pending;
        {
            std::optional<typename Policy::UniqueLock> lock;
            if (!reentrant) {
                lock.emplace(state_->mutex);
                state_->dispatching_thread.store(this_thread);
            }
            pending.swap(state_->message_queue);
        }

        for (auto& msg : pending) {
            std::vector<SubscriptionEntry> snapshot;
            {
                std::optional<typename Policy::UniqueLock> lock;
                if (!reentrant) lock.emplace(state_->mutex);
                state_->router.match(msg.topic, [&](TopicRouter::Id id) {
                    if (auto it = state_->entries.find(id); it != state_->entries.end())
                        snapshot.push_back(it->second);
                });
            }

            std::stable_sort(snapshot.begin(), snapshot.end(),
                [](const SubscriptionEntry& a, const SubscriptionEntry& b) {
                    return a.priority > b.priority;
                });

            for (const auto& entry : snapshot) {
                if (entry.filter && !entry.filter(msg.packed))
                    continue;
                try {
                    entry.callable(msg.packed);
                } catch (...) {
                    try {
                        if (state_->error_handler)
                            state_->error_handler(std::current_exception(), msg.topic);
                    } catch (...) {}
                }
            }
        }

        if (!reentrant)
            state_->dispatching_thread.store(std::thread::id{});
    }

    // ── Error handler ────────────────────────────────────────────────────

    /// Set callback for subscriber exceptions.
    void set_error_handler(ErrorHandler handler) {
        typename Policy::UniqueLock lock(state_->mutex);
        state_->error_handler = std::move(handler);
    }

    // ── Introspection ────────────────────────────────────────────────────

    /// Number of subscribers matching this exact topic.
    [[nodiscard]] std::size_t subscriber_count(std::string_view topic) const {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->router.subscriber_count(topic);
    }

    /// True if any subscriber would receive a publish on this topic.
    [[nodiscard]] bool has_subscribers(std::string_view topic) const {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->router.has_subscribers(topic);
    }

    /// Returns topics with exact-match subscribers. Does not include
    /// wildcard patterns or predicate-based subscriptions.
    [[nodiscard]] std::vector<std::string> active_topics() const {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->router.active_topics();
    }

private:
    using ErasedCallable = std::function<void(const std::vector<std::any>&)>;

    using ErasedFilter = std::function<bool(const std::vector<std::any>&)>;

    struct SubscriptionEntry {
        ErasedCallable callable;
        int priority;
        std::string topic;
        ErasedFilter filter;
    };

    struct RetainedMessage {
        std::vector<std::any> packed;
    };

    struct QueuedMessage {
        std::string topic;
        std::vector<std::any> packed;
    };

    struct State {
        TopicRouter router;
        std::unordered_map<TopicRouter::Id, SubscriptionEntry> entries;
        std::vector<QueuedMessage> message_queue;
        ErrorHandler error_handler;
        std::uint64_t next_id = 1;
        std::unordered_map<std::string, std::size_t> retention_config;
        std::unordered_map<std::string, std::deque<RetainedMessage>> retained_messages;
        mutable typename Policy::Mutex mutex;
        std::atomic<std::thread::id> dispatching_thread{};
    };

    std::shared_ptr<State> state_;

    // Replay retained messages to a newly subscribed callable.
    // Copies the callable by value to avoid dangling references (Sutter fix #1).
    void replay_retained(TopicRouter::Id id, std::string_view topic,
                         const ErasedCallable& callable, const SubscribeOptions& opts) {
        if (!opts.replay) return;

        std::deque<RetainedMessage> to_replay;
        ErasedFilter entry_filter;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            auto rit = state_->retained_messages.find(std::string(topic));
            if (rit != state_->retained_messages.end())
                to_replay = rit->second;
            if (auto eit = state_->entries.find(id); eit != state_->entries.end())
                entry_filter = eit->second.filter;
        }
        // Dispatch outside lock, using copies — no dangling refs
        for (const auto& msg : to_replay) {
            try {
                if (!entry_filter || entry_filter(msg.packed))
                    callable(msg.packed);
            } catch (...) {}
        }
    }

    TopicRouter::Id allocate_id() {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->next_id++;
    }

    Subscription make_subscription(TopicRouter::Id id) {
        auto weak = std::weak_ptr<State>(state_);
        return Subscription(id, [weak, id]() noexcept {
            if (auto s = weak.lock()) {
                try {
                    const auto this_thread = std::this_thread::get_id();
                    const bool reentrant =
                        (s->dispatching_thread.load() == this_thread);
                    std::optional<typename Policy::UniqueLock> lock;
                    if (!reentrant) lock.emplace(s->mutex);
                    s->router.remove(id);
                    s->entries.erase(id);
                } catch (...) {}
            }
        });
    }

    template <typename F>
    static ErasedCallable make_erased(F&& handler) {
        constexpr auto arity = detail::callable_arity<F>;

        if constexpr (arity == 0) {
            return [f = std::forward<F>(handler)](const std::vector<std::any>&) mutable {
                f();
            };
        } else {
            return [f = std::forward<F>(handler)](const std::vector<std::any>& args) mutable {
                if (args.size() < arity) return;
                call_erased_impl(f, args, std::make_index_sequence<arity>{});
            };
        }
    }

    template <typename F, std::size_t... Is>
    static void call_erased_impl(F& f, const std::vector<std::any>& args,
                                  std::index_sequence<Is...>) {
        using ArgsTuple = detail::callable_args_t<std::decay_t<F>>;
        f(std::any_cast<const std::remove_cvref_t<std::tuple_element_t<Is, ArgsTuple>>&>(
            args[Is])...);
    }

    template <typename Pred>
    static ErasedFilter make_erased_filter(Pred&& pred) {
        constexpr auto arity = detail::callable_arity<Pred>;

        if constexpr (arity == 0) {
            return [p = std::forward<Pred>(pred)](const std::vector<std::any>&) -> bool {
                return p();
            };
        } else {
            return [p = std::forward<Pred>(pred)](const std::vector<std::any>& args) mutable -> bool {
                if (args.size() < arity) return false;
                return call_erased_filter_impl(p, args, std::make_index_sequence<arity>{});
            };
        }
    }

    template <typename Pred, std::size_t... Is>
    static bool call_erased_filter_impl(Pred& p, const std::vector<std::any>& args,
                                         std::index_sequence<Is...>) {
        using ArgsTuple = detail::callable_args_t<std::decay_t<Pred>>;
        return p(std::any_cast<const std::remove_cvref_t<std::tuple_element_t<Is, ArgsTuple>>&>(
            args[Is])...);
    }
};

using SharedBroadcaster = Broadcaster<MultiThreaded>;

} // namespace edict
