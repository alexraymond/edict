#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace edict::detail {

class TopicTree {
public:
    using Id = std::uint64_t;

    static bool validate_topic(std::string_view topic) noexcept {
        if (topic.empty() || topic.front() == '/' || topic.back() == '/') return false;
        bool seen_globstar = false;
        std::size_t pos = 0;
        while (pos <= topic.size()) {
            auto sep = topic.find('/', pos);
            auto len = (sep == std::string_view::npos) ? std::string_view::npos : sep - pos;
            auto seg = topic.substr(pos, len);
            if (seg.empty()) return false;
            if (seen_globstar) return false;
            if (seg == "**") seen_globstar = true;
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
            auto it = node->children.find(std::string(seg));
            if (it == node->children.end()) { found = false; return; }
            node = &it->second;
        });
        if (found) std::erase(node->ids, id);
    }

    template <typename F>
    void match(std::string_view topic, F&& on_match) const {
        std::vector<std::string_view> segments;
        for_each_segment(topic, [&segments](std::string_view seg) {
            segments.push_back(seg);
        });
        match_impl(root_, segments, 0, on_match);
    }

private:
    struct Node {
        std::unordered_map<std::string, Node> children;
        std::vector<Id> ids;
    };

    Node root_;

    static void for_each_segment(std::string_view s, auto&& fn) {
        std::size_t pos = 0;
        while (pos <= s.size()) {
            auto sep = s.find('/', pos);
            auto len = (sep == std::string_view::npos) ? std::string_view::npos : sep - pos;
            fn(s.substr(pos, len));
            if (sep == std::string_view::npos) break;
            pos = sep + 1;
        }
    }

    template <typename F>
    void match_impl(const Node& node, const std::vector<std::string_view>& segments,
                    std::size_t depth, F&& on_match) const {
        if (auto it = node.children.find("**"); it != node.children.end())
            for (auto id : it->second.ids)
                on_match(id);

        if (depth == segments.size()) {
            for (auto id : node.ids)
                on_match(id);
            return;
        }

        if (auto it = node.children.find(std::string(segments[depth])); it != node.children.end())
            match_impl(it->second, segments, depth + 1, on_match);

        if (auto it = node.children.find("*"); it != node.children.end())
            match_impl(it->second, segments, depth + 1, on_match);
    }
};

} // namespace edict::detail
