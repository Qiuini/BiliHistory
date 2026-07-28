"""
敏感信息加密存储 - 基于机器码派生密钥

把 Cookie 等敏感信息以加密形式保存到 .secrets.json，
密钥由本机机器码通过 PBKDF2 派生，因此：
- 同一份加密文件换机器后无法解密（除非机器码相同）。
- 其他程序即使拿到 .secrets.json，也无法直接读取明文 Cookie。

加密格式：
    {
        "_enc": "fernet-v1",
        "data": "<base64(salt + ciphertext)>"
    }

如果检测到旧版明文 .secrets.json，会自动读取并在保存时迁移为加密格式。
"""
import base64
import json
import os
from pathlib import Path
from typing import Any

from cryptography.fernet import Fernet, InvalidToken
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

from licensing import license_manager as lm
from logger import logger

SALT_LEN = 16
ITERATIONS = 120_000
FORMAT_TAG = "fernet-v1"


def _derive_key(machine_id: str, salt: bytes) -> bytes:
    """用机器码 + 盐派生 Fernet 密钥"""
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=32,
        salt=salt,
        iterations=ITERATIONS,
    )
    return base64.urlsafe_b64encode(kdf.derive(machine_id.encode("utf-8")))


def _encrypt(plaintext: dict, machine_id: str) -> dict:
    """加密字典，返回带 _enc 标记的存储格式"""
    salt = os.urandom(SALT_LEN)
    key = _derive_key(machine_id, salt)
    payload = json.dumps(plaintext, ensure_ascii=False).encode("utf-8")
    ciphertext = Fernet(key).encrypt(payload)
    data = base64.urlsafe_b64encode(salt + ciphertext).decode("ascii")
    return {"_enc": FORMAT_TAG, "data": data}


def _decrypt(store_obj: dict, machine_id: str) -> dict:
    """解密存储对象；失败返回空字典"""
    if store_obj.get("_enc") != FORMAT_TAG:
        return {}
    raw = base64.urlsafe_b64decode(store_obj.get("data", "").encode("ascii"))
    salt, ciphertext = raw[:SALT_LEN], raw[SALT_LEN:]
    key = _derive_key(machine_id, salt)
    try:
        payload = Fernet(key).decrypt(ciphertext)
        return json.loads(payload.decode("utf-8"))
    except (InvalidToken, ValueError, TypeError) as e:
        logger.warning(f"无法解密敏感信息文件（机器码可能已变更）: {e}")
        return {}


class SecretsStore:
    """加密敏感信息存储"""

    def __init__(self, path: str | Path, machine_id_provider=None):
        self.path = Path(path)
        self._machine_id_provider = machine_id_provider or lm.machine_id

    def _machine_id(self) -> str:
        return self._machine_id_provider()

    def load(self) -> dict[str, Any]:
        """加载敏感信息；兼容旧版明文"""
        if not self.path.is_file():
            return {}

        try:
            with open(self.path, "r", encoding="utf-8") as f:
                content = json.load(f)
        except (json.JSONDecodeError, UnicodeDecodeError):
            logger.warning("敏感信息文件格式异常，按空处理")
            return {}
        except FileNotFoundError:
            return {}

        if not isinstance(content, dict):
            return {}

        # 新版加密格式
        if "_enc" in content:
            return _decrypt(content, self._machine_id())

        # 旧版明文：直接读取，下次保存时自动迁移
        return content

    def save(self, data: dict[str, Any]) -> None:
        """保存敏感信息（加密）"""
        self.path.parent.mkdir(parents=True, exist_ok=True)
        encrypted = _encrypt(data, self._machine_id())
        with open(self.path, "w", encoding="utf-8") as f:
            json.dump(encrypted, f, ensure_ascii=False, indent=2)

    def get(self, key: str, default: Any = None) -> Any:
        return self.load().get(key, default)

    def set(self, key: str, value: Any) -> None:
        data = self.load()
        data[key] = value
        self.save(data)
