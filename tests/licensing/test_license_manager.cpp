#include <gtest/gtest.h>

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "crypto.h"
#include "license_manager.h"

#include <openssl/evp.h>
#include <openssl/pem.h>

using namespace bili;

namespace {

class LicenseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_licensePath = QDir(m_dir->path()).filePath(QStringLiteral("license.dat"));

        m_keypair = generateRsaKeypair();
        const QByteArray pubPem = exportPublicKey(m_keypair);
        // 注入临时公钥 + 固定机器码，避免依赖真实机器码
        m_manager = std::make_unique<LicenseManager>(pubPem,
            []() { return QStringLiteral("test-machine-id"); });
    }

    void TearDown() override
    {
        m_manager.reset();
        if (m_keypair) {
            EVP_PKEY_free(m_keypair);
            m_keypair = nullptr;
        }
    }

    QString makeCode(const QString& typ, qint64 exp = -1, const QString& mid = QString())
    {
        QJsonObject payload;
        payload[QStringLiteral("lid")] = QStringLiteral("test123");
        payload[QStringLiteral("typ")] = typ;
        payload[QStringLiteral("iat")] = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        if (exp >= 0) {
            payload[QStringLiteral("exp")] = exp;
        }
        if (!mid.isEmpty()) {
            payload[QStringLiteral("mid")] = mid;
        }

        const QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        const QByteArray signature = sign(payloadBytes);
        return Crypto::base64UrlEncode(payloadBytes) + QStringLiteral(".") + Crypto::base64UrlEncode(signature);
    }

private:
    EVP_PKEY* generateRsaKeypair()
    {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        EXPECT_NE(ctx, nullptr);
        EXPECT_GT(EVP_PKEY_keygen_init(ctx), 0);
        EXPECT_GT(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048), 0);
        EVP_PKEY* pkey = nullptr;
        EXPECT_GT(EVP_PKEY_keygen(ctx, &pkey), 0);
        EVP_PKEY_CTX_free(ctx);
        return pkey;
    }

    QByteArray exportPublicKey(EVP_PKEY* pkey)
    {
        BIO* bio = BIO_new(BIO_s_mem());
        EXPECT_NE(bio, nullptr);
        EXPECT_EQ(PEM_write_bio_PUBKEY(bio, pkey), 1);
        char* data = nullptr;
        long len = BIO_get_mem_data(bio, &data);
        QByteArray pem(data, static_cast<int>(len));
        BIO_free(bio);
        return pem;
    }

    QByteArray sign(const QByteArray& payload)
    {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EXPECT_NE(ctx, nullptr);
        EXPECT_EQ(EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, m_keypair), 1);
        EXPECT_EQ(EVP_DigestSignUpdate(ctx, payload.constData(), static_cast<size_t>(payload.size())), 1);
        size_t sigLen = 0;
        EXPECT_EQ(EVP_DigestSignFinal(ctx, nullptr, &sigLen), 1);
        QByteArray signature(static_cast<int>(sigLen), 0);
        EXPECT_EQ(EVP_DigestSignFinal(ctx,
                                      reinterpret_cast<unsigned char*>(signature.data()),
                                      &sigLen),
                  1);
        signature.resize(static_cast<int>(sigLen));
        EVP_MD_CTX_free(ctx);
        return signature;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    EVP_PKEY* m_keypair = nullptr;

public:
    QString m_licensePath;
    std::unique_ptr<LicenseManager> m_manager;
};

} // namespace

TEST_F(LicenseTest, ValidCodeVerifies)
{
    const QString code = makeCode(QStringLiteral("buyout"));
    const auto info = m_manager->verifyCode(code);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->typ, QStringLiteral("buyout"));
    EXPECT_TRUE(info->isBuyout());
    EXPECT_EQ(info->exp, -1);
}

TEST_F(LicenseTest, TamperedCodeRejected)
{
    QString code = makeCode(QStringLiteral("buyout"));
    code = code.left(code.size() - 3) + (code.endsWith(QStringLiteral("aaa")) ? QStringLiteral("bbb") : QStringLiteral("aaa"));
    EXPECT_FALSE(m_manager->verifyCode(code).has_value());
}

TEST_F(LicenseTest, ExpiredCodeRejected)
{
    const qint64 expired = QDateTime::currentDateTimeUtc().addSecs(-10).toSecsSinceEpoch();
    const QString code = makeCode(QStringLiteral("month"), expired);
    EXPECT_FALSE(m_manager->verifyCode(code).has_value());
}

TEST_F(LicenseTest, MachineBoundMismatchRejected)
{
    // 注入的 machineIdProvider 返回 "test-machine-id"，绑定到别的机器码应被拒绝
    const QString code = makeCode(QStringLiteral("buyout"), -1, QStringLiteral("deadbeefdeadbeef"));
    EXPECT_TRUE(m_manager->verifyCode(code).has_value());
    EXPECT_FALSE(m_manager->activate(code, m_licensePath).has_value());
    EXPECT_FALSE(m_manager->currentLicense(m_licensePath).has_value());
}

TEST_F(LicenseTest, MachineBoundMatchAccepted)
{
    // 新增：机器码匹配时应成功激活
    const QString code = makeCode(QStringLiteral("buyout"), -1, QStringLiteral("test-machine-id"));
    ASSERT_TRUE(m_manager->activate(code, m_licensePath).has_value());
    EXPECT_TRUE(m_manager->currentLicense(m_licensePath).has_value());
}

TEST_F(LicenseTest, ActivateAndPersist)
{
    const QString code = makeCode(QStringLiteral("buyout"));
    EXPECT_FALSE(m_manager->isLicensed(m_licensePath));
    ASSERT_TRUE(m_manager->activate(code, m_licensePath).has_value());
    EXPECT_TRUE(m_manager->isLicensed(m_licensePath));
    EXPECT_TRUE(m_manager->currentLicense(m_licensePath).has_value());
}
