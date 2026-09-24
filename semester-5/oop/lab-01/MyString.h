#ifndef MYSTRING_H
#define MYSTRING_H
#include <string>

class MyString {
public:
    MyString();
    MyString(const char* source);
    ~MyString();

    int capacity() const;
    bool empty() const;
    void clear();

    int size() const;
    const char* c_str() const;

    MyString(const MyString& other);
    MyString& operator=(const char* source);
    MyString& operator=(const MyString& other);

    MyString(int count, char ch);
    MyString& operator=(char ch);
    void shrink_to_fit();

    MyString(const std::string& source);
    MyString(const char* source, int count);
    MyString(const std::string& source, int count);
    MyString(const MyString& source, int count);

    MyString& operator=(const std::string& source);

    void insert(int index, int count, char ch);
private:
    char* data_;
    int size_;
    int capacity_;
    void swap(MyString& other);
};

#endif
