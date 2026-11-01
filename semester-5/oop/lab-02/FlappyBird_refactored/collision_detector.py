# Проверка столкновений по геометрии и готовым маскам, без Pygame.

from bird import Bird
from game_types import HitMask, RectData, Theme
from ground import Ground
from pipe_pair import PipePair


class CollisionDetector:
    def __init__(self, masks: dict[str, HitMask]) -> None:
        # Сам словарь копируем, а вложенные маски считаем данными только для чтения.
        self._masks = masks.copy()

    def check(self, bird: Bird, ground: Ground,
              pipes: list[PipePair], theme: Theme) -> str | None:
        bird_rect = bird.get_rect()
        if ground.touches(bird_rect):
            return 'ground'
        if not pipes:
            return None

        # В Theme: фон, птица, трубы. Фон на столкновения не влияет.
        _, bird_color, pipe_color = theme
        bird_mask = self._get_mask(f'bird_{bird_color}_{bird.get_frame()}', bird_rect)
        for pair in pipes:
            for part, pipe_rect in (('upper', pair.get_upper_rect()),
                                    ('lower', pair.get_lower_rect())):
                pipe_mask = self._get_mask(f'pipe_{pipe_color}_{part}', pipe_rect)
                if self.__pixel_collision(bird_rect, pipe_rect, bird_mask, pipe_mask):
                    return 'pipe'
        return None

    def _get_mask(self, name: str, rect: RectData) -> HitMask:
        mask = self._masks[name]
        _, _, width, height = rect
        if len(mask) != width or any(len(column) != height for column in mask):
            raise ValueError(f'Размер маски {name} не совпадает с размером изображения')
        return mask

    def __pixel_collision(self, a: RectData, b: RectData,
                          mask_a: HitMask, mask_b: HitMask) -> bool:
        # pygame.Rect в исходнике отбрасывает дробную часть координат к нулю.
        ax, ay = int(a[0]), int(a[1])
        bx, by = int(b[0]), int(b[1])
        left = max(ax, bx)
        top = max(ay, by)
        right = min(ax + a[2], bx + b[2])
        bottom = min(ay + a[3], by + b[3])
        if left >= right or top >= bottom:
            return False

        # Экранная точка переводится в координаты каждой из двух картинок.
        for x in range(left, right):
            for y in range(top, bottom):
                if mask_a[x - ax][y - ay] and mask_b[x - bx][y - by]:
                    return True
        return False
