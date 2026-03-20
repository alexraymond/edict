#pragma once

#include <edict/Error.h>
#include <edict/Policy.h>
#include <edict/Subscription.h>
#include <edict/TopicRouter.h>
#include <edict/detail/Traits.h>

#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace edict {

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
                std::move(erased), opts.priority, std::string(topic)});
        }

        return make_subscription(id);
    }

    // ── Subscribe: exact topic + member function ─────────────────────────

    template <typename T, typename MF>
    [[nodiscard]] Subscription subscribe(std::string_view topic, T* obj, MF method,
                                          SubscribeOptions opts = {}) {
        return subscribe(topic, detail::bind_member(obj, method), opts);
    }

    // ── Subscribe: wildcard pattern ──────────────────────────────────────

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
                std::move(erased), opts.priority, std::string(pattern)});
        }

        return make_subscription(id);
    }

    // ── Subscribe: predicate-based topic matching ────────────────────────

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
                std::move(erased), opts.priority, {}});
        }

        return make_subscription(id);
    }

    // ── Publish ──────────────────────────────────────────────────────────

    template <typename... Args>
    void publish(std::string_view topic, const Args&... args) {
        std::vector<std::any> packed;
        if constexpr (sizeof...(Args) > 0) {
            packed.reserve(sizeof...(Args));
            (packed.emplace_back(args), ...);
        }

        // Snapshot matched entries by value — safe for concurrent unsubscribe
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

        auto topic_str = std::string(topic);
        for (const auto& entry : snapshot) {
            try {
                entry.callable(packed);
            } catch (...) {
                try {
                    if (state_->error_handler)
                        state_->error_handler(std::current_exception(), topic_str);
                } catch (...) {}
            }
        }
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
            std::vector<SubscriptionEntry> snapshot;
            {
                typename Policy::UniqueLock lock(state_->mutex);
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
    }

    // ── Error handler ────────────────────────────────────────────────────

    void set_error_handler(ErrorHandler handler) {
        typename Policy::UniqueLock lock(state_->mutex);
        state_->error_handler = std::move(handler);
    }

    // ── Introspection ────────────────────────────────────────────────────

    [[nodiscard]] std::size_t subscriber_count(std::string_view topic) const {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->router.subscriber_count(topic);
    }

    [[nodiscard]] bool has_subscribers(std::string_view topic) const {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->router.has_subscribers(topic);
    }

    [[nodiscard]] std::vector<std::string> active_topics() const {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->router.active_topics();
    }

private:
    using ErasedCallable = std::function<void(const std::vector<std::any>&)>;

    struct SubscriptionEntry {
        ErasedCallable callable;
        int priority;
        std::string topic;
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
        mutable typename Policy::Mutex mutex;
    };

    std::shared_ptr<State> state_;

    TopicRouter::Id allocate_id() {
        typename Policy::UniqueLock lock(state_->mutex);
        return state_->next_id++;
    }

    Subscription make_subscription(TopicRouter::Id id) {
        auto weak = std::weak_ptr<State>(state_);
        return Subscription(id, [weak, id]() noexcept {
            if (auto s = weak.lock()) {
                try {
                    typename Policy::UniqueLock lock(s->mutex);
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
};

using SharedBroadcaster = Broadcaster<MultiThreaded>;

} // namespace edict
