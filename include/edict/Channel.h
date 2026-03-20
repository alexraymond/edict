#pragma once

#include <edict/Error.h>
#include <edict/Subscription.h>
#include <edict/detail/Traits.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace edict {

/**
 * Typed pub/sub channel with zero type erasure on the publish path.
 * Subscribers stored in priority order. Publish dispatches via snapshot
 * for reentrancy safety. Any callable satisfying Subscribable<Args...>
 * works — free functions, lambdas, member functions, functors.
 * Partial argument matching: zero-arg watchers and partial subscribers
 * are supported alongside full-arg handlers.
 */
template <typename... Args>
class Channel {
public:
    explicit Channel(std::string topic = {}) : topic_(std::move(topic)) {}

    /// Subscribe a callable to this channel.
    /// Returns a move-only Subscription that unsubscribes on destruction.
    template <typename F>
        requires detail::Subscribable<F, Args...>
    [[nodiscard]] Subscription subscribe(F&& handler, SubscribeOptions opts = {}) {
        auto id = next_id_++;

        constexpr auto arity = detail::callable_arity<F>;
        std::function<void(const Args&...)> adapted;

        if constexpr (arity == 0) {
            adapted = [f = std::forward<F>(handler)](const Args&...) mutable { f(); };
        } else if constexpr (arity == sizeof...(Args)) {
            adapted = std::forward<F>(handler);
        } else {
            adapted = [f = std::forward<F>(handler)](const Args&... args) mutable {
                auto tup = std::forward_as_tuple(args...);
                call_with_prefix(f, tup, std::make_index_sequence<arity>{});
            };
        }

        Entry entry{id, std::move(adapted), opts.priority};
        auto pos = std::ranges::upper_bound(entries_, entry,
            [](const Entry& a, const Entry& b) { return a.priority > b.priority; });
        entries_.insert(pos, std::move(entry));

        auto weak = std::weak_ptr<bool>(alive_);
        auto remover = [this, weak, id]() noexcept {
            if (auto lock = weak.lock()) {
                std::erase_if(entries_, [id](const Entry& e) { return e.id == id; });
            }
        };

        return Subscription(id, std::move(remover));
    }

    /// Subscribe a member function. Binds obj + method into a callable.
    template <typename T, typename MF>
    [[nodiscard]] Subscription subscribe(T* obj, MF method, SubscribeOptions opts = {}) {
        return subscribe(detail::bind_member(obj, method), opts);
    }

    /// Publish to all subscribers. Exception-safe: if a subscriber throws,
    /// remaining subscribers still fire. Reentrant-safe via snapshot dispatch.
    void publish(const Args&... args) const {
        if (entries_.empty()) return;
        auto snapshot = entries_;
        for (const auto& entry : snapshot) {
            try {
                entry.callable(args...);
            } catch (...) {
                try {
                    if (error_handler_)
                        error_handler_(std::current_exception(), topic_);
                } catch (...) {}
            }
        }
    }

    /// Set callback for subscriber exceptions.
    void set_error_handler(ErrorHandler handler) {
        error_handler_ = std::move(handler);
    }

    [[nodiscard]] const std::string& topic() const noexcept { return topic_; }
    [[nodiscard]] std::size_t subscriber_count() const noexcept { return entries_.size(); }
    [[nodiscard]] bool has_subscribers() const noexcept { return !entries_.empty(); }

private:
    template <typename F, typename Tuple, std::size_t... Is>
    static void call_with_prefix(F& f, Tuple& tup, std::index_sequence<Is...>) {
        f(std::get<Is>(tup)...);
    }

    struct Entry {
        Subscription::Id id;
        std::function<void(const Args&...)> callable;
        int priority;
    };

    std::string topic_;
    std::vector<Entry> entries_;
    Subscription::Id next_id_ = 1;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    ErrorHandler error_handler_;
};

} // namespace edict
