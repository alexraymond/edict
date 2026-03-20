#pragma once

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace edict::detail {

// ── callable_traits: extract arity and arg types from any callable ────────────

template <typename F, typename = void>
struct callable_traits;

// Functors/lambdas via non-template operator()
template <typename F>
    requires requires { &std::remove_cvref_t<F>::operator(); }
struct callable_traits<F, void>
    : callable_traits<decltype(&std::remove_cvref_t<F>::operator())> {};

// Bare function types
template <typename R, typename... Args>
struct callable_traits<R(Args...), void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct callable_traits<R(Args...) noexcept, void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

// Function pointers
template <typename R, typename... Args>
struct callable_traits<R(*)(Args...), void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct callable_traits<R(*)(Args...) noexcept, void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

// Function references
template <typename R, typename... Args>
struct callable_traits<R(&)(Args...), void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct callable_traits<R(&)(Args...) noexcept, void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

// Member function pointers (non-const, const, noexcept, const noexcept)
template <typename R, typename T, typename... Args>
struct callable_traits<R(T::*)(Args...), void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename T, typename... Args>
struct callable_traits<R(T::*)(Args...) const, void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename T, typename... Args>
struct callable_traits<R(T::*)(Args...) noexcept, void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename T, typename... Args>
struct callable_traits<R(T::*)(Args...) const noexcept, void> {
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};

// ── Convenience aliases ──────────────────────────────────────────────────────

template <typename F>
inline constexpr std::size_t callable_arity =
    callable_traits<std::remove_cvref_t<F>>::arity;

template <typename F>
using callable_args_t =
    typename callable_traits<std::remove_cvref_t<F>>::args;

template <typename MF>
inline constexpr std::size_t member_callable_arity =
    callable_traits<MF>::arity;

// ── SFINAE-safe detection ────────────────────────────────────────────────────

template <typename F, typename = void>
inline constexpr bool has_callable_traits_v = false;

template <typename F>
inline constexpr bool has_callable_traits_v<
    F, std::void_t<typename callable_traits<std::remove_cvref_t<F>>::args>
> = true;

// ── Subscribable concept ─────────────────────────────────────────────────────
//
// F is Subscribable<Args...> when:
//   1. F has detectable callable traits (not a generic lambda)
//   2. F's arity <= sizeof...(Args)
//   3. F is invocable with the first callable_arity<F> types from Args...
//
// This enables partial argument matching: a zero-arg watcher, a partial
// subscriber, or a full subscriber all satisfy Subscribable for the same
// published argument list.

namespace sub_detail {

template <typename F, typename ArgsTuple, typename Indices>
struct invocable_with_prefix_impl;

template <typename F, typename ArgsTuple, std::size_t... Is>
struct invocable_with_prefix_impl<F, ArgsTuple, std::index_sequence<Is...>>
    : std::is_invocable<F, std::tuple_element_t<Is, ArgsTuple>...> {};

template <typename F, std::size_t N, typename... Args>
inline constexpr bool invocable_with_prefix_v =
    invocable_with_prefix_impl<
        F, std::tuple<Args...>, std::make_index_sequence<N>
    >::value;

} // namespace sub_detail

template <typename F, typename... Args>
concept Subscribable =
    has_callable_traits_v<F> &&
    (callable_arity<F> <= sizeof...(Args)) &&
    sub_detail::invocable_with_prefix_v<F, callable_arity<F>, Args...>;

// ── bind_member ──────────────────────────────────────────────────────────────
// Wraps (object_pointer, member_function) into a callable.
// IMPORTANT: The caller must ensure obj outlives the returned callable.

// Non-const member function
template <typename T, typename R, typename... MArgs>
[[nodiscard]] auto bind_member(T* obj, R(T::*method)(MArgs...)) {
    return [obj, method](MArgs... args) -> R {
        return (obj->*method)(std::forward<MArgs>(args)...);
    };
}

// Const member function
template <typename T, typename R, typename... MArgs>
[[nodiscard]] auto bind_member(T* obj, R(T::*method)(MArgs...) const) {
    return [obj, method](MArgs... args) -> R {
        return (obj->*method)(std::forward<MArgs>(args)...);
    };
}

// Noexcept member function
template <typename T, typename R, typename... MArgs>
[[nodiscard]] auto bind_member(T* obj, R(T::*method)(MArgs...) noexcept) {
    return [obj, method](MArgs... args) noexcept -> R {
        return (obj->*method)(std::forward<MArgs>(args)...);
    };
}

// Const noexcept member function
template <typename T, typename R, typename... MArgs>
[[nodiscard]] auto bind_member(T* obj, R(T::*method)(MArgs...) const noexcept) {
    return [obj, method](MArgs... args) noexcept -> R {
        return (obj->*method)(std::forward<MArgs>(args)...);
    };
}

} // namespace edict::detail
