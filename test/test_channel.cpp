#include <doctest/doctest.h>
#include <edict/Channel.h>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("Channel: typed subscribe and publish") {
    edict::Channel<int, std::string> ch;
    int ri = 0; std::string rs;
    auto sub = ch.subscribe([&](int i, const std::string& s) { ri = i; rs = s; });
    ch.publish(42, "hello");
    CHECK(ri == 42);
    CHECK(rs == "hello");
}

TEST_CASE("Channel: zero-arg watcher") {
    edict::Channel<int, std::string> ch;
    int count = 0;
    auto sub = ch.subscribe([&]() { ++count; });
    ch.publish(1, "a");
    ch.publish(2, "b");
    CHECK(count == 2);
}

TEST_CASE("Channel: partial arg — first only") {
    edict::Channel<int, std::string> ch;
    int received = 0;
    auto sub = ch.subscribe([&](int i) { received = i; });
    ch.publish(99, "ignored");
    CHECK(received == 99);
}

TEST_CASE("Channel: member function subscribe") {
    struct H { int v = 0; void on(int x) { v = x; } };
    edict::Channel<int> ch;
    H h;
    auto sub = ch.subscribe(&h, &H::on);
    ch.publish(123);
    CHECK(h.v == 123);
}

TEST_CASE("Channel: priority ordering") {
    edict::Channel<> ch;
    std::vector<int> order;
    auto s1 = ch.subscribe([&]() { order.push_back(1); }, {.priority = 0});
    auto s2 = ch.subscribe([&]() { order.push_back(2); }, {.priority = 10});
    auto s3 = ch.subscribe([&]() { order.push_back(3); }, {.priority = 5});
    ch.publish();
    REQUIRE(order.size() == 3);
    CHECK(order[0] == 2);
    CHECK(order[1] == 3);
    CHECK(order[2] == 1);
}

TEST_CASE("Channel: RAII unsubscribe") {
    edict::Channel<int> ch;
    int count = 0;
    { auto sub = ch.subscribe([&](int) { ++count; }); ch.publish(1); }
    ch.publish(2);
    CHECK(count == 1);
}

TEST_CASE("Channel: cancel()") {
    edict::Channel<int> ch;
    int count = 0;
    auto sub = ch.subscribe([&](int) { ++count; });
    ch.publish(1);
    sub.cancel();
    ch.publish(2);
    CHECK(count == 1);
}

TEST_CASE("Channel: multiple subscribers") {
    edict::Channel<int> ch;
    int a = 0, b = 0;
    auto s1 = ch.subscribe([&](int v) { a = v; });
    auto s2 = ch.subscribe([&](int v) { b = v * 2; });
    ch.publish(10);
    CHECK(a == 10);
    CHECK(b == 20);
}

TEST_CASE("Channel: exception isolation") {
    edict::Channel<> ch;
    int count = 0;
    auto s1 = ch.subscribe([&]() { ++count; }, {.priority = 10});
    auto s2 = ch.subscribe([&]() { throw std::runtime_error("boom"); }, {.priority = 5});
    auto s3 = ch.subscribe([&]() { ++count; }, {.priority = 0});
    ch.publish();
    CHECK(count == 2);
}

TEST_CASE("Channel: no subscribers is no-op") {
    edict::Channel<int> ch;
    CHECK_NOTHROW(ch.publish(1));
}

TEST_CASE("Channel: subscriber_count") {
    edict::Channel<int> ch;
    CHECK(ch.subscriber_count() == 0);
    auto s1 = ch.subscribe([](int) {});
    CHECK(ch.subscriber_count() == 1);
    s1.cancel();
    CHECK(ch.subscriber_count() == 0);
}

TEST_CASE("Channel: move subscription out") {
    edict::Channel<int> ch;
    int val = 0;
    edict::Subscription moved;
    { auto sub = ch.subscribe([&](int v) { val = v; }); moved = std::move(sub); }
    ch.publish(42);
    CHECK(val == 42);
}
