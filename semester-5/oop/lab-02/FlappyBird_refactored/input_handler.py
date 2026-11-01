# Перевод событий Pygame в команды игры.

import pygame

from interfaces import IInputHandler


class InputHandler(IInputHandler):
    def __init__(self) -> None:
        self._key_map = {pygame.K_SPACE: 'action', pygame.K_UP: 'action',
                         pygame.K_ESCAPE: 'quit'}

    def poll(self) -> list[str]:
        commands = []
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                commands.append('quit')
            elif event.type == pygame.KEYDOWN:
                command = self._key_map.get(event.key)
                if command is not None:
                    commands.append(command)
        return commands
