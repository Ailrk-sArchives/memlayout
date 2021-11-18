# memory layout descrption

Header only library for creating layout descrption for data types.

### Install

Just grab the `memlayout` folder in to your third party.

-- TODO

### Example
```c++
#include "../memlayout/memlayout.hpp"
#include <any>
#include <iostream>
#include <vector>

using namespace memlayout;

struct A {
    int a;
    double b;
    char c;
    int d;
};

int main(void) {

    // creating a layout descrption at compile time.
    if (auto l1 = Layout::create<int>()) {
        std::cout << to_string(l1.value()) << std::endl;
    }

    if (auto l2 = Layout::create<A>()) {
        std::cout << to_string(l2.value()) << std::endl;
    }

    if (auto v = HasLayout<int>::create(10)) {
        std::cout << v.value().get() << std::endl;
        std::cout << to_string(v.value().layout()) << std::endl;
    }

    // accessing the layout information from erased type wrapper.
    std::vector<std::any> erased{
        make_some_has_layout(std::vector<int>{ 1, 2, 3 }),
        make_some_has_layout(A{ 10, 2.2, 'a', 12 }), make_some_has_layout(3)
    };

    for (auto &v : erased) {
        if (auto v1 = any_cast<std::optional<SomeHasLayout>>(v)) {
            std::cout << to_string(v1.value().layout()) << std::endl;
        }
    }
    return 0;
}
```
