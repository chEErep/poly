#Состояние и поведение птицы без зависимости от Pygame

from itertools import cycle #для бесконечного повтора картинок крыльев
from math import isfinite #для проверка координаты

from game_types import RectData #короткое название (х,у,ширина,высота)


class Bird:
    #Одна птица. Один вызов update соответствует кадру исходной игры

    def __init__(self, x: float, y: float, width: int, height: int) -> None:
        if not isinstance(width, int) or not isinstance(height, int):
            raise TypeError("Размеры птицы должны быть целыми числами")
        if width <= 0 or height <= 0:
            raise ValueError("Размеры птицы должны быть положительными")
        if not isfinite(x) or not isfinite(y):
            raise ValueError("Координаты птицы должны быть конечными числами")

        self._x = float(x)
        self._y = float(y)
        self._width = width
        self._height = height
        self._velocity_y = -9.0 #вертикальная скорость
        self._angle = 45.0
        self._flapped = False #был ли взмах пперед ближайшим обновлением

        self._frame_index = 0  #какую картинку крыльев сейчас используем
        self._frames = cycle((0, 1, 2, 1))
        self._loop_iter = 0
        self._idle_offset = 0  #смещение покачивания на зсатавке
        self._idle_direction = 1

    @property
    def velocity_y(self) -> float:
        return self._velocity_y

    def start_flight(self) -> None:
        #Начать полёт с текущей высоты, сохранив фазу анимации крыльев
        self._velocity_y = -9.0
        self._angle = 45.0
        self._flapped = False
        self._frame_index = 0
        self._loop_iter = 0

    def flap(self) -> bool:
        #Выполнить взмах
        if self._y <= -2 * self._height:
            return False
        self._velocity_y = -9.0
        self._flapped = True
        return True

    def update(self, ground_y: float) -> None:
        #Один кадр полёта: крылья, наклон, скорость, затем координата
        self._check_ground(ground_y)
        self._advance_frame(interval=3) #обновляем картинку крыльев

        if self._angle > -90:
            self._angle -= 3
        if self._velocity_y < 10 and not self._flapped:
            self._velocity_y += 1
        if self._flapped:
            self._flapped = False
            self._angle = 45.0

        self._y += min(self._velocity_y, ground_y - self._y - self._height)

    def update_idle(self) -> None:
        #Покачивание от -8 до +8 пикселей и крылья на заставке
        self._advance_frame(interval=5)
        if abs(self._idle_offset) == 8:
            self._idle_direction *= -1
        self._idle_offset += self._idle_direction
        self._y += self._idle_direction

    def update_fall(self, ground_y: float, rotate: bool) -> None:
        #Падение после столкновения. после удара о трубу птица вращается
        self._check_ground(ground_y)
        self._frame_index = 1  # После проигрыша в исходнике используется средний кадр
        self._flapped = False
        if self._y + self._height < ground_y - 1:
            self._y += min(self._velocity_y, ground_y - self._y - self._height)
        if self._velocity_y < 15:
            self._velocity_y += 2
        if rotate and self._angle > -90:
            self._angle -= 7

    def get_rect(self) -> RectData:
        #Положение и размеры
        return self._x, self._y, self._width, self._height

    def get_angle(self) -> float:
        #Внутренний угол
        return self._angle

    def get_frame(self) -> int:
        #номер картинки крыльев
        return self._frame_index

    def _advance_frame(self, interval: int) -> None:
        if (self._loop_iter + 1) % interval == 0:
            self._frame_index = next(self._frames)
        self._loop_iter = (self._loop_iter + 1) % 30

    def _check_ground(self, ground_y: float) -> None:
        if not isfinite(ground_y):
            raise ValueError("Координата земли должна быть конечным числом")
