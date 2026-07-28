"""
开发者工具：生成 Ed25519 密钥对（仅需运行一次）

用法：
    python tools/keygen.py

产物：
    - tools/private_key.pem   私钥（务必保密、勿入库、勿打包进客户端）
    - 终端打印公钥 hex，将其粘贴到 src/licensing/keys.py 的 PUBLIC_KEY_HEX

若 tools/private_key.pem 已存在，默认不覆盖（除非加 --force）。
"""
import argparse
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

PRIVATE_KEY_PATH = Path(__file__).parent / "private_key.pem"


def main() -> None:
    parser = argparse.ArgumentParser(description="生成 Ed25519 授权密钥对")
    parser.add_argument("--force", action="store_true", help="覆盖已存在的私钥")
    args = parser.parse_args()

    if PRIVATE_KEY_PATH.exists() and not args.force:
        print(f"[跳过] 私钥已存在: {PRIVATE_KEY_PATH}")
        print("       如需重新生成请加 --force（注意：旧激活码将全部失效）")
        return

    private_key = Ed25519PrivateKey.generate()
    pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    PRIVATE_KEY_PATH.write_bytes(pem)

    public_hex = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    ).hex()

    print(f"[完成] 私钥已写入: {PRIVATE_KEY_PATH}")
    print("=" * 60)
    print("请将下面这行公钥粘贴到 src/licensing/keys.py 的 PUBLIC_KEY_HEX：")
    print(f'PUBLIC_KEY_HEX = "{public_hex}"')
    print("=" * 60)


if __name__ == "__main__":
    main()
