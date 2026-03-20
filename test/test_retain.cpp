#include <doctest/doctest.h>
#include <edict/Broadcaster.h>

TEST_CASE("retain: late subscriber receives retained messages") {
    edict::Broadcaster<> b;
    b.retain("test", 3);
    b.publish("test", 1);
    b.publish("test", 2);
    b.publish("test", 3);
    b.publish("test", 4); // oldest (1) evicted

    int sum = 0;
    auto sub = b.subscribe("test", [&](int v) { sum += v; }, {.replay = true});
    CHECK(sum == 2 + 3 + 4);
}

TEST_CASE("retain: non-replay subscriber doesn't get history") {
    edict::Broadcaster<> b;
    b.retain("test", 5);
    b.publish("test", 1);
    b.publish("test", 2);

    int count = 0;
    auto sub = b.subscribe("test", [&]() { ++count; }); // no replay
    CHECK(count == 0);
}

TEST_CASE("retain: zero count disables retention") {
    edict::Broadcaster<> b;
    b.retain("test", 3);
    b.publish("test", 1);
    b.retain("test", 0); // disable

    int count = 0;
    auto sub = b.subscribe("test", [&]() { ++count; }, {.replay = true});
    CHECK(count == 0);
}

TEST_CASE("retain: subscribe_pattern rejects replay") {
    edict::Broadcaster<> b;
    CHECK_THROWS_AS(
        (void)b.subscribe_pattern("a/*", [](){}, {.replay=true}),
        std::invalid_argument);
}

TEST_CASE("retain: predicate subscribe rejects replay") {
    edict::Broadcaster<> b;
    CHECK_THROWS_AS(
        (void)b.subscribe([](std::string_view){ return true; }, [](){}, {.replay=true}),
        std::invalid_argument);
}

TEST_CASE("retain: queued messages populate retention") {
    edict::Broadcaster<> b;
    b.retain("test", 5);

    b.queue("test", 1);
    b.queue("test", 2);
    b.dispatch();

    int sum = 0;
    auto sub = b.subscribe("test", [&](int v) { sum += v; }, {.replay = true});
    CHECK(sum == 1 + 2);
}
