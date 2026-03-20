#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace edict::detail {

/// Maximum topic depth to prevent stack overflow from untrusted input.
inline constexpr std::size_t max_topic_depth = 64;

/// Transparent hash for heterogeneous string_view lookup on unordered_map<string>.
/// Eliminates heap allocation when looking up by string_view.
struct StringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

class TopicTree {
public:
    using Id = std::uint64_t;

    static bool validate_topic(std::string_view topic) noexcept {
        if (topic.empty() || topic.front() == '/' || topic.back() == '/') return false;
        bool seen_globstar = false;
        std::size_t depth = 0;
        std::size_t pos = 0;
        while (pos <= topic.size()) {
            auto sep = topic.find('/', pos);
            auto len = (sep == std::string_view::npos) ? std::string_view::npos : sep - pos;
            auto seg = topic.substr(pos, len);
            if (seg.empty()) return false;
            if (seen_globstar) return false;
            if (seg == "**") seen_globstar = true;
            if (++depth > max_topic_depth) return false;
            if (sep == std::string_view::npos) break;
            pos = sep + 1;
        }
        return true;
    }

    static bool validate_publish_topic(std::string_view topic) noexcept {
        if (!validate_topic(topic)) return false;
        return topic.find('*') == std::string_view::npos;
    }

    void insert(std::string_view pattern, Id id) {
        if (!validate_topic(pattern))
            throw std::invalid_argument("edict: invalid topic pattern");
        auto* node = &root_;
        for_each_segment(pattern, [&](std::string_view seg) {
            node = &node->children[std::string(seg)];
        });
        node->ids.push_back(id);
    }

    void remove(std::string_view pattern, Id id) {
        auto* node = &root_;
        bool found = true;
        for_each_segment(pattern, [&](std::string_view seg) {
            if (!found) return;
            auto it = node->children.find(seg);
            if (it == node->children.end()) { found = false; return; }
            node = &it->second;
        });
        if (found) std::erase(node->ids, id);
    }

    /// Zero-allocation matching. Walks the topic string segment by segment,
    /// recursing directly into the trie without pre-splitting into a vector.
    template <typename F>
    void match(std::string_view topic, F&& on_match) const {
        match_walk(root_, topic, 0, on_match);
    }

    /// Short-circuit: returns true on first match, without visiting remaining nodes.
    [[nodiscard]] bool has_any_match(std::string_view topic) const {
        return has_match_walk(root_, topic, 0);
    }

private:
    struct Node {
        std::unordered_map<std::string, Node, StringHash, std::equal_to<>> children;
        std::vector<Id> ids;
    };

    Node root_;

    // Extract the next segment starting at `pos`. Returns {segment, next_pos}.
    // next_pos == npos means this was the last segment.
    static std::pair<std::string_view, std::size_t> next_segment(
            std::string_view topic, std::size_t pos) {
        auto sep = topic.find('/', pos);
        auto len = (sep == std::string_view::npos) ? std::string_view::npos : sep - pos;
        return {topic.substr(pos, len), sep};
    }

    static void for_each_segment(std::string_view s, auto&& fn) {
        std::size_t pos = 0;
        while (pos <= s.size()) {
            auto [seg, sep] = next_segment(s, pos);
            fn(seg);
            if (sep == std::string_view::npos) break;
            pos = sep + 1;
        }
    }

    template <typename F>
    void match_walk(const Node& node, std::string_view topic,
                    std::size_t pos, F&& on_match) const {
        // ** matches zero or more remaining segments (terminal only)
        if (auto it = node.children.find(std::string_view{"**"}); it != node.children.end())
            for (auto id : it->second.ids)
                on_match(id);

        if (pos > topic.size()) {
            for (auto id : node.ids)
                on_match(id);
            return;
        }

        auto [seg, sep] = next_segment(topic, pos);
        std::size_t next_pos = (sep == std::string_view::npos) ? topic.size() + 1 : sep + 1;

        if (auto it = node.children.find(seg); it != node.children.end())
            match_walk(it->second, topic, next_pos, on_match);

        if (auto it = node.children.find(std::string_view{"*"}); it != node.children.end())
            match_walk(it->second, topic, next_pos, on_match);
    }

    [[nodiscard]] bool has_match_walk(const Node& node, std::string_view topic,
                                       std::size_t pos) const {
        if (auto it = node.children.find(std::string_view{"**"}); it != node.children.end())
            if (!it->second.ids.empty()) return true;

        if (pos > topic.size())
            return !node.ids.empty();

        auto [seg, sep] = next_segment(topic, pos);
        std::size_t next_pos = (sep == std::string_view::npos) ? topic.size() + 1 : sep + 1;

        if (auto it = node.children.find(seg); it != node.children.end())
            if (has_match_walk(it->second, topic, next_pos)) return true;

        if (auto it = node.children.find(std::string_view{"*"}); it != node.children.end())
            if (has_match_walk(it->second, topic, next_pos)) return true;

        return false;
    }
};

} // namespace edict::detail
