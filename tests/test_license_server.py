"""授权服务器测试：首次激活记录机器码与 IP，换机器拒绝"""
import json
import os
import sys
import tempfile
import threading
from http.server import HTTPServer

import pytest
import requests
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))

from licensing import keys, license_manager as lm
import license_server


@pytest.fixture
def signing_key(monkeypatch):
    """生成临时 Ed25519 密钥对，注入服务器与客户端；固定本机机器码便于测试"""
    priv = Ed25519PrivateKey.generate()
    pub_hex = priv.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    ).hex()
    monkeypatch.setattr(keys, "PUBLIC_KEY_HEX", pub_hex)
    monkeypatch.setattr(lm, "machine_id", lambda: "machine_a")
    license_server._set_private_key(priv)
    return priv


@pytest.fixture
def tmp_license(monkeypatch, tmp_path):
    monkeypatch.setattr(lm.paths, "license_file", lambda: tmp_path / "license.dat")


@pytest.fixture
def running_server(signing_key, tmp_path):
    """启动一个临时授权服务器，返回 base_url 与 admin_key"""
    admin_key = "test-admin-key-123"
    db_path = tmp_path / "licenses_test.db"
    store = license_server.LicenseStore(str(db_path))
    class Handler(license_server._Handler):
        pass

    Handler.store = store
    Handler.admin_key = admin_key

    server = HTTPServer(("127.0.0.1", 0), Handler)
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    yield f"http://127.0.0.1:{port}", admin_key
    server.shutdown()


def test_issue_and_activate(running_server, tmp_license):
    url, admin_key = running_server

    # 管理员签发一个激活码
    resp = requests.post(
        f"{url}/admin/issue",
        headers={"X-Admin-Key": admin_key},
        json={"type": "buyout", "count": 1},
        timeout=5,
    )
    assert resp.status_code == 200
    lic_key = resp.json()["keys"][0]

    # 客户端首次激活
    resp = requests.post(
        f"{url}/api/activate",
        json={"license_key": lic_key, "machine_id": "machine_a"},
        timeout=5,
    )
    assert resp.status_code == 200
    data = resp.json()
    assert data["success"]
    code = data["code"]
    assert "." in code

    # 本地离线验签应通过
    info = lm.activate(code)
    assert info is not None
    assert info.typ == "buyout"


def test_second_machine_rejected(running_server):
    url, admin_key = running_server

    resp = requests.post(
        f"{url}/admin/issue",
        headers={"X-Admin-Key": admin_key},
        json={"type": "month", "count": 1},
        timeout=5,
    )
    lic_key = resp.json()["keys"][0]

    # 机器 A 激活成功
    r1 = requests.post(
        f"{url}/api/activate",
        json={"license_key": lic_key, "machine_id": "machine_a"},
        timeout=5,
    )
    assert r1.status_code == 200

    # 机器 B 用同一份 key 激活应被拒绝
    r2 = requests.post(
        f"{url}/api/activate",
        json={"license_key": lic_key, "machine_id": "machine_b"},
        timeout=5,
    )
    assert r2.status_code == 403
    assert r2.json()["error"] == "MACHINE_MISMATCH"


def test_verify_endpoint(running_server):
    url, admin_key = running_server

    resp = requests.post(
        f"{url}/admin/issue",
        headers={"X-Admin-Key": admin_key},
        json={"type": "buyout", "count": 1},
        timeout=5,
    )
    lic_key = resp.json()["keys"][0]

    requests.post(
        f"{url}/api/activate",
        json={"license_key": lic_key, "machine_id": "machine_a"},
        timeout=5,
    )

    resp = requests.post(
        f"{url}/api/verify",
        json={"license_key": lic_key, "machine_id": "machine_a"},
        timeout=5,
    )
    data = resp.json()
    assert data["activated"]
    assert data["machine_id"] == "machine_a"
    assert data["ip_address"] == "127.0.0.1"
    assert data["machine_match"]


def test_admin_unauthorized(running_server):
    url, _ = running_server
    resp = requests.post(
        f"{url}/admin/issue",
        headers={"X-Admin-Key": "wrong-key"},
        json={"type": "buyout", "count": 1},
        timeout=5,
    )
    assert resp.status_code == 401
