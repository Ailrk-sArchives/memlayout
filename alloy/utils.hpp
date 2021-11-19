#ifndef _ALLOY_UTILS_HPP
#define _ALLOY_UTILS_HPP
#pragma once

#include <any>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <typeinfo>

#define M_CEXPR inline constexpr

namespace alloy::details {
M_CEXPR static bool is_power_of_two(size_t n) noexcept {
    if (n == 0)
        return false;
    return (n & (n - 1)) == 0;
}

//
// Bit twiddling
//

// TODO: verify
template <typename T> T bit_set_range(T value, size_t offset, size_t n) {
    constexpr auto cap = std::numeric_limits<T>::max();
    constexpr size_t bits = sizeof(T) * 8;
    T mask = cap >> (bits - n);
    return value | (mask << offset);
}

template <typename T> T bit_clear_range(T value, size_t offset, size_t n) {
    constexpr size_t cap = std::numeric_limits<T>::max();
    constexpr size_t bits = sizeof(T) * 8;
    T mask = cap >> (bits - n);
    return value & ~(mask << offset);
}

// get the size and the alignment of type T
template <typename T>
constexpr std::pair<size_t, size_t> size_align() noexcept {
    return std::make_pair(sizeof(T), alignof(T));
}

//
// memory safe binary operation
//

// binary operation on (Z, n). This handles overflow.
M_CEXPR decltype(auto) wrap_op(auto op, size_t n, auto x, auto y) noexcept {
    static_assert(
        std::is_same_v<decltype(x), decltype(y)>,
        "wrapping arithmetics are required to perform on the same type");
    static_assert(
        std::is_unsigned_v<decltype(x)>,
        "wrapping arithmetics are required to perform on the same type");

    return op(x, y) % n;
}

M_CEXPR decltype(auto) wrap_add(auto x, auto y) noexcept {
    size_t n = sizeof(decltype(x));
    return wrap_op([](auto x, auto y) { return x + y; }, n, x, y);
}

M_CEXPR decltype(auto) wrap_sub(auto x, auto y) {
    size_t n = sizeof(decltype(y));
    return wrap_op([](auto x, auto y) { return x - y; }, n, x, y);
}

// Perform binop and detect overflow or underflow. if either happends, return
// nothing.
M_CEXPR decltype(auto) checked_op(auto op, auto check, auto x,
                                  auto y) noexcept {
    static_assert(
        std::is_same_v<decltype(x), decltype(y)>,
        "checked arithmetics are required to perform on the same type.");
    static_assert(std::is_unsigned_v<decltype(x)>,
                  "signed overflow is undefined.");

    constexpr auto cap = std::numeric_limits<decltype(x)>::max();
    constexpr auto bot = std::numeric_limits<decltype(x)>::min();
    auto raw_result = op(x, y);
    if (check(raw_result, x, y)) {
        return std::optional<decltype(x)>();
    }
    return std::optional<decltype(x)>{ raw_result };
}

// #define SET_RANGE_TO_1(from, to) ()

M_CEXPR decltype(auto) checked_add(auto x, auto y) {
    return checked_op([](auto x, auto y) noexcept { return x + y; },
                      [](auto res, auto x, auto y) noexcept {
                          return res < (x | y)
                                     ? std::optional<decltype(x)>()
                                     : std::optional<decltype(x)>(res);
                      },
                      x, y);
}

// 16
M_CEXPR decltype(auto) checked_mul(auto x, auto y) {
    return checked_op([](auto x, auto y) noexcept { return x * y; },
                      [](auto res, auto x, auto y) noexcept {
                          constexpr auto sz = sizeof(decltype(x));
                          constexpr size_t half = sz / 2;
                          auto a{ (x >> half) * bit_clear_range(y, 0, half) };
                          auto b{ bit_clear_range(x, 0, half) * (y >> half) };
                          return (x >> half) * (y >> half) + (a >> half) +
                                 (b >> half);
                      },
                      x, y);
}

} // namespace alloy::details
#endif
