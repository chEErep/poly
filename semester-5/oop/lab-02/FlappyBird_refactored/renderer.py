# Отрисовка одного состояния игры, без изменения игровых объектов.

import pygame

from asset_manager import AssetManager
from bird import Bird
from game_types import Theme
from ground import Ground
from interfaces import IRenderer
from pipe_pair import PipePair
from settings import SCREEN_HEIGHT, SCREEN_WIDTH


class Renderer(IRenderer):
    def __init__(self, screen: pygame.Surface, assets: AssetManager) -> None:
        self._screen = screen
        self._assets = assets

    def draw(self, state: str, bird: Bird, ground: Ground,
             pipes: list[PipePair], score: int, theme: Theme) -> None:
        background, bird_color, pipe_color = theme
        self._screen.blit(self._assets.get_image(f'background_{background}'), (0, 0))

        for pair in pipes:
            self._screen.blit(self._assets.get_image(f'pipe_{pipe_color}_upper'),
                              pair.get_upper_rect()[:2])
            self._screen.blit(self._assets.get_image(f'pipe_{pipe_color}_lower'),
                              pair.get_lower_rect()[:2])
        self._screen.blit(self._assets.get_image('base'), ground.get_position())

        if state != 'welcome':
            self.draw_score(score)
        frame = 1 if state == 'game_over' else bird.get_frame()
        image = self._assets.get_image(f'bird_{bird_color}_{frame}')
        if state == 'playing':
            image = pygame.transform.rotate(image, min(20, bird.get_angle()))
        elif state == 'game_over':
            image = pygame.transform.rotate(image, bird.get_angle())
        self._screen.blit(image, bird.get_rect()[:2])

        if state == 'welcome':
            message = self._assets.get_image('message')
            self._screen.blit(message, (int((SCREEN_WIDTH - message.get_width()) / 2),
                                        int(SCREEN_HEIGHT * 0.12)))
        elif state == 'game_over':
            self._screen.blit(self._assets.get_image('gameover'), (50, 180))
        pygame.display.flip()

    def draw_score(self, score: int) -> None:
        digits = [self._assets.get_image(f'digit_{digit}') for digit in str(score)]
        x = (SCREEN_WIDTH - sum(image.get_width() for image in digits)) / 2
        for image in digits:
            self._screen.blit(image, (x, SCREEN_HEIGHT * 0.1))
            x += image.get_width()
