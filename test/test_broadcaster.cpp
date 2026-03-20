#include <doctest/doctest.h>
#include <edict/Broadcaster.h>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("Broadcaster: subscribe and publish — lambda") {
    edict::Broadcaster<> b;
    int received = 0;
    auto sub = b.subscribe("test", [&](int v) { received = v; });
    b.publish("test", 42);
    CHECK(received == 42);
}

TEST_CASE("Broadcaster: zero-arg lambda") {
    edict::Broadcaster<> b;
    int count = 0;
    auto sub = b.subscribe("test", [&]() { ++count; });
    b.publish("test", 1, 2, 3);
    CHECK(count == 1);
}

TEST_CASE("Broadcaster: multi-arg") {
    edict::Broadcaster<> b;
    int ri = 0; std::string rs;
    auto sub = b.subscribe("test", [&](int i, const std::string& s) { ri = i; rs = s; });
    b.publish("test", 42, std::string("hello"));
    CHECK(ri == 42);
    CHECK(rs == "hello");
}

TEST_CASE("Broadcaster: member function") {
    struct H { int v = 0; void on(int x) { v = x; } };
    edict::Broadcaster<> b;
    H h;
    auto sub = b.subscribe("test", &h, &H::on);
    b.publish("test", 77);
    CHECK(h.v == 77);
}

TEST_CASE("Broadcaster: partial arg matching") {
    edict::Broadcaster<> b;
    int received = 0;
    auto sub = b.subscribe("test", [&](int v) { received = v; });
    b.publish("test", 42, std::string("extra"));
    CHECK(received == 42);
}

TEST_CASE("Broadcaster: wildcard via subscribe_pattern") {
    edict::Broadcaster<> b;
    int count = 0;
    auto sub = b.subscribe_pattern("events/*", [&]() { ++count; });
    b.publish("events/foo");
    b.publish("events/bar");
    b.publish("other/thing");
    CHECK(count == 2);
}

TEST_CASE("Broadcaster: predicate subscription") {
    edict::Broadcaster<> b;
    int count = 0;
    auto sub = b.subscribe(
        [](std::string_view topic) { return topic.starts_with("log/"); },
        [&]() { ++count; });
    b.publish("log/info");
    b.publish("log/error");
    b.publish("other/thing");
    CHECK(count == 2);
}

TEST_CASE("Broadcaster: priority ordering") {
    edict::Broadcaster<> b;
    std::vector<int> order;
    auto s1 = b.subscribe("test", [&]() { order.push_back(1); }, {.priority = 0});
    auto s2 = b.subscribe("test", [&]() { order.push_back(2); }, {.priority = 10});
    auto s3 = b.subscribe("test", [&]() { order.push_back(3); }, {.priority = 5});
    b.publish("test");
    REQUIRE(order.size() == 3);
    CHECK(order[0] == 2);
    CHECK(order[1] == 3);
    CHECK(order[2] == 1);
}

TEST_CASE("Broadcaster: queue and dispatch") {
    edict::Broadcaster<> b;
    int val = 0;
    auto sub = b.subscribe("test", [&](int v) { val = v; });
    b.queue("test", 42);
    CHECK(val == 0);
    b.dispatch();
    CHECK(val == 42);
}

TEST_CASE("Broadcaster: queue FIFO") {
    edict::Broadcaster<> b;
    std::vector<int> vals;
    auto sub = b.subscribe("test", [&](int v) { vals.push_back(v); });
    b.queue("test", 1);
    b.queue("test", 2);
    b.queue("test", 3);
    b.dispatch();
    CHECK(vals == std::vector{1, 2, 3});
}

TEST_CASE("Broadcaster: RAII unsubscribe") {
    edict::Broadcaster<> b;
    int count = 0;
    { auto sub = b.subscribe("test", [&]() { ++count; }); b.publish("test"); }
    b.publish("test");
    CHECK(count == 1);
}

TEST_CASE("Broadcaster: safe after destruction") {
    edict::Subscription sub;
    { edict::Broadcaster<> b; sub = b.subscribe("test", []() {}); }
    CHECK_NOTHROW(sub.cancel());
}

TEST_CASE("Broadcaster: no subscribers no-op") {
    edict::Broadcaster<> b;
    CHECK_NOTHROW(b.publish("test", 1, 2, 3));
}

TEST_CASE("Broadcaster: exception isolation") {
    edict::Broadcaster<> b;
    int count = 0;
    auto s1 = b.subscribe("test", [&]() { ++count; }, {.priority = 10});
    auto s2 = b.subscribe("test", [&]() { throw std::runtime_error("boom"); }, {.priority = 5});
    auto s3 = b.subscribe("test", [&]() { ++count; }, {.priority = 0});
    b.publish("test");
    CHECK(count == 2);
}

TEST_CASE("Broadcaster: error handler") {
    edict::Broadcaster<> b;
    std::string caught;
    b.set_error_handler([&](std::exception_ptr, std::string_view topic) {
        caught = std::string(topic);
    });
    auto sub = b.subscribe("oops", [&]() { throw std::runtime_error("fail"); });
    b.publish("oops");
    CHECK(caught == "oops");
}

TEST_CASE("Broadcaster: subscriber_count") {
    edict::Broadcaster<> b;
    CHECK(b.subscriber_count("test") == 0);
    auto s1 = b.subscribe("test", []() {});
    CHECK(b.subscriber_count("test") == 1);
    s1.cancel();
    CHECK(b.subscriber_count("test") == 0);
}

TEST_CASE("Broadcaster: active_topics") {
    edict::Broadcaster<> b;
    auto s1 = b.subscribe("a", []() {});
    auto s2 = b.subscribe("b", []() {});
    auto topics = b.active_topics();
    std::sort(topics.begin(), topics.end());
    CHECK(topics == std::vector<std::string>{"a", "b"});
}

TEST_CASE("Broadcaster: independent topics") {
    edict::Broadcaster<> b;
    int a = 0, bc = 0;
    auto s1 = b.subscribe("a", [&]() { ++a; });
    auto s2 = b.subscribe("b", [&]() { ++bc; });
    b.publish("a");
    CHECK(a == 1);
    CHECK(bc == 0);
}
