"""
lab4.py
-------
Telegram-бот с системой регистрации/аутентификации и бинарной классификацией
изображений на основе модели из лабораторной работы №3.

Поддерживаемые команды:
    /start, /help        — справка по командам
    /register <пароль>   — регистрация нового пользователя
    /login <пароль>      — вход в систему
    /logout              — выход из текущей сессии
    /predict             — классификация изображения (нужно отправить фото
                           с подписью /predict)

Режим работы — LONG POLLING: бот сам периодически опрашивает Telegram API
методом getUpdates. НЕ требует публичного HTTPS-адреса, ngrok'а и
развёрнутого веб-сервера. Просто запустил — и работает.

Запуск:
    python lab4.py

Дополнительно: модуль содержит функцию get_flask_app(), создающую Flask-
приложение с маршрутами для webhook'а. Она нужна для прохождения тестов
и при желании переключиться на webhook-режим без переписывания кода.
"""

import argparse
import hmac
import logging
from pathlib import Path
from threading import Lock
from auth import AuthStore
from dotenv import load_dotenv
import os
from typing import Dict, Optional

from flask import Flask, Response, request

import telebot
from telebot import TeleBot

from predict import predict_image


# Токен бота, выдаётся @BotFather. Формат "число:строка" — тесты это
# проверяют. Перед локальным запуском замените на свой реальный токен.
load_dotenv(Path(__file__).with_name(".env"))
TOKEN: str = os.environ.get("TOKEN", "")

# URL для webhook — должен быть HTTPS. По умолчанию заглушка, перед
# реальным использованием webhook замените на адрес туннеля (ngrok и т.п.).
WEBHOOK_URL: str = os.environ.get("WEBHOOK_URL", "")
WEBHOOK_SECRET = os.environ.get("WEBHOOK_SECRET", "")
_auth = None
_auth_lock = Lock()

def get_auth():
    global _auth
    with _auth_lock:
        if _auth is None:
            _auth = AuthStore(os.environ.get("AUTH_DB", str(Path(__file__).with_name("data") / "users.db")))
    return _auth

# Порт, на котором Flask-приложение принимает запросы от Telegram при
# использовании webhook.
PORT: int = int(os.environ.get("PORT", "8080"))

# Бот и Flask-приложение создаются лениво при первом обращении.
bot: Optional[TeleBot] = None
flask_app: Optional[Flask] = None


logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    level=logging.INFO,
)
logger = logging.getLogger(__name__)


def _safe_send(chat_id: int, text: str) -> None:
    """Обёртка вокруг bot.send_message с обработкой ошибок Telegram."""
    if bot is None:
        logger.error("Попытка отправить сообщение до инициализации бота")
        return
    try:
        bot.send_message(chat_id, text)
    except Exception as e:
        logger.exception("Не удалось отправить сообщение в чат %s: %s", chat_id, e)


def handle_start(message) -> None:
    """Обработчик /start и /help — выводит справку по командам."""
    help_text = (
        "Доступные команды:\n\n"
        "/register <пароль> — регистрация нового пользователя\n"
        "/login <пароль>    — вход в систему\n"
        "/logout            — завершение сессии\n"
        "/predict           — классификация изображения (человек/лошадь)\n\n"
        "Пример использования:\n"
        "1. /register mysecretpass\n"
        "2. /login mysecretpass\n"
        "3. Отправьте фото с подписью /predict"
    )
    _safe_send(message.chat.id, help_text)


def handle_register(message) -> None:
    """
    Регистрация пользователя.
    Ожидается ввод: /register <пароль>
    """
    try:
        user_id = message.from_user.id

        # Парсим текст команды: после "/register" должен идти пароль.
        parts = message.text.split(maxsplit=1)
        if len(parts) < 2 or not parts[1].strip():
            _safe_send(message.chat.id, "Использование: /register <пароль>")
            return
        password = parts[1].strip()

        if getattr(message.chat, "type", "private") != "private":
            _safe_send(message.chat.id, "Регистрация доступна только в личном чате с ботом.")
            return
        try:
            created = get_auth().register(user_id, password)
        except ValueError as exc:
            _safe_send(message.chat.id, str(exc))
            return
        _safe_send(message.chat.id, "Регистрация прошла успешно. Теперь выполните /login <пароль>" if created
                   else "Вы уже зарегистрированы. Используйте /login для входа.")
    except Exception as e:
        logger.exception("Ошибка в handle_register: %s", e)
        _safe_send(message.chat.id, "Произошла ошибка при регистрации.")


def handle_login(message) -> None:
    """
    Вход в систему.
    Ожидается ввод: /login <пароль>
    """
    try:
        user_id = message.from_user.id

        parts = message.text.split(maxsplit=1)
        if len(parts) < 2 or not parts[1].strip():
            _safe_send(message.chat.id, "Использование: /login <пароль>")
            return
        password = parts[1].strip()

        if getattr(message.chat, "type", "private") != "private":
            _safe_send(message.chat.id, "Вход доступен только в личном чате с ботом.")
            return
        if not get_auth().login(user_id, password):
            _safe_send(message.chat.id, "Неверные данные или слишком много попыток входа. Повторите позже.")
            return
        _safe_send(message.chat.id, "Вход выполнен. Теперь отправьте фото с подписью /predict.")
    except Exception as e:
        logger.exception("Ошибка в handle_login: %s", e)
        _safe_send(message.chat.id, "Произошла ошибка при входе.")


def handle_logout(message) -> None:
    """
    Выход из сессии. Сбрасывает флаг authenticated в False,
    но пароль и регистрацию сохраняет.
    """
    try:
        user_id = message.from_user.id

        if not get_auth().authenticated(user_id):
            _safe_send(message.chat.id, "Вы и так не в сессии.")
            return

        get_auth().logout(user_id)
        _safe_send(message.chat.id, "Вы вышли из сессии.")
    except Exception as e:
        logger.exception("Ошибка в handle_logout: %s", e)
        _safe_send(message.chat.id, "Произошла ошибка при выходе.")


def _is_authenticated(user_id: int) -> bool:
    """Проверка: пользователь зарегистрирован и находится в сессии."""
    return get_auth().authenticated(user_id)


def _download_photo_bytes(message) -> Optional[bytes]:
    """
    Скачивает фотографию из сообщения и возвращает её как байты.
    Из нескольких размеров фото берём самый большой (последний в списке).
    """
    if bot is None:
        return None
    try:
        if (getattr(message.photo[-1], "file_size", 0) or 0) > 8 * 1024 * 1024:
            return None
        file_id = message.photo[-1].file_id
        file_info = bot.get_file(file_id)
        if (file_info.file_size or 0) > 8 * 1024 * 1024:
            return None
        data = bot.download_file(file_info.file_path)
        return data if len(data) <= 8 * 1024 * 1024 else None
    except Exception as e:
        logger.exception("Не удалось скачать фото: %s", e)
        return None


def _label_to_russian(label: str) -> str:
    """
    Переводит метку модели в человеко-понятный ответ.
    Тренировочный датасет был horses/humans, у нас задача бинарная
    'животное / человек'.
    """
    label_lower = (label or "").lower()
    if label_lower in ("humans", "human", "person", "people"):
        return "Человек"
    if label_lower in ("horse", "horses"):
        return "Лошадь"
    return label


def handle_predict(message) -> None:
    """
    Обработка команды /predict. Срабатывает на фото с подписью '/predict'.

    Логика:
      1) Если пользователь не авторизован — отправляем сообщение из ТЗ:
         "Вы не авторизованы, сначала используйте /login".
      2) Скачиваем фото в байты.
      3) Передаём байты в predict_image из predict.py.
      4) Отправляем результат: "Человек" или "Лошадь".
    """
    try:
        user_id = message.from_user.id

        # Проверка авторизации (формулировка точно как в ТЗ).
        if not _is_authenticated(user_id):
            _safe_send(message.chat.id,
                       "Вы не авторизованы, сначала используйте /login")
            return

        # /predict как текстовая команда, без фото.
        if not getattr(message, "photo", None):
            _safe_send(message.chat.id,
                       "Чтобы классифицировать, отправьте ФОТО с подписью /predict.")
            return

        image_bytes = _download_photo_bytes(message)
        if image_bytes is None:
            _safe_send(message.chat.id, "Не удалось загрузить изображение.")
            return

        # Классификация. predict_image возвращает имя класса ('horses'/'humans').
        try:
            label = predict_image(image_bytes)
        except Exception as e:
            logger.exception("Ошибка в predict_image: %s", e)
            _safe_send(message.chat.id, "Не удалось классифицировать изображение.")
            return

        answer = _label_to_russian(label)
        _safe_send(message.chat.id, answer)
    except Exception as e:
        logger.exception("Ошибка в handle_predict: %s", e)
        _safe_send(message.chat.id, "Произошла ошибка при классификации.")


def handle_photo(message) -> None:
    """
    Обработчик "просто фото" — фото без подписи /predict
    или фото от неавторизованного пользователя.
    """
    try:
        user_id = message.from_user.id

        if not _is_authenticated(user_id):
            _safe_send(message.chat.id,
                       "Вы не авторизованы, сначала используйте /login")
            return

        # Авторизован, но фото пришло без подписи /predict.
        _safe_send(message.chat.id,
                   "Чтобы классифицировать изображение, отправьте фото с подписью /predict.")
    except Exception as e:
        logger.exception("Ошибка в handle_photo: %s", e)
        _safe_send(message.chat.id, "Произошла ошибка при обработке фото.")


def handle_unknown(message) -> None:
    """Фоллбэк: любое некорректное сообщение — отвечаем подсказкой."""
    try:
        _safe_send(message.chat.id,
                   "Не понял команду. Используйте /help для списка доступных команд.")
    except Exception as e:
        logger.exception("Ошибка в handle_unknown: %s", e)



def _register_handlers(bot_instance: TeleBot) -> None:
    """Привязывает функции-обработчики к командам и типам сообщений."""

    @bot_instance.message_handler(commands=["start", "help"])
    def _start(message):
        handle_start(message)

    @bot_instance.message_handler(commands=["register"])
    def _register(message):
        handle_register(message)

    @bot_instance.message_handler(commands=["login"])
    def _login(message):
        handle_login(message)

    @bot_instance.message_handler(commands=["logout"])
    def _logout(message):
        handle_logout(message)

    # ВАЖНО: декоратор @message_handler(commands=[...]) фильтрует
    # команду по message.text. У фотографий text=None, есть только caption.
    # Чтобы ловить фото с подписью /predict, используем func-фильтр.
    def _caption_is_predict(m) -> bool:
        cap = (getattr(m, "caption", None) or "").strip().lower()
        return cap.startswith("/predict")

    # Фото с подписью /predict (надёжная маршрутизация через caption).
    @bot_instance.message_handler(content_types=["photo"], func=_caption_is_predict)
    def _predict_photo_caption(message):
        handle_predict(message)

    # Декларация как в шаблоне ТЗ — для совместимости с тестами.
    @bot_instance.message_handler(commands=["predict"], content_types=["photo"])
    def _predict_with_photo(message):
        handle_predict(message)

    # /predict как текстовая команда без фото — отправим подсказку.
    @bot_instance.message_handler(commands=["predict"])
    def _predict_no_photo(message):
        handle_predict(message)

    # Любое другое фото (без подписи /predict).
    @bot_instance.message_handler(content_types=["photo"])
    def _photo(message):
        handle_photo(message)

    # Всё остальное — fallback.
    @bot_instance.message_handler(
        func=lambda m: True,
        content_types=["text", "sticker", "document", "audio", "voice", "video"]
    )
    def _unknown(message):
        handle_unknown(message)


def get_bot() -> TeleBot:
    """Ленивая инициализация бота — создаётся при первом обращении."""
    global bot
    if bot is None:
        # TeleBot(TOKEN) не делает реальных запросов к API при создании,
        # поэтому даже с placeholder-токеном объект создастся успешно.
        # Реальная проверка токена произойдёт только при попытке отправить
        # запрос (например, infinity_polling в main()).
        if not TOKEN:
            raise ValueError("Задайте TOKEN в переменной окружения или .env")
        bot = TeleBot(TOKEN)
        _register_handlers(bot)
    return bot



def get_flask_app() -> Flask:
    """Ленивая инициализация Flask-приложения."""
    global flask_app
    if flask_app is None:
        flask_app = Flask(__name__)
        _setup_routes(flask_app)
    return flask_app


def _setup_routes(app: Flask) -> None:
    """Регистрирует маршруты Flask-приложения."""

    app.config["MAX_CONTENT_LENGTH"] = 1024 * 1024

    @app.route("/webhook", methods=["POST"])
    def telegram_webhook():
        """Принимает POST-запросы от Telegram и передаёт их боту."""
        provided = request.headers.get("X-Telegram-Bot-Api-Secret-Token", "")
        if not WEBHOOK_SECRET or not hmac.compare_digest(provided, WEBHOOK_SECRET):
            return Response("Forbidden", status=403)
        b = get_bot()
        try:
            json_data = request.get_json(silent=True)
            if not isinstance(json_data, dict) or "update_id" not in json_data:
                return Response("Invalid update", status=400)
            if json_data:
                update = telebot.types.Update.de_json(json_data)
                b.process_new_updates([update])
            return Response("OK", status=200)
        except Exception as e:
            logger.exception("Ошибка в webhook-обработчике: %s", e)
            return Response("Error", status=500)

    @app.route("/health", methods=["GET"])
    def healthcheck():
        """Проверка живости сервера."""
        return Response("OK", status=200)



def main():
    parser = argparse.ArgumentParser(description="Бот классификации изображений")
    parser.add_argument("--mode", choices=["polling", "webhook"], default="polling")
    args = parser.parse_args()
    b = get_bot()
    if args.mode == "webhook":
        if not WEBHOOK_URL.startswith("https://") or not WEBHOOK_SECRET:
            raise ValueError("Для webhook нужны HTTPS WEBHOOK_URL и WEBHOOK_SECRET")
        b.set_webhook(url=WEBHOOK_URL.rstrip("/") + "/webhook", secret_token=WEBHOOK_SECRET)
        get_flask_app().run(host="127.0.0.1", port=PORT)
    else:
        b.remove_webhook()
        logger.info("Запуск бота в режиме long polling...")
        b.infinity_polling(skip_pending=True)


if __name__ == "__main__":
    main()
