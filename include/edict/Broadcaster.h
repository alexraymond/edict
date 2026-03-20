#pragma once

#include <edict/Error.h>
#include <edict/Policy.h>
#include <edict/Subscription.h>
#include <edict/detail/TopicRouter.h>
#include <edict/detail/TopicTree.h>
#include <edict/detail/Traits.h>

#include <algorithm>
#include <any>
#include <cstdint>
#include <deque>
#include <functional>
#include <stdexcept>
#include <memory>
#include <string>
#include <string_view>
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

    template <typename F>
        requires detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe(std::string_view topic, F&& handler,
                                          SubscribeOptions opts = {}) {
        if (!detail::TopicTree::validate_publish_topic(topic))
            throw std::invalid_argument("edict: invalid topic string");
        // erased is deliberately copied (not moved) into the entry — replay_retained needs it below.
        auto erased = make_erased(std::forward<F>(handler));
        detail::TopicRouter::Id id;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            id = state_->next_id++;
            state_->router.add_exact(std::string(topic), id);
            state_->entries.emplace(id, SubscriptionEntry{
                erased, opts.priority, std::string(topic), {}});
        }
        replay_retained(topic, erased, {}, opts);
        return make_subscription(id);
    }

    template <typename T, typename MF>
    [[nodiscard]] Subscription subscribe(std::string_view topic, T* obj, MF method,
                                          SubscribeOptions opts = {}) {
        return subscribe(topic, detail::bind_member(obj, method), opts);
    }

    template <typename F>
        requires detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe_pattern(std::string_view pattern, F&& handler,
                                                  SubscribeOptions opts = {}) {
        if (opts.replay)
            throw std::invalid_argument("edict: replay not supported for pattern subscriptions");
        auto erased = make_erased(std::forward<F>(handler));
        detail::TopicRouter::Id id;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            id = state_->next_id++;
            state_->router.add_pattern(pattern, id);
            state_->entries.emplace(id, SubscriptionEntry{
                std::move(erased), opts.priority, std::string(pattern), {}});
        }
        return make_subscription(id);
    }

    template <typename Pred, typename F>
        requires std::invocable<Pred, std::string_view> && detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe(Pred&& predicate, F&& handler,
                                          SubscribeOptions opts = {}) {
        if (opts.replay)
            throw std::invalid_argument("edict: replay not supported for predicate subscriptions");
        auto erased = make_erased(std::forward<F>(handler));
        detail::TopicRouter::Id id;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            id = state_->next_id++;
            state_->router.add_predicate(std::forward<Pred>(predicate), id);
            state_->entries.emplace(id, SubscriptionEntry{
                std::move(erased), opts.priority, {}, {}});
        }
        return make_subscription(id);
    }

    template <typename F, typename Pred>
        requires detail::has_callable_traits_v<F>
    [[nodiscard]] Subscription subscribe(std::string_view topic, F&& handler,
                                          Filter<Pred> filt, SubscribeOptions opts = {}) {
        if (!detail::TopicTree::validate_publish_topic(topic))
            throw std::invalid_argument("edict: invalid topic string");
        auto erased = make_erased(std::forward<F>(handler));
        auto erased_filter = make_erased_filter(std::move(filt.predicate));
        detail::TopicRouter::Id id;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            id = state_->next_id++;
            state_->router.add_exact(std::string(topic), id);
            state_->entries.emplace(id, SubscriptionEntry{
                erased, opts.priority, std::string(topic), erased_filter});
        }
        replay_retained(topic, erased, erased_filter, opts);
        return make_subscription(id);
    }

    // ── Retain ──────────────────────────────────────────────────────────

    void retain(std::string_view topic, std::size_t count) {
        typename Policy::UniqueLock lock(state_->mutex);
        auto key = std::string(topic);
        if (count == 0) {
            state_->retention_config.erase(key);
            state_->retained_messages.erase(key);
        } else {
            state_->retention_config[key] = count;
        }
    }

    // ── Publish ──────────────────────────────────────────────────────────

    template <typename... Args>
    void publish(std::string_view topic, const Args&... args) {
        if (!detail::TopicTree::validate_publish_topic(topic))
            return; // invalid topic — no subscribers will match anyway

        std::vector<std::any> packed;
        if constexpr (sizeof...(Args) > 0) {
            packed.reserve(sizeof...(Args));
            (packed.emplace_back(args), ...);
        }

        auto topic_str = std::string(topic);

        // Step 1: collect snapshot + store retained under lock
        auto snap = collect_snapshot(topic);
        // Note: snapshot and retention are not atomically consistent — a concurrent
        // retain(topic, 0) between these calls may clear retention before our store.
        // This is acceptable: the message was already dispatched to current subscribers.
        store_retained(topic_str, packed);

        // Step 2: dispatch outside lock (callbacks may re-enter)
        dispatch_to(snap.entries, packed, topic_str, snap.error_handler);
    }

    // ── Queue / Dispatch ─────────────────────────────────────────────────

    template <typename... Args>
    void queue(std::string_view topic, Args&&... args) {
        if (!detail::TopicTree::validate_publish_topic(topic))
            return;
        std::vector<std::any> packed;
        if constexpr (sizeof...(Args) > 0) {
            packed.reserve(sizeof...(Args));
            (packed.emplace_back(std::forward<Args>(args)), ...);
        }
        typename Policy::UniqueLock lock(state_->mutex);
        state_->message_queue.emplace_back(
            QueuedMessage{std::string(topic), std::move(packed)});
    }

    void dispatch() {
        std::vector<QueuedMessage> pending;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            pending.swap(state_->message_queue);
        }
        for (auto& msg : pending) {
            auto snap = collect_snapshot(msg.topic);
            store_retained(msg.topic, msg.packed);
            dispatch_to(snap.entries, msg.packed, msg.topic, snap.error_handler);
        }
    }

    // ── Error handler ────────────────────────────────────────────────────

    void set_error_handler(ErrorHandler handler) {
        typename Policy::UniqueLock lock(state_->mutex);
        state_->error_handler = std::move(handler);
    }

    // ── Introspection ────────────────────────────────────────────────────

    [[nodiscard]] std::size_t subscriber_count(std::string_view topic) const {
        std::size_t n = 0;
        decltype(state_->router.predicates_snapshot()) preds;
        {
            typename Policy::SharedLock lock(state_->mutex);
            state_->router.match_static(topic, [&](detail::TopicRouter::Id) { ++n; });
            preds = state_->router.predicates_snapshot();
        }
        for (const auto& [pred, id] : preds)
            if (pred(topic)) ++n;
        return n;
    }

    [[nodiscard]] bool has_subscribers(std::string_view topic) const {
        decltype(state_->router.predicates_snapshot()) preds;
        {
            typename Policy::SharedLock lock(state_->mutex);
            bool found = false;
            state_->router.match_static(topic, [&](detail::TopicRouter::Id) { found = true; });
            if (found) return true;
            preds = state_->router.predicates_snapshot();
        }
        for (const auto& [pred, id] : preds)
            if (pred(topic)) return true;
        return false;
    }

    [[nodiscard]] std::vector<std::string> active_topics() const {
        typename Policy::SharedLock lock(state_->mutex);
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

    struct QueuedMessage {
        std::string topic;
        std::vector<std::any> packed;
    };

    struct State {
        detail::TopicRouter router;
        std::unordered_map<detail::TopicRouter::Id, SubscriptionEntry> entries;
        std::vector<QueuedMessage> message_queue;
        ErrorHandler error_handler;
        std::uint64_t next_id = 1;
        std::unordered_map<std::string, std::size_t, detail::StringHash, std::equal_to<>> retention_config;
        std::unordered_map<std::string, std::deque<std::vector<std::any>>, detail::StringHash, std::equal_to<>> retained_messages;
        mutable typename Policy::Mutex mutex;
    };

    std::shared_ptr<State> state_;

    // ── Core helpers ─────────────────────────────────────────────────────

    struct Snapshot {
        std::vector<SubscriptionEntry> entries;
        ErrorHandler error_handler;
    };

    // Collect matching entries + error handler.
    // Static matches (exact + wildcard) are collected under SharedLock.
    // Predicate matches are evaluated outside the lock to prevent deadlock
    // if a predicate calls back into the Broadcaster.
    Snapshot collect_snapshot(std::string_view topic) {
        Snapshot snap;
        decltype(state_->router.predicates_snapshot()) preds;
        {
            typename Policy::SharedLock lock(state_->mutex);
            state_->router.match_static(topic, [&](detail::TopicRouter::Id id) {
                if (auto it = state_->entries.find(id); it != state_->entries.end())
                    snap.entries.push_back(it->second);
            });
            snap.error_handler = state_->error_handler;
            preds = state_->router.predicates_snapshot();
        }
        // Predicate evaluation on snapshot — safe from data race AND reentrant predicates
        for (const auto& [pred, id] : preds) {
            if (pred(topic)) {
                typename Policy::SharedLock lock(state_->mutex);
                if (auto it = state_->entries.find(id); it != state_->entries.end())
                    snap.entries.push_back(it->second);
            }
        }
        // O(N log N) where N is matching subscribers. For hot paths with many
        // subscribers on the same topic, use Channel<Args...> which maintains
        // sorted order at insert time.
        std::stable_sort(snap.entries.begin(), snap.entries.end(),
            [](const SubscriptionEntry& a, const SubscriptionEntry& b) {
                return a.priority > b.priority;
            });
        return snap;
    }

    // Dispatch to snapshot. No lock held — callbacks may re-enter.
    void dispatch_to(const std::vector<SubscriptionEntry>& entries,
                     const std::vector<std::any>& packed,
                     const std::string& topic_str,
                     const ErrorHandler& err_handler) {
        for (const auto& entry : entries) {
            try {
                if (entry.filter && !entry.filter(packed))
                    continue;
                entry.callable(packed);
            } catch (...) {
                try {
                    if (err_handler)
                        err_handler(std::current_exception(), topic_str);
                } catch (...) {}
            }
        }
    }

    void store_retained(const std::string& topic_str,
                        const std::vector<std::any>& packed) {
        typename Policy::UniqueLock lock(state_->mutex);
        auto rit = state_->retention_config.find(topic_str);
        if (rit != state_->retention_config.end()) {
            auto& msgs = state_->retained_messages[topic_str];
            msgs.push_back(packed);
            while (msgs.size() > rit->second)
                msgs.pop_front();
        }
    }

    void replay_retained(std::string_view topic,
                         const ErasedCallable& callable,
                         const ErasedFilter& filt,
                         const SubscribeOptions& opts) {
        if (!opts.replay) return;

        std::deque<std::vector<std::any>> to_replay;
        ErrorHandler err_handler;
        {
            typename Policy::SharedLock lock(state_->mutex);
            auto rit = state_->retained_messages.find(topic);
            if (rit != state_->retained_messages.end())
                to_replay = rit->second;
            err_handler = state_->error_handler;
        }

        auto topic_str = std::string(topic);
        for (const auto& packed : to_replay) {
            if (filt && !filt(packed)) continue;
            try {
                callable(packed);
            } catch (...) {
                try {
                    if (err_handler)
                        err_handler(std::current_exception(), topic_str);
                } catch (...) {}
            }
        }
    }

    // Create a Subscription whose remover safely removes the entry.
    Subscription make_subscription(detail::TopicRouter::Id id) {
        auto weak = std::weak_ptr<State>(state_);
        return Subscription(id, [weak, id]() noexcept {
            if (auto s = weak.lock()) {
                try {
                    // Safe to lock even from within a callback — the mutex is
                    // never held during dispatch_to() callback invocation.
                    typename Policy::UniqueLock lock(s->mutex);
                    s->router.remove(id);
                    s->entries.erase(id);
                } catch (...) {}
            }
        });
    }

    // ── Type erasure helpers ─────────────────────────────────────────────

    template <typename F>
    static ErasedCallable make_erased(F&& handler) {
        constexpr auto arity = detail::callable_arity<F>;
        if constexpr (arity == 0) {
            return [f = std::forward<F>(handler)](const std::vector<std::any>&) mutable { f(); };
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
            return [p = std::forward<Pred>(pred)](const std::vector<std::any>&) -> bool { return p(); };
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
