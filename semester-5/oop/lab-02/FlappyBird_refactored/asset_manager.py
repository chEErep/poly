# Загрузка изображений, звуков и масок столкновений.

from pathlib import Path
import sys

import pygame

from game_types import HitMask


class AssetManager:
    def __init__(self, asset_dir: Path | None = None) -> None:
        if asset_dir is None:
            asset_dir = Path(__file__).resolve().parent / 'assets'
        self._asset_dir = Path(asset_dir)
        self._images: dict[str, pygame.Surface] = {}
        self._sounds: dict[str, pygame.mixer.Sound] = {}
        self._masks: dict[str, HitMask] = {}

    def load(self) -> None:
        # Окно нужно для convert/convert_alpha, микшер — для загрузки Sound.
        if pygame.display.get_surface() is None:
            raise RuntimeError('Перед загрузкой ресурсов нужно создать окно Pygame')
        if pygame.mixer.get_init() is None:
            raise RuntimeError('Перед загрузкой звуков нужно инициализировать pygame.mixer')

        sprites = self._asset_dir / 'sprites'
        audio = self._asset_dir / 'audio'
        # Готовим новые словари отдельно: ошибка файла не оставит половину загрузки.
        images: dict[str, pygame.Surface] = {}
        sounds: dict[str, pygame.mixer.Sound] = {}
        masks: dict[str, HitMask] = {}

        for name in ('base', 'message', 'gameover'):
            images[name] = pygame.image.load(str(sprites / f'{name}.png')).convert_alpha()
        for digit in range(10):
            images[f'digit_{digit}'] = pygame.image.load(
                str(sprites / f'{digit}.png')).convert_alpha()
        for index, name in enumerate(('day', 'night')):
            images[f'background_{index}'] = pygame.image.load(
                str(sprites / f'background-{name}.png')).convert()
        for color_index, color in enumerate(('red', 'blue', 'yellow')):
            for frame_index, pose in enumerate(('upflap', 'midflap', 'downflap')):
                name = f'bird_{color_index}_{frame_index}'
                images[name] = pygame.image.load(
                    str(sprites / f'{color}bird-{pose}.png')).convert_alpha()
                masks[name] = self._make_hitmask(images[name])
        for color_index, color in enumerate(('green', 'red')):
            lower = pygame.image.load(str(sprites / f'pipe-{color}.png')).convert_alpha()
            upper = pygame.transform.flip(lower, False, True)
            for part, image in (('upper', upper), ('lower', lower)):
                name = f'pipe_{color_index}_{part}'
                images[name] = image
                masks[name] = self._make_hitmask(image)

        extension = '.wav' if sys.platform == 'win32' else '.ogg'
        for name in ('wing', 'point', 'hit', 'die', 'swoosh'):
            sounds[name] = pygame.mixer.Sound(str(audio / f'{name}{extension}'))

        self._images = images
        self._sounds = sounds
        self._masks = masks

    def get_image(self, name: str) -> pygame.Surface:
        return self._images[name]

    def get_sound(self, name: str) -> pygame.mixer.Sound:
        # Возвращаем загруженный объект; воспроизведение будет у SoundPlayer.
        return self._sounds[name]

    def get_mask(self, name: str) -> HitMask:
        return self._masks[name]

    def get_masks(self) -> dict[str, HitMask]:
        # Сами маски общие и после загрузки не изменяются.
        return self._masks.copy()

    def _make_hitmask(self, image: pygame.Surface) -> HitMask:
        mask: HitMask = []
        for x in range(image.get_width()):
            column = []
            for y in range(image.get_height()):
                column.append(bool(image.get_at((x, y))[3]))
            mask.append(column)
        return mask
