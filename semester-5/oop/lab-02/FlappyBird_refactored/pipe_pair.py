#Верхняя и нижняя трубы одного препятствия

from math import isfinite

from game_types import RectData


class PipePair:
    #Одна пара труб с общей координатой X и одним проходом между ними

    def __init__(self, x: float, gap_y: float, gap_size: int,
                 width: int, height: int, step_x: float) -> None:
        if not all(isinstance(value, int) for value in (gap_size, width, height)):
            raise TypeError('Размеры прохода и труб должны быть целыми числами')
        if gap_size <= 0 or width <= 0 or height <= 0:
            raise ValueError('Размеры прохода и труб должны быть положительными')
        if not all(isfinite(value) for value in (x, gap_y, step_x)):
            raise ValueError('Координаты и шаг труб должны быть конечными числами')
        if step_x >= 0:
            raise ValueError('Шаг труб должен быть отрицательным: движение влево')

        self._x = float(x)
        self._gap_y = float(gap_y) #Верхняя граница прохода
        self._gap_size = gap_size #Размер прохода по вертикали
        self._width = width
        self._height = height
        self._step_x = float(step_x)
        self._passed = False  #Отметили ли прохождение этой пары

    def update(self) -> None:
        #Переместить обе трубы одним изменением общей координаты
        self._x += self._step_x

    def get_upper_rect(self) -> RectData:
    #Верхняя труба заканчивается у верхнего края прохода
        return self._x, self._gap_y - self._height, self._width, self._height

    def get_lower_rect(self) -> RectData:
        #Нижняя труба начинается под проходом
        return self._x, self._gap_y + self._gap_size, self._width, self._height

    def is_offscreen(self) -> bool:
        #Сообщить, что пару можно удалить
        return self._x + self._width < 0

    def register_passage(self, bird_mid_x: float) -> bool:
        #Однократно сообщить о прохождении
        if not isfinite(bird_mid_x):
            raise ValueError('Координата середины птицы должна быть конечной')
        if not self._passed and self._x + self._width / 2 <= bird_mid_x:
            self._passed = True
            return True
        return False
