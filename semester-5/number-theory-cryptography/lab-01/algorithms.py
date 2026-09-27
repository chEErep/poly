"""Три расширенных алгоритма вычисления НОД для положительных целых чисел.

Каждая строка журнала удовлетворяет равенству a*x + b*y = r, где a и b —
аргументы в исходном порядке. В бинарном алгоритме r — изменённое после
операции значение одного из двух рабочих остатков с учётом общего множителя.
"""

from dataclasses import dataclass


@dataclass(frozen=True)
class Step:
    number: int
    operation: str
    remainder: int
    x: int
    y: int


@dataclass(frozen=True)
class Result:
    gcd: int
    x: int
    y: int
    iterations: int
    steps: tuple[Step, ...]


def _check_input(a: int, b: int) -> None:
    if type(a) is not int or type(b) is not int:
        raise TypeError("a и b должны быть целыми числами")
    if a <= 0 or b <= 0:
        raise ValueError("a и b должны быть положительными")


def _ordered(a: int, b: int) -> tuple[int, int, bool]:
    return (a, b, False) if a >= b else (b, a, True)


def _original_coefficients(x: int, y: int, swapped: bool) -> tuple[int, int]:
    return (y, x) if swapped else (x, y)


def extended_euclid(a: int, b: int, *, trace: bool = False) -> Result:
    """Обычные неотрицательные остатки, как в лекции об алгоритме Евклида."""
    _check_input(a, b)
    first, second, swapped = _ordered(a, b)
    r0, r1 = first, second
    x0, x1 = 1, 0
    y0, y1 = 0, 1
    steps: list[Step] = []
    if trace:
        for label, r, x, y in (("начало A", r0, x0, y0), ("начало B", r1, x1, y1)):
            ox, oy = _original_coefficients(x, y, swapped)
            steps.append(Step(len(steps), label, r, ox, oy))

    iterations = 0
    while r1:
        q, r2 = divmod(r0, r1)
        x2 = x0 - q * x1
        y2 = y0 - q * y1
        iterations += 1
        if trace:
            ox, oy = _original_coefficients(x2, y2, swapped)
            steps.append(Step(len(steps), f"q={q}", r2, ox, oy))
        r0, r1 = r1, r2
        x0, x1 = x1, x2
        y0, y1 = y1, y2

    x, y = _original_coefficients(x0, y0, swapped)
    return Result(r0, x, y, iterations, tuple(steps))


def extended_least_remainder(a: int, b: int, *, trace: bool = False) -> Result:
    """Алгоритм Евклида с остатком наименьшего абсолютного значения.

    При равных по модулю вариантах выбирается обычный остаток. Отрицательные
    остатки и коэффициенты сохраняются со знаком. Финальный НОД положителен.
    """
    _check_input(a, b)
    first, second, swapped = _ordered(a, b)
    r0, r1 = first, second
    x0, x1 = 1, 0
    y0, y1 = 0, 1
    steps: list[Step] = []
    if trace:
        for label, r, x, y in (("начало A", r0, x0, y0), ("начало B", r1, x1, y1)):
            ox, oy = _original_coefficients(x, y, swapped)
            steps.append(Step(len(steps), label, r, ox, oy))

    iterations = 0
    while r1:
        q, remainder = divmod(r0, r1)
        if 2 * abs(remainder) > abs(r1):
            q += 1
        r2 = r0 - q * r1
        x2 = x0 - q * x1
        y2 = y0 - q * y1
        iterations += 1
        if trace:
            ox, oy = _original_coefficients(x2, y2, swapped)
            steps.append(Step(len(steps), f"q={q}", r2, ox, oy))
        r0, r1 = r1, r2
        x0, x1 = x1, x2
        y0, y1 = y1, y2

    if r0 < 0:
        r0, x0, y0 = -r0, -x0, -y0
        if trace:
            ox, oy = _original_coefficients(x0, y0, swapped)
            steps.append(Step(len(steps), "смена знака НОД", r0, ox, oy))
    x, y = _original_coefficients(x0, y0, swapped)
    return Result(r0, x, y, iterations, tuple(steps))


def extended_binary_euclid(a: int, b: int, *, trace: bool = False) -> Result:
    """Расширенный бинарный алгоритм Евклида без деления с остатком."""
    _check_input(a, b)
    first, second, swapped = _ordered(a, b)
    reduced_a, reduced_b = first, second
    common_power = 1
    while reduced_a % 2 == 0 and reduced_b % 2 == 0:
        reduced_a //= 2
        reduced_b //= 2
        common_power *= 2

    u, v = reduced_a, reduced_b
    A, B = 1, 0  # u = A*reduced_a + B*reduced_b
    C, D = 0, 1  # v = C*reduced_a + D*reduced_b
    steps: list[Step] = []

    def log(operation: str, value: int, x: int, y: int) -> None:
        ox, oy = _original_coefficients(x, y, swapped)
        steps.append(Step(len(steps), operation, common_power * value, ox, oy))

    if trace:
        log("начало u", u, A, B)
        log("начало v", v, C, D)

    iterations = 0
    while u:
        while u % 2 == 0:
            u //= 2
            if A % 2 == 0 and B % 2 == 0:
                A //= 2
                B //= 2
            else:
                A = (A + reduced_b) // 2
                B = (B - reduced_a) // 2
            iterations += 1
            if trace:
                log("u/2", u, A, B)

        while v % 2 == 0:
            v //= 2
            if C % 2 == 0 and D % 2 == 0:
                C //= 2
                D //= 2
            else:
                C = (C + reduced_b) // 2
                D = (D - reduced_a) // 2
            iterations += 1
            if trace:
                log("v/2", v, C, D)

        if u >= v:
            u -= v
            A -= C
            B -= D
            iterations += 1
            if trace:
                log("u-v", u, A, B)
        else:
            v -= u
            C -= A
            D -= B
            iterations += 1
            if trace:
                log("v-u", v, C, D)

    gcd = common_power * v
    x, y = _original_coefficients(C, D, swapped)
    if trace:
        log("результат v", v, C, D)
    return Result(gcd, x, y, iterations, tuple(steps))


ALGORITHMS = (
    ("Расширенный алгоритм Евклида", extended_euclid),
    ("Расширенный бинарный алгоритм Евклида", extended_binary_euclid),
    ("Расширенный алгоритм Евклида с усечёнными остатками", extended_least_remainder),
)
