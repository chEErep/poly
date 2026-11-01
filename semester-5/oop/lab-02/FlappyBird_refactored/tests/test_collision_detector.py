import unittest

from bird import Bird
from collision_detector import CollisionDetector
from ground import Ground
from pipe_pair import PipePair


class CollisionDetectorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.ground = Ground(100, 48)
        self.bird = Bird(10, 10, 3, 3)
        self.pipe = PipePair(10, 4, 6, 3, 3, -1)
        solid = [[True] * 3 for _ in range(3)]
        self.masks = {'bird_0_0': solid, 'pipe_0_upper': solid, 'pipe_0_lower': solid}

    def test_no_obstacles(self) -> None:
        self.assertIsNone(CollisionDetector({}).check(self.bird, self.ground, [], (0, 0, 0)))

    def test_ground_collision(self) -> None:
        bird = Bird(10, 97, 3, 3)
        self.assertEqual(CollisionDetector({}).check(bird, self.ground, [], (0, 0, 0)), 'ground')

    def test_transparent_pixels_do_not_collide(self) -> None:
        self.masks['bird_0_0'] = [[False] * 3, [False, True, False], [False] * 3]
        self.masks['pipe_0_lower'] = [[True, False, False], [False] * 3, [False] * 3]
        self.assertIsNone(CollisionDetector(self.masks).check(
            self.bird, self.ground, [self.pipe], (0, 0, 0)))

    def test_common_occupied_pixel_collides(self) -> None:
        self.masks['bird_0_0'] = [[False] * 3, [False] * 3, [False, True, False]]
        self.masks['pipe_0_lower'] = [[False] * 3, [False, True, False], [False] * 3]
        pipe = PipePair(11, 4, 6, 3, 3, -1)
        self.assertEqual(CollisionDetector(self.masks).check(
            self.bird, self.ground, [pipe], (0, 0, 0)), 'pipe')


if __name__ == '__main__':
    unittest.main()
