"""
开发者工具：签发激活码（收款后运行，生成激活码发给用户）

用法示例：
    # 月付会员（30 天）
    python tools/issue_license.py --type month
    # 月付，自定义天数
    python tools/issue_license.py --type month --days 31
    # 买断（永久）
    python tools/issue_license.py --type buyout
    # 绑定用户机器码（用户在“激活”对话框可看到自己的机器码）
    python tools/issue_license.py --type buyout --machine 1a2b3c4d5e6f7890

输出：一行激活码字符串，直接发给用户。
"""
import argparse
import base64
import json
import time
import uuid
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

PRIVATE_KEY_PATH = Path(__file__).parent / "private_key.pem"


def _b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _load_private_key() -> Ed25519PrivateKey:
    if not PRIVATE_KEY_PATH.exists():
        raise SystemExit(f"未找到私钥 {PRIVATE_KEY_PATH}，请先运行 python tools/keygen.py")
    key = serialization.load_pem_private_key(PRIVATE_KEY_PATH.read_bytes(), password=None)
    if not isinstance(key, Ed25519PrivateKey):
        raise SystemExit("私钥类型不是 Ed25519")
    return key


def main() -> None:
    parser = argparse.ArgumentParser(description="签发离线激活码")
    parser.add_argument("--type", choices=["month", "buyout"], required=True, help="授权类型")
    parser.add_argument("--days", type=int, default=30, help="月付天数（buyout 忽略），默认 30")
    parser.add_argument("--machine", default=None, help="绑定的机器码（可选，不填则不绑定）")
    parser.add_argument("--id", dest="lid", default=None, help="许可 ID（可选，默认随机）")
    args = parser.parse_args()

    now = int(time.time())
    exp = None if args.type == "buyout" else now + args.days * 86400
    payload = {
        "lid": args.lid or uuid.uuid4().hex[:12],
        "typ": args.type,
        "iat": now,
        "exp": exp,
        "mid": args.machine,
    }
    payload_bytes = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")

    private_key = _load_private_key()
    signature = private_key.sign(payload_bytes)
    code = f"{_b64url(payload_bytes)}.{_b64url(signature)}"

    print("=" * 60)
    print(f"类型: {args.type}   到期: {'永久' if exp is None else time.strftime('%Y-%m-%d %H:%M', time.localtime(exp))}"
          + (f"   绑定机器码: {args.machine}" if args.machine else "   未绑定机器码"))
    print(f"许可ID: {payload['lid']}")
    print("激活码（发给用户）：")
    print(code)
    print("=" * 60)


if __name__ == "__main__":
    main()
