#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "config.h"

using namespace bili;

class ConfigCookieTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_config = std::make_unique<Config>();
        m_config->loadDefaults();
        m_dir = std::make_unique<QTemporaryDir>();
        m_config->setSecretsPath(QDir(m_dir->path()).filePath(QStringLiteral("secrets.json")));

        // 清除环境变量，避免影响测试
        qputenv("BILI_COOKIE", "");
    }

    void TearDown() override
    {
        qputenv("BILI_COOKIE", "");
    }

    std::unique_ptr<Config> m_config;
    std::unique_ptr<QTemporaryDir> m_dir;
};

TEST_F(ConfigCookieTest, SaveAndLoadCookie)
{
    const QString cookie = QStringLiteral("SESSDATA=abc123");
    m_config->saveCookie(cookie);

    EXPECT_EQ(m_config->cookie(), cookie);
}

TEST_F(ConfigCookieTest, EnvCookieTakesPrecedence)
{
    const QString envCookie = QStringLiteral("SESSDATA=from-env");
    qputenv("BILI_COOKIE", envCookie.toUtf8());

    m_config->saveCookie(QStringLiteral("SESSDATA=from-file"));

    EXPECT_EQ(m_config->cookie(), envCookie);
}

TEST_F(ConfigCookieTest, MissingCookieReturnsEmpty)
{
    EXPECT_TRUE(m_config->cookie().isEmpty());
}
