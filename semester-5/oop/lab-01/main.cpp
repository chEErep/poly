#include <iostream>
#include "MyString.h"

int main() {
    std::string standard_source = "wonderful";
    MyString our_source("wonderful");

    MyString a("hello amazing world");
    a.replace(6, 7, standard_source, 6);
    std::cout << a.c_str() << ' ' << a.size() << ' ' << a.capacity() << '\n';

    MyString b("hello amazing world");
    b.replace(6, 7, our_source, 6);
    std::cout << b.c_str() << ' ' << b.size() << ' ' << b.capacity() << '\n';

    MyString c("hello amazing world");
    c.replace(6, 7, "wonderful", 1, 2);
    std::cout << c.c_str() << ' ' << c.size() << ' ' << c.capacity() << '\n';

    MyString d("hello amazing world");
    d.replace(6, 7, standard_source, 1, 2);
    std::cout << d.c_str() << ' ' << d.size() << ' ' << d.capacity() << '\n';

    MyString e("hello amazing world");
    e.replace(6, 7, our_source, 1, 2);
    std::cout << e.c_str() << ' ' << e.size() << ' ' << e.capacity() << '\n';

    return 0;
}