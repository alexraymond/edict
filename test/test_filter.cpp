#include <doctest/doctest.h>
#include <edict/Broadcaster.h>

TEST_CASE("filter: subscriber only called when filter passes") {
    edict::Broadcaster<> b;
    int received = 0;
    auto sub = b.subscribe("test",
        [&](int v) { received = v; },
        edict::filter([](int v) { return v > 50; }));

    b.publish("test", 10);
    CHECK(received == 0);
    b.publish("test", 100);
    CHECK(received == 100);
}

TEST_CASE("filter: zero-arg filter") {
    edict::Broadcaster<> b;
    bool allow = false;
    int count = 0;
    auto sub = b.subscribe("test",
        [&]() { ++count; },
        edict::filter([&]() { return allow; }));

    b.publish("test");
    CHECK(count == 0);
    allow = true;
    b.publish("test");
    CHECK(count == 1);
}

TEST_CASE("filter: doesn't affect unfiltered subscribers") {
    edict::Broadcaster<> b;
    int filtered_count = 0, unfiltered_count = 0;

    auto s1 = b.subscribe("test", [&]() { ++unfiltered_count; });
    auto s2 = b.subscribe("test",
        [&]() { ++filtered_count; },
        edict::filter([]() { return false; }));

    b.publish("test");
    CHECK(unfiltered_count == 1);
    CHECK(filtered_count == 0);
}

TEST_CASE("filter: combined with replay") {
    edict::Broadcaster<> b;
    b.retain("test", 5);
    b.publish("test", 10);
    b.publish("test", 60);
    b.publish("test", 30);
    b.publish("test", 80);

    int sum = 0;
    auto sub = b.subscribe("test",
        [&](int v) { sum += v; },
        edict::filter([](int v) { return v > 50; }),
        {.replay = true});
    CHECK(sum == 60 + 80);  // only values > 50 replayed
}
