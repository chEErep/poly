#include "MyString.h"
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

// 1
MyString::MyString() : data_(nullptr), size_(0),
                       capacity_(0) {
}

// 4
MyString::MyString(const MyString& other)
    : MyString(other.c_str()){
    }

// 2
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

// 9
MyString::~MyString() {
    delete[] data_;
}

// 15
int MyString::size() const {
    return size_;
}

// 14
const char* MyString::c_str() const {
    if (data_ == nullptr) {
        return "";
    }
    return data_;
    }

// 16
int MyString::capacity() const {
    return capacity_;
}

// 17
bool MyString::empty() const {
    return size_ == 0;
}

// 10
void MyString::clear() {
    size_=0;
    if (data_ != nullptr) {
        data_[0]='\0';
    }
}

// Helper: swap object state
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
// 12 (const char* source)
MyString& MyString::operator=(const char* source) {
    MyString temp(source);

    if (capacity_ >= temp.size_ + 1) {
        for (int i = 0; i <= temp.size_; ++i) {
            data_[i] = temp.data_[i];
        }
        size_ = temp.size_;
    } else {
        swap(temp);
    }

    return *this;
}

// 12 (MyString source)
MyString& MyString::operator=(const MyString& other) {
    return *this = other.c_str();
}

// 8
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

// 13
MyString& MyString::operator=(char ch) {
    char source[2]= {ch, '\0'};
    return *this = source;
}

// 11
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

// 3
MyString::MyString(const std::string& source)
    : MyString(source.c_str()) {
}

// 5
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

// 6
MyString::MyString(const std::string& source, int count)
    :MyString(source.c_str(),count){
}
// 7
MyString::MyString(const MyString &source, int count)
    :MyString(source.c_str(), count){
}

// 12 (std::string source)
MyString &MyString::operator=(const std::string &source) {
    return *this = source.c_str();
}

// 18
void MyString::insert(int index, int count, char ch) {
    MyString repeated(count,ch);
    insert_chars(index,repeated.c_str(),count);
}
// Helper: insert characters into the buffer
void MyString::insert_chars(int index, const char *source, int count) {
    if (index < 0 || index > size_) {
        throw std::out_of_range("invalid index");
    }
    if (count < 0 || (count > 0 && source == nullptr)) {
        throw std::invalid_argument("invalid source or count");
    }
    if (count == 0) {
        return;
    }
    if (count > std::numeric_limits<int>::max() - 1 - size_) {
        throw std::length_error("string is too long");
    }

    int new_size = size_ + count;
    int new_capacity = capacity_;
    if (new_capacity < new_size + 1) {
        new_capacity = new_size + 1;
    }

    char* new_data = new char[new_capacity];
    const char* old_data = c_str();

    for (int i = 0; i < index; ++i) {
        new_data[i] = old_data[i];
    }
    for (int i = 0; i < count; ++i) {
        new_data[index + i] = source[i];
    }
    for (int i = index; i <= size_; ++i) {
        new_data[i + count] = old_data[i];
    }

    delete[] data_;
    data_ = new_data;
    size_ = new_size;
    capacity_ = new_capacity;
}

// 19-21
void MyString::insert(int index, const char *source) {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    std::size_t length = std::strlen(source);
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("string is too long");
    }
    insert_chars(index, source, static_cast<int>(length));
}
void MyString::insert(int index, const std::string &source) {
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("string is too long");
    }
    insert_chars(index, source.data(), static_cast<int>(source.size()));
}
void MyString::insert(int index, const MyString &source) {
    insert_chars(index, source.c_str(), source.size());
}

// Helper: limit the inserted prefix
void MyString::insert_prefix(int index, const char* source,
                             int count, std::size_t available) {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    if (count < 0) {
        throw std::invalid_argument("negative count");
    }
    if (static_cast<std::size_t>(count) > available) {
        count = static_cast<int>(available);
    }

    insert_chars(index, source, count);
}

// 22-24
void MyString::insert(int index, const char* source, int count) {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    insert_prefix(index, source, count, std::strlen(source));
}

void MyString::insert(int index, const std::string& source, int count) {
    insert_prefix(index, source.data(), count, source.size());
}

void MyString::insert(int index, const MyString& source, int count) {
    insert_prefix(index, source.c_str(), count,
                  static_cast<std::size_t>(source.size()));
}


// Helper: select a source slice
void MyString::insert_slice(int index, const char *source, std::size_t length, int s_index, int count) {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    if (s_index < 0 || static_cast<std::size_t>(s_index) > length) {
        throw std::out_of_range("invalid source index");
    }
    std::size_t start = static_cast<std::size_t>(s_index);
    insert_prefix(index, source+start,count, length-start);
}

// 25-27
void MyString::insert(int index, const char* source,
                      int s_index, int count) {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    insert_slice(index, source, std::strlen(source), s_index, count);
}

void MyString::insert(int index, const std::string& source,
                      int s_index, int count) {
    insert_slice(index, source.data(), source.size(), s_index, count);
}

void MyString::insert(int index, const MyString& source,
                      int s_index, int count) {
    insert_slice(index, source.c_str(),
                 static_cast<std::size_t>(source.size()), s_index, count);
}

// 28
void MyString::append(int count, char ch) {
    insert(size_,count,ch);
}

// 29-31
void MyString::append(const char *source) {
    insert(size_,source);
}

void MyString::append(const std::string& source) {
    insert(size_, source);
}

void MyString::append(const MyString& source) {
    insert(size_, source);
}

// 32-34
void MyString::append(const char* source, int count) {
    insert(size_, source, count);
}

void MyString::append(const std::string& source, int count) {
    insert(size_, source, count);
}

void MyString::append(const MyString& source, int count) {
    insert(size_, source, count);
}

// 35-37
void MyString::append(const char* source, int s_index, int count) {
    insert(size_, source, s_index, count);
}
void MyString::append(const std::string& source, int s_index, int count) {
    insert(size_, source, s_index, count);
}

void MyString::append(const MyString& source, int s_index, int count) {
    insert(size_, source, s_index, count);
}

// 38
void MyString::erase (int index, int count) {
    if (index < 0 || index >size_) {
        throw std::out_of_range("invalid index");
    }
    if (count < 0) {
        throw std::invalid_argument("negative count");
    }
// Clamp count to the number of remaining characters.
    if (count > size_ - index) {
        count = size_ - index;
    }
    if (count ==0) {
        return;
    }
    // Shift the suffix left, including the terminating null character.
    for (int i=index; i+count <=size_;++i) {
        data_[i]=data_[i+count];
    }
    size_ -= count;
}

// 39-41
void MyString::replace(int index, int count, const char* source) {
    if (index < 0 || index > size_) {
        throw std::out_of_range("invalid index");
    }
    if (count < 0) {
        throw std::invalid_argument("negative count");
    }

    MyString replacement(source);
    erase(index, count);
    insert(index, replacement);
}
void MyString::replace(int index, int count, const std::string& source) {
    replace(index, count, source.c_str());
}

void MyString::replace(int index, int count, const MyString& source) {
    replace(index, count, source.c_str());
}

// 42-44
void MyString::replace(int index, int count,
                       const char* source, int s_count) {
    MyString replacement(source, s_count);
    replace(index, count, replacement.c_str());
}
void MyString::replace(int index, int count, const std::string& source, int s_count) {
    replace(index, count, source.c_str(), s_count);
}

void MyString::replace(int index, int count, const MyString& source, int s_count) {
    replace(index, count, source.c_str(), s_count);
}

// 45-47
void MyString::replace(int index, int count, const char* source,
                       int s_index, int s_count) {
    MyString fragment;
    fragment.append(source, s_index, s_count);
    replace(index, count, fragment.c_str());
}
void MyString::replace(int index, int count, const std::string& source, int s_index, int s_count) {
    replace(index, count, source.c_str(), s_index, s_count);
}

void MyString::replace(int index, int count, const MyString& source, int s_index, int s_count) {
    replace(index, count, source.c_str(), s_index, s_count);
}

// 48
MyString MyString::substr(int index) const {
    if (index < 0 || index > size_) {
        throw std::out_of_range("invalid index");
    }
    return substr(index, size_ - index);
}

// 49
MyString MyString::substr(int index, int count) const {
    if (index < 0 || index > size_) {
        throw std::out_of_range("invalid index");
    }
    return MyString(c_str() + index, count);
}

// 50-52
MyString MyString::operator+(const char* source) const {
    MyString result(*this);
    result.append(source);
    return result;
}

MyString MyString::operator+(const std::string& source) const {
    MyString result(*this);
    result.append(source);
    return result;
}

MyString MyString::operator+(const MyString& source) const {
    MyString result(*this);
    result.append(source);
    return result;
}

// 53-55
MyString& MyString::operator+=(const char* source) {
    append(source);
    return *this;
}

MyString& MyString::operator+=(const std::string& source) {
    append(source);
    return *this;
}

MyString& MyString::operator+=(const MyString& source) {
    append(source);
    return *this;
}

// Helper: validate an element index
void MyString::check_index(int index) const {
    if (index < 0 || index >= size_) {
        throw std::out_of_range("invalid index");
    }
}

// 56
char& MyString::operator[](int index) {
    check_index(index);
    return data_[index];
}

const char& MyString::operator[](int index) const {
    check_index(index);
    return data_[index];
}

// 57
int MyString::compare(const MyString& other) const {
    int common_size = size_;
    if (other.size_ < common_size) {
        common_size = other.size_;
    }
    for (int i=0; i<common_size; ++i) {
        unsigned char left = static_cast<unsigned char>((*this)[i]);
        unsigned char right = static_cast<unsigned char>(other[i]);

        if (left < right) {
            return -1;
        }
        if (left > right) {
            return 1;
        }
    }

    if (size_ < other.size_ ) {
        return -1;
    }
    if (size_ > other.size_) {
        return 1;
    }
    return 0;
}

// 58-60 (six comparison operators in Table 1)
bool MyString::operator==(const MyString& other) const {
    return compare(other) == 0;
}

bool MyString::operator!=(const MyString& other) const {
    return compare(other) != 0;
}

bool MyString::operator<(const MyString& other) const {
    return compare(other) < 0;
}

bool MyString::operator<=(const MyString& other) const {
    return compare(other) <= 0;
}

bool MyString::operator>(const MyString& other) const {
    return compare(other) > 0;
}

bool MyString::operator>=(const MyString& other) const {
    return compare(other) >= 0;
}

// Helper: search for a character sequence
int MyString::find_chars(const char* source,
                         std::size_t length, int start) const {
    if (start < 0 || start > size_) {
        throw std::out_of_range("invalid start index");
    }
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    if (length == 0) {
        return start;
    }

    std::size_t available = static_cast<std::size_t>(size_ - start);
    if (length > available) {
        return -1;
    }

    const char* text = c_str();

    for (std::size_t i = static_cast<std::size_t>(start);
         i + length <= static_cast<std::size_t>(size_);
         ++i) {
        bool same = true;

        for (std::size_t j = 0; j < length; ++j) {
            if (text[i + j] != source[j]) {
                same = false;
                break;
            }
        }

        if (same) {
            return static_cast<int>(i);
        }
         }

    return -1;
}


// 61-63
int MyString::find(const char* source) const {
    return find(source, 0);
}

int MyString::find(const std::string& source) const {
    return find(source, 0);
}

int MyString::find(const MyString& source) const {
    return find(source, 0);
}

// 64-66
int MyString::find(const char* source, int index) const {
    if (source == nullptr) {
        throw std::invalid_argument("source is null");
    }
    return find_chars(source, std::strlen(source), index);
}

int MyString::find(const std::string& source, int index) const {
    return find_chars(source.data(), source.size(), index);
}

int MyString::find(const MyString& source, int index) const {
    return find_chars(source.c_str(),
                      static_cast<std::size_t>(source.size()), index);
}