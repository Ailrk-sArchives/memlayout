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
    if (auto l1 = Layout::create<int>()) {
        std::cout << to_string(l1.value()) << std::endl;
    }
    std::cout << "no" << std::endl;

    if (auto l2 = Layout::create<A>()) {
        std::cout << to_string(l2.value()) << std::endl;
    }
    std::cout << "no" << std::endl;

    return 0;
}
