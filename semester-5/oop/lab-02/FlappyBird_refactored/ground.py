#Поверхность земли и её циклическая прокрутка

from math import isfinite

from game_types import RectData


class Ground:
    #Хранит уровень земли и положение её повторяющейся картинки

    def __init__(self, y: float, repeat_width: int, offset_x: float = 0.0) -> None:
        if not isinstance(repeat_width, int):
            raise TypeError('Период повторения должен быть целым числом')
        if repeat_width <= 0:
            raise ValueError('Период повторения должен быть положительным')
        if not isfinite(y) or not isfinite(offset_x):
            raise ValueError('Координаты земли должны быть конечными числами')
        self._y = float(y) #Вертикальная координата поверхности
        self._repeat_width = repeat_width #Через какое расстояние можно вернуть картинку
        self._offset_x = -((-offset_x) % repeat_width)

    def update(self, step: float = 4.0) -> None:
        #Сдвинуть картинку влево на step пикселей с возвратом через период
        if not isfinite(step) or step < 0:
            raise ValueError('Шаг прокрутки должен быть конечным и неотрицательным')
        self._offset_x = -((-self._offset_x + step) % self._repeat_width)

    def touches(self, rect: RectData) -> bool:
        #Коснулся ли низ прямоугольника земли (с исходным допуском в 1 пиксель)
        _, y, _, height = rect
        return y + height >= self._y - 1

    def get_y(self) -> float:
        #Вертикальная координата поверхности
        return self._y

    def get_position(self) -> tuple[float, float]:
        #Положение картинки земли для отрисовки
        return self._offset_x, self._y
