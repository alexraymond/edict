#include <doctest/doctest.h>
#include <edict/TopicRouter.h>
#include <algorithm>
#include <vector>

using edict::TopicRouter;

std::vector<TopicRouter::Id> collect_matches(const TopicRouter& r, std::string_view topic) {
    std::vector<TopicRouter::Id> ids;
    r.match(topic, [&](TopicRouter::Id id) { ids.push_back(id); });
    std::sort(ids.begin(), ids.end());
    return ids;
}

TEST_CASE("TopicRouter: exact match") {
    TopicRouter r;
    r.add_exact("a/b", 1);
    r.add_exact("a/b", 2);
    r.add_exact("x/y", 3);
    auto ids = collect_matches(r, "a/b");
    CHECK(ids.size() == 2);
    CHECK(std::ranges::find(ids, 1) != ids.end());
    CHECK(std::ranges::find(ids, 2) != ids.end());
    CHECK(collect_matches(r, "x/y") == std::vector<TopicRouter::Id>{3});
    CHECK(collect_matches(r, "no/match").empty());
}

TEST_CASE("TopicRouter: wildcard * single level") {
    TopicRouter r;
    r.add_pattern("a/*/c", 1);
    CHECK(collect_matches(r, "a/b/c") == std::vector<TopicRouter::Id>{1});
    CHECK(collect_matches(r, "a/x/c") == std::vector<TopicRouter::Id>{1});
    CHECK(collect_matches(r, "a/b/d").empty());
}

TEST_CASE("TopicRouter: wildcard ** multi-level") {
    TopicRouter r;
    r.add_pattern("a/**", 1);
    CHECK(collect_matches(r, "a/b") == std::vector<TopicRouter::Id>{1});
    CHECK(collect_matches(r, "a/b/c/d") == std::vector<TopicRouter::Id>{1});
    CHECK(collect_matches(r, "a") == std::vector<TopicRouter::Id>{1});
    CHECK(collect_matches(r, "b/c").empty());
}

TEST_CASE("TopicRouter: predicate") {
    TopicRouter r;
    r.add_predicate([](std::string_view t) { return t.size() < 5; }, 1);
    CHECK(collect_matches(r, "ab") == std::vector<TopicRouter::Id>{1});
    CHECK(collect_matches(r, "toolong").empty());
}

TEST_CASE("TopicRouter: combined") {
    TopicRouter r;
    r.add_exact("a/b", 1);
    r.add_pattern("a/*", 2);
    r.add_predicate([](std::string_view t) { return t.starts_with("a/"); }, 3);
    auto ids = collect_matches(r, "a/b");
    CHECK(ids.size() == 3);
}

TEST_CASE("TopicRouter: remove by id") {
    TopicRouter r;
    r.add_exact("a/b", 1);
    r.add_exact("a/b", 2);
    r.remove(1);
    CHECK(collect_matches(r, "a/b") == std::vector<TopicRouter::Id>{2});
}

TEST_CASE("TopicRouter: remove pattern") {
    TopicRouter r;
    r.add_pattern("a/*", 1);
    r.remove(1);
    CHECK(collect_matches(r, "a/b").empty());
}

TEST_CASE("TopicRouter: remove predicate") {
    TopicRouter r;
    r.add_predicate([](std::string_view) { return true; }, 1);
    r.remove(1);
    CHECK(collect_matches(r, "anything").empty());
}

TEST_CASE("TopicRouter: subscriber_count and has_subscribers") {
    TopicRouter r;
    r.add_exact("a", 1);
    r.add_exact("a", 2);
    r.add_pattern("*", 3);
    CHECK(r.subscriber_count("a") == 3);
    CHECK(r.has_subscribers("a"));
    CHECK(r.has_subscribers("nothing")); // * matches any single segment
    CHECK_FALSE(r.has_subscribers("a/b")); // * does NOT match multi-segment
}

TEST_CASE("TopicRouter: active_topics") {
    TopicRouter r;
    r.add_exact("a", 1);
    r.add_exact("b", 2);
    auto topics = r.active_topics();
    std::sort(topics.begin(), topics.end());
    CHECK(topics == std::vector<std::string>{"a", "b"});
}
