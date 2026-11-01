# Воспроизведение уже загруженных эффектов.

from asset_manager import AssetManager
from interfaces import ISoundPlayer


class SoundPlayer(ISoundPlayer):
    def __init__(self, assets: AssetManager) -> None:
        self._assets = assets

    def play(self, event: str) -> None:
        self._assets.get_sound(event).play()
