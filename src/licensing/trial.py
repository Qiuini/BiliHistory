"""
试用时间门禁 - 首次使用起 30 天内免费

存储 paths.trial_file()：{"start": iso, "sig": hex}
sig = HMAC_SHA256(TRIAL_HMAC_SECRET, start)；读取时校验，签名不符视为已过期。

限制：纯本地时间可被改系统时间/删文件绕过，首发可接受。
"""
import hashlib
import hmac
import json
import time
from datetime import datetime, timedelta
from pathlib import Path

import paths
from logger import logger
from licensing import keys

TRIAL_DAYS = 30  # 免费试用天数


def _sign(start: str) -> str:
    msg = start.encode("utf-8")
    return hmac.new(keys.TRIAL_HMAC_SECRET, msg, hashlib.sha256).hexdigest()


def _now() -> str:
    return datetime.now().strftime("%Y-%m-%dT%H:%M:%S")


def _parse_iso(value: str) -> datetime:
    return datetime.strptime(value, "%Y-%m-%dT%H:%M:%S")


def _read() -> dict:
    """读取并校验试用记录；文件不存在返回初始态，篡改返回已过期态"""
    tf = paths.trial_file()
    if not Path(tf).is_file():
        return {"start": _now(), "tampered": False}
    try:
        data = json.loads(Path(tf).read_text(encoding="utf-8"))
        start = str(data.get("start") or data.get("first_run") or _now())
        sig = str(data.get("sig", ""))
    except Exception:
        logger.warning("试用记录损坏，判定为已过期")
        return {"start": _now(), "tampered": True}

    if not hmac.compare_digest(sig, _sign(start)):
        logger.warning("试用记录签名不符（疑似篡改），判定为已过期")
        return {"start": start, "tampered": True}
    return {"start": start, "tampered": False}


def _write(start: str) -> None:
    data = {"start": start, "sig": _sign(start)}
    try:
        paths.trial_file().write_text(
            json.dumps(data, ensure_ascii=False), encoding="utf-8"
        )
    except Exception as e:
        logger.error(f"写入试用记录失败: {e}")


def remaining_days() -> int:
    """剩余试用天数（<=0 表示已过期）"""
    st = _read()
    if st.get("tampered"):
        return 0
    try:
        start = _parse_iso(st["start"])
    except Exception:
        return 0
    expired_at = start + timedelta(days=TRIAL_DAYS)
    left = (expired_at - datetime.now()).days
    # 首日按 1 天展示，避免 start 当天显示 0
    return max(0, left + 1)


def is_active() -> bool:
    """当前是否处于有效试用期内"""
    return remaining_days() > 0


def consume() -> int:
    """保留兼容旧调用：时间试用不消耗次数，仅初始化试用记录"""
    st = _read()
    if st.get("tampered"):
        return 0
    _write(st["start"])
    return remaining_days()
