#include "crypto.h"
#include "exceptions.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

namespace bili {

QByteArray Crypto::randomBytes(int length)
{
    QByteArray out(length, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), length) != 1) {
        throw LicensingException(QStringLiteral("随机字节生成失败"));
    }
    return out;
}

QByteArray Crypto::pbkdf2(const QByteArray& password,
                          const QByteArray& salt,
                          int iterations,
                          int keyLength)
{
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
    if (!kdf) {
        throw LicensingException(QStringLiteral("PBKDF2 KDF 不可用"));
    }

    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!ctx) {
        throw LicensingException(QStringLiteral("创建 PBKDF2 上下文失败"));
    }

    int iter = iterations;
    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
    params[1] = OSSL_PARAM_construct_octet_string("salt",
                                                  const_cast<char*>(salt.constData()),
                                                  static_cast<size_t>(salt.size()));
    params[2] = OSSL_PARAM_construct_int("iter", &iter);
    params[3] = OSSL_PARAM_construct_octet_string("pass",
                                                  const_cast<char*>(password.constData()),
                                                  static_cast<size_t>(password.size()));
    params[4] = OSSL_PARAM_construct_end();

    QByteArray out(keyLength, 0);
    if (EVP_KDF_derive(ctx,
                       reinterpret_cast<unsigned char*>(out.data()),
                       static_cast<size_t>(keyLength),
                       params) != 1) {
        EVP_KDF_CTX_free(ctx);
        throw LicensingException(QStringLiteral("PBKDF2 密钥派生失败"));
    }

    EVP_KDF_CTX_free(ctx);
    return out;
}

Crypto::GcmEnvelope Crypto::encryptGcm(const QByteArray& plaintext, const QByteArray& key)
{
    if (key.size() != AesKeyLength) {
        throw LicensingException(QStringLiteral("AES-256-GCM 密钥长度必须为 32 字节"));
    }

    GcmEnvelope env;
    env.iv = randomBytes(GcmIvLength);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw LicensingException(QStringLiteral("创建加密上下文失败"));
    }

    if (EVP_EncryptInit_ex2(ctx,
                            EVP_aes_256_gcm(),
                            reinterpret_cast<const unsigned char*>(key.constData()),
                            reinterpret_cast<const unsigned char*>(env.iv.constData()),
                            nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 加密初始化失败"));
    }

    env.ciphertext.resize(plaintext.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(env.ciphertext.data()),
                          &len,
                          reinterpret_cast<const unsigned char*>(plaintext.constData()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 加密失败"));
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char*>(env.ciphertext.data()) + len,
                            &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 加密收尾失败"));
    }
    env.ciphertext.resize(len + finalLen);

    env.tag.resize(GcmTagLength);
    if (EVP_CIPHER_CTX_ctrl(ctx,
                            EVP_CTRL_GCM_GET_TAG,
                            GcmTagLength,
                            reinterpret_cast<unsigned char*>(env.tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 获取认证标签失败"));
    }

    EVP_CIPHER_CTX_free(ctx);
    return env;
}

QByteArray Crypto::decryptGcm(const GcmEnvelope& envelope, const QByteArray& key)
{
    if (key.size() != AesKeyLength) {
        throw LicensingException(QStringLiteral("AES-256-GCM 密钥长度必须为 32 字节"));
    }
    if (envelope.iv.size() != GcmIvLength || envelope.tag.size() != GcmTagLength) {
        throw LicensingException(QStringLiteral("GCM 密文信封格式异常"));
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw LicensingException(QStringLiteral("创建解密上下文失败"));
    }

    if (EVP_DecryptInit_ex2(ctx,
                            EVP_aes_256_gcm(),
                            reinterpret_cast<const unsigned char*>(key.constData()),
                            reinterpret_cast<const unsigned char*>(envelope.iv.constData()),
                            nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 解密初始化失败"));
    }

    if (EVP_CIPHER_CTX_ctrl(ctx,
                            EVP_CTRL_GCM_SET_TAG,
                            GcmTagLength,
                            const_cast<void*>(reinterpret_cast<const void*>(envelope.tag.constData()))) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 设置认证标签失败"));
    }

    QByteArray plaintext(envelope.ciphertext.size(), 0);
    int len = 0;
    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(plaintext.data()),
                          &len,
                          reinterpret_cast<const unsigned char*>(envelope.ciphertext.constData()),
                          envelope.ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 解密失败"));
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char*>(plaintext.data()) + len,
                            &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw LicensingException(QStringLiteral("GCM 解密认证失败"));
    }
    plaintext.resize(len + finalLen);

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

QString Crypto::base64UrlEncode(const QByteArray& data)
{
    return QString::fromUtf8(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray Crypto::base64UrlDecode(const QString& str)
{
    QByteArray padded = str.toUtf8();
    const int pad = 4 - (padded.size() % 4);
    if (pad != 4) {
        padded.append(pad, '=');
    }
    return QByteArray::fromBase64(padded, QByteArray::Base64UrlEncoding);
}

QByteArray Crypto::hmacSha256(const QByteArray& key, const QByteArray& message)
{
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) {
        throw LicensingException(QStringLiteral("HMAC MAC 不可用"));
    }

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (!ctx) {
        throw LicensingException(QStringLiteral("创建 HMAC 上下文失败"));
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(ctx,
                     reinterpret_cast<const unsigned char*>(key.constData()),
                     static_cast<size_t>(key.size()),
                     params) != 1) {
        EVP_MAC_CTX_free(ctx);
        throw LicensingException(QStringLiteral("HMAC 初始化失败"));
    }

    if (EVP_MAC_update(ctx,
                       reinterpret_cast<const unsigned char*>(message.constData()),
                       static_cast<size_t>(message.size())) != 1) {
        EVP_MAC_CTX_free(ctx);
        throw LicensingException(QStringLiteral("HMAC 更新失败"));
    }

    QByteArray out(32, 0);
    size_t outLen = 0;
    if (EVP_MAC_final(ctx,
                      reinterpret_cast<unsigned char*>(out.data()),
                      &outLen,
                      static_cast<size_t>(out.size())) != 1) {
        EVP_MAC_CTX_free(ctx);
        throw LicensingException(QStringLiteral("HMAC 收尾失败"));
    }
    out.resize(static_cast<int>(outLen));

    EVP_MAC_CTX_free(ctx);
    return out;
}

} // namespace bili
