"""
在线激活客户端 - 连接授权服务器完成首次激活并获取签名激活码

流程：
1. 客户端把 license_key + machine_id 提交到服务器
2. 服务器记录机器码与 IP，并返回 Ed25519 签名的激活码
3. 客户端用现有 license_manager.activate() 保存并离线验签

失败后自动回退到离线激活码输入方式。
"""
import json
from typing import Optional
from urllib.error import URLError
from urllib.request import Request, urlopen

from licensing import license_manager as lm
from logger import logger


def activate_online(server_url: str, license_key: str,
                    machine_id: Optional[str] = None,
                    timeout: int = 10) -> Optional[str]:
    """在线激活；成功返回签名的离线激活码，失败返回 None

    Args:
        server_url: 授权服务器地址，例如 http://127.0.0.1:8787
        license_key: 服务器发放的激活码（如 XXXX-XXXX-XXXX）
        machine_id: 本机机器码，默认自动获取
        timeout: 请求超时秒数
    """
    if not server_url or not license_key:
        return None

    mid = machine_id or lm.machine_id()
    url = server_url.rstrip("/") + "/api/activate"
    payload = json.dumps({
        "license_key": license_key.strip(),
        "machine_id": mid,
    }, ensure_ascii=False).encode("utf-8")

    req = Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except URLError as e:
        logger.warning(f"连接授权服务器失败: {e}")
        return None
    except Exception as e:
        logger.warning(f"在线激活请求异常: {e}")
        return None

    if not data.get("success"):
        logger.warning(f"在线激活被拒绝: {data.get('error')}")
        return None

    code = data.get("code", "").strip()
    if not code:
        return None

    # 用本地离线逻辑再做一次完整校验（验签 + 机器码绑定）
    info = lm.activate(code)
    if info is None:
        logger.warning("服务器返回的激活码本地校验失败")
        return None
    return code
