#include <iostream>
#include "MyString.h"

int main() {
    MyString a("abc");
    a.insert(1, "XY");
    std::cout << a.c_str() << '\n';  // aXYbc

    MyString b("abc");
    b.insert(1, std::string("XY"));
    std::cout << b.c_str() << '\n';  // aXYbc

    MyString c("abc");
    c.insert(1, MyString("XY"));
    std::cout << c.c_str() << '\n';  // aXYbc

    MyString self("ab");
    self.insert(1, self);
    std::cout << self.c_str() << '\n';  // aabb
    return 0;
}