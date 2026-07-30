#include <gtest/gtest.h>

#include <QJsonObject>

#include "api_client.h"
#include "config.h"
#include "core/exceptions.h"
#include "coro_test_helper.h"
#include "http_client.h"
#include "test_http_server.h"

using namespace bili;

namespace {

class ApiClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_config = std::make_unique<Config>();
        m_config->loadDefaults();
        m_config->setValue(QStringLiteral("retry_wait_ms"), 50);
        m_config->setValue(QStringLiteral("http_backoff_factor"), 0.5);
        m_config->setValue(QStringLiteral("http_total_retries"), 1);

        m_server = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_server->start());

        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(m_server->serverPort());
        m_config->setValue(QStringLiteral("base_url"), base);

        m_http = std::make_unique<HttpClient>(m_config.get());
        m_client = std::make_unique<ApiClient>(m_config.get(), m_http.get());
    }

    void TearDown() override {
        m_client.reset();
        m_http.reset();
        m_server.reset();
        m_config.reset();
    }

    std::unique_ptr<Config> m_config;
    std::unique_ptr<TestHttpServer> m_server;
    std::unique_ptr<HttpClient> m_http;
    std::unique_ptr<ApiClient> m_client;
};

} // namespace

TEST_F(ApiClientTest, ParseSuccessCodeZero) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0,"data":{"list":[]}})"));

    const QJsonObject result = test::awaitTask(
        m_client->getHistoryPage(0, 0, QString(), QStringLiteral("SESSDATA=test")));

    EXPECT_EQ(result.value(QStringLiteral("code")).toInt(), 0);
}

TEST_F(ApiClientTest, CodeMinus101EmitsCookieException) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":-101,"message":"账号未登录"})"));

    try {
        test::awaitTask(m_client->getFavoriteFolders(QStringLiteral("SESSDATA=test")));
        FAIL() << "expected CookieException";
    } catch (const CookieException&) {
        // expected
    }
}

TEST_F(ApiClientTest, NonZeroApiCodeEmitsApiException) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":-500,"message":"请求过于频繁"})"));

    try {
        test::awaitTask(m_client->getUserCard(QStringLiteral("12345"), QStringLiteral("SESSDATA=test")));
        FAIL() << "expected ApiException";
    } catch (const ApiException& e) {
        EXPECT_EQ(e.apiCode(), -500);
    }
}

TEST_F(ApiClientTest, Http401MapsToCookieException) {
    m_server->enqueueResponse(401, QByteArrayLiteral("{}"));

    try {
        test::awaitTask(m_client->getFollowingPage(1, 50, QStringLiteral("123"), QStringLiteral("SESSDATA=test")));
        FAIL() << "expected CookieException";
    } catch (const CookieException& e) {
        EXPECT_EQ(e.statusCode(), 401);
    }
}
