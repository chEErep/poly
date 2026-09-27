import random
import sys
import unittest
from math import gcd
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from algorithms import ALGORITHMS, extended_least_remainder  # noqa: E402
from variant60 import PAIRS  # noqa: E402


class AlgorithmTests(unittest.TestCase):
    def verify_pair(self, a: int, b: int) -> None:
        expected = gcd(a, b)
        for name, function in ALGORITHMS:
            with self.subTest(pair=(a, b), algorithm=name):
                result = function(a, b, trace=True)
                self.assertEqual(result.gcd, expected)
                self.assertEqual(a * result.x + b * result.y, expected)
                self.assertGreaterEqual(result.iterations, 1)
                for step in result.steps:
                    self.assertEqual(a * step.x + b * step.y, step.remainder)
                self.assertEqual(
                    abs(next(step.remainder for step in reversed(result.steps) if step.remainder)),
                    expected,
                )

    def test_variant_60(self) -> None:
        for a, b in PAIRS:
            self.verify_pair(a, b)
            self.verify_pair(b, a)

    def test_small_edge_cases(self) -> None:
        for a, b in ((1, 1), (2, 2), (8, 12), (12, 8), (13, 37), (32, 96)):
            self.verify_pair(a, b)

    def test_random_positive_pairs(self) -> None:
        rng = random.Random(60)
        for _ in range(100):
            self.verify_pair(rng.randint(1, 10_000), rng.randint(1, 10_000))

    def test_least_remainder_bound(self) -> None:
        for a, b in PAIRS:
            steps = extended_least_remainder(a, b, trace=True).steps
            for index in range(2, len(steps)):
                if steps[index].operation.startswith("q="):
                    self.assertLessEqual(
                        2 * abs(steps[index].remainder),
                        abs(steps[index - 1].remainder),
                    )

    def test_invalid_input(self) -> None:
        for _, function in ALGORITHMS:
            for values in ((0, 1), (1, 0), (-1, 2), (1.5, 2), (True, 2)):
                with self.subTest(function=function.__name__, values=values):
                    with self.assertRaises((TypeError, ValueError)):
                        function(*values)


if __name__ == "__main__":
    unittest.main()
