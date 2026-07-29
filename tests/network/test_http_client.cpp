#include <gtest/gtest.h>

#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include "config.h"
#include "exceptions.h"
#include "http_client.h"
#include "test_http_server.h"

using namespace bili;

namespace {

class HttpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().loadDefaults();
        // 缩短重试等待，避免单元测试耗时过长
        Config::instance().setValue(QStringLiteral("retry_wait_ms"), 50);
        Config::instance().setValue(QStringLiteral("http_backoff_factor"), 0.5);
        Config::instance().setValue(QStringLiteral("http_total_retries"), 2);

        m_server = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_server->start());

        m_client = std::make_unique<HttpClient>();
    }

    void TearDown() override {
        m_client.reset();
        m_server.reset();
    }

    QUrl serverUrl(const QString& path) const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server->serverPort()).arg(path));
    }

    std::unique_ptr<TestHttpServer> m_server;
    std::unique_ptr<HttpClient> m_client;
};

} // namespace

TEST_F(HttpClientTest, GetSuccess) {
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0})"));

    bool finished = false;
    NetworkResponse response;

    QEventLoop loop;
    auto reply = m_client->get(serverUrl(QStringLiteral("/success")));
    QObject::connect(reply, &NetworkReply::finished, &loop, [&](const NetworkResponse& r) {
        response = r;
        finished = true;
        loop.quit();
    });
    QObject::connect(reply, &NetworkReply::error, &loop, &QEventLoop::quit);

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(finished);
    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(response.body, QByteArrayLiteral(R"({"code":0})"));
    EXPECT_EQ(m_server->requestCount(), 1);
}

TEST_F(HttpClientTest, Retry500ThenSuccess) {
    m_server->enqueueResponse(500, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(503, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(200, QByteArrayLiteral(R"({"code":0})"));

    bool finished = false;
    NetworkResponse response;

    QEventLoop loop;
    auto reply = m_client->get(serverUrl(QStringLiteral("/retry")));
    QObject::connect(reply, &NetworkReply::finished, &loop, [&](const NetworkResponse& r) {
        response = r;
        finished = true;
        loop.quit();
    });
    QObject::connect(reply, &NetworkReply::error, &loop, &QEventLoop::quit);

    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(finished);
    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(m_server->requestCount(), 3);
}

TEST_F(HttpClientTest, RetryExhaustedEmitsNetworkException) {
    m_server->enqueueResponse(500, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(502, QByteArrayLiteral("{}"));
    m_server->enqueueResponse(503, QByteArrayLiteral("{}"));

    bool gotError = false;
    NetworkException error(QStringLiteral(""));

    QEventLoop loop;
    auto reply = m_client->get(serverUrl(QStringLiteral("/retry-exhausted")));
    QObject::connect(reply, &NetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &NetworkReply::error, &loop, [&](const NetworkException& e) {
        gotError = true;
        error = e;
        loop.quit();
    });

    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(gotError);
    EXPECT_EQ(error.statusCode(), 503);
    EXPECT_EQ(m_server->requestCount(), 3);
}

TEST_F(HttpClientTest, Status401EmitsCookieExceptionWithoutRetry) {
    m_server->enqueueResponse(401, QByteArrayLiteral("{}"));

    bool gotError = false;
    bool isCookieError = false;
    int actualStatusCode = 0;

    QEventLoop loop;
    auto reply = m_client->get(serverUrl(QStringLiteral("/cookie-error")));
    QObject::connect(reply, &NetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &NetworkReply::error, &loop, [&](const NetworkException& e) {
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
    EXPECT_EQ(m_server->requestCount(), 1);
}

TEST_F(HttpClientTest, RequestTimeout) {
    m_server->enqueueResponse(200, QByteArrayLiteral("{}"), 500);

    bool gotError = false;

    QEventLoop loop;
    auto reply = m_client->get(serverUrl(QStringLiteral("/timeout")), {}, 0, 100);
    QObject::connect(reply, &NetworkReply::error, &loop, [&](const NetworkException&) {
        gotError = true;
        loop.quit();
    });

    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(gotError);
}

TEST_F(HttpClientTest, CancelRequest) {
    m_server->enqueueResponse(200, QByteArrayLiteral("{}"), 2000);

    bool cancelled = false;

    QEventLoop loop;
    auto reply = m_client->get(serverUrl(QStringLiteral("/cancel")));
    QObject::connect(reply, &NetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &NetworkReply::error, &loop, &QEventLoop::quit);
    QObject::connect(reply, &NetworkReply::cancelled, &loop, [&]() {
        cancelled = true;
        loop.quit();
    });

    QTimer::singleShot(100, reply, &NetworkReply::cancel);
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_TRUE(cancelled);
}
