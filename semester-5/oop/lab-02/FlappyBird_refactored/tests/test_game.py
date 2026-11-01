import random
import unittest
from unittest.mock import Mock, call

from collision_detector import CollisionDetector
from game import Game
from interfaces import IInputHandler, IRenderer, ISoundPlayer


class GameTests(unittest.TestCase):
    def setUp(self) -> None:
        self.sound = Mock(spec=ISoundPlayer)
        detector = Mock(spec=CollisionDetector)
        detector.check.return_value = None
        self.game = Game(Mock(spec=IInputHandler), Mock(spec=IRenderer), self.sound,
                         detector, lambda: None, (34, 24), (52, 320), 48,
                         rng=random.Random(4))

    def test_start_preserves_height_and_creates_pipes(self) -> None:
        self.game.update()
        y = self.game.bird.get_rect()[1]
        self.game.handle_command('action')
        self.assertEqual(self.game.state, 'playing')
        self.assertEqual(self.game.bird.get_rect()[1], y)
        self.assertEqual(len(self.game.pipes), 2)
        self.sound.play.assert_called_once_with('wing')

    def test_finish_uses_collision_reason(self) -> None:
        self.game.start_game()
        self.sound.reset_mock()
        self.game.finish_round('pipe')
        self.assertEqual(self.game.state, 'game_over')
        self.assertEqual(self.sound.play.call_args_list, [call('hit'), call('die')])

    def test_retry_is_allowed_after_landing(self) -> None:
        self.game.start_game()
        self.game.finish_round('pipe')
        self.game.handle_command('action')
        self.assertEqual(self.game.state, 'game_over')
        for _ in range(100):
            self.game.update()
        self.game.handle_command('action')
        self.assertEqual(self.game.state, 'welcome')
        self.assertEqual(self.game.score, 0)
        self.assertEqual(self.game.pipes, ())


if __name__ == '__main__':
    unittest.main()
