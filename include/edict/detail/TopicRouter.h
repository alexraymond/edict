#pragma once

#include <edict/detail/TopicTree.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace edict::detail {

class TopicRouter {
public:
    using Id = std::uint64_t;
    using Predicate = std::function<bool(std::string_view)>;

    void add_exact(const std::string& topic, Id id) {
        exact_[topic].insert(id);
        reg_[id] = ExactReg{topic};
    }

    void add_pattern(std::string_view pattern, Id id) {
        tree_.insert(pattern, id);
        reg_[id] = PatternReg{std::string(pattern)};
    }

    void add_predicate(Predicate pred, Id id) {
        predicates_.emplace_back(std::move(pred), id);
        reg_[id] = PredicateReg{};
    }

    void remove(Id id) {
        auto it = reg_.find(id);
        if (it == reg_.end()) return;
        std::visit([&](auto& r) {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, ExactReg>) {
                if (auto eit = exact_.find(r.topic); eit != exact_.end()) {
                    eit->second.erase(id);
                    if (eit->second.empty()) exact_.erase(eit);
                }
            } else if constexpr (std::is_same_v<T, PatternReg>) {
                tree_.remove(r.pattern, id);
            } else {
                std::erase_if(predicates_, [id](const auto& p) { return p.second == id; });
            }
        }, it->second);
        reg_.erase(it);
    }

    /// Match exact topics and wildcard patterns only (lock-safe — no user code invoked).
    template <typename F>
    void match_static(std::string_view topic, F&& on_match) const {
        if (auto it = exact_.find(topic); it != exact_.end())
            for (auto id : it->second)
                on_match(id);
        tree_.match(topic, on_match);
    }

    /// Match user predicates only. May invoke arbitrary user code — do NOT
    /// call while holding a lock that user code might try to acquire.
    template <typename F>
    void match_predicates(std::string_view topic, F&& on_match) const {
        for (const auto& [pred, id] : predicates_)
            if (pred(topic))
                on_match(id);
    }

    /// Full match (exact + wildcards + predicates). Only safe when no lock is held.
    template <typename F>
    void match(std::string_view topic, F&& on_match) const {
        match_static(topic, on_match);
        match_predicates(topic, on_match);
    }

    [[nodiscard]] bool has_subscribers(std::string_view topic) const {
        // Short-circuit: stop as soon as any match is found
        if (auto it = exact_.find(topic); it != exact_.end() && !it->second.empty())
            return true;
        bool found = false;
        tree_.match(topic, [&](Id) { found = true; });
        if (found) return true;
        for (const auto& [pred, id] : predicates_)
            if (pred(topic)) return true;
        return false;
    }

    [[nodiscard]] std::size_t subscriber_count(std::string_view topic) const {
        std::size_t n = 0;
        match(topic, [&](Id) { ++n; });
        return n;
    }

    [[nodiscard]] std::vector<std::string> active_topics() const {
        std::vector<std::string> topics;
        for (const auto& [topic, ids] : exact_)
            if (!ids.empty()) topics.push_back(topic);
        return topics;
    }

private:
    struct ExactReg { std::string topic; };
    struct PatternReg { std::string pattern; };
    struct PredicateReg {};

    // Transparent hash enables find(string_view) without allocating std::string.
    std::unordered_map<std::string, std::unordered_set<Id>, StringHash, std::equal_to<>> exact_;
    detail::TopicTree tree_;
    std::vector<std::pair<Predicate, Id>> predicates_;
    std::unordered_map<Id, std::variant<ExactReg, PatternReg, PredicateReg>> reg_;
};

} // namespace edict::detail
