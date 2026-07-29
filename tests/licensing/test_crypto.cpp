#include <gtest/gtest.h>

#include "crypto.h"
#include "licensing/exceptions.h"

using namespace bili;

TEST(CryptoTest, RandomBytesAreDifferent)
{
    const QByteArray a = Crypto::randomBytes(16);
    const QByteArray b = Crypto::randomBytes(16);
    EXPECT_EQ(a.size(), 16);
    EXPECT_EQ(b.size(), 16);
    EXPECT_NE(a, b);
}

TEST(CryptoTest, Pbkdf2DerivesConsistentKey)
{
    const QByteArray key1 = Crypto::pbkdf2("password", "salt");
    const QByteArray key2 = Crypto::pbkdf2("password", "salt");
    EXPECT_EQ(key1.size(), Crypto::AesKeyLength);
    EXPECT_EQ(key1, key2);
}

TEST(CryptoTest, Pbkdf2DifferentSaltsProduceDifferentKeys)
{
    const QByteArray key1 = Crypto::pbkdf2("password", "salt1");
    const QByteArray key2 = Crypto::pbkdf2("password", "salt2");
    EXPECT_NE(key1, key2);
}

TEST(CryptoTest, GcmRoundTrip)
{
    const QByteArray key = Crypto::randomBytes(Crypto::AesKeyLength);
    const QByteArray plaintext = QByteArrayLiteral("Hello, BiliHistory!");

    const Crypto::GcmEnvelope env = Crypto::encryptGcm(plaintext, key);
    const QByteArray decrypted = Crypto::decryptGcm(env, key);

    EXPECT_EQ(decrypted, plaintext);
}

TEST(CryptoTest, GcmTamperedCiphertextFails)
{
    const QByteArray key = Crypto::randomBytes(Crypto::AesKeyLength);
    const QByteArray plaintext = QByteArrayLiteral("secret");

    Crypto::GcmEnvelope env = Crypto::encryptGcm(plaintext, key);
    env.ciphertext[0] = static_cast<char>(env.ciphertext[0] ^ 0xFF);

    EXPECT_THROW(Crypto::decryptGcm(env, key), LicensingException);
}

TEST(CryptoTest, GcmWrongKeyFails)
{
    const QByteArray key = Crypto::randomBytes(Crypto::AesKeyLength);
    const QByteArray wrongKey = Crypto::randomBytes(Crypto::AesKeyLength);
    const QByteArray plaintext = QByteArrayLiteral("secret");

    const Crypto::GcmEnvelope env = Crypto::encryptGcm(plaintext, key);
    EXPECT_THROW(Crypto::decryptGcm(env, wrongKey), LicensingException);
}

TEST(CryptoTest, Base64UrlRoundTrip)
{
    const QByteArray data = QByteArrayLiteral("\x00\x01\xFE\xFF");
    const QString encoded = Crypto::base64UrlEncode(data);
    EXPECT_FALSE(encoded.contains('='));
    EXPECT_FALSE(encoded.contains('+'));
    EXPECT_FALSE(encoded.contains('/'));
    EXPECT_EQ(Crypto::base64UrlDecode(encoded), data);
}

TEST(CryptoTest, HmacSha256IsDeterministic)
{
    const QByteArray key = QByteArrayLiteral("key");
    const QByteArray msg = QByteArrayLiteral("message");
    const QByteArray h1 = Crypto::hmacSha256(key, msg);
    const QByteArray h2 = Crypto::hmacSha256(key, msg);
    EXPECT_EQ(h1.size(), 32);
    EXPECT_EQ(h1, h2);
}

TEST(CryptoTest, HmacSha256DifferentKeysDiffer)
{
    const QByteArray msg = QByteArrayLiteral("message");
    const QByteArray h1 = Crypto::hmacSha256(QByteArrayLiteral("key1"), msg);
    const QByteArray h2 = Crypto::hmacSha256(QByteArrayLiteral("key2"), msg);
    EXPECT_NE(h1, h2);
}
