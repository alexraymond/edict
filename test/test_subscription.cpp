#include <doctest/doctest.h>
#include <edict/Subscription.h>
#include <memory>

using namespace edict;

TEST_CASE("default Subscription is inactive") {
    Subscription sub;
    CHECK_FALSE(sub.active());
    CHECK(sub.id() == 0);
}

TEST_CASE("destructor calls remover") {
    int calls = 0;
    {
        Subscription sub{1, [&calls]() noexcept { ++calls; }};
        CHECK(sub.active());
        CHECK(sub.id() == 1);
    }
    CHECK(calls == 1);
}

TEST_CASE("cancel is idempotent") {
    int calls = 0;
    Subscription sub{1, [&calls]() noexcept { ++calls; }};
    sub.cancel();
    CHECK(calls == 1);
    CHECK_FALSE(sub.active());
    sub.cancel();
    CHECK(calls == 1);
}

TEST_CASE("move constructor transfers ownership") {
    int calls = 0;
    Subscription a{1, [&calls]() noexcept { ++calls; }};
    Subscription b{std::move(a)};
    CHECK_FALSE(a.active());
    CHECK(b.active());
    CHECK(b.id() == 1);
    b.cancel();
    CHECK(calls == 1);
}

TEST_CASE("move assignment cancels previous and transfers") {
    int calls_a = 0, calls_b = 0;
    Subscription a{1, [&calls_a]() noexcept { ++calls_a; }};
    Subscription b{2, [&calls_b]() noexcept { ++calls_b; }};
    b = std::move(a);
    CHECK(calls_b == 1); // old b cancelled
    CHECK_FALSE(a.active());
    b.cancel();
    CHECK(calls_a == 1);
}

TEST_CASE("moved-from is safe to destroy") {
    int calls = 0;
    Subscription a{1, [&calls]() noexcept { ++calls; }};
    Subscription b{std::move(a)};
    CHECK(calls == 0);
}

TEST_CASE("safe when broadcaster destroyed first") {
    auto state = std::make_shared<int>(42);
    std::weak_ptr<int> weak = state;

    Subscription sub{1, [weak]() noexcept {
        if (auto locked = weak.lock()) { *locked = 0; }
    }};

    state.reset();
    CHECK(weak.expired());
    sub.cancel(); // must not crash
    CHECK_FALSE(sub.active());
}

TEST_CASE("SubscriptionGroup holds multiple and cleans up") {
    int calls = 0;
    {
        SubscriptionGroup group;
        group += Subscription{1, [&calls]() noexcept { ++calls; }};
        group += Subscription{2, [&calls]() noexcept { ++calls; }};
        group += Subscription{3, [&calls]() noexcept { ++calls; }};
        CHECK(group.size() == 3);
    }
    CHECK(calls == 3);
}

TEST_CASE("cancel_all then reuse") {
    int calls = 0;
    SubscriptionGroup group;
    group += Subscription{1, [&calls]() noexcept { ++calls; }};
    group += Subscription{2, [&calls]() noexcept { ++calls; }};
    group.cancel_all();
    CHECK(calls == 2);
    CHECK(group.size() == 0);
    group += Subscription{3, [&calls]() noexcept { ++calls; }};
    CHECK(group.size() == 1);
}

TEST_CASE("SubscriptionGroup is move-only") {
    SubscriptionGroup a;
    a += Subscription{1, []() noexcept {}};
    SubscriptionGroup b{std::move(a)};
    CHECK(b.size() == 1);
    CHECK(a.size() == 0);
}
