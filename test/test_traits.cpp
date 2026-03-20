#include <doctest/doctest.h>
#include <edict/detail/Traits.h>
#include <string>

using namespace edict::detail;

// Test helpers
void free_func_0() {}
void free_func_1(int) {}
void free_func_2(int, double) {}
int  free_func_3(int, double, const std::string&) { return 0; }
void free_func_noexcept(int) noexcept {}

struct Functor0 { void operator()() {} };
struct Functor2 { int  operator()(int, float) { return 0; } };
struct ConstFunctor { void operator()(int) const {} };

struct Widget {
    void on_event(int) {}
    void on_const_event(int, double) const {}
    void on_noexcept(int) noexcept {}
    void on_const_noexcept(int) const noexcept {}
};

// callable_arity: free functions
static_assert(callable_arity<decltype(&free_func_0)> == 0);
static_assert(callable_arity<decltype(&free_func_1)> == 1);
static_assert(callable_arity<decltype(&free_func_2)> == 2);
static_assert(callable_arity<decltype(&free_func_3)> == 3);
static_assert(callable_arity<decltype(&free_func_noexcept)> == 1);

// callable_arity: function references
static_assert(callable_arity<decltype(free_func_0)> == 0);
static_assert(callable_arity<decltype(free_func_2)> == 2);

// callable_arity: lambdas
static_assert(callable_arity<decltype([](){})> == 0);
static_assert(callable_arity<decltype([](int, int){})> == 2);

// callable_arity: functors
static_assert(callable_arity<Functor0> == 0);
static_assert(callable_arity<Functor2> == 2);
static_assert(callable_arity<ConstFunctor> == 1);

// callable_arity: member function pointers
static_assert(member_callable_arity<decltype(&Widget::on_event)> == 1);
static_assert(member_callable_arity<decltype(&Widget::on_const_event)> == 2);
static_assert(member_callable_arity<decltype(&Widget::on_noexcept)> == 1);
static_assert(member_callable_arity<decltype(&Widget::on_const_noexcept)> == 1);

// callable_args_t
static_assert(std::is_same_v<callable_args_t<decltype(&free_func_2)>, std::tuple<int, double>>);
static_assert(std::is_same_v<callable_args_t<decltype(&Widget::on_const_event)>, std::tuple<int, double>>);

// has_callable_traits_v
static_assert(has_callable_traits_v<decltype(&free_func_1)>);
static_assert(has_callable_traits_v<Functor0>);
static_assert(has_callable_traits_v<decltype([](int){})>);
static_assert(!has_callable_traits_v<int>);
static_assert(!has_callable_traits_v<std::string>);

// Subscribable concept
static_assert(Subscribable<decltype(&free_func_0), int, double>);   // zero-arg watcher
static_assert(Subscribable<decltype(&free_func_0)>);                 // zero args published
static_assert(Subscribable<decltype(&free_func_1), int, double, float>); // partial
static_assert(Subscribable<decltype(&free_func_2), int, double>);    // full
static_assert(!Subscribable<decltype(&free_func_3), int, double>);   // too many required
static_assert(Subscribable<decltype([](int){}), int, double, float>); // lambda partial
static_assert(!Subscribable<int, int>);                               // non-callable
static_assert(!Subscribable<decltype([](const std::string&){}), int>); // type mismatch

TEST_CASE("bind_member: non-const method") {
    struct Counter {
        int n = 0;
        void increment(int by) { n += by; }
    };
    Counter c;
    auto bound = bind_member(&c, &Counter::increment);
    bound(3);
    bound(7);
    CHECK(c.n == 10);
}

TEST_CASE("bind_member: const method") {
    struct Getter {
        int val = 42;
        int get() const { return val; }
    };
    Getter g;
    auto bound = bind_member(&g, &Getter::get);
    CHECK(bound() == 42);
}
