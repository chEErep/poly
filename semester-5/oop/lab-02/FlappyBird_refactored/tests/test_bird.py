import unittest

from bird import Bird


class BirdTests(unittest.TestCase):
    def test_flap_and_gravity(self) -> None:
        bird = Bird(57, 244, 34, 24)
        self.assertTrue(bird.flap())
        bird.update(404.48)
        self.assertEqual(bird.velocity_y, -9)
        self.assertEqual(bird.get_rect()[1], 235)
        bird.update(404.48)
        self.assertEqual(bird.velocity_y, -8)
        self.assertEqual(bird.get_rect()[1], 227)

    def test_upper_limit_rejects_flap(self) -> None:
        bird = Bird(57, -48, 34, 24)
        self.assertFalse(bird.flap())

    def test_bird_stops_at_ground(self) -> None:
        bird = Bird(57, 244, 34, 24)
        for _ in range(100):
            bird.update(404.48)
        self.assertAlmostEqual(bird.get_rect()[1] + 24, 404.48)
        self.assertEqual(bird.velocity_y, 10)

    def test_idle_bobbing_stays_in_bounds(self) -> None:
        bird = Bird(57, 244, 34, 24)
        positions = []
        for _ in range(32):
            bird.update_idle()
            positions.append(bird.get_rect()[1])
        self.assertEqual(min(positions), 236)
        self.assertEqual(max(positions), 252)
        self.assertEqual(positions[-1], 244)

    def test_fall_rotates_after_pipe_collision(self) -> None:
        bird = Bird(57, 244, 34, 24)
        bird.update_fall(404.48, rotate=True)
        self.assertEqual(bird.get_rect()[1], 235)
        self.assertEqual(bird.get_angle(), 38)
        self.assertEqual(bird.get_frame(), 1)


if __name__ == '__main__':
    unittest.main()
