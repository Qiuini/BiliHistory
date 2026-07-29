#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "config.h"

using namespace bili;

class ConfigCookieTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_dir = std::make_unique<QTemporaryDir>();
        Config::instance().setSecretsPath(QDir(m_dir->path()).filePath(QStringLiteral("secrets.json")));

        // 清除环境变量，避免影响测试
        qputenv("BILI_COOKIE", "");
    }

    void TearDown() override
    {
        Config::instance().setSecretsPath(QString());
        qputenv("BILI_COOKIE", "");
    }

    std::unique_ptr<QTemporaryDir> m_dir;
};

TEST_F(ConfigCookieTest, SaveAndLoadCookie)
{
    const QString cookie = QStringLiteral("SESSDATA=abc123");
    Config::instance().saveCookie(cookie);

    EXPECT_EQ(Config::instance().cookie(), cookie);
}

TEST_F(ConfigCookieTest, EnvCookieTakesPrecedence)
{
    const QString envCookie = QStringLiteral("SESSDATA=from-env");
    qputenv("BILI_COOKIE", envCookie.toUtf8());

    Config::instance().saveCookie(QStringLiteral("SESSDATA=from-file"));

    EXPECT_EQ(Config::instance().cookie(), envCookie);
}

TEST_F(ConfigCookieTest, MissingCookieReturnsEmpty)
{
    EXPECT_TRUE(Config::instance().cookie().isEmpty());
}
