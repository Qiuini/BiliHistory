#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "secrets_store.h"

using namespace bili;

class SecretsStoreTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_path = QDir(m_dir->path()).filePath(QStringLiteral("secrets.json"));
    }

    QString m_path;
    std::unique_ptr<QTemporaryDir> m_dir;
};

TEST_F(SecretsStoreTest, RoundTrip)
{
    SecretsStore store(m_path, []() { return QStringLiteral("test-machine-id"); });
    QJsonObject data;
    data[QStringLiteral("cookie")] = QStringLiteral("test-cookie-value");
    data[QStringLiteral("extra")] = 123;
    store.save(data);

    EXPECT_EQ(store.get(QStringLiteral("cookie")).toString(), QStringLiteral("test-cookie-value"));
    EXPECT_EQ(store.get(QStringLiteral("extra")).toInt(), 123);
}

TEST_F(SecretsStoreTest, EncryptedOnDisk)
{
    SecretsStore store(m_path, []() { return QStringLiteral("test-machine-id"); });
    QJsonObject data;
    data[QStringLiteral("cookie")] = QStringLiteral("secret");
    store.save(data);

    QFile file(m_path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray raw = file.readAll();
    file.close();

    EXPECT_TRUE(raw.contains("_enc"));
    EXPECT_TRUE(raw.contains("aes-256-gcm-v1"));
    EXPECT_FALSE(raw.contains("secret"));
}

TEST_F(SecretsStoreTest, WrongMachineFails)
{
    {
        SecretsStore store(m_path, []() { return QStringLiteral("test-machine-id"); });
        QJsonObject data;
        data[QStringLiteral("cookie")] = QStringLiteral("secret");
        store.save(data);
    }

    SecretsStore evil(m_path, []() { return QStringLiteral("different-machine"); });
    EXPECT_TRUE(evil.get(QStringLiteral("cookie")).toString().isEmpty());
}

TEST_F(SecretsStoreTest, PlaintextMigration)
{
    QFile file(m_path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"({"cookie":"plain-cookie"})");
    file.close();

    SecretsStore store(m_path, []() { return QStringLiteral("test-machine-id"); });
    EXPECT_EQ(store.get(QStringLiteral("cookie")).toString(), QStringLiteral("plain-cookie"));

    store.set(QStringLiteral("cookie"), QStringLiteral("plain-cookie"));

    QFile file2(m_path);
    ASSERT_TRUE(file2.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray raw = file2.readAll();
    file2.close();

    EXPECT_TRUE(raw.contains("aes-256-gcm-v1"));
}
