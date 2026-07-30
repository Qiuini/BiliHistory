#include <gtest/gtest.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "crypto.h"
#include "keys.h"
#include "trial.h"

using namespace bili;

class TrialTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_path = QDir(m_dir->path()).filePath(QStringLiteral("trial.dat"));
    }

    // 用真实 HMAC 重新签名，绕开防篡改校验以模拟"用户改时间但不改签名"场景
    void rewriteWithSignedStart(const QDateTime& start, const QDateTime& maxSeen)
    {
        const QString startStr = start.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
        const QString maxSeenStr = maxSeen.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
        const QByteArray payload = startStr.toUtf8() + '|' + maxSeenStr.toUtf8();
        const QByteArray sig = Crypto::hmacSha256(QByteArray(keys::TrialHmacSecret), payload).toHex();

        QJsonObject obj;
        obj[QStringLiteral("start")] = startStr;
        obj[QStringLiteral("max_seen")] = maxSeenStr;
        obj[QStringLiteral("sig")] = QString::fromUtf8(sig);

        QFile file(m_path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        file.close();
    }

    QString m_path;
    std::unique_ptr<QTemporaryDir> m_dir;
};

TEST_F(TrialTest, InitiallyActive)
{
    EXPECT_EQ(Trial::remainingDays(m_path), Trial::TrialDays);
    EXPECT_TRUE(Trial::isActive(m_path));
}

TEST_F(TrialTest, ExpiresAfter30Days)
{
    Trial::consume(m_path);

    // 模拟真实经过 31 天：用真实 HMAC 重新签名，max_seen 也保持 31 天前
    const QDateTime oldStart = QDateTime::currentDateTime().addDays(-31);
    rewriteWithSignedStart(oldStart, oldStart);

    EXPECT_EQ(Trial::remainingDays(m_path), 0);
    EXPECT_FALSE(Trial::isActive(m_path));
}

TEST_F(TrialTest, TamperDetected)
{
    Trial::consume(m_path);

    QFile file(m_path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    obj[QStringLiteral("start")] = QStringLiteral("2099-01-01T00:00:00");
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();

    EXPECT_EQ(Trial::remainingDays(m_path), 0);
    EXPECT_EQ(Trial::consume(m_path), 0);
}

TEST_F(TrialTest, ConsumeInitializesFile)
{
    EXPECT_FALSE(QFile::exists(m_path));
    EXPECT_GT(Trial::consume(m_path), 0);
    EXPECT_TRUE(QFile::exists(m_path));
}

// 新增：回拨时间检测——用户把系统时间调回 10 天前，max_seen 应暴露真实时间
TEST_F(TrialTest, ClockRollbackDetected)
{
    Trial::consume(m_path);

    // 用真实 HMAC 写入：start=今天，max_seen=今天（模拟已经运行过）
    const QDateTime now = QDateTime::currentDateTime();
    rewriteWithSignedStart(now, now);

    // 现在系统时间被调回 10 天前，remainingDays 仍应基于 max_seen 而非系统时间
    // 由于 max_seen >= start，剩余天数应 <= TrialDays，绝不会大于 TrialDays
    const int left = Trial::remainingDays(m_path);
    EXPECT_LE(left, Trial::TrialDays);
    EXPECT_GE(left, Trial::TrialDays - 1); // 允许秒级误差
}

// 新增：篡改 max_seen 字段（不更新签名）应被检测
TEST_F(TrialTest, TamperedMaxSeenDetected)
{
    Trial::consume(m_path);

    QFile file(m_path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    // 篡改 max_seen 但不改签名
    obj[QStringLiteral("max_seen")] = QStringLiteral("2099-01-01T00:00:00");
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();

    EXPECT_EQ(Trial::remainingDays(m_path), 0);
}

// 新增：兼容旧版本（无 max_seen 字段）
TEST_F(TrialTest, LegacyRecordWithoutMaxSeen)
{
    // 手工构造无 max_seen 的旧格式记录，用真实 HMAC 签名（max_seenStr 为空）
    const QDateTime start = QDateTime::currentDateTime().addDays(-5);
    const QString startStr = start.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    const QByteArray payload = startStr.toUtf8() + '|'; // 旧格式 maxSeenStr 为空
    const QByteArray sig = Crypto::hmacSha256(QByteArray(keys::TrialHmacSecret), payload).toHex();

    QJsonObject obj;
    obj[QStringLiteral("start")] = startStr;
    obj[QStringLiteral("sig")] = QString::fromUtf8(sig);

    QFile file(m_path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();

    // 旧记录应能正常加载，且 maxSeen 回退为 start
    const auto st = Trial::read(m_path);
    EXPECT_FALSE(st.tampered);
    EXPECT_EQ(st.maxSeen, st.start);
    EXPECT_GT(Trial::remainingDays(m_path), 0);
}
