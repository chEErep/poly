"""Лабораторная № 1, вариант 60: три способа найти НОД и коэффициенты Безу.

Запуск: py -3 lab1.py
Полный журнал: py -3 lab1.py --full
Измерение времени: py -3 lab1.py --benchmark
Дополнительная проверка: py -3 lab1.py --check
"""

import sys
from math import gcd
from random import Random
from statistics import median
from timeit import Timer


# Числа из файла варианта 60. Порядок a и b здесь сохранён как в задании.
PAIRS = (
    (16735888242474090869, 35015242647626873167),
    (1653434949056268781306577859232861185133,
     450012011238885459609665876552984226571),
    (40279180034337470637152813274868796238885816532239877120941653139958895345319197,
     10724704845539293246448075729372138572882194722686629655115681350140323676757681),
)


def prepare(a, b):
    """Проверить вход и временно поставить большее число первым."""
    # bool в Python считается подклассом int, но True как число нам не нужен.
    if type(a) is not int or type(b) is not int:
        raise TypeError("a и b должны быть целыми числами")
    if a <= 0 or b <= 0:
        raise ValueError("a и b должны быть положительными")
    if a >= b:
        return a, b, False
    return b, a, True


def add_step(steps, action, r, x, y, swapped):
    """Записать шаг так, чтобы всегда выполнялось исходное a*x + b*y = r."""
    if steps is None:  # Во время замера скорости журнал не создаётся.
        return
    if swapped:
        x, y = y, x
    steps.append((len(steps), action, r, x, y))


def euclid(a, b, save_steps=True):
    """Обычный расширенный алгоритм Евклида."""
    first, second, swapped = prepare(a, b)
    r0, r1 = first, second

    # Каждый остаток храним вместе с его представлением через first и second:
    # r0 = x0*first + y0*second, r1 = x1*first + y1*second.
    x0, y0 = 1, 0
    x1, y1 = 0, 1
    steps = [] if save_steps else None
    add_step(steps, "начало A", r0, x0, y0, swapped)
    add_step(steps, "начало B", r1, x1, y1, swapped)
    operations = 0

    while r1 != 0:
        q, r2 = divmod(r0, r1)  # r0 = q*r1 + r2.
        x2 = x0 - q * x1
        y2 = y0 - q * y1
        add_step(steps, f"q={q}", r2, x2, y2, swapped)
        operations += 1

        # Пара (r1, r2) станет исходной для следующего шага.
        r0, r1 = r1, r2
        x0, x1 = x1, x2
        y0, y1 = y1, y2

    # Перестановка нужна только внутри алгоритма. Ответ относится к исходным a,b.
    if swapped:
        x0, y0 = y0, x0
    return r0, x0, y0, operations, steps or []


def least_remainder(a, b, save_steps=True):
    """Расширенный алгоритм с остатком наименьшего абсолютного значения."""
    first, second, swapped = prepare(a, b)
    r0, r1 = first, second
    x0, y0 = 1, 0
    x1, y1 = 0, 1
    steps = [] if save_steps else None
    add_step(steps, "начало A", r0, x0, y0, swapped)
    add_step(steps, "начало B", r1, x1, y1, swapped)
    operations = 0

    while r1 != 0:
        q, ordinary_remainder = divmod(r0, r1)
        # Если обычный остаток больше половины делителя по модулю,
        # берём соседнее частное. Новый остаток может стать отрицательным.
        if 2 * abs(ordinary_remainder) > abs(r1):
            q += 1
        r2 = r0 - q * r1
        x2 = x0 - q * x1
        y2 = y0 - q * y1
        add_step(steps, f"q={q}", r2, x2, y2, swapped)
        operations += 1
        r0, r1 = r1, r2
        x0, x1 = x1, x2
        y0, y1 = y1, y2

    # НОД по определению неотрицателен; здесь входы положительны, значит НОД > 0.
    if r0 < 0:
        r0, x0, y0 = -r0, -x0, -y0
        add_step(steps, "смена знака НОД", r0, x0, y0, swapped)
    if swapped:
        x0, y0 = y0, x0
    return r0, x0, y0, operations, steps or []


def binary_euclid(a, b, save_steps=True):
    """Расширенный бинарный алгоритм Евклида: деление на 2 и вычитание."""
    first, second, swapped = prepare(a, b)
    common = 1
    while first % 2 == 0 and second % 2 == 0:
        first //= 2
        second //= 2
        common *= 2

    # u и v представлены через уменьшенные first и second:
    # u = ux*first + uy*second, v = vx*first + vy*second.
    u, v = first, second
    ux, uy = 1, 0
    vx, vy = 0, 1
    steps = [] if save_steps else None
    add_step(steps, "начало u", common * u, ux, uy, swapped)
    add_step(steps, "начало v", common * v, vx, vy, swapped)
    operations = 0

    while u != 0:
        while u % 2 == 0:
            u //= 2
            if ux % 2 == 0 and uy % 2 == 0:
                ux //= 2
                uy //= 2
            else:
                # Добавляем к представлению ноль:
                # second*first - first*second = 0.
                # После этого оба коэффициента можно разделить на 2.
                ux = (ux + second) // 2
                uy = (uy - first) // 2
            operations += 1
            add_step(steps, "u/2", common * u, ux, uy, swapped)

        while v % 2 == 0:
            v //= 2
            if vx % 2 == 0 and vy % 2 == 0:
                vx //= 2
                vy //= 2
            else:
                vx = (vx + second) // 2
                vy = (vy - first) // 2
            operations += 1
            add_step(steps, "v/2", common * v, vx, vy, swapped)

        if u >= v:
            u -= v
            ux -= vx
            uy -= vy
            action, r, x, y = "u-v", u, ux, uy
        else:
            v -= u
            vx -= ux
            vy -= uy
            action, r, x, y = "v-u", v, vx, vy
        operations += 1
        add_step(steps, action, common * r, x, y, swapped)

    add_step(steps, "результат v", common * v, vx, vy, swapped)
    if swapped:
        vx, vy = vy, vx
    return common * v, vx, vy, operations, steps or []


METHODS = (
    ("Расширенный алгоритм Евклида", euclid),
    ("Расширенный бинарный алгоритм Евклида", binary_euclid),
    ("Расширенный алгоритм Евклида с усечёнными остатками", least_remainder),
)


def benchmark(method, a, b):
    """Медиана времени одного вызова без записи шагов, в микросекундах."""
    timer = Timer(lambda: method(a, b, False))
    timer.timeit(number=10)  # Короткий прогрев перед измерением.
    count = 10
    while count < 100_000 and timer.timeit(number=count) < 0.05:
        count *= 10
    measurements = timer.repeat(repeat=7, number=count)
    return count, median(measurements) * 1_000_000 / count


def check():
    """Проверить результаты на варианте и дополнительных небольших числах."""
    examples = list(PAIRS) + [(b, a) for a, b in PAIRS]
    examples += [(1, 1), (1, 17), (8, 12), (12, 8), (42, 42)]
    random = Random(60)  # Фиксированное зерно: набор тестов повторяемый.
    examples += [(random.randint(1, 10**10), random.randint(1, 10**10))
                 for _ in range(100)]

    for a, b in examples:
        for name, method in METHODS:
            d, x, y, _, steps = method(a, b)
            if d != gcd(a, b) or a * x + b * y != d:
                raise AssertionError(f"Неверный ответ: {name}, a={a}, b={b}")
            for number, _, r, sx, sy in steps:
                if a * sx + b * sy != r:
                    raise AssertionError(f"Неверный шаг {number}: {name}")
            if method is least_remainder:
                for i in range(2, len(steps)):
                    # На каждом шаге деления новый остаток не больше
                    # половины предыдущего делителя по модулю.
                    if (steps[i][1].startswith("q=")
                            and 2 * abs(steps[i][2]) > abs(steps[i - 1][2])):
                        raise AssertionError("Усечённый остаток слишком велик")

    for bad_a, bad_b in ((0, 1), (-1, 2), (True, 3), (1.5, 2)):
        for _, method in METHODS:
            try:
                method(bad_a, bad_b)
            except (TypeError, ValueError):
                pass  # Некорректные входные данные должны отклоняться.
            else:
                raise AssertionError("Некорректный ввод принят")
    print(f"Проверено {len(examples)} пар для каждого из трёх методов: ошибок нет.")


def main():
    allowed = {"--full", "--benchmark", "--check"}
    unknown = set(sys.argv[1:]) - allowed
    if unknown:
        raise SystemExit(f"Неизвестный параметр: {', '.join(sorted(unknown))}")
    if "--check" in sys.argv:
        check()
        return

    for pair_number, (a, b) in enumerate(PAIRS, start=1):
        print(f"\nПАРА {pair_number}\na = {a}\nb = {b}")
        for name, method in METHODS:
            d, x, y, operations, steps = method(a, b)
            if d != gcd(a, b) or a * x + b * y != d:
                raise AssertionError(f"Ошибка результата: пара {pair_number}, {name}")
            print(f"\n{name}")
            print(f"НОД = {d}; x = {x}; y = {y}")
            print(f"Проверка: a*x + b*y = {a * x + b * y}")
            print(f"Основных операций: {operations}; строк журнала: {len(steps)}")
            print("№ | операция | r | x | y")
            shown = steps
            if "--full" not in sys.argv and len(steps) > 20:
                shown = steps[:5] + [None] + steps[-5:]
            for step in shown:
                if step is None:
                    print("... (средние строки опущены)")
                else:
                    number, action, r, sx, sy = step
                    print(f"{number} | {action} | {r} | {sx} | {sy}")

        if "--benchmark" in sys.argv:
            print("\nВремя без записи журнала (медиана 7 серий):")
            for name, method in METHODS:
                count, microseconds = benchmark(method, a, b)
                print(f"{name}: {microseconds:.3f} мкс/вызов; {count} вызовов в серии")


if __name__ == "__main__":
    main()
