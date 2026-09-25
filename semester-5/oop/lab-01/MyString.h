#ifndef MYSTRING_H
#define MYSTRING_H
#include <string>
#include <cstddef>

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

    void insert(int index, const char*source);
    void insert(int index, const std::string& source);
    void insert(int index, const MyString& source);

    void insert(int index, const char* source, int count);
    void insert(int index,const std::string& source, int count);
    void insert(int index, const MyString& source, int count);

    void insert(int index, const char* source, int s_index, int count);
    void insert(int index, const std::string& source, int s_index, int count);
    void insert(int index, const MyString& source, int s_index, int count);

    void append(int count,char ch);
    void append(const char* source);
    void append(const std::string& source);
    void append(const MyString& source);
    void append(const char* source, int count);
    void append(const std::string& source, int count);
    void append(const MyString& source, int count);
    void append(const char* source, int s_index, int count);
    void append(const std::string& source, int s_index, int count);
    void append(const MyString& source, int s_index, int count);

    void erase (int index, int count);
private:
    char* data_;
    int size_;
    int capacity_;
    void swap(MyString& other);
    void insert_chars(int index, const char* source, int count);

    void insert_prefix(int index, const char* source, int count, std::size_t available);

    void insert_slice(int index, const char* source, std::size_t length, int s_index, int count);
};

#endif
