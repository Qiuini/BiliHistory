#include <gtest/gtest.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "trial.h"

using namespace bili;

class TrialTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_path = QDir(m_dir->path()).filePath(QStringLiteral("trial.dat"));
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
    const QDateTime oldStart = QDateTime::currentDateTime().addDays(-31);
    Trial::consume(m_path);

    QFile file(m_path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray content = file.readAll();
    file.close();

    QJsonObject obj = QJsonDocument::fromJson(content).object();
    obj[QStringLiteral("start")] = oldStart.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    obj[QStringLiteral("sig")] = QStringLiteral("invalid");

    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();

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
