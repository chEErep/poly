"""Расчёт варианта 60 и подготовка данных для отчёта.

Запуск: python main.py --benchmark --output results/variant60-results.txt
"""

import argparse
from math import gcd as reference_gcd
from pathlib import Path
from statistics import median
from timeit import Timer

from algorithms import ALGORITHMS, Result, Step
from variant60 import PAIRS


def visible_steps(steps: tuple[Step, ...], full: bool) -> list[Step | None]:
    if full or len(steps) <= 20:
        return list(steps)
    return [*steps[:5], None, *steps[-5:]]


def benchmark(function, a: int, b: int) -> tuple[int, float]:
    """Возвращает число вызовов в серии и медиану времени одного вызова (мкс)."""
    timer = Timer(lambda: function(a, b))
    timer.timeit(number=10)  # прогрев
    number = 10
    while number < 100_000 and timer.timeit(number=number) < 0.05:
        number *= 10
    samples = timer.repeat(repeat=7, number=number)
    return number, median(samples) * 1_000_000 / number


def render_result(name: str, result: Result, a: int, b: int, full: bool) -> list[str]:
    lines = [f"АЛГОРИТМ: {name}"]
    lines.append(f"НОД = {result.gcd}")
    lines.append(f"x = {result.x}")
    lines.append(f"y = {result.y}")
    lines.append(f"Проверка: a*x + b*y = {a * result.x + b * result.y}")
    lines.append(f"Основных операций = {result.iterations}")
    lines.append(f"Строк журнала с инициализацией = {len(result.steps)}")
    lines.append("№ | операция | r | x | y")
    for step in visible_steps(result.steps, full):
        if step is None:
            lines.append("... (средние строки опущены по требованию задания)")
        else:
            lines.append(
                f"{step.number} | {step.operation} | {step.remainder} | {step.x} | {step.y}"
            )
    lines.append("")
    return lines


def build_output(*, full: bool, with_benchmark: bool) -> str:
    lines = [
        "Теоретико-числовые методы в криптографии — лабораторная работа №1",
        "Вариант 60. Все коэффициенты приведены к исходному порядку чисел.",
        "Для каждой строки журнала выполнено a*x + b*y = r.",
        "В бинарном алгоритме r — изменённый рабочий остаток с общим множителем.",
        "",
    ]
    for pair_no, (a, b) in enumerate(PAIRS, start=1):
        lines.extend((f"ПАРА {pair_no}", f"a = {a}", f"b = {b}", ""))
        expected = reference_gcd(a, b)
        for name, function in ALGORITHMS:
            result = function(a, b, trace=True)
            if result.gcd != expected or a * result.x + b * result.y != expected:
                raise AssertionError(f"Ошибка результата: пара {pair_no}, {name}")
            for step in result.steps:
                if a * step.x + b * step.y != step.remainder:
                    raise AssertionError(f"Ошибка строки {step.number}: пара {pair_no}, {name}")
            lines.extend(render_result(name, result, a, b, full))
        if with_benchmark:
            lines.append("Измерение без формирования журнала, медиана 7 серий:")
            for name, function in ALGORITHMS:
                count, microseconds = benchmark(function, a, b)
                lines.append(f"{name}: {microseconds:.3f} мкс/вызов; {count} вызовов в серии")
            lines.append("")
        lines.append("=" * 72)
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--full", action="store_true", help="вывести все строки журнала")
    parser.add_argument("--benchmark", action="store_true", help="измерить время работы")
    parser.add_argument("--output", type=Path, help="записать результат в UTF-8 файл")
    args = parser.parse_args()
    output = build_output(full=args.full, with_benchmark=args.benchmark)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
        print(f"Результаты записаны: {args.output}")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
