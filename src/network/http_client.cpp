#include "http_client.h"

#include "coro/network_awaitable.h"
#include "coro/timer_awaitable.h"
#include "logger.h"

#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>

#include <cmath>
#include <optional>

namespace bili {

namespace {

const char* methodName(HttpMethod method)
{
    switch (method) {
    case HttpMethod::Get:    return "GET";
    case HttpMethod::Post:   return "POST";
    case HttpMethod::Put:    return "PUT";
    case HttpMethod::Delete: return "DELETE";
    }
    return "?";
}

} // namespace

HttpClient::HttpClient(IConfig* config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_manager(new QNetworkAccessManager(this))
{
    Q_ASSERT(m_config != nullptr);
}

HttpClient::~HttpClient() = default;

coro::Task<NetworkResponse> HttpClient::request(HttpMethod method,
                                                const QUrl& url,
                                                const QHash<QString, QString>& headers,
                                                const QByteArray& body,
                                                const QByteArray& contentType,
                                                coro::CancellationToken::Ptr token,
                                                int maxRetries,
                                                int timeoutMs)
{
    const int retriesCap = maxRetries >= 0 ? maxRetries : m_config->httpTotalRetries();
    const int timeoutCap = timeoutMs > 0 ? timeoutMs : m_config->requestTimeoutMs();

    int retriesDone = 0;
    while (true) {
        QElapsedTimer elapsed;
        elapsed.start();

        std::optional<NetworkResponse> response;
        std::exception_ptr exception;
        try {
            response = co_await requestOnce(method, url, headers, body, contentType, token, timeoutCap);
        } catch (const CookieException&) {
            // Cookie/授权错误不重试
            throw;
        } catch (...) {
            exception = std::current_exception();
        }

        if (response) {
            response->elapsedMs = elapsed.elapsed();

            const QString summary = QStringLiteral("[%1] %2 -> %3 (%4ms)")
                                        .arg(QString::fromLatin1(methodName(method)),
                                             url.toString())
                                        .arg(response->statusCode)
                                        .arg(response->elapsedMs);
            if (response->elapsedMs >= m_config->requestSlowWarningMs()) {
                Logger::warning(summary + QStringLiteral(" [慢请求]"));
            } else {
                Logger::info(summary);
            }
            if (!response->body.isEmpty()) {
                const QString preview = QString::fromUtf8(response->body.left(500)).replace('\n', ' ');
                Logger::debug(QStringLiteral("响应预览: %1").arg(preview));
            }

            co_return std::move(*response);
        }

        if (retriesDone >= retriesCap) {
            std::rethrow_exception(exception);
        }

        // 取消后不再进入重试等待，立即把异常抛给调用方。
        if (token && token->isCancelled()) {
            std::rethrow_exception(exception);
        }

        ++retriesDone;
        const int delayMs = nextDelayMs(retriesDone);
        Logger::warning(QStringLiteral("请求失败，%1ms 后第 %2/%3 次重试: %4")
                            .arg(delayMs)
                            .arg(retriesDone)
                            .arg(retriesCap)
                            .arg(url.toString()));
        co_await coro::sleepFor(delayMs);
    }
}

coro::Task<NetworkResponse> HttpClient::get(const QUrl& url,
                                            const QHash<QString, QString>& headers,
                                            coro::CancellationToken::Ptr token,
                                            int maxRetries,
                                            int timeoutMs)
{
    co_return co_await request(HttpMethod::Get, url, headers, QByteArray(), QByteArray(),
                               token, maxRetries, timeoutMs);
}

coro::Task<NetworkResponse> HttpClient::post(const QUrl& url,
                                             const QByteArray& body,
                                             const QHash<QString, QString>& headers,
                                             const QByteArray& contentType,
                                             coro::CancellationToken::Ptr token,
                                             int maxRetries,
                                             int timeoutMs)
{
    co_return co_await request(HttpMethod::Post, url, headers, body, contentType,
                               token, maxRetries, timeoutMs);
}

coro::Task<NetworkResponse> HttpClient::put(const QUrl& url,
                                            const QByteArray& body,
                                            const QHash<QString, QString>& headers,
                                            const QByteArray& contentType,
                                            coro::CancellationToken::Ptr token,
                                            int maxRetries,
                                            int timeoutMs)
{
    co_return co_await request(HttpMethod::Put, url, headers, body, contentType,
                               token, maxRetries, timeoutMs);
}

coro::Task<NetworkResponse> HttpClient::del(const QUrl& url,
                                            const QHash<QString, QString>& headers,
                                            coro::CancellationToken::Ptr token,
                                            int maxRetries,
                                            int timeoutMs)
{
    co_return co_await request(HttpMethod::Delete, url, headers, QByteArray(), QByteArray(),
                               token, maxRetries, timeoutMs);
}

coro::Task<NetworkResponse> HttpClient::requestOnce(HttpMethod method,
                                                    const QUrl& url,
                                                    const QHash<QString, QString>& headers,
                                                    const QByteArray& body,
                                                    const QByteArray& contentType,
                                                    coro::CancellationToken::Ptr token,
                                                    int timeoutMs)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Referer", "https://www.bilibili.com/");

    if (!headers.contains(QStringLiteral("User-Agent"))) {
        const QStringList agents = m_config->userAgents();
        if (!agents.isEmpty()) {
            const int idx = QRandomGenerator::global()->bounded(agents.size());
            request.setRawHeader("User-Agent", agents[idx].toUtf8());
        }
    }

    // 有 body 的请求需要设置 Content-Type（用户自定义 headers 优先）
    if (!body.isEmpty() && !headers.contains(QStringLiteral("Content-Type"))) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(contentType));
    }

    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    request.setTransferTimeout(timeoutMs);

    QNetworkReply* reply = nullptr;
    switch (method) {
    case HttpMethod::Get:
        reply = m_manager->get(request);
        break;
    case HttpMethod::Post:
        reply = m_manager->post(request, body);
        break;
    case HttpMethod::Put:
        reply = m_manager->put(request, body);
        break;
    case HttpMethod::Delete:
        reply = m_manager->deleteResource(request);
        break;
    }
    Q_ASSERT(reply != nullptr);

    // 慢请求告警：不阻塞协程，仅做记录。
    // 注意：reply 在协程恢复后会被 deleteLater，定时器触发时可能已销毁，
    // 因此必须用 QPointer 守护，避免悬空指针解引用（use-after-free）。
    QPointer<QNetworkReply> replyGuard(reply);
    QTimer::singleShot(m_config->requestSlowWarningMs(), this, [replyGuard, url, this]() {
        if (replyGuard && replyGuard->isRunning()) {
            Logger::warning(QStringLiteral("[慢请求] %1 已运行超过 %2ms")
                                .arg(url.toString())
                                .arg(m_config->requestSlowWarningMs()));
        }
    });

    try {
        const QByteArray responseBody = co_await coro::NetworkAwaitable(reply, token);
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();

        reply->deleteLater();

        if (netErr == QNetworkReply::NoError && statusCode >= 200 && statusCode < 300) {
            NetworkResponse response;
            response.statusCode = statusCode;
            response.body = responseBody;
            response.url = url;
            co_return response;
        }

        if (statusCode == 401 || statusCode == 403) {
            throw CookieException(QStringLiteral("Cookie 无效或已过期，请更新 Cookie"));
        }
        throw NetworkException(QStringLiteral("请求失败: %1, 状态码: %2, 网络错误: %3")
                                   .arg(url.toString())
                                   .arg(statusCode)
                                   .arg(netErr),
                               statusCode);
    } catch (...) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        throw;
    }
}

int HttpClient::nextDelayMs(int retriesDone) const
{
    const double factor = m_config->httpBackoffFactor();
    const int base = m_config->retryWaitMs();
    int delay = static_cast<int>(base * std::pow(2.0, retriesDone - 1) * factor);
    if (delay < 100) {
        delay = 100;
    }
    return delay;
}

} // namespace bili