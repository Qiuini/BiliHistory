#pragma once

namespace bili::keys {

// 内置 RSA-2048 公钥（PEM 格式），仅用于离线激活码验签。
// 对应私钥保存在 tools/private_key.pem，不得入库或打包进客户端。
extern const char* const PublicKeyPem;

// 试用期记录防篡改 HMAC 密钥。
extern const char* const TrialHmacSecret;

} // namespace bili::keys
