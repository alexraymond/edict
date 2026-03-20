#include <doctest/doctest.h>
#include <edict/Blackboard.h>

#include <optional>
#include <string>
#include <vector>

TEST_CASE("Blackboard: set and get int") {
    edict::Blackboard<> bb;
    bb.set("score", 42);
    auto result = bb.get<int>("score");
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
}

TEST_CASE("Blackboard: set and get string") {
    edict::Blackboard<> bb;
    bb.set("name", std::string("Alice"));
    auto result = bb.get<std::string>("name");
    REQUIRE(result.has_value());
    CHECK(result.value() == "Alice");
}

TEST_CASE("Blackboard: set and get complex type") {
    edict::Blackboard<> bb;
    std::vector<int> data{1, 2, 3};
    bb.set("data", data);
    auto result = bb.get<std::vector<int>>("data");
    REQUIRE(result.has_value());
    CHECK(result.value() == std::vector<int>{1, 2, 3});
}

TEST_CASE("Blackboard: get missing key returns KeyNotFound") {
    edict::Blackboard<> bb;
    auto result = bb.get<int>("missing");
    REQUIRE(!result.has_value());
    CHECK(result.error() == edict::Error::KeyNotFound);
}

TEST_CASE("Blackboard: get wrong type returns BadCast") {
    edict::Blackboard<> bb;
    bb.set("score", 42);
    auto result = bb.get<std::string>("score");
    REQUIRE(!result.has_value());
    CHECK(result.error() == edict::Error::BadCast);
}

TEST_CASE("Blackboard: observe with full signature (optional<T>, T)") {
    edict::Blackboard<> bb;
    std::optional<int> observed_old;
    int observed_new = 0;
    bool fired = false;

    auto sub = bb.observe<int>("score", [&](std::optional<int> old_val, int new_val) {
        observed_old = old_val;
        observed_new = new_val;
        fired = true;
    });

    bb.set("score", 42);
    REQUIRE(fired);
    CHECK(!observed_old.has_value());
    CHECK(observed_new == 42);

    fired = false;
    bb.set("score", 100);
    REQUIRE(fired);
    CHECK(observed_old.has_value());
    CHECK(observed_old.value() == 42);
    CHECK(observed_new == 100);
}

TEST_CASE("Blackboard: observe with single arg (T new_val only)") {
    edict::Blackboard<> bb;
    int received = 0;

    auto sub = bb.observe<int>("score", [&](std::optional<int> old_val) {
        // With partial arg matching, first arg = optional<T> (old value)
        received++;
    });

    bb.set("score", 42);
    CHECK(received == 1);
}

TEST_CASE("Blackboard: observe with zero args (just notified)") {
    edict::Blackboard<> bb;
    int count = 0;

    auto sub = bb.observe<int>("score", [&]() { ++count; });

    bb.set("score", 42);
    CHECK(count == 1);
    bb.set("score", 100);
    CHECK(count == 2);
}

TEST_CASE("Blackboard: first set fires with nullopt as old value") {
    edict::Blackboard<> bb;
    bool was_nullopt = false;

    auto sub = bb.observe<int>("key", [&](std::optional<int> old_val, int) {
        was_nullopt = !old_val.has_value();
    });

    bb.set("key", 1);
    CHECK(was_nullopt);
}

TEST_CASE("Blackboard: RAII unobserve") {
    edict::Blackboard<> bb;
    int count = 0;

    {
        auto sub = bb.observe<int>("key", [&]() { ++count; });
        bb.set("key", 1);
        CHECK(count == 1);
    }
    // Subscription destroyed — observer removed
    bb.set("key", 2);
    CHECK(count == 1);
}

TEST_CASE("Blackboard: has()") {
    edict::Blackboard<> bb;
    CHECK(!bb.has("key"));
    bb.set("key", 42);
    CHECK(bb.has("key"));
}

TEST_CASE("Blackboard: erase()") {
    edict::Blackboard<> bb;
    bb.set("key", 42);
    REQUIRE(bb.has("key"));
    bb.erase("key");
    CHECK(!bb.has("key"));
    CHECK(!bb.get<int>("key").has_value());
}

TEST_CASE("Blackboard: keys()") {
    edict::Blackboard<> bb;
    bb.set("a", 1);
    bb.set("b", 2);
    bb.set("c", 3);
    auto k = bb.keys();
    std::sort(k.begin(), k.end());
    CHECK(k == std::vector<std::string>{"a", "b", "c"});
}
