#include <iostream>
#include "MyString.h"

int main() {
    MyString a("xy");
    a.append(std::string("ABCDE"), 1, 2);  // добавит "BC"
    MyString source("12345");
    a.append(source, 2, 2);                // добавит "34"
    std::cout << a.c_str() << ' ' << a.size() << ' ' << a.capacity() << '\n';

    MyString b("Hello world!");
    b.erase(5, 6);
    std::cout << b.c_str() << ' ' << b.size() << ' ' << b.capacity() << '\n';
    return 0;
}