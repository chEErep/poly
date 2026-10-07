from mystring import MyString


# Конструкторы: обычная строка, часть строки, повторение символа и копирование.
assert MyString().c_str() == ""
assert MyString("hello").c_str() == "hello"
assert MyString("hello", 3).c_str() == "hel"
assert MyString(3, "B").c_str() == "BBB"
source = MyString("hello")
assert MyString(source).c_str() == "hello"
assert MyString(source, 2).c_str() == "he"
assert str(source) == "hello"
assert len(source) == source.size() == 5
assert not source.empty()

# Присваивание в Python вызываем через assign(): a = b только меняет ссылку.
target = MyString("old")
assert target.assign("new") is target
assert target.c_str() == "new"
assert target.assign(source) is target
assert target.c_str() == "hello"
assert target.assign_char("!") is target
assert target.c_str() == "!"

# Вставка: символы, строка, MyString, префикс и фрагмент источника.
s = MyString("ab")
s.insert(1, 2, "X")
assert s.c_str() == "aXXb"
s.insert(0, "c")
s.insert(s.size(), MyString("d"))
assert s.c_str() == "caXXbd"
s.insert(0, "123", 2)
s.insert(s.size(), MyString("456"), 2)
assert s.c_str() == "12caXXbd45"
s.insert(2, "ABC", 1, 2)
s.insert(s.size(), MyString("wxyz"), 1, 2)
assert s.c_str() == "12BCcaXXbd45xy"

# Добавление в конец использует те же варианты источника, что insert().
s = MyString("a")
s.append(2, "!")
s.append("b")
s.append(MyString("c"))
s.append("def", 2)
s.append(MyString("ghi"), 2)
s.append("jkl", 1, 2)
s.append(MyString("mno"), 1, 2)
assert s.c_str() == "a!!bcdeghklno"

# Удаление, замена, сохранение ёмкости и независимый результат substr().
s = MyString("Hello World")
old_capacity = s.capacity()
s.erase(5, 1)
assert s.c_str() == "HelloWorld" and s.capacity() == old_capacity
s.replace(0, 5, "A")
assert s.c_str() == "AWorld" and s.capacity() == old_capacity
s.replace(0, 1, MyString("Hi"))
assert s.c_str() == "HiWorld"
s.replace(0, 2, "test", 2)
assert s.c_str() == "teWorld"
s.replace(0, 2, MyString("abcd"), 3)
assert s.c_str() == "abcWorld"
s.replace(0, 3, "12345", 1, 2)
assert s.c_str() == "23World"
s.replace(0, 2, MyString("uvwxyz"), 2, 2)
assert s.c_str() == "wxWorld"
tail = s.substr(2)
part = s.substr(2, 3)
assert tail.c_str() == "World" and part.c_str() == "Wor"
s.clear()
assert s.empty() and s.size() == 0 and tail.c_str() == "World"
assert s.capacity() >= old_capacity
s.shrink_to_fit()
assert s.capacity() == 0

# + создаёт новый объект, += меняет существующий, в том числе при s += s.
left = MyString("ab")
assert (left + "c").c_str() == "abc"
assert (left + MyString("d")).c_str() == "abd"
assert left.c_str() == "ab"
left += "c"
left += MyString("d")
left += left
assert left.c_str() == "abcdabcd"

# Индексация, сравнение и поиск вызывают соответствующие методы C++.
s = MyString("cat")
assert s[1] == "a"
s[1] = "u"
assert s.c_str() == "cut"
assert MyString("a").compare(MyString("b")) < 0
assert MyString("a") == MyString("a")
assert MyString("a") != MyString("b")
assert MyString("a") < MyString("b")
assert MyString("a") <= MyString("a")
assert MyString("b") > MyString("a")
assert MyString("b") >= MyString("b")
s = MyString("abc abc")
assert s.find("abc") == 0
assert s.find(MyString("abc"), 1) == 4
assert s.find("xyz") == -1

# Неверные аргументы должны доходить до Python как исключения.
for action, error in (
    (lambda: s[100], IndexError),
    (lambda: s.insert(-1, "x"), IndexError),
    (lambda: s.find("a", -1), IndexError),
    (lambda: s.__setitem__(0, "two"), ValueError),
    (lambda: s.__setitem__(0, "\0"), ValueError),
):
    try:
        action()
    except error:
        pass
    else:
        raise AssertionError("Ожидалось исключение " + error.__name__)

print("Все проверки обёртки прошли")
