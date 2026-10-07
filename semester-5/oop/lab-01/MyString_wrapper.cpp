#include <pybind11/pybind11.h>
#include <stdexcept>
#include <string>
#include "MyString.h"

namespace py = pybind11;

PYBIND11_MODULE(mystring, m) {
    // При импорте регистрируем связи с C++ методами. Сами методы вызываются позже.
    py::class_<MyString> cls(m, "MyString");

    // Python str передаем в перегрузки const char*, а Python MyString — в const MyString&.
    // Перегрузки со std::string не дублируем: отдельного типа std::string в Python нет.
    cls.def(py::init<>());
    cls.def(py::init<const char*>());
    cls.def(py::init<const char*, int>());
    cls.def(py::init<int, char>());
    cls.def(py::init<const MyString&>());       // Конструктор копирования -> MyString(const char*).
    cls.def(py::init<const MyString&, int>());

    // Геттеры читают тот же объект. Для c_str() pybind11 создает Python str.
    cls.def("c_str", &MyString::c_str);
    cls.def("__str__", &MyString::c_str);
    cls.def("size", &MyString::size);
    cls.def("__len__", &MyString::size);
    cls.def("capacity", &MyString::capacity);
    cls.def("empty", &MyString::empty);
    cls.def("clear", &MyString::clear);
    cls.def("shrink_to_fit", &MyString::shrink_to_fit);

    // В Python a = b меняет ссылку a. Чтобы вызвать C++ operator=, есть assign().
    // Результат operator= — ссылка на self, поэтому сохраняем связь с его временем жизни.
    cls.def("assign", static_cast<MyString& (MyString::*)(const char*)>(&MyString::operator=),
            py::return_value_policy::reference_internal);
    cls.def("assign", static_cast<MyString& (MyString::*)(const MyString&)>(&MyString::operator=),
            py::return_value_policy::reference_internal);
    cls.def("assign_char", static_cast<MyString& (MyString::*)(char)>(&MyString::operator=),
            py::return_value_policy::reference_internal);

    // static_cast выбирает перегрузку при сборке. При вызове Python выбирает подходящую из них.
    // insert(index, count, ch) -> временный MyString -> insert_chars().
    cls.def("insert", static_cast<void (MyString::*)(int, int, char)>(&MyString::insert));
    // insert(index, source) -> insert_chars().
    cls.def("insert", static_cast<void (MyString::*)(int, const char*)>(&MyString::insert));
    cls.def("insert", static_cast<void (MyString::*)(int, const MyString&)>(&MyString::insert));
    // Вставка префикса -> insert_prefix() -> insert_chars().
    cls.def("insert", static_cast<void (MyString::*)(int, const char*, int)>(&MyString::insert));
    cls.def("insert", static_cast<void (MyString::*)(int, const MyString&, int)>(&MyString::insert));
    // Вставка фрагмента -> insert_slice() -> insert_prefix() -> insert_chars().
    cls.def("insert", static_cast<void (MyString::*)(int, const char*, int, int)>(&MyString::insert));
    cls.def("insert", static_cast<void (MyString::*)(int, const MyString&, int, int)>(&MyString::insert));

    // append() вызывает соответствующий insert(size_, ...), поэтому общая работа в insert_chars().
    cls.def("append", static_cast<void (MyString::*)(int, char)>(&MyString::append));
    cls.def("append", static_cast<void (MyString::*)(const char*)>(&MyString::append));
    cls.def("append", static_cast<void (MyString::*)(const MyString&)>(&MyString::append));
    cls.def("append", static_cast<void (MyString::*)(const char*, int)>(&MyString::append));
    cls.def("append", static_cast<void (MyString::*)(const MyString&, int)>(&MyString::append));
    cls.def("append", static_cast<void (MyString::*)(const char*, int, int)>(&MyString::append));
    cls.def("append", static_cast<void (MyString::*)(const MyString&, int, int)>(&MyString::append));

    // erase() сдвигает хвост влево. replace() создает копию источника,
    // затем вызывает erase() и insert(), чтобы использовать общий код вставки.
    cls.def("erase", &MyString::erase);
    cls.def("replace", static_cast<void (MyString::*)(int, int, const char*)>(&MyString::replace));
    cls.def("replace", static_cast<void (MyString::*)(int, int, const MyString&)>(&MyString::replace));
    cls.def("replace", static_cast<void (MyString::*)(int, int, const char*, int)>(&MyString::replace));
    cls.def("replace", static_cast<void (MyString::*)(int, int, const MyString&, int)>(&MyString::replace));
    cls.def("replace", static_cast<void (MyString::*)(int, int, const char*, int, int)>(&MyString::replace));
    cls.def("replace", static_cast<void (MyString::*)(int, int, const MyString&, int, int)>(&MyString::replace));

    // substr(index) -> substr(index, count) -> конструктор MyString(const char*, int).
    // Возвращается новый объект, а не ссылка на исходную строку.
    cls.def("substr", static_cast<MyString (MyString::*)(int) const>(&MyString::substr));
    cls.def("substr", static_cast<MyString (MyString::*)(int, int) const>(&MyString::substr));

    // operator+ копирует левый объект и вызывает append(); operator+= меняет сам объект.
    cls.def("__add__", static_cast<MyString (MyString::*)(const char*) const>(&MyString::operator+),
            py::is_operator());
    cls.def("__add__", static_cast<MyString (MyString::*)(const MyString&) const>(&MyString::operator+),
            py::is_operator());
    cls.def("__iadd__", static_cast<MyString& (MyString::*)(const char*)>(&MyString::operator+=),
            py::return_value_policy::reference_internal, py::is_operator());
    cls.def("__iadd__", static_cast<MyString& (MyString::*)(const MyString&)>(&MyString::operator+=),
            py::return_value_policy::reference_internal, py::is_operator());

    // Чтение символа -> operator[] const -> check_index().
    cls.def("__getitem__", static_cast<const char& (MyString::*)(int) const>(&MyString::operator[]));
    // Запись -> operator[] -> check_index(). Не допускаем '\0': это конец C-строки.
    cls.def("__setitem__", [](MyString& self, int index, const std::string& value) {
        if (value.size() != 1 || value[0] == '\0') {
            throw std::invalid_argument("expected one non-null byte");
        }
        self[index] = value[0];
    });

    // Все сравнения используют compare(); он сравнивает символы через operator[].
    cls.def("compare", &MyString::compare);
    cls.def("__eq__", &MyString::operator==, py::is_operator());
    cls.def("__ne__", &MyString::operator!=, py::is_operator());
    cls.def("__lt__", &MyString::operator<, py::is_operator());
    cls.def("__le__", &MyString::operator<=, py::is_operator());
    cls.def("__gt__", &MyString::operator>, py::is_operator());
    cls.def("__ge__", &MyString::operator>=, py::is_operator());

    // find(source) -> find(source, 0) -> find_chars(); результат -1, если совпадения нет.
    cls.def("find", static_cast<int (MyString::*)(const char*) const>(&MyString::find));
    cls.def("find", static_cast<int (MyString::*)(const MyString&) const>(&MyString::find));
    cls.def("find", static_cast<int (MyString::*)(const char*, int) const>(&MyString::find));
    cls.def("find", static_cast<int (MyString::*)(const MyString&, int) const>(&MyString::find));
}
