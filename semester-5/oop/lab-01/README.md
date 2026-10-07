# ЛР 1 — MyString

Лабораторная работа по объектно-ориентированному программированию: собственный строковый класс на C++, алгоритмическая декомпозиция, граф вызовов и Python-обёртка на pybind11.

Строка хранится в динамическом массиве `char`. `std::string` поддерживается как тип входного аргумента; внутреннее хранение и алгоритмы реализованы самостоятельно.

## Возможности

- Создание пустой строки, строки из символов, префикса и глубокой копии другого объекта.
- Присваивание, вставка, добавление, удаление, замена и получение подстроки.
- Конкатенация через `+` и `+=`, индексация, сравнение и поиск.
- Получение размера и ёмкости, очистка и `shrink_to_fit()`.
- Проверка некорректных аргументов с исключениями C++.
- Использование класса из Python, включая `len`, `str`, индексацию и операторы сравнения.

В коде 72 публичные сигнатуры и 6 внутренних помощников. Таблица методички содержит 66 пунктов: часть пунктов соответствует нескольким перегрузкам.

`size()` не включает завершающий нуль, а `capacity()` включает место под него. Присваивание короткой строки и `clear()` сохраняют достаточную ёмкость; намеренное уменьшение выполняет `shrink_to_fit()`.

## Файлы

| Путь | Содержание |
|---|---|
| [MyString.h](MyString.h) | Интерфейс класса и поля |
| [MyString.cpp](MyString.cpp) | Реализация методов и общих алгоритмов |
| [main.cpp](main.cpp) | Собственные проверки C++ с `assert` |
| [MyString_wrapper.cpp](MyString_wrapper.cpp) | Регистрация класса и методов в pybind11 |
| [test_wrapper.py](test_wrapper.py) | Собственные проверки Python-интерфейса |
| [CMakeLists.txt](CMakeLists.txt) | Сборка программы и необязательной обёртки |
| [MyString_decomposition.drawio](MyString_decomposition.drawio) | Схема алгоритмической декомпозиции |
| [graphs/](graphs/) | Графы фактических вызовов: DOT, SVG и draw.io |
| [report/defense-guide.md](report/defense-guide.md) | Теория, разбор кода и подготовка к защите |
| [materials/](materials/) | Методичка, правила оформления и архивы курса |
| [mystring_h_path.txt](mystring_h_path.txt) | Путь к заголовку для учебной проверяющей системы |

## Сборка и проверка C++

Нужны CMake 3.20 или новее, Ninja и Clang с поддержкой C++17. Команды выполняются из папки этой лабораторной. `cmake`, `ninja` и `clang++` должны быть доступны в `PATH`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Запуск в PowerShell на Windows:

```powershell
.\build\oop_lab_01.exe
```

На Linux:

```sh
./build/oop_lab_01
```

Ожидаемый результат: `All checks passed`. Сборка `Debug` сохраняет проверки `assert`; при определённом `NDEBUG` они отключаются.

В CLion открой эту папку как CMake-проект, выбери Clang в Toolchains, профиль Debug и цель `oop_lab_01`. Для обычной программы опция `BUILD_PYTHON_WRAPPER` выключена по умолчанию.

Пример работы класса:

```cpp
#include <iostream>
#include "MyString.h"

int main() {
    MyString text("cat");
    text.append("!");
    std::cout << text.c_str() << ' ' << text.size() << '\n';
    // cat! 4
}
```

## Python-обёртка

Дополнительно нужны Python с файлами для разработки и pybind11. Сборка обёртки включается опцией `BUILD_PYTHON_WRAPPER=ON`; модуль называется `mystring`.

Для обычного Python pybind11 можно установить командой `python -m pip install pybind11`. При использовании MSYS2 зависимости устанавливаются для выбранной среды. В этой работе проверена среда **MSYS2 CLANG64**: Clang 22.1.8, Python 3.14.7 и pybind11 3.1.0.

В Windows важно использовать Python из той же среды, под которую собирается модуль. Для сборки CLANG64 выбери `python.exe` из `msys64/clang64/bin`, включая интерпретатор в PyCharm. Совпадение номера версии с обычным Windows Python само по себе не означает совместимость бинарного модуля.

В PowerShell, когда нужный Python и Clang доступны в `PATH`:

```powershell
$pythonExe = (Get-Command python).Source
$pybind11Dir = & $pythonExe -m pybind11 --cmakedir

cmake -S . -B build-python -G Ninja -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_CXX_COMPILER=clang++ `
    -DBUILD_PYTHON_WRAPPER=ON `
    "-DPython_EXECUTABLE=$pythonExe" `
    "-Dpybind11_DIR=$pybind11Dir"
cmake --build build-python

$env:PYTHONPATH = (Resolve-Path .\build-python).Path
& $pythonExe .\test_wrapper.py
```

Ожидаемый результат: `Все проверки обёртки прошли`. Этот файл запускается обычным Python и использует `assert`; режим `python -O` отключает эти проверки.

Для Linux при установленных зависимостях:

```sh
cmake -S . -B build-python -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=clang++ -DBUILD_PYTHON_WRAPPER=ON \
    -DPython_EXECUTABLE="$(command -v python3)" \
    -Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)"
cmake --build build-python
PYTHONPATH="$PWD/build-python" python3 test_wrapper.py
```

Пример после сборки и настройки пути импорта:

```python
from mystring import MyString

text = MyString("cat")
text.append("!")
assert str(text) == "cat!"
assert len(text) == 4

copy = MyString(text)
copy[0] = "B"
assert str(copy) == "Bat!"
assert str(text) == "cat!"
```

Python-присваивание `a = b` связывает имена с объектом. Для вызова C++-оператора присваивания в обёртке доступны `assign()` и `assign_char()`.

Настройка модуля использует `find_package(pybind11)` и `pybind11_add_module`: [документация сборки pybind11](https://pybind11.readthedocs.io/en/stable/compiling.html).

## Декомпозиция и граф вызовов

Основные цепочки переиспользования:

```text
operator+= → append → insert → insert_chars
insert с фрагментом → insert_slice → insert_prefix → insert_chars
операторы сравнения → compare → operator[] const → check_index
find → find_chars
```

Граф получен из LLVM IR без оптимизации. Полная версия содержит 78 сигнатур и 106 прямых связей. В укрупнённом рисунке перегрузки объединены в 31 семейство с 41 связью между семействами. Стрелка направлена от вызывающего метода к вызываемому.

- [Редактируемый граф draw.io](graphs/MyString_families.drawio).
- [Изображение графа SVG](graphs/MyString_families.svg).
- [Полный граф с сигнатурами DOT](graphs/MyString_methods.dot).
- [Подробности построения и проверки](graphs/README.md).

Пунктир от конструкторов к деструктору обозначает очистку при исключении в теле делегирующего конструктора. Эта связь присутствует в LLVM IR и не означает уничтожение объекта при каждом успешном создании.

В [архиве курса](materials/course-archives/build_call_graph-master.tar.gz) находятся исходные `build_cg.sh` и `dot_unmangler.py`. При изменении реализации граф нужно построить заново; сохранённые рисунки относятся к текущему коду класса.

## Проверки и статус

Обязательная часть лабораторной сдана: класс, декомпозиция, граф и обёртка. Дополнительные задания в этой версии не реализованы.

В дополнение к собственным проверкам предоставленный комплект C++ проверен локально: **65 Success, 0 Error**. Учебная проверяющая система для версии GitLab `d50ab35` сообщила успешное прохождение тестов MyString, Valgrind, сборки Python-обёртки и pytest. Это результат учебной проверки; GitHub Actions в этой папке не настроены.

## Ограничения

- Индексация и размер определяются в байтах. Работа с Unicode-символами как с отдельными элементами не реализована.
- Поддержка встроенного `\0` неполная: операции через C-строку и операции с явной длиной могут вести себя по-разному.
- Вставка выделяет новый массив даже при достаточной ёмкости. Многократное добавление небольших фрагментов может быть затратным.
- `replace` выполняется через удаление и вставку; откат к исходной строке при отказе последующей вставки не гарантируется.
- Перемещающие операции и итераторы не реализованы.

Теория, объяснение указателей, ссылок и `const`, разбор pybind11 и сценарий защиты приведены в [файле подготовки](report/defense-guide.md).
