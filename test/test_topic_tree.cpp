#include <doctest/doctest.h>
#include <edict/detail/TopicTree.h>
#include <algorithm>
#include <vector>

using edict::detail::TopicTree;

std::vector<TopicTree::Id> collect(const TopicTree& tree, std::string_view topic) {
    std::vector<TopicTree::Id> result;
    tree.match(topic, [&](TopicTree::Id id) { result.push_back(id); });
    return result;
}

bool contains(const std::vector<TopicTree::Id>& v, TopicTree::Id id) {
    return std::ranges::find(v, id) != v.end();
}

TEST_CASE("validate_topic") {
    CHECK(TopicTree::validate_topic("player/damage"));
    CHECK(TopicTree::validate_topic("a"));
    CHECK(TopicTree::validate_topic("a/b/c/d/e"));
    CHECK(TopicTree::validate_topic("player/*"));
    CHECK(TopicTree::validate_topic("player/**"));
    CHECK(TopicTree::validate_topic("**"));
    CHECK_FALSE(TopicTree::validate_topic(""));
    CHECK_FALSE(TopicTree::validate_topic("/player"));
    CHECK_FALSE(TopicTree::validate_topic("player/"));
    CHECK_FALSE(TopicTree::validate_topic("player//damage"));
    CHECK_FALSE(TopicTree::validate_topic("player/**/damage"));
}

TEST_CASE("validate_publish_topic rejects wildcards") {
    CHECK(TopicTree::validate_publish_topic("player/damage"));
    CHECK(TopicTree::validate_publish_topic("a/b/c"));
    CHECK_FALSE(TopicTree::validate_publish_topic("player/*"));
    CHECK_FALSE(TopicTree::validate_publish_topic("player/**"));
    CHECK_FALSE(TopicTree::validate_publish_topic("**"));
}

TEST_CASE("exact match") {
    TopicTree tree;
    tree.insert("player/damage", 1);
    tree.insert("player/heal", 2);
    CHECK(collect(tree, "player/damage").size() == 1);
    CHECK(contains(collect(tree, "player/damage"), 1));
    CHECK(contains(collect(tree, "player/heal"), 2));
    CHECK(collect(tree, "player/death").empty());
}

TEST_CASE("single wildcard *") {
    TopicTree tree;
    tree.insert("player/*/fire", 1);
    tree.insert("player/damage/*", 2);
    auto m1 = collect(tree, "player/damage/fire");
    CHECK(contains(m1, 1));
    CHECK(contains(m1, 2));
    CHECK(contains(collect(tree, "player/heal/fire"), 1));
    CHECK_FALSE(contains(collect(tree, "player/heal/fire"), 2));
    CHECK(collect(tree, "player/damage").empty());
}

TEST_CASE("multi wildcard **") {
    TopicTree tree;
    tree.insert("player/**", 1);
    CHECK(contains(collect(tree, "player"), 1));
    CHECK(contains(collect(tree, "player/damage"), 1));
    CHECK(contains(collect(tree, "player/damage/fire"), 1));
    CHECK(collect(tree, "enemy/damage").empty());
}

TEST_CASE("root-level **") {
    TopicTree tree;
    tree.insert("**", 1);
    CHECK(contains(collect(tree, "anything"), 1));
    CHECK(contains(collect(tree, "a/b/c"), 1));
}

TEST_CASE("mixed patterns") {
    TopicTree tree;
    tree.insert("player/damage/fire", 1);
    tree.insert("player/*/fire", 2);
    tree.insert("player/**", 3);
    auto m = collect(tree, "player/damage/fire");
    CHECK(contains(m, 1));
    CHECK(contains(m, 2));
    CHECK(contains(m, 3));
}

TEST_CASE("remove") {
    TopicTree tree;
    tree.insert("player/damage", 1);
    tree.insert("player/damage", 2);
    CHECK(collect(tree, "player/damage").size() == 2);
    tree.remove("player/damage", 1);
    auto m = collect(tree, "player/damage");
    CHECK(m.size() == 1);
    CHECK(contains(m, 2));
}

TEST_CASE("remove nonexistent id is no-op") {
    TopicTree tree;
    tree.insert("a", 1);
    tree.remove("a", 999);
    CHECK(collect(tree, "a").size() == 1);
}

TEST_CASE("callback-based match") {
    TopicTree tree;
    tree.insert("a", 1);
    int count = 0;
    tree.match("a", [&](TopicTree::Id) { ++count; });
    CHECK(count == 1);
}
