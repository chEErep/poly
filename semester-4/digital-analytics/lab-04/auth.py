"""Регистрации в SQLite и сессии одного процесса бота."""
import os
import sqlite3
import time
from contextlib import closing
from pathlib import Path
from threading import RLock
from werkzeug.security import generate_password_hash, check_password_hash


class AuthStore:
    def __init__(self, path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.lock = RLock()
        self.sessions = {}
        self.attempts = {}
        with closing(sqlite3.connect(self.path)) as db, db:
            db.execute('CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, password_hash TEXT NOT NULL)')
        if os.name != 'nt':
            self.path.chmod(0o600)

    def register(self, user_id, password):
        if not 8 <= len(password) <= 256:
            raise ValueError('Пароль должен содержать от 8 до 256 символов.')
        password_hash = generate_password_hash(password)
        with self.lock, closing(sqlite3.connect(self.path)) as db, db:
            try:
                db.execute('INSERT INTO users VALUES (?, ?)', (user_id, password_hash))
            except sqlite3.IntegrityError:
                return False
        return True

    def login(self, user_id, password):
        if len(password) > 256:
            return False
        now = time.monotonic()
        with self.lock, closing(sqlite3.connect(self.path)) as db:
            attempts = [t for t in self.attempts.get(user_id, []) if now - t < 60]
            if len(attempts) >= 5:
                return False
            attempts.append(now)
            self.attempts[user_id] = attempts
            row = db.execute('SELECT password_hash FROM users WHERE id=?', (user_id,)).fetchone()
            if row is None or not check_password_hash(row[0], password):
                return False
            self.sessions[user_id] = now + 1800
            self.attempts.pop(user_id, None)
            return True

    def authenticated(self, user_id):
        with self.lock:
            if self.sessions.get(user_id, 0) > time.monotonic():
                return True
            self.sessions.pop(user_id, None)
            return False

    def logout(self, user_id):
        with self.lock:
            self.sessions.pop(user_id, None)
