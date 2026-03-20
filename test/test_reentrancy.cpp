#include <doctest/doctest.h>
#include <edict/Broadcaster.h>
#include <edict/Channel.h>
#include <vector>

TEST_CASE("Channel: publish during publish") {
    edict::Channel<int> ch;
    std::vector<int> received;
    auto sub = ch.subscribe([&](int v) {
        received.push_back(v);
        if (v == 1) ch.publish(2);
    });
    ch.publish(1);
    CHECK(received.size() == 2);
    CHECK(received[0] == 1);
    CHECK(received[1] == 2);
}

TEST_CASE("Channel: self-cancel during dispatch") {
    edict::Channel<> ch;
    int count = 0;
    edict::Subscription sub;
    sub = ch.subscribe([&]() { ++count; sub.cancel(); });
    ch.publish();
    CHECK(count == 1);
    ch.publish();
    CHECK(count == 1);
    CHECK(ch.subscriber_count() == 0);
}

TEST_CASE("Channel: subscribe during dispatch — snapshot") {
    edict::Channel<> ch;
    int orig = 0, added = 0;
    edict::Subscription new_sub;
    auto sub = ch.subscribe([&]() {
        ++orig;
        if (orig == 1) new_sub = ch.subscribe([&]() { ++added; });
    });
    ch.publish();
    CHECK(orig == 1);
    CHECK(added == 0);
    ch.publish();
    CHECK(orig == 2);
    CHECK(added == 1);
}

TEST_CASE("Channel: deep reentrant publish") {
    edict::Channel<int> ch;
    std::vector<int> received;
    auto sub = ch.subscribe([&](int d) {
        received.push_back(d);
        if (d < 5) ch.publish(d + 1);
    });
    ch.publish(1);
    CHECK(received == std::vector{1, 2, 3, 4, 5});
}

TEST_CASE("Channel: exception — others still called") {
    edict::Channel<> ch;
    std::vector<int> called;
    auto s1 = ch.subscribe([&]() { called.push_back(1); }, {.priority = 10});
    auto s2 = ch.subscribe([&]() { called.push_back(2); throw std::runtime_error("x"); }, {.priority = 5});
    auto s3 = ch.subscribe([&]() { called.push_back(3); }, {.priority = 0});
    ch.publish();
    CHECK(called == std::vector{1, 2, 3});
}

TEST_CASE("Broadcaster: publish during publish") {
    edict::Broadcaster<> b;
    std::vector<int> received;
    auto sub = b.subscribe("test", [&](int v) {
        received.push_back(v);
        if (v == 1) b.publish("test", 2);
    });
    b.publish("test", 1);
    CHECK(received.size() == 2);
    CHECK(received[0] == 1);
    CHECK(received[1] == 2);
}

TEST_CASE("Broadcaster: self-cancel during dispatch") {
    edict::Broadcaster<> b;
    int count = 0;
    edict::Subscription sub;
    sub = b.subscribe("test", [&]() { ++count; sub.cancel(); });
    b.publish("test");
    CHECK(count == 1);
    b.publish("test");
    CHECK(count == 1);
}

TEST_CASE("Broadcaster: exception — others still called") {
    edict::Broadcaster<> b;
    std::vector<int> called;
    auto s1 = b.subscribe("test", [&]() { called.push_back(1); }, {.priority = 10});
    auto s2 = b.subscribe("test", [&]() { called.push_back(2); throw std::runtime_error("x"); }, {.priority = 5});
    auto s3 = b.subscribe("test", [&]() { called.push_back(3); }, {.priority = 0});
    b.publish("test");
    CHECK(called == std::vector{1, 2, 3});
}
