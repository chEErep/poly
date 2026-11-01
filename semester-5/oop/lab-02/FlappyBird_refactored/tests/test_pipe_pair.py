import unittest

from pipe_pair import PipePair


class PipePairTests(unittest.TestCase):
    def test_gap_geometry(self) -> None:
        pair = PipePair(200, 150, 100, 52, 320, -4)
        self.assertEqual(pair.get_upper_rect(), (200, -170, 52, 320))
        self.assertEqual(pair.get_lower_rect(), (200, 250, 52, 320))

    def test_both_pipes_move_together(self) -> None:
        pair = PipePair(200, 150, 100, 52, 320, -4)
        pair.update()
        self.assertEqual(pair.get_upper_rect()[0], 196)
        self.assertEqual(pair.get_lower_rect()[0], 196)

    def test_passage_counts_once(self) -> None:
        pair = PipePair(50, 150, 100, 52, 320, -4)
        self.assertFalse(pair.register_passage(74))
        pair.update()
        self.assertTrue(pair.register_passage(74))
        self.assertFalse(pair.register_passage(74))

    def test_offscreen_detection(self) -> None:
        pair = PipePair(-50, 150, 100, 52, 320, -4)
        self.assertFalse(pair.is_offscreen())
        pair.update()
        self.assertTrue(pair.is_offscreen())


if __name__ == '__main__':
    unittest.main()
