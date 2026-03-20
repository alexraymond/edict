#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace edict {

enum class Error : std::uint8_t {
    InvalidTopic,
    TypeMismatch,
    KeyNotFound,
    BadCast,
};

[[nodiscard]] constexpr std::string_view error_message(Error e) noexcept {
    switch (e) {
        case Error::InvalidTopic: return "invalid topic: no leading/trailing '/', no empty segments";
        case Error::TypeMismatch: return "type mismatch between publisher and subscriber";
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
template <typename Pred>
struct Filter {
    Pred predicate;
};

template <typename Pred>
[[nodiscard]] Filter<Pred> filter(Pred&& pred) {
    return Filter<Pred>{std::forward<Pred>(pred)};
}

} // namespace edict
