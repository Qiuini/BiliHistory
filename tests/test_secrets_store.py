"""敏感信息加密存储测试"""
import json

import pytest

from secrets_store import SecretsStore


@pytest.fixture
def store(tmp_path):
    return SecretsStore(tmp_path / "secrets.json", machine_id_provider=lambda: "test-machine-id")


def test_round_trip(store):
    store.save({"cookie": "test-cookie-value", "extra": 123})
    assert store.get("cookie") == "test-cookie-value"
    assert store.get("extra") == 123


def test_encrypted_on_disk(store):
    store.save({"cookie": "secret"})
    raw = json.loads(store.path.read_text(encoding="utf-8"))
    assert raw.get("_enc") == "fernet-v1"
    assert "data" in raw
    # 密文中不应出现明文 cookie
    assert "secret" not in json.dumps(raw)


def test_wrong_machine_fails(store, tmp_path):
    store.save({"cookie": "secret"})
    evil = SecretsStore(store.path, machine_id_provider=lambda: "different-machine")
    assert evil.get("cookie") is None


def test_plaintext_migration(store):
    """旧版明文 secrets.json 可读取，保存后转为加密"""
    store.path.parent.mkdir(parents=True, exist_ok=True)
    store.path.write_text(json.dumps({"cookie": "plain-cookie"}, ensure_ascii=False), encoding="utf-8")
    assert store.get("cookie") == "plain-cookie"
    store.set("cookie", "plain-cookie")
    raw = json.loads(store.path.read_text(encoding="utf-8"))
    assert raw.get("_enc") == "fernet-v1"
