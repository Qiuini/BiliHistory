#include <gtest/gtest.h>

#include <QEventLoop>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include "api_client.h"
#include "config.h"
#include "exceptions.h"
#include "test_http_server.h"

using namespace bili;

namespace {

class ApiClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().loadDefaults();
        Config::instance().setValue(QStringLiteral("retry_wait_ms"), 50);
        Config::instance().setValue(QStringLiteral("http_backoff_factor"), 0.5);
        Config::instance().setValue(QStringLiteral("http_total_retries"), 1);

        m_server = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_server->start());

        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(m_server->serverPort());
        Config::instance().setValue(QStringLiteral("base_url"), base);

        m_client = std::make_unique<ApiClient>();
    }

    void TearDown() override {
        m_client.reset();
        m_server.reset();
    }

    std::unique_ptr<TestHttpServer> m_server;
    std::unique_ptr<ApiClient> m_client;
};

} // namespace

TEST_F(ApiClientTest, ParseSuccessCodeZero) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0,"data":{"list":[]}})"));

    bool finished = false;
    QJsonObject result;

    QEventLoop loop;
    auto request = m_client->getHistoryPage(0, 0, QString(), QStringLiteral("SESSDATA=test"));
    QObject::connect(request, &ApiRequest::finished, &loop, [&](const QJsonObject& root) {
        finished = true;
        result = root;
        loop.quit();
    });
    QObject::connect(request, &ApiRequest::error, &loop, &QEventLoop::quit);

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(finished);
    EXPECT_EQ(result.value(QStringLiteral("code")).toInt(), 0);
}

TEST_F(ApiClientTest, CodeMinus101EmitsCookieException) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":-101,"message":"账号未登录"})"));

    bool gotError = false;
    bool isCookieError = false;

    QEventLoop loop;
    auto request = m_client->getFavoriteFolders(QStringLiteral("SESSDATA=test"));
    QObject::connect(request, &ApiRequest::finished, &loop, &QEventLoop::quit);
    QObject::connect(request, &ApiRequest::error, &loop, [&](const NetworkException& e) {
        gotError = true;
        isCookieError = dynamic_cast<const CookieException*>(&e) != nullptr;
        loop.quit();
    });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(gotError);
    EXPECT_TRUE(isCookieError);
}

TEST_F(ApiClientTest, NonZeroApiCodeEmitsApiException) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":-500,"message":"请求过于频繁"})"));

    bool gotError = false;
    bool isApiError = false;
    int actualApiCode = 0;

    QEventLoop loop;
    auto request = m_client->getUserCard(QStringLiteral("12345"), QStringLiteral("SESSDATA=test"));
    QObject::connect(request, &ApiRequest::finished, &loop, &QEventLoop::quit);
    QObject::connect(request, &ApiRequest::error, &loop, [&](const NetworkException& e) {
        gotError = true;
        if (const ApiException* apiErr = dynamic_cast<const ApiException*>(&e)) {
            isApiError = true;
            actualApiCode = apiErr->apiCode();
        }
        loop.quit();
    });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(gotError);
    EXPECT_TRUE(isApiError);
    EXPECT_EQ(actualApiCode, -500);
}

TEST_F(ApiClientTest, Http401MapsToCookieException) {
    m_server->enqueueResponse(401, QByteArrayLiteral("{}"));

    bool gotError = false;
    bool isCookieError = false;
    int actualStatusCode = 0;

    QEventLoop loop;
    auto request = m_client->getFollowingPage(1, 50, QStringLiteral("123"), QStringLiteral("SESSDATA=test"));
    QObject::connect(request, &ApiRequest::finished, &loop, &QEventLoop::quit);
    QObject::connect(request, &ApiRequest::error, &loop, [&](const NetworkException& e) {
        gotError = true;
        if (const CookieException* cookieErr = dynamic_cast<const CookieException*>(&e)) {
            isCookieError = true;
            actualStatusCode = cookieErr->statusCode();
        }
        loop.quit();
    });

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(gotError);
    EXPECT_TRUE(isCookieError);
    EXPECT_EQ(actualStatusCode, 401);
}
