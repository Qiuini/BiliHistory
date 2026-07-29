#pragma once

#include <QByteArray>
#include <QString>

namespace bili {

class Crypto {
public:
    struct GcmEnvelope {
        QByteArray salt;
        QByteArray iv;
        QByteArray tag;
        QByteArray ciphertext;
    };

    static constexpr int DefaultPbkdf2Iterations = 120'000;
    static constexpr int AesKeyLength = 32;
    static constexpr int GcmIvLength = 12;
    static constexpr int GcmTagLength = 16;
    static constexpr int SaltLength = 16;

    static QByteArray randomBytes(int length);

    static QByteArray pbkdf2(const QByteArray& password,
                             const QByteArray& salt,
                             int iterations = DefaultPbkdf2Iterations,
                             int keyLength = AesKeyLength);

    static GcmEnvelope encryptGcm(const QByteArray& plaintext, const QByteArray& key);
    static QByteArray decryptGcm(const GcmEnvelope& envelope, const QByteArray& key);

    static QString base64UrlEncode(const QByteArray& data);
    static QByteArray base64UrlDecode(const QString& str);

    static QByteArray hmacSha256(const QByteArray& key, const QByteArray& message);

private:
    Crypto() = delete;
};

} // namespace bili
