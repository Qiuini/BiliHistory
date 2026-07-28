"""授权模块测试：激活码验签、过期、机器码绑定、试用时间"""
import base64
import json
import time
from datetime import datetime, timedelta

import pytest
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

from licensing import keys, license_manager as lm, trial


def _b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _make_code(priv: Ed25519PrivateKey, *, typ="buyout", exp=None, mid=None, lid="test123"):
    payload = {"lid": lid, "typ": typ, "iat": int(time.time()), "exp": exp, "mid": mid}
    payload_bytes = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    sig = priv.sign(payload_bytes)
    return f"{_b64url(payload_bytes)}.{_b64url(sig)}"


@pytest.fixture
def signing_key(monkeypatch):
    """生成临时密钥对，把公钥注入内置常量，返回私钥用于签发测试码"""
    priv = Ed25519PrivateKey.generate()
    from cryptography.hazmat.primitives import serialization
    pub_hex = priv.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    ).hex()
    monkeypatch.setattr(keys, "PUBLIC_KEY_HEX", pub_hex)
    return priv


@pytest.fixture
def tmp_license(monkeypatch, tmp_path):
    monkeypatch.setattr(lm.paths, "license_file", lambda: tmp_path / "license.dat")


@pytest.fixture
def tmp_trial(monkeypatch, tmp_path):
    monkeypatch.setattr(trial.paths, "trial_file", lambda: tmp_path / "trial.dat")


# ---------------- 验签 ----------------
def test_valid_code_verifies(signing_key):
    code = _make_code(signing_key, typ="buyout")
    info = lm.verify_code(code)
    assert info is not None
    assert info.typ == "buyout"
    assert info.is_buyout
    assert info.exp is None


def test_tampered_code_rejected(signing_key):
    code = _make_code(signing_key, typ="buyout")
    tampered = code[:-3] + ("aaa" if not code.endswith("aaa") else "bbb")
    assert lm.verify_code(tampered) is None


def test_expired_code_rejected(signing_key):
    code = _make_code(signing_key, typ="month", exp=int(time.time()) - 10)
    assert lm.verify_code(code) is None


def test_wrong_public_key_rejected(signing_key, monkeypatch):
    code = _make_code(signing_key, typ="buyout")
    # 换成另一把公钥
    other = Ed25519PrivateKey.generate()
    from cryptography.hazmat.primitives import serialization
    monkeypatch.setattr(keys, "PUBLIC_KEY_HEX", other.public_key().public_bytes(
        encoding=serialization.Encoding.Raw, format=serialization.PublicFormat.Raw).hex())
    assert lm.verify_code(code) is None


# ---------------- 机器码绑定 ----------------
def test_machine_bound_mismatch_rejected(signing_key, tmp_license):
    code = _make_code(signing_key, typ="buyout", mid="deadbeefdeadbeef")
    # 验签本身通过，但激活时机器码不符
    assert lm.verify_code(code) is not None
    assert lm.activate(code) is None
    assert lm.current_license() is None


def test_machine_bound_match_ok(signing_key, tmp_license):
    code = _make_code(signing_key, typ="buyout", mid=lm.machine_id())
    assert lm.activate(code) is not None
    assert lm.current_license() is not None


def test_activate_and_persist(signing_key, tmp_license):
    code = _make_code(signing_key, typ="buyout")
    assert not lm.is_licensed()
    assert lm.activate(code) is not None
    assert lm.is_licensed()


# ---------------- 试用时间 ----------------
def test_trial_active_initially(tmp_trial):
    assert trial.remaining_days() == trial.TRIAL_DAYS
    assert trial.is_active()


def test_trial_expires_after_30_days(tmp_trial, monkeypatch):
    start = (datetime.now() - timedelta(days=31)).strftime("%Y-%m-%dT%H:%M:%S")
    monkeypatch.setattr(trial, "_read", lambda: {"start": start, "tampered": False})
    assert trial.remaining_days() == 0
    assert not trial.is_active()


def test_trial_tamper_detected(tmp_trial):
    trial.consume()
    tf = trial.paths.trial_file()
    # 手动篡改 start 但不更新签名
    import json as _json
    data = _json.loads(tf.read_text(encoding="utf-8"))
    data["start"] = "2099-01-01T00:00:00"
    tf.write_text(_json.dumps(data), encoding="utf-8")
    # 篡改后视为已过期
    assert trial.remaining_days() == 0
    assert trial.consume() == 0
