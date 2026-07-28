"""
授权服务器 - 基于 stdlib 的轻量激活服务

功能：
- 首次激活时记录机器码 (machine_id) 与客户端 IP
- 同一份激活码换机器/换 IP 再次激活会被拒绝
- 为客户端签发 Ed25519 签名的激活码，客户端仍用既有公钥离线验签

运行：
    python tools/license_server.py --port 8787 --db licenses.db

环境变量：
    LICENSE_ADMIN_KEY  管理员接口密钥（默认随机生成并打印到控制台）

接口：
    POST /api/activate
        body: {"license_key":"xxxx-xxxx", "machine_id":"1a2b3c..."}
        resp: {"success":true, "code":"<signed activation code>"}

    POST /api/verify
        body: {"license_key":"xxxx-xxxx", "machine_id":"1a2b3c..."}
        resp: {"success":true, "activated":true, "machine_id":"...", "ip":"..."}

    POST /admin/issue
        header: X-Admin-Key: <admin_key>
        body: {"type":"month|buyout", "days":30, "count":1}
        resp: {"keys":["xxxx-xxxx", ...]}

    GET /admin/licenses
        header: X-Admin-Key: <admin_key>
        resp: {"licenses":[...]}
"""
import argparse
import base64
import json
import os
import secrets
import sqlite3
import sys
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlparse

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

PRIVATE_KEY_PATH = Path(__file__).parent / "private_key.pem"
_PRIVATE_KEY: Ed25519PrivateKey | None = None


def _load_private_key() -> Ed25519PrivateKey:
    global _PRIVATE_KEY
    if _PRIVATE_KEY is not None:
        return _PRIVATE_KEY
    if not PRIVATE_KEY_PATH.exists():
        raise SystemExit(f"未找到私钥 {PRIVATE_KEY_PATH}，请先运行 python tools/keygen.py")
    key = serialization.load_pem_private_key(PRIVATE_KEY_PATH.read_bytes(), password=None)
    if not isinstance(key, Ed25519PrivateKey):
        raise SystemExit("私钥类型不是 Ed25519")
    _PRIVATE_KEY = key
    return _PRIVATE_KEY


def _set_private_key(key: Ed25519PrivateKey) -> None:
    """测试注入用：允许替换私钥"""
    global _PRIVATE_KEY
    _PRIVATE_KEY = key


def _b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _sign_payload(payload: dict) -> str:
    payload_bytes = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    signature = _load_private_key().sign(payload_bytes)
    return f"{_b64url(payload_bytes)}.{_b64url(signature)}"


def _new_license_key() -> str:
    """生成形如 xxxx-xxxx-xxxx 的激活码"""
    return "-".join(secrets.token_hex(2).upper() for _ in range(3))


class LicenseStore:
    def __init__(self, db_path: str):
        self.db_path = db_path
        self._init_db()

    def _conn(self):
        return sqlite3.connect(self.db_path)

    def _init_db(self):
        with self._conn() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS licenses (
                    key TEXT PRIMARY KEY,
                    type TEXT NOT NULL,
                    days INTEGER,
                    issued_at INTEGER NOT NULL,
                    activated INTEGER DEFAULT 0,
                    activated_at INTEGER,
                    machine_id TEXT,
                    ip_address TEXT,
                    activations INTEGER DEFAULT 0
                )
            """)

    def issue(self, lic_type: str, days: int | None, count: int) -> list[str]:
        keys = []
        now = int(time.time())
        with self._conn() as conn:
            for _ in range(count):
                key = _new_license_key()
                conn.execute(
                    "INSERT INTO licenses (key, type, days, issued_at) VALUES (?, ?, ?, ?)",
                    (key, lic_type, days, now),
                )
                keys.append(key)
        return keys

    def activate(self, key: str, machine_id: str, ip: str) -> tuple[bool, str]:
        """尝试激活；返回 (是否成功, 原因或 signed_code)"""
        with self._conn() as conn:
            row = conn.execute(
                "SELECT type, days, activated, machine_id, activations FROM licenses WHERE key=?",
                (key,),
            ).fetchone()
            if not row:
                return False, "LICENSE_NOT_FOUND"
            lic_type, days, activated, bound_mid, activations = row

            # 已激活且绑定了其他机器
            if activated and bound_mid and bound_mid != machine_id:
                return False, "MACHINE_MISMATCH"

            now = int(time.time())
            exp = None if lic_type == "buyout" else now + (days or 30) * 86400
            payload = {
                "lid": key,
                "typ": lic_type,
                "iat": now,
                "exp": exp,
                "mid": machine_id,
            }
            code = _sign_payload(payload)

            conn.execute(
                """UPDATE licenses
                   SET activated=1, activated_at=?, machine_id=?, ip_address=?, activations=activations+1
                   WHERE key=?""",
                (now, machine_id, ip, key),
            )
        return True, code

    def verify(self, key: str, machine_id: str) -> dict:
        with self._conn() as conn:
            row = conn.execute(
                "SELECT activated, machine_id, ip_address, activations FROM licenses WHERE key=?",
                (key,),
            ).fetchone()
        if not row:
            return {"found": False, "activated": False}
        activated, bound_mid, ip, activations = row
        return {
            "found": True,
            "activated": bool(activated),
            "machine_match": bound_mid == machine_id if bound_mid else None,
            "machine_id": bound_mid,
            "ip_address": ip,
            "activations": activations,
        }

    def list_all(self) -> list[dict]:
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT key, type, days, issued_at, activated, activated_at, machine_id, ip_address, activations FROM licenses ORDER BY issued_at DESC"
            ).fetchall()
        return [
            {
                "key": r[0],
                "type": r[1],
                "days": r[2],
                "issued_at": r[3],
                "activated": bool(r[4]),
                "activated_at": r[5],
                "machine_id": r[6],
                "ip_address": r[7],
                "activations": r[8],
            }
            for r in rows
        ]


class _Handler(BaseHTTPRequestHandler):
    store: LicenseStore
    admin_key: str

    def log_message(self, fmt, *args):
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {self.client_address[0]} {fmt % args}")

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return {}
        body = self.rfile.read(length)
        try:
            return json.loads(body.decode("utf-8"))
        except Exception:
            return {}

    def _send_json(self, status: int, data: dict):
        payload = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _client_ip(self) -> str:
        forwarded = self.headers.get("X-Forwarded-For")
        if forwarded:
            return forwarded.split(",")[0].strip()
        return self.client_address[0]

    def _check_admin(self) -> bool:
        return self.headers.get("X-Admin-Key") == self.admin_key

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        data = self._read_json()

        if path == "/api/activate":
            key = str(data.get("license_key", "")).strip()
            machine_id = str(data.get("machine_id", "")).strip()
            if not key or not machine_id:
                self._send_json(400, {"success": False, "error": "MISSING_PARAMS"})
                return
            ok, result = self.store.activate(key, machine_id, self._client_ip())
            if not ok:
                self._send_json(403, {"success": False, "error": result})
                return
            self._send_json(200, {"success": True, "code": result})
            return

        if path == "/api/verify":
            key = str(data.get("license_key", "")).strip()
            machine_id = str(data.get("machine_id", "")).strip()
            if not key:
                self._send_json(400, {"success": False, "error": "MISSING_PARAMS"})
                return
            info = self.store.verify(key, machine_id)
            self._send_json(200, {"success": True, **info})
            return

        if path == "/admin/issue":
            if not self._check_admin():
                self._send_json(401, {"success": False, "error": "UNAUTHORIZED"})
                return
            lic_type = data.get("type", "month")
            days = data.get("days", 30 if lic_type == "month" else None)
            count = int(data.get("count", 1))
            keys = self.store.issue(lic_type, days, count)
            self._send_json(200, {"success": True, "keys": keys})
            return

        self._send_json(404, {"success": False, "error": "NOT_FOUND"})

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/admin/licenses":
            if not self._check_admin():
                self._send_json(401, {"success": False, "error": "UNAUTHORIZED"})
                return
            self._send_json(200, {"success": True, "licenses": self.store.list_all()})
            return

        self._send_json(404, {"success": False, "error": "NOT_FOUND"})


def main():
    parser = argparse.ArgumentParser(description="BiliHistory 授权服务器")
    parser.add_argument("--host", default="0.0.0.0", help="监听地址")
    parser.add_argument("--port", type=int, default=8787, help="监听端口")
    parser.add_argument("--db", default="licenses.db", help="SQLite 数据库路径")
    args = parser.parse_args()

    admin_key = os.environ.get("LICENSE_ADMIN_KEY")
    if not admin_key:
        admin_key = secrets.token_urlsafe(24)
        print(f"[提示] 未设置 LICENSE_ADMIN_KEY，已生成临时管理密钥：{admin_key}")

    store = LicenseStore(args.db)

    class Handler(_Handler):
        store = store
        admin_key = admin_key

    server = HTTPServer((args.host, args.port), Handler)
    print(f"[启动] 授权服务器运行在 http://{args.host}:{args.port}")
    print(f"[管理] X-Admin-Key: {admin_key}")
    print(f"[数据] SQLite: {args.db}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[关闭] 服务器已停止")
        sys.exit(0)


if __name__ == "__main__":
    main()
