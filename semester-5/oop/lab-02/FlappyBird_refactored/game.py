# Координация раунда. События, графика, звуки и пиксельный алгоритм вынесены.

from collections.abc import Callable
import random

from bird import Bird
from collision_detector import CollisionDetector
from game_types import Theme
from ground import Ground
from interfaces import IInputHandler, IRenderer, ISoundPlayer
from pipe_pair import PipePair
from settings import GROUND_Y, PIPE_GAP_SIZE, PIPE_STEP_X, SCREEN_HEIGHT, SCREEN_WIDTH


class Game:
    def __init__(self, input_handler: IInputHandler, renderer: IRenderer,
                 sound_player: ISoundPlayer, collision_detector: CollisionDetector,
                 wait_frame: Callable[[], None], bird_size: tuple[int, int],
                 pipe_size: tuple[int, int], repeat_width: int,
                 rng: random.Random | None = None, pipe_step_x: float = PIPE_STEP_X) -> None:
        self._input_handler = input_handler
        self._renderer = renderer
        self._sound_player = sound_player
        self._collision_detector = collision_detector
        self._wait_frame = wait_frame
        self._bird_size = bird_size
        self._pipe_size = pipe_size
        self._repeat_width = repeat_width
        self._rng = rng if rng is not None else random.Random()
        self._pipe_step_x = pipe_step_x
        self._running = True
        self.new_round()

    @property
    def state(self) -> str:
        return self._state

    @property
    def score(self) -> int:
        return self._score

    @property
    def bird(self) -> Bird:
        return self._bird

    @property
    def ground(self) -> Ground:
        return self._ground

    @property
    def pipes(self) -> tuple[PipePair, ...]:
        return tuple(self._pipes)

    @property
    def theme(self) -> Theme:
        return self._theme

    def run(self) -> None:
        while self._running:
            for command in self._input_handler.poll():
                self.handle_command(command)
                if not self._running:
                    break
            if not self._running:
                break
            self.update()
            self._renderer.draw(self._state, self._bird, self._ground,
                                self._pipes, self._score, self._theme)
            self._wait_frame()

    def handle_command(self, command: str) -> None:
        if command == 'quit':
            self._running = False
        elif command == 'action':
            if self._state == 'welcome':
                self.start_game()
            elif self._state == 'playing':
                if self._bird.flap():
                    self._sound_player.play('wing')
            elif self._ground.touches(self._bird.get_rect()):
                self.new_round()

    def new_round(self) -> None:
        self._theme = (self._rng.randint(0, 1), self._rng.randint(0, 2), self._rng.randint(0, 1))
        width, height = self._bird_size
        self._bird = Bird(int(SCREEN_WIDTH * 0.2), int((SCREEN_HEIGHT - height) / 2), width, height)
        self._ground = Ground(GROUND_Y, self._repeat_width)
        self._pipes: list[PipePair] = []
        self._score = 0
        self._state = 'welcome'
        self._crash_cause: str | None = None

    def start_game(self) -> None:
        if self._state != 'welcome':
            return
        self._bird.start_flight()
        self.spawn_pipe(SCREEN_WIDTH + 200)
        self.spawn_pipe(SCREEN_WIDTH + 200 + SCREEN_WIDTH / 2)
        self._state = 'playing'
        self._sound_player.play('wing')

    def update(self) -> None:
        if self._state == 'welcome':
            self._bird.update_idle()
            self._ground.update(step=4)
        elif self._state == 'playing':
            self._update_playing()
        elif self._state == 'game_over':
            self._bird.update_fall(self._ground.get_y(), rotate=self._crash_cause == 'pipe')

    def _update_playing(self) -> None:
        # Как в исходнике: столкновение и очки проверяются до движения кадра.
        cause = self._collision_detector.check(self._bird, self._ground, self._pipes, self._theme)
        if cause is not None:
            self.finish_round(cause)
            self._bird.update_fall(self._ground.get_y(), rotate=cause == 'pipe')
            return

        bird_x, _, bird_width, _ = self._bird.get_rect()
        for pair in self._pipes:
            if pair.register_passage(bird_x + bird_width / 2):
                self._score += 1
                self._sound_player.play('point')
        self._bird.update(self._ground.get_y())
        self._ground.update(step=100)
        self._update_pipes()

    def _update_pipes(self) -> None:
        for pair in self._pipes:
            pair.update()
        # Нет нижней границы 0: событие не теряется при перескоке через порог.
        if self._pipes and len(self._pipes) < 3 and self._pipes[0].get_upper_rect()[0] < 5:
            self.spawn_pipe(SCREEN_WIDTH + 10)
        self._pipes = [pair for pair in self._pipes if not pair.is_offscreen()]

    def spawn_pipe(self, x: float) -> None:
        gap_y = self._rng.randrange(0, int(GROUND_Y * 0.6 - PIPE_GAP_SIZE))
        gap_y += int(GROUND_Y * 0.2)
        width, height = self._pipe_size
        self._pipes.append(PipePair(x, gap_y, PIPE_GAP_SIZE, width, height, self._pipe_step_x))

    def finish_round(self, cause: str) -> None:
        if cause not in ('ground', 'pipe'):
            raise ValueError('Причина столкновения должна быть ground или pipe')
        if self._state != 'playing':
            return
        self._state = 'game_over'
        self._crash_cause = cause
        self._sound_player.play('hit')
        if cause == 'pipe':
            self._sound_player.play('die')
