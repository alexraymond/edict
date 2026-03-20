#pragma once

#include <edict/Error.h>
#include <edict/Policy.h>
#include <edict/Subscription.h>
#include <edict/TopicRouter.h>
#include <edict/detail/TopicTree.h>
#include <edict/detail/Traits.h>

#include <algorithm>
#include <any>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
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
        std::vector<std::any> packed;
        if constexpr (sizeof...(Args) > 0) {
            packed.reserve(sizeof...(Args));
            (packed.emplace_back(args), ...);
        }

        auto topic_str = std::string(topic);

        // Step 1: collect snapshot under lock, then release
        auto snapshot = collect_snapshot(topic);

        // Step 2: dispatch outside lock (callbacks may re-enter)
        dispatch_to(snapshot, packed, topic_str);

        // Step 3: store retained under lock
        store_retained(topic_str, packed);
    }

    // ── Queue / Dispatch ─────────────────────────────────────────────────

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

    void dispatch() {
        std::vector<QueuedMessage> pending;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            pending.swap(state_->message_queue);
        }
        for (auto& msg : pending) {
            auto snapshot = collect_snapshot(msg.topic);
            dispatch_to(snapshot, msg.packed, msg.topic);
        }
    }

    // ── Error handler ────────────────────────────────────────────────────

    void set_error_handler(ErrorHandler handler) {
        typename Policy::UniqueLock lock(state_->mutex);
        state_->error_handler = std::move(handler);
    }

    // ── Introspection ────────────────────────────────────────────────────

    [[nodiscard]] std::size_t subscriber_count(std::string_view topic) const {
        typename Policy::SharedLock lock(state_->mutex);
        return state_->router.subscriber_count(topic);
    }

    [[nodiscard]] bool has_subscribers(std::string_view topic) const {
        typename Policy::SharedLock lock(state_->mutex);
        return state_->router.has_subscribers(topic);
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
        TopicRouter router;
        std::unordered_map<TopicRouter::Id, SubscriptionEntry> entries;
        std::vector<QueuedMessage> message_queue;
        ErrorHandler error_handler;
        std::uint64_t next_id = 1;
        std::unordered_map<std::string, std::size_t> retention_config;
        std::unordered_map<std::string, std::deque<std::vector<std::any>>> retained_messages;
        mutable typename Policy::Mutex mutex;
        // Tracks the thread currently dispatching, for safe reentrant cancel().
        std::atomic<std::thread::id> dispatching_thread{};
    };

    std::shared_ptr<State> state_;

    // ── Core helpers ─────────────────────────────────────────────────────

    // Collect matching entries into a priority-sorted snapshot.
    // Lock is acquired and released within this call.
    std::vector<SubscriptionEntry> collect_snapshot(std::string_view topic) {
        std::vector<SubscriptionEntry> snapshot;
        {
            typename Policy::UniqueLock lock(state_->mutex);
            state_->router.match(topic, [&](TopicRouter::Id id) {
                if (auto it = state_->entries.find(id); it != state_->entries.end())
                    snapshot.push_back(it->second);
            });
        }
        std::stable_sort(snapshot.begin(), snapshot.end(),
            [](const SubscriptionEntry& a, const SubscriptionEntry& b) {
                return a.priority > b.priority;
            });
        return snapshot;
    }

    // Dispatch to a snapshot of entries. No lock held — callbacks may re-enter.
    // Sets dispatching_thread for the duration so that cancel() from within
    // a callback can skip lock acquisition (avoiding deadlock on non-recursive mutex).
    void dispatch_to(const std::vector<SubscriptionEntry>& snapshot,
                     const std::vector<std::any>& packed,
                     const std::string& topic_str) {
        auto tid = std::this_thread::get_id();
        bool was_dispatching =
            (state_->dispatching_thread.load(std::memory_order_acquire) == tid);

        if (!was_dispatching)
            state_->dispatching_thread.store(tid, std::memory_order_release);

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

        if (!was_dispatching)
            state_->dispatching_thread.store(std::thread::id{}, std::memory_order_release);
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
        {
            typename Policy::UniqueLock lock(state_->mutex);
            auto rit = state_->retained_messages.find(std::string(topic));
            if (rit != state_->retained_messages.end())
                to_replay = rit->second;
        }

        auto topic_str = std::string(topic);
        for (const auto& packed : to_replay) {
            if (filt && !filt(packed)) continue;
            try {
                callable(packed);
            } catch (...) {
                try {
                    if (state_->error_handler)
                        state_->error_handler(std::current_exception(), topic_str);
                } catch (...) {}
            }
        }
    }

    TopicRouter::Id allocate_id() {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->next_id++;
    }

    // Create a Subscription whose remover safely removes the entry.
    // If cancel() is called from within a dispatch callback (same thread),
    // skip lock acquisition to avoid deadlock on non-recursive shared_mutex.
    Subscription make_subscription(TopicRouter::Id id) {
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
