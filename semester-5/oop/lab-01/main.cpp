#include <iostream>
#include "MyString.h"

int main() {
    MyString empty;
    empty.insert(0, 1, '!');
    std::cout << empty.c_str() << ' '
              << empty.size() << ' '
              << empty.capacity() << '\n';
    return 0;
}