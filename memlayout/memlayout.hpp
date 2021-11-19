#ifndef _MEM_LAYOUT_HPP
#define _MEM_LAYOUT_HPP
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

// Some general invariants:
//   1. align > 0
//   2. align is power of 2.
//   3. size < max_size(size_t) - (align - 1)

#define M_CEXPR inline constexpr

namespace memlayout::detail {
M_CEXPR static bool is_power_of_two(size_t n) noexcept {
    if (n == 0)
        return false;
    return (n & (n - 1)) == 0;
}

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

// binary operation on (Z, n). This handles overflow.
M_CEXPR decltype(auto) wrap_op(auto op, size_t n, auto x, auto y) noexcept {
    static_assert(
        std::is_same_v<decltype(x), decltype(y)>,
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

} /* namespace memlayout::detail */

namespace memlayout {
using namespace detail;
class Layout;
template <typename T> class HasLayout;

//
// Layout description for a given data type.
//

class Layout {
    size_t size_;
    size_t align_;

    M_CEXPR Layout(size_t size, size_t align) noexcept
        : size_(size)
        , align_(align) {}

    M_CEXPR static std::optional<Layout> from_size_align(size_t size,
                                                         size_t align) {
        if (align != 0 && is_power_of_two(align))
            return {};
        if (size > std::numeric_limits<size_t>::max() - (align - 1))
            return {};
        return { Layout(size, align) };
    }

  public:
    M_CEXPR Layout() noexcept
        : size_(0)
        , align_(0) {}

    M_CEXPR size_t size() const noexcept { return size_; }
    M_CEXPR size_t align() const noexcept { return align_; }

    // create an aligned dangling pointer
    inline char *aligned_dangling_ptr() { return (char *)align(); }

    // align to `align` if  this.align is not already aligned.
    M_CEXPR std::optional<Layout> align_to(size_t align) {
        return { Layout::from_size_align(size(),
                                         std::max(this->align(), align)) };
    }

    // return required padding for this->size have this->align.
    // rounded_up_size = (size + align - 1) & !(align - 1)
    // where (align - 1) ensures when overflow rounded up size is 0.
    // required_padding = rounded_up_size - size.
    M_CEXPR size_t required_padding(size_t align) {
        auto size = this->size();
        auto rounded_up_size = wrap_sub(wrap_add(size, align), (size_t)1) &
                               !wrap_sub(align, (size_t)1);
        return wrap_sub(rounded_up_size, size);
    }

    // return layout that has size added with padding for given alignment.
    M_CEXPR Layout pad_to_align() {
        auto new_size = required_padding(align()) + size();
        return Layout::from_size_align(new_size, align()).value();
    }

    // repeat Layout n times with padding in between.
    M_CEXPR std::optional<std::pair<Layout, size_t>> repeat(size_t n) {
        size_t padded_size = size() + required_padding(align());
        if (auto allocate_size = checked_mul(padded_size, n)) {
            return { { Layout::from_size_align(allocate_size.value(), align())
                           .value(),
                       padded_size } };
        }
        return {};
    }

    // repeat Layout n times without adding padding.
    M_CEXPR std::optional<Layout> repeat_packed(size_t n) {
        if (auto sz = checked_mul(size(), n)) {
            if (auto layout = Layout::from_size_align(sz.value(), align())) {
                return layout;
            }
        }
        return {};
    }

    // extend layout A with layout B, adding proper padding.
    M_CEXPR std::optional<std::pair<Layout, size_t>> extend(Layout after) {
        size_t new_align = std::max(align(), after.align());
        size_t padding = required_padding(new_align);

        if (auto offset = checked_add(size(), padding)) {
            if (auto new_size = checked_add(offset.value(), after.size())) {
                if (auto layout =
                        Layout::from_size_align(new_size.value(), new_align)) {
                    return { { layout.value(), offset.value() } };
                }
            }
        }

        return {};
    }

    // extend with the same aligment.
    M_CEXPR std::optional<Layout> extend_packed(Layout after) {
        if (auto new_size = checked_add(size(), after.size())) {
            return Layout::from_size_align(new_size.value(), align());
        }
        return {};
    }

    // layout for std::array<n, T>
    template <typename T> M_CEXPR std::optional<Layout> array(size_t n) {
        if (auto layout = Layout::create<T>()) {
            if (auto p = layout.value().repeat(n)) {
                auto [layout, offset] = p;
                return { layout.pad_to_align() };
            }
        }
        return {};
    }

    // create a new layout at compile time.
    template <typename T> M_CEXPR static std::optional<Layout> create() {
        auto [size, align] = size_align<T>();
        return from_size_align(size, align);
    }

    friend inline std::string to_string(const Layout &self) {
        return "<Layout| size:" + std::to_string(self.size()) +
               ", align: " + std::to_string(self.align()) + ">";
    }
};

//
// wrap a pointer to a value with it's layout.
//

template <typename T> class HasLayout {
    std::unique_ptr<T> value_;
    Layout layout_;

    HasLayout(T &&value, Layout layout)
        : value_(std::make_unique<T>(std::move(value)))
        , layout_(std::move(layout)) {}

  public:
    M_CEXPR static std::optional<HasLayout<T>> create(T &&value) {

        if (auto l = Layout::create<std::remove_cv_t<T>>()) {
            return { HasLayout<T>(std::forward<T &&>(value), l.value()) };
        }
        return {};
    }

    M_CEXPR T &ref() { return *value_; }
    M_CEXPR T get() { return *value_; }
    M_CEXPR Layout layout() { return layout_; }
};

//
// erase the value, only provides layout. This is useful at runtime when dealing
// with the layout of a collection of values without knowing their types.
//

class SomeHasLayout {
    void *has_layout;
    Layout (*layout_)(void *);

  public:
    template <typename T>
    inline explicit SomeHasLayout(HasLayout<T> &&h)
        : has_layout(new HasLayout<T>(std::move(h)))
        , layout_([](void *has_layout) {
            return static_cast<HasLayout<T> *>(has_layout)->layout();
        }) {}

    inline Layout layout() { return layout_(has_layout); }
};

// smart constructor for HasLayout.
template <typename T> std::optional<HasLayout<T>> make_has_layout(T &&value) {
    return HasLayout<T>::create(std::forward<T &&>(value));
}

// smart constructor for HasSomeLayout.
template <typename T>
std::optional<SomeHasLayout> make_some_has_layout(T &&value) {
    if (auto v1 = HasLayout<T>::create(std::forward<T &&>(value))) {
        return { SomeHasLayout(std::move(v1.value())) };
    }
    return {};
}

//
// Some instantiations
//

template class HasLayout<char>;
template class HasLayout<double>;
template class HasLayout<uint8_t>;
template class HasLayout<uint16_t>;
template class HasLayout<uint32_t>;
template class HasLayout<uint64_t>;
template class HasLayout<int8_t>;
template class HasLayout<int16_t>;
template class HasLayout<int32_t>;
template class HasLayout<int64_t>;

} // namespace memlayout

#endif
