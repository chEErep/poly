# Граф вызовов MyString

Граф построен по MyString.cpp из коммита d50ab35. SHA-256 исходного файла: `b7b24fbaf72203308cf4abebf9a69fc055bd44cdb16ea8d92b3369f0d061e66a`.

## Какие файлы открыть

- MyString_families.drawio — редактируемый граф по семействам методов для diagrams.net / draw.io.
- MyString_families.svg — векторное изображение для отчёта; построено Graphviz из того же набора связей.
- MyString_families.dot — исходные данные сгруппированного графа для Graphviz.
- MyString_methods.dot — полный граф методов: 78 различных сигнатур, 106 прямых связей.

## Как читать

Стрелка A → B означает: хотя бы одна перегрузка A вызывает хотя бы одну перегрузку B.
Это не означает, что все перегрузки A вызывают B, что вызов обязательно выполнится при каждом запуске или что показан порядок выполнения.
В один блок объединены одноимённые перегрузки, как в образце одногруппника. Серым обозначены шесть private-функций.
На основном рисунке 31 семейств и 41 связей между семействами.
Петли внутри семейства скрыты; эти связи перечислены ниже и сохранены в полном DOT.
Делегирование из find(source) в find(source, 0) не является рекурсией: это разные перегрузки.
Методы без стрелок тоже присутствуют: они работают с полями или вызывают только внешние функции.

## Как получены связи

Использованы Clang 22.1.8, LLVM opt 22.1.8, llvm-cxxfilt, предоставленный dot_unmangler.py и Graphviz 16.1.0.
Из MyString.cpp получен LLVM IR с параметрами `-std=c++17 -O0 -S -emit-llvm`.
Затем запущен `opt -passes=dot-callgraph -disable-output MyString.ll` и dot_unmangler.py с параметром --cxxfilt.
Оптимизация отключена, чтобы сохранить явные вызовы методов. Pybind11-обёртка и main.cpp в этот граф не включены.

В этой сборке Clang создаёт девять LLVM aliases для конструкторов и деструктора (например, C1 → C2).
Обычный dot-callgraph не показывает часть вызовов через эти псевдонимы.
В промежуточной копии IR ссылки на псевдонимы заменены ссылками на те же функции, после чего запущен `opt -passes=verify,dot-callgraph`.
Исходный C++-код не изменён. dot_unmangler.py успешно применён также к этому графу.
Результат независимо сверен с инструкциями call и invoke в исходном IR с разрешением псевдонимов: совпадают все 106 связей между методами MyString.
Библиотечные вызовы strlen, new/delete, std::string, обработчики исключений и LLVM intrinsics исключены из рисунка класса.
Конструкторы и деструктор MyString сохранены: они участвуют в создании и очистке временных объектов.
Пунктир MyString → ~MyString показывает сгенерированную очистку при исключении в делегирующем конструкторе.

## Почему числа отличаются от 66 пунктов таблицы

Таблица задания и число C++-сигнатур считаются по-разному. В пункте 12 SOURCE_STR соответствует трём перегрузкам присваивания; в пунктах 58–60 указаны пары операторов сравнения. Кроме того, есть const-перегрузка operator[] и шесть private-функций.
В коде 72 public-сигнатуры и 6 private-сигнатур, всего 78.
Восемь конструкторов объединены в семейство MyString::MyString; деструктор — отдельный блок.
Подпись «перегрузки» показывает количество сигнатур в текущем коде, а не число пунктов задания.

| Семейство | Количество сигнатур |
|---|---:|
| `MyString::MyString` | 8 |
| `MyString::append` | 10 |
| `MyString::c_str` | 1 |
| `MyString::capacity` | 1 |
| `MyString::check_index` | 1 |
| `MyString::clear` | 1 |
| `MyString::compare` | 1 |
| `MyString::empty` | 1 |
| `MyString::erase` | 1 |
| `MyString::find` | 6 |
| `MyString::find_chars` | 1 |
| `MyString::insert` | 10 |
| `MyString::insert_chars` | 1 |
| `MyString::insert_prefix` | 1 |
| `MyString::insert_slice` | 1 |
| `MyString::operator!=` | 1 |
| `MyString::operator+` | 3 |
| `MyString::operator+=` | 3 |
| `MyString::operator<` | 1 |
| `MyString::operator<=` | 1 |
| `MyString::operator=` | 4 |
| `MyString::operator==` | 1 |
| `MyString::operator>` | 1 |
| `MyString::operator>=` | 1 |
| `MyString::operator[]` | 2 |
| `MyString::replace` | 9 |
| `MyString::shrink_to_fit` | 1 |
| `MyString::size` | 1 |
| `MyString::substr` | 2 |
| `MyString::swap` | 1 |
| `MyString::~MyString` | 1 |

## Скрытые связи внутри одного семейства

- `MyString::MyString(MyString const&)` → `MyString::MyString(char const*)`
- `MyString::MyString(MyString const&, int)` → `MyString::MyString(char const*, int)`
- `MyString::MyString(char const*)` → `MyString::MyString()`
- `MyString::MyString(char const*, int)` → `MyString::MyString()`
- `MyString::MyString(int, char)` → `MyString::MyString()`
- `MyString::MyString(std::string const&)` → `MyString::MyString(char const*)`
- `MyString::MyString(std::string const&, int)` → `MyString::MyString(char const*, int)`
- `MyString::find(MyString const&) const` → `MyString::find(MyString const&, int) const`
- `MyString::find(char const*) const` → `MyString::find(char const*, int) const`
- `MyString::find(std::string const&) const` → `MyString::find(std::string const&, int) const`
- `MyString::operator=(MyString const&)` → `MyString::operator=(char const*)`
- `MyString::operator=(char)` → `MyString::operator=(char const*)`
- `MyString::operator=(std::string const&)` → `MyString::operator=(char const*)`
- `MyString::replace(int, int, MyString const&)` → `MyString::replace(int, int, char const*)`
- `MyString::replace(int, int, MyString const&, int)` → `MyString::replace(int, int, char const*, int)`
- `MyString::replace(int, int, MyString const&, int, int)` → `MyString::replace(int, int, char const*, int, int)`
- `MyString::replace(int, int, char const*, int)` → `MyString::replace(int, int, char const*)`
- `MyString::replace(int, int, char const*, int, int)` → `MyString::replace(int, int, char const*)`
- `MyString::replace(int, int, std::string const&)` → `MyString::replace(int, int, char const*)`
- `MyString::replace(int, int, std::string const&, int)` → `MyString::replace(int, int, char const*, int)`
- `MyString::replace(int, int, std::string const&, int, int)` → `MyString::replace(int, int, char const*, int, int)`
- `MyString::substr(int) const` → `MyString::substr(int, int) const`

## Сравнение с декомпозицией

Главная цепочка добавления текста подтверждена: operator+ / operator+= → append → insert.
Перегрузки insert сводятся к insert_chars напрямую или через insert_slice → insert_prefix → insert_chars.
В replace общий код используется через erase и insert; вариант с фрагментом источника также вызывает append при подготовке временной строки.
Все шесть операторов сравнения используют compare; compare вызывает const operator[], а тот — check_index.
find сводится к find_chars. Получение размера и данных источника отражено стрелками к size и c_str.
Присваивание сохраняет достаточную ёмкость существующего буфера; swap вызывается только в ветке нехватки места.
Функции MakeRoom / memcopy из примера преподавателя в этой реализации не объявлены: их работу выполняет insert_chars.

## Что сказать на защите

«Я получил граф вызовов из LLVM IR своей реализации, преобразовал имена функций и оставил методы MyString.
Для читаемости объединил перегрузки по имени. Стрелка идёт от вызывающего метода к вызываемому.
Например, append использует insert, а общая вставка выполняется в insert_chars; это подтверждает декомпозицию.
Конструкторы и деструктор видны из-за работы с временными объектами. Скрытые связи между перегрузками сохранены в полном графе».

Технические промежуточные файлы находятся в игнорируемой папке cmake-build-clang64/callgraph.
После изменения MyString.cpp граф нужно построить заново; редактирование только картинки не обновляет фактические связи.
