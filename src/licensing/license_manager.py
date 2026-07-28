"""
授权管理 - 离线 Ed25519 激活码验签

激活码格式：  b64url(payload_json) + "." + b64url(signature)
payload 字段： lid(许可ID) / typ(month|buyout) / iat(签发秒) / exp(到期秒,可空) / mid(机器码,可空)

客户端仅用内置公钥验签，无需联网。
"""
import base64
import hashlib
import json
import platform
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

import paths
from logger import logger
from licensing import keys


@dataclass
class LicenseInfo:
    lid: str                      # 许可 ID
    typ: str                      # month | buyout
    iat: int                      # 签发时间（Unix 秒）
    exp: Optional[int] = None     # 到期时间（Unix 秒），None = 永久
    mid: Optional[str] = None     # 绑定机器码，None = 不绑定

    @property
    def is_buyout(self) -> bool:
        return self.typ == "buyout"

    @property
    def is_expired(self) -> bool:
        return self.exp is not None and time.time() > self.exp

    def expiry_text(self) -> str:
        if self.exp is None:
            return "永久"
        return time.strftime("%Y-%m-%d %H:%M", time.localtime(self.exp))


def _b64url_decode(s: str) -> bytes:
    pad = "=" * (-len(s) % 4)
    return base64.urlsafe_b64decode(s + pad)


def machine_id() -> str:
    """生成设备指纹：hostname + MAC + 平台 的 sha256（取前 16 位十六进制）"""
    raw = f"{platform.node()}|{uuid.getnode()}|{platform.system()}|{platform.machine()}"
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:16]


def _public_key() -> Optional[Ed25519PublicKey]:
    hexkey = keys.PUBLIC_KEY_HEX.strip()
    if not hexkey or set(hexkey) == {"0"}:
        logger.error("未回填内置公钥（keys.PUBLIC_KEY_HEX），无法校验激活码")
        return None
    try:
        return Ed25519PublicKey.from_public_bytes(bytes.fromhex(hexkey))
    except Exception as e:
        logger.error(f"内置公钥无效: {e}")
        return None


def verify_code(code: str) -> Optional[LicenseInfo]:
    """验证激活码，通过返回 LicenseInfo，否则返回 None（不校验机器码绑定，仅验签+过期）"""
    if not code or "." not in code:
        return None
    pub = _public_key()
    if pub is None:
        return None
    try:
        payload_b64, sig_b64 = code.strip().split(".", 1)
        payload_bytes = _b64url_decode(payload_b64)
        signature = _b64url_decode(sig_b64)
        pub.verify(signature, payload_bytes)
    except (InvalidSignature, ValueError, Exception) as e:
        logger.warning(f"激活码验签失败: {e}")
        return None

    try:
        data = json.loads(payload_bytes.decode("utf-8"))
        info = LicenseInfo(
            lid=str(data["lid"]),
            typ=str(data["typ"]),
            iat=int(data["iat"]),
            exp=(int(data["exp"]) if data.get("exp") is not None else None),
            mid=(str(data["mid"]) if data.get("mid") else None),
        )
    except (KeyError, ValueError, TypeError) as e:
        logger.warning(f"激活码内容异常: {e}")
        return None

    if info.is_expired:
        logger.warning("激活码已过期")
        return None
    return info


def _matches_machine(info: LicenseInfo) -> bool:
    """机器码绑定校验：未绑定则通过；绑定则需与本机一致"""
    if not info.mid:
        return True
    return info.mid == machine_id()


def activate(code: str) -> Optional[LicenseInfo]:
    """激活：验签通过且机器码匹配则持久化，返回 LicenseInfo，失败返回 None"""
    info = verify_code(code)
    if info is None:
        return None
    if not _matches_machine(info):
        logger.warning("激活码绑定的机器码与本机不符")
        return None
    try:
        paths.license_file().write_text(code.strip(), encoding="utf-8")
        logger.info(f"激活成功：{info.typ}，到期 {info.expiry_text()}")
    except Exception as e:
        logger.error(f"写入授权文件失败: {e}")
        return None
    return info


def current_license() -> Optional[LicenseInfo]:
    """读取本地授权并完整校验（验签 + 过期 + 机器码绑定），有效返回 LicenseInfo"""
    lf = paths.license_file()
    if not Path(lf).is_file():
        return None
    try:
        code = lf.read_text(encoding="utf-8").strip()
    except Exception:
        return None
    info = verify_code(code)
    if info is None:
        return None
    if not _matches_machine(info):
        logger.warning("本地授权的机器码与本机不符，视为无效")
        return None
    return info


def is_licensed() -> bool:
    """当前是否拥有有效付费授权"""
    return current_license() is not None
