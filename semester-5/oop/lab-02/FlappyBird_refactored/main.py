# Точка входа: создаёт ресурсы и соединяет готовые объекты.

import sys

import pygame

from asset_manager import AssetManager
from collision_detector import CollisionDetector
from game import Game
from input_handler import InputHandler
from renderer import Renderer
from settings import FPS, SCREEN_HEIGHT, SCREEN_WIDTH
from sound_player import SoundPlayer


def main() -> int:
    pygame.init()
    try:
        screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption('Flappy Bird')
        clock = pygame.time.Clock()
        assets = AssetManager()
        assets.load()

        def wait_frame() -> None:
            clock.tick(FPS)

        game = Game(InputHandler(), Renderer(screen, assets), SoundPlayer(assets),
                    CollisionDetector(assets.get_masks()), wait_frame,
                    bird_size=assets.get_image('bird_0_0').get_size(),
                    pipe_size=assets.get_image('pipe_0_lower').get_size(),
                    repeat_width=assets.get_image('base').get_width() - SCREEN_WIDTH)
        game.run()
        return 0
    except (pygame.error, OSError, RuntimeError) as error:
        print(f'Не удалось запустить игру: {error}', file=sys.stderr)
        return 1
    finally:
        pygame.quit()


if __name__ == '__main__':
    raise SystemExit(main())
