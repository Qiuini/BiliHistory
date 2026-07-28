"""
内置密钥常量 - 客户端仅持有【公钥】用于离线验签

- PUBLIC_KEY_HEX: Ed25519 公钥（32 字节的十六进制）。由 tools/keygen.py 生成后回填。
- TRIAL_HMAC_SECRET: 试用计数文件防篡改用的 HMAC 密钥。

安全说明：
- 私钥（tools/private_key.pem）绝不入库、绝不打包进客户端。
- 公钥公开无风险；有它也无法伪造激活码（伪造需私钥签名）。
- TRIAL_HMAC_SECRET 仅用于阻挡普通用户手改试用文件，逆向可提取，属首发可接受范围。
"""

# 由 tools/keygen.py 生成后回填（占位值，未回填时验签一律失败）
PUBLIC_KEY_HEX = "c871299950d15c89917d71152f97033932296a0c4d79d00cd448053afaaad64b"

# 试用计数防篡改密钥（可自行改成任意随机串后重新打包）
TRIAL_HMAC_SECRET = b"bili-history-trial-2026-x7Kq3Zp9"
