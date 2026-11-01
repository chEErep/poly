# Небольшие контракты ввода, отображения и звука

from abc import ABC, abstractmethod

from bird import Bird
from game_types import Theme
from ground import Ground
from pipe_pair import PipePair


class IInputHandler(ABC):
    @abstractmethod
    def poll(self) -> list[str]:
        #Вернуть команды action/quit, не изменяя игровые объекты
        raise NotImplementedError


class IRenderer(ABC):
    @abstractmethod
    def draw(self, state: str, bird: Bird, ground: Ground,
             pipes: list[PipePair], score: int, theme: Theme) -> None:
        #Показать кадр, не изменяя переданное состояние игры
        raise NotImplementedError


class ISoundPlayer(ABC):
    @abstractmethod
    def play(self, event: str) -> None:
        #Воспроизвести эффект без изменения правил, счёта и состояния раунда
        raise NotImplementedError
