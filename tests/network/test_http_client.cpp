#include <gtest/gtest.h>

#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include "config.h"
#include "core/exceptions.h"
#include "coro/cancellation_token.h"
#include "coro_test_helper.h"
#include "http_client.h"
#include "test_http_server.h"

using namespace bili;

namespace {

class HttpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_config = std::make_unique<Config>();
        m_config->loadDefaults();
        // 缩短重试等待，避免单元测试耗时过长
        m_config->setValue(QStringLiteral("retry_wait_ms"), 50);
        m_config->setValue(QStringLiteral("http_backoff_factor"), 0.5);
        m_config->setValue(QStringLiteral("http_total_retries"), 2);

        m_server = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_server->start());

        m_client = std::make_unique<HttpClient>(m_config.get());
    }

    void TearDown() override {
        m_client.reset();
        m_server.reset();
        m_config.reset();
    }

    QUrl serverUrl(const QString& path) const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server->serverPort()).arg(path));
    }

    std::unique_ptr<Config> m_config;
    std::unique_ptr<TestHttpServer> m_server;
    std::unique_ptr<HttpClient> m_client;
};

} // namespace

TEST_F(HttpClientTest, GetSuccess) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0})"));

    const NetworkResponse response = test::awaitTask(
        m_client->get(serverUrl(QStringLiteral("/success"))));

    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(response.body, QByteArrayLiteral(R"({"code":0})"));
    EXPECT_EQ(m_server->requestCount(), 1);
}

TEST_F(HttpClientTest, Retry500ThenSuccess) {
    m_server->enqueueResponse(500, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(503, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0})"));

    const NetworkResponse response = test::awaitTask(
        m_client->get(serverUrl(QStringLiteral("/retry"))));

    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(response.body, QByteArrayLiteral(R"({"code":0})"));
    EXPECT_EQ(m_server->requestCount(), 3);
}

TEST_F(HttpClientTest, RetryExhaustedEmitsNetworkException) {
    m_server->enqueueResponse(500, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(502, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(503, QByteArrayLiteral("{}"));

    try {
        test::awaitTask(m_client->get(serverUrl(QStringLiteral("/retry-exhausted"))));
        FAIL() << "expected NetworkException";
    } catch (const NetworkException& e) {
        EXPECT_EQ(e.statusCode(), 503);
        EXPECT_EQ(m_server->requestCount(), 3);
    }
}

TEST_F(HttpClientTest, Status401EmitsCookieExceptionWithoutRetry) {
    m_server->enqueueResponse(401, QByteArrayLiteral("{}"));

    try {
        test::awaitTask(m_client->get(serverUrl(QStringLiteral("/cookie-error"))));
        FAIL() << "expected CookieException";
    } catch (const CookieException& e) {
        EXPECT_EQ(e.statusCode(), 401);
        EXPECT_EQ(m_server->requestCount(), 1);
    }
}

TEST_F(HttpClientTest, RequestTimeout) {
    m_server->enqueueResponse(200, QByteArrayLiteral("{}"), 500);

    try {
        test::awaitTask(m_client->get(serverUrl(QStringLiteral("/timeout")), {}, nullptr, 0, 100));
        FAIL() << "expected NetworkException";
    } catch (const NetworkException&) {
        // expected
    }
}

TEST_F(HttpClientTest, CancelRequest) {
    m_server->enqueueResponse(200, QByteArrayLiteral("{}"), 2000);

    auto token = std::make_shared<coro::CancellationToken>();

    QEventLoop loop;
    bool cancelled = false;
    QTimer::singleShot(100, &loop, [&]() {
        token->cancel();
    });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);

    try {
        test::awaitTask(m_client->get(serverUrl(QStringLiteral("/cancel")), {}, token, 0, 5000), 10000);
        FAIL() << "expected NetworkException after cancel";
    } catch (const NetworkException&) {
        cancelled = true;
    }

    EXPECT_TRUE(cancelled);
}

TEST_F(HttpClientTest, PostSendsBodyAndMethod) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0})"));

    const QByteArray payload = QByteArrayLiteral(R"({"k":"v"})");
    const NetworkResponse response = test::awaitTask(
        m_client->post(serverUrl(QStringLiteral("/submit")), payload));

    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(m_server->lastMethod(), QByteArrayLiteral("POST"));
    EXPECT_EQ(m_server->lastBody(), payload);
    EXPECT_EQ(m_server->requestCount(), 1);
}

TEST_F(HttpClientTest, PutSendsBodyAndMethod) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0})"));

    const QByteArray payload = QByteArrayLiteral(R"({"id":1})");
    const NetworkResponse response = test::awaitTask(
        m_client->put(serverUrl(QStringLiteral("/update")), payload));

    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(m_server->lastMethod(), QByteArrayLiteral("PUT"));
    EXPECT_EQ(m_server->lastBody(), payload);
}

TEST_F(HttpClientTest, DeleteUsesCorrectMethod) {
    m_server->enqueueResponse(204, QByteArrayLiteral(""));

    const NetworkResponse response = test::awaitTask(
        m_client->del(serverUrl(QStringLiteral("/remove"))));

    EXPECT_EQ(response.statusCode, 204);
    EXPECT_EQ(m_server->lastMethod(), QByteArrayLiteral("DELETE"));
    EXPECT_EQ(m_server->requestCount(), 1);
}

TEST_F(HttpClientTest, PostRetriesOnServerError) {
    m_server->enqueueResponse(500, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"ok":true})"));

    const NetworkResponse response = test::awaitTask(
        m_client->post(serverUrl(QStringLiteral("/retry-post")),
                       QByteArrayLiteral(R"({"x":1})")));

    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(response.body, QByteArrayLiteral(R"({"ok":true})"));
    EXPECT_EQ(m_server->requestCount(), 2);
    // 重试时 body 应被重新发送
    EXPECT_EQ(m_server->lastBody(), QByteArrayLiteral(R"({"x":1})"));
}
