#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <string_view>
#include <utility>

namespace edict {

enum class Error : std::uint8_t {
    InvalidTopic,
    KeyNotFound,
    BadCast,
};

[[nodiscard]] constexpr std::string_view error_message(Error e) noexcept {
    switch (e) {
        case Error::InvalidTopic: return "invalid topic: no leading/trailing '/', no empty segments";
        case Error::KeyNotFound:  return "key not found in blackboard";
        case Error::BadCast:      return "requested type does not match stored type";
    }
    return "unknown error";
}

/// Options for subscribe(). Use designated initializers: {.priority = 10}.
/// Higher priority fires first. Default is 0.
struct SubscribeOptions {
    int priority = 0;
    bool replay = false;
};

/// Filter wrapper — wraps a predicate over published args.
/// Usage: `edict::filter([](int v) { return v > 0; })`
template <typename Pred>
struct Filter {
    Pred predicate;
};

template <typename Pred>
[[nodiscard]] Filter<Pred> filter(Pred&& pred) {
    return Filter<Pred>{std::forward<Pred>(pred)};
}

/// Error handler for subscriber exceptions.
using ErrorHandler = std::function<void(std::exception_ptr, std::string_view topic)>;

} // namespace edict
