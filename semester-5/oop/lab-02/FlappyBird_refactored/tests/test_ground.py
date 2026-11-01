import unittest

from ground import Ground


class GroundTests(unittest.TestCase):
    def test_scroll_repeats(self) -> None:
        ground = Ground(404.48, 48)
        ground.update()
        self.assertEqual(ground.get_position(), (-4, 404.48))
        for _ in range(11):
            ground.update()
        self.assertEqual(ground.get_position(), (0, 404.48))

    def test_surface_contact(self) -> None:
        ground = Ground(404.48, 48)
        self.assertFalse(ground.touches((57, 379, 34, 24)))
        self.assertTrue(ground.touches((57, 380.48, 34, 24)))


if __name__ == '__main__':
    unittest.main()
