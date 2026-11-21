import io
import sqlite3
from types import SimpleNamespace
import pytest
from PIL import Image
from auth import AuthStore
import lab4
from predict import predict_image


def test_password_hash_persistence_and_logout(tmp_path):
    path=tmp_path/'users.db'
    auth=AuthStore(path)
    assert auth.register(123,'correct-password')
    assert not auth.register(123,'other-password')
    with sqlite3.connect(path) as db:
        stored=db.execute('SELECT password_hash FROM users').fetchone()[0]
    assert stored!='correct-password' and 'correct-password' not in stored
    other=AuthStore(path)
    assert not other.login(123,'wrong')
    assert other.login(123,'correct-password')
    assert other.authenticated(123)
    other.logout(123)
    assert not other.authenticated(123)


def test_login_rate_limit(tmp_path):
    auth=AuthStore(tmp_path/'users.db')
    auth.register(123,'correct-password')
    for _ in range(5): assert not auth.login(123,'wrong')
    assert not auth.login(123,'correct-password')


def test_webhook_requires_secret(monkeypatch):
    monkeypatch.setattr(lab4,'WEBHOOK_SECRET','test-secret')
    client=lab4.get_flask_app().test_client()
    assert client.post('/webhook',json={'update_id':1}).status_code==403
    assert client.post('/webhook',json={},headers={'X-Telegram-Bot-Api-Secret-Token':'wrong'}).status_code==403
    monkeypatch.setattr(lab4,'get_bot',lambda: SimpleNamespace(process_new_updates=lambda x: None))
    assert client.post('/webhook',json={},headers={'X-Telegram-Bot-Api-Secret-Token':'test-secret'}).status_code==400
    assert client.post('/webhook',json={'update_id':1},headers={'X-Telegram-Bot-Api-Secret-Token':'test-secret'}).status_code==200


def test_predict_rejects_bad_input():
    with pytest.raises(ValueError): predict_image(b'')
    with pytest.raises(ValueError): predict_image(b'x'*(8*1024*1024+1))
    with pytest.raises(Exception): predict_image(b'not an image')


def test_real_weights_inference():
    image=io.BytesIO()
    Image.new('RGB',(64,64),'white').save(image,format='PNG')
    assert predict_image(image.getvalue()) in ('horses','humans')


def test_predict_requires_authentication(monkeypatch):
    replies=[]
    monkeypatch.setattr(lab4,'get_auth',lambda: SimpleNamespace(authenticated=lambda user: False))
    monkeypatch.setattr(lab4,'_safe_send',lambda chat,text: replies.append(text))
    monkeypatch.setattr(lab4,'predict_image',lambda data: pytest.fail('Inference без входа'))
    message=SimpleNamespace(from_user=SimpleNamespace(id=1),chat=SimpleNamespace(id=1),photo=[object()])
    lab4.handle_predict(message)
    assert replies and 'не авторизованы' in replies[0]


def test_missing_token_fails_before_network(monkeypatch):
    monkeypatch.setattr(lab4,'bot',None)
    monkeypatch.setattr(lab4,'TOKEN','')
    with pytest.raises(ValueError,match='TOKEN'): lab4.get_bot()
