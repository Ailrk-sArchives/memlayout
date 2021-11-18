#ifndef _MEM_LAYOUT_HPP
#define _MEM_LAYOUT_HPP
#pragma once

#include <any>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <typeinfo>

#define M_CEXPR inline constexpr

namespace memlayout::detail {
M_CEXPR static bool is_power_of_two(size_t n) {
    int counter = 0;
    for (size_t bits = sizeof(size_t) * 8; bits > 0; --bits) {
        if (((n << bits) | n) == 0) {
            counter++;
        }
    }
    return counter == 1;
}

// get the size and the alignment of type T
template <typename T> constexpr std::pair<size_t, size_t> size_align() {
    return std::make_pair(sizeof(T), alignof(T));
}

} /* namespace memlayout::detail */

namespace memlayout {

class Layout;
template <typename T> class HasLayout;

class Layout {
    size_t size_;
    size_t align_;

    M_CEXPR Layout(size_t size, size_t align) noexcept
        : size_(size)
        , align_(align) {}

    M_CEXPR static std::optional<Layout> from_size_align(size_t size,
                                                         size_t align) {
        if (align != 0 && detail::is_power_of_two(align)) {
            return {};
        }

        if (size > std::numeric_limits<size_t>::max()) {
            return {};
        }

        return { Layout(size, align) };
    }

  public:
    M_CEXPR Layout() noexcept
        : size_(0)
        , align_(0) {}

    M_CEXPR size_t size() const noexcept { return size_; }
    M_CEXPR size_t align() const noexcept { return align_; }

    // create a new layout at compile time.
    template <typename T> M_CEXPR static std::optional<Layout> create() {
        auto [size, align] = detail::size_align<T>();
        return from_size_align(size, align);
    }

    friend inline std::string to_string(const Layout &self) {
        return "<Layout| size:" + std::to_string(self.size()) +
               ", align: " + std::to_string(self.align()) + ">";
    }
};

// wrap a value with it's layout
template <typename T> class HasLayout {
    T value_;
    Layout layout_;

    HasLayout(T &&value, Layout layout)
        : value_(std::move(value))
        , layout_(std::move(layout)) {}

  public:
    M_CEXPR static std::optional<HasLayout<T>> create(T &&value) {

        if (auto l = Layout::create<std::remove_cv_t<T>>()) {
            return { HasLayout<T>(std::forward<T &&>(value), l.value()) };
        }
        return {};
    }

    M_CEXPR T &ref() { return value_; }
    M_CEXPR T get() { return value_; }
    M_CEXPR Layout layout() { return layout_; }
};

class SomeHasLayout {
    void *has_layout;
    Layout (*layout_)(void*);

  public:
    template <typename T>
    explicit SomeHasLayout(HasLayout<T> &&h)
        : has_layout(new HasLayout<T>(std::move(h)))
        , layout_([](void *has_layout) {
            return static_cast<HasLayout<T> *>(has_layout)->layout();
        }) {}

    Layout layout() {
        return layout_(has_layout);
    }
};

template <typename T> std::optional<HasLayout<T>> make_has_layout(T &&value) {
    return HasLayout<T>::create(std::forward<T &&>(value));
}

template <typename T>
std::optional<SomeHasLayout> make_some_has_layout(T &&value) {
    if (auto v1 = HasLayout<T>::create(std::forward<T &&>(value))) {
        return { SomeHasLayout(std::move(v1.value())) };
    }
    return {};
}

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
