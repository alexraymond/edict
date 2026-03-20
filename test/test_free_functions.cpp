#include <doctest/doctest.h>
#include <edict/global.h>

#include <optional>
#include <string>

TEST_CASE("global: subscribe and publish") {
    edict::reset();
    int received = 0;
    auto sub = edict::subscribe("test", [&](int v) { received = v; });
    edict::publish("test", 42);
    CHECK(received == 42);
}

TEST_CASE("global: set and get") {
    edict::reset();
    edict::set("score", 42);
    auto result = edict::get<int>("score");
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
}

TEST_CASE("global: observe") {
    edict::reset();
    int new_val = 0;
    auto sub = edict::observe<int>("key", [&](std::optional<int>, int v) {
        new_val = v;
    });
    edict::set("key", 99);
    CHECK(new_val == 99);
}

TEST_CASE("global: queue and dispatch") {
    edict::reset();
    int received = 0;
    auto sub = edict::subscribe("test", [&](int v) { received = v; });
    edict::queue("test", 77);
    CHECK(received == 0);
    edict::dispatch();
    CHECK(received == 77);
}

TEST_CASE("global: reset clears state") {
    edict::reset();
    edict::set("key", 42);
    CHECK(edict::has("key"));
    edict::reset();
    CHECK(!edict::has("key"));
}

TEST_CASE("global: has and erase") {
    edict::reset();
    CHECK(!edict::has("x"));
    edict::set("x", 1);
    CHECK(edict::has("x"));
    edict::erase("x");
    CHECK(!edict::has("x"));
}

TEST_CASE("global: member function subscribe") {
    edict::reset();
    struct Handler {
        int v = 0;
        void on_event(int x) { v = x; }
    };
    Handler h;
    auto sub = edict::subscribe("test", &h, &Handler::on_event);
    edict::publish("test", 55);
    CHECK(h.v == 55);
}
