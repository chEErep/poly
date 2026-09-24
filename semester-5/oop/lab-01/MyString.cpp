#include "MyString.h"
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>

MyString::MyString() : data_(nullptr), size_(0),
                       capacity_(0) {
}

MyString::MyString(const MyString& other)
    : MyString(other.c_str()){
    }

MyString::MyString(const char *source) : MyString() {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    const std::size_t length = std::strlen(source);
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max() - 1)) {
        throw std::length_error("source is too long");
    }
    size_ = static_cast<int>(length);
    capacity_ = size_ + 1;
    data_=new char[capacity_];

    for (int i=0; i < size_; ++i) {
        data_[i] = source[i];
    }
    data_[size_] = '\0';
}

MyString::~MyString() {
    delete[] data_;
}

int MyString::size() const {
    return size_;
}

const char* MyString::c_str() const {
    if (data_ == nullptr) {
        return "";
    }
    return data_;
    }

int MyString::capacity() const {
    return capacity_;
}

bool MyString::empty() const {
    return size_ == 0;
}

void MyString::clear() {
    size_=0;
    if (data_ != nullptr) {
        data_[0]='\0';
    }
}

void MyString::swap(MyString& other) {
    char* old_data=data_;
    data_=other.data_;
    other.data_=old_data;

    int old_size=size_;
    size_=other.size_;
    other.size_=old_size;

    int old_capacity=capacity_;
    capacity_=other.capacity_;
    other.capacity_=old_capacity;
}
MyString &MyString::operator=(const char *source) {
    MyString temp(source);
    swap(temp);
    return *this;
}

MyString& MyString::operator=(const MyString& other) {
    return *this = other.c_str();
}

MyString::MyString(int count, char ch): MyString() {
    if (count <0) {
        throw std::invalid_argument("count is negative");
    }
    if (count == std::numeric_limits<int>::max()) {
        throw std::length_error("string is too long");
    }
    if (count==0) {
        return;
    }

    data_=new char[count+1];
    size_=count;
    capacity_=count+1;

    for (int i =0; i<count;++i) {
        data_[i]=ch;
    }
    data_[size_]='\0';
}

MyString& MyString::operator=(char ch) {
    char source[2]= {ch, '\0'};
    return *this = source;
}

void MyString::shrink_to_fit() {
    if (size_==0) {
        delete[] data_;
        data_ = nullptr;
        capacity_ = 0;
        return;
    }
    if (capacity_==size_+1) {
        return;
    }
    char* new_data = new char[size_+1];
    for (int i=0; i<=size_;++i) {
        new_data[i]=data_[i];
    }
    delete[] data_;
    data_ = new_data;
    capacity_=size_+1;
}

MyString::MyString(const std::string& source)
    : MyString(source.c_str()) {
}

MyString::MyString(const char *source, int count) :MyString() {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    if (count < 0) {
        throw std::invalid_argument("count is negative");
    }
    const std::size_t length = std::strlen(source);
    if (length < static_cast<std::size_t>(count)) {
        count=static_cast<int>(length);
    }
    if (count==0 ) {
        return;
    }
    if (count==std::numeric_limits<int>::max()) {
        throw std::length_error("string is too long");
    }
    data_=new char[count+1];
    size_=count;
    capacity_=count+1;
    for (int i=0; i < count; ++i) {
        data_[i]=source[i];
    }
    data_[count]='\0';
}

MyString::MyString(const std::string& source, int count)
    :MyString(source.c_str(),count){
}
MyString::MyString(const MyString &source, int count)
    :MyString(source.c_str(), count){
}

MyString &MyString::operator=(const std::string &source) {
    return *this = source.c_str();
}

void MyString::insert(int index, int count, char ch) {
    if (index <0 || index > size_) {
        throw std::out_of_range("index is out of range");
    }
    if (count < 0) {
        throw std::invalid_argument("count is negative");
    }
    if (count == 0) {
        return;
    }
    if (count>std::numeric_limits<int>::max()-1-size_) {
        throw std::length_error("string is too long");
    }
    int new_size=size_+count;
    int new_capacity=capacity_;
    if (new_capacity<new_size+1) {
        new_capacity=new_size+1;
    }
    char* new_data = new char[new_capacity];
    const char* old_data=c_str();

    for (int i=0; i < index;++i) {
        new_data[i]=old_data[i];
    }
    for (int i=0;i<count; ++i) {
        new_data[index+i]=ch;
    }
    for (int i=index; i<=size_; ++i) {
        new_data[i+count]=old_data[i];
    }
    delete[] data_;
    data_=new_data;
    size_=new_size;
    capacity_=new_capacity;
}
