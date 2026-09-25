#include <cassert>
#include <cstring>
#include <iostream>
#include "MyString.h"

int main() {
    MyString text("cat");
    assert(text.size() == 3);
    assert(std::strcmp(text.c_str(), "cat") == 0);

    text.insert(1, "X");
    assert(std::strcmp(text.c_str(), "cXat") == 0);

    text.append("!");
    assert(std::strcmp(text.c_str(), "cXat!") == 0);

    text.erase(1, 1);
    assert(std::strcmp(text.c_str(), "cat!") == 0);

    text.replace(3, 1, "?");
    assert(std::strcmp(text.c_str(), "cat?") == 0);

    MyString copy = text;
    copy[0] = 'B';
    assert(std::strcmp(copy.c_str(), "Bat?") == 0);
    assert(std::strcmp(text.c_str(), "cat?") == 0);

    assert(std::strcmp(text.substr(1, 2).c_str(), "at") == 0);
    assert(MyString("abc") < MyString("abd"));
    assert(MyString("banana").find("ana", 2) == 3);

    std::cout << "All checks passed\n";
}