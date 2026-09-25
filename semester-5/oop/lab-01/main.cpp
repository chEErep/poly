#include <iostream>
#include "MyString.h"

int main() {
    MyString a("abcd");
    MyString b("abce");
    MyString same("abcd");

    std::cout << (a == b) << ' ' << (a != b) << ' '
              << (a < b) << ' ' << (a <= b) << ' '
              << (a > b) << ' ' << (a >= b) << '\n';

    std::cout << (a == same) << ' ' << (a != same) << ' '
              << (a < same) << ' ' << (a <= same) << ' '
              << (a > same) << ' ' << (a >= same) << '\n';

    return 0;
}