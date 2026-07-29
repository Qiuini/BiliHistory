#include "http_client.h"

#include "config.h"
#include "logger.h"

#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>

#include <cmath>

namespace bili {

struct HttpClient::RequestContext {
    NetworkReply* wrapper = nullptr;
    QNetworkReply* reply = nullptr;
    QElapsedTimer elapsed;
    QUrl url;
    QHash<QString, QString> headers;
    int maxRetries = 3;
    int retries = 0;
    int timeoutMs = 30000;
    bool cancelled = false;
    bool slowWarned = false;
};

NetworkReply::NetworkReply(QObject* parent)
    : QObject(parent)
{
}

void NetworkReply::cancel() {
    if (m_cancelFunc) {
        m_cancelFunc();
    }
}

void NetworkReply::setCancelFunc(std::function<void()> func) {
    m_cancelFunc = std::move(func);
}

HttpClient::HttpClient(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

HttpClient::~HttpClient() {
    cancelAll();
}

NetworkReply* HttpClient::get(const QUrl& url,
                              const QHash<QString, QString>& headers,
                              int maxRetries,
                              int timeoutMs)
{
    auto wrapper = new NetworkReply(this);
    auto ctx = std::make_shared<RequestContext>();
    ctx->wrapper = wrapper;
    ctx->url = url;
    ctx->headers = headers;
    ctx->maxRetries = maxRetries >= 0 ? maxRetries : Config::instance().httpTotalRetries();
    ctx->timeoutMs = timeoutMs > 0 ? timeoutMs : Config::instance().requestTimeoutMs();
    ctx->retries = 0;
    ctx->cancelled = false;
    ctx->slowWarned = false;
    ctx->reply = nullptr;

    wrapper->setCancelFunc([ctx]() {
        ctx->cancelled = true;
        if (ctx->reply) {
            ctx->reply->abort();
        }
    });

    m_contexts.insert(ctx);
    startRequest(ctx);
    return wrapper;
}

void HttpClient::cancelAll() {
    const auto copy = m_contexts;
    for (const auto& ctx : copy) {
        ctx->cancelled = true;
        if (ctx->reply) {
            ctx->reply->abort();
        }
    }
}

void HttpClient::startRequest(std::shared_ptr<RequestContext> ctx) {
    QNetworkRequest request(ctx->url);
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Referer", "https://www.bilibili.com/");

    if (!ctx->headers.contains(QStringLiteral("User-Agent"))) {
        const QStringList agents = Config::instance().userAgents();
        if (!agents.isEmpty()) {
            const int idx = QRandomGenerator::global()->bounded(agents.size());
            request.setRawHeader("User-Agent", agents[idx].toUtf8());
        }
    }

    for (auto it = ctx->headers.cbegin(); it != ctx->headers.cend(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    request.setTransferTimeout(ctx->timeoutMs);

    QNetworkReply* netReply = m_manager->get(request);
    ctx->reply = netReply;
    ctx->elapsed.start();

    QTimer::singleShot(Config::instance().requestSlowWarningMs(), this, [ctx]() {
        if (ctx->reply && ctx->reply->isRunning() && !ctx->slowWarned) {
            ctx->slowWarned = true;
            Logger::warning(QStringLiteral("[慢请求] %1 已运行超过 %2ms")
                                .arg(ctx->url.toString())
                                .arg(Config::instance().requestSlowWarningMs()));
        }
    });

    connect(netReply, &QNetworkReply::finished, this, [this, ctx]() { onFinished(ctx); });
}

void HttpClient::onFinished(std::shared_ptr<RequestContext> ctx) {
    if (!m_contexts.contains(ctx)) {
        return;
    }

    QNetworkReply* reply = ctx->reply;
    if (!reply) {
        return;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QByteArray body = reply->readAll();
    const qint64 elapsedMs = ctx->elapsed.elapsed();
    const QUrl url = ctx->url;
    const bool needRetry = shouldRetry(ctx);

    reply->deleteLater();
    ctx->reply = nullptr;

    if (ctx->cancelled) {
        Logger::info(QStringLiteral("[CANCELLED] %1").arg(url.toString()));
        emit ctx->wrapper->cancelled();
        ctx->wrapper->deleteLater();
        m_contexts.remove(ctx);
        return;
    }

    const QString summary = QStringLiteral("[GET] %1 -> %2 (%3ms)")
                                .arg(url.toString())
                                .arg(QString::number(statusCode))
                                .arg(QString::number(elapsedMs));
    if (elapsedMs >= Config::instance().requestSlowWarningMs()) {
        Logger::warning(summary + QStringLiteral(" [慢请求]"));
    } else {
        Logger::info(summary);
    }
    if (!body.isEmpty()) {
        QString preview = QString::fromUtf8(body.left(500)).replace('\n', ' ');
        Logger::debug(QStringLiteral("响应预览: %1").arg(preview));
    }

    if (netErr == QNetworkReply::NoError && statusCode >= 200 && statusCode < 300) {
        NetworkResponse response;
        response.statusCode = statusCode;
        response.body = body;
        response.url = url;
        response.elapsedMs = elapsedMs;
        emit ctx->wrapper->finished(response);
        ctx->wrapper->deleteLater();
        m_contexts.remove(ctx);
        return;
    }

    if (needRetry) {
        ++ctx->retries;
        const int delayMs = nextDelayMs(ctx->retries);
        Logger::warning(QStringLiteral("请求失败，%1ms 后第 %2/%3 次重试: %4")
                            .arg(delayMs)
                            .arg(ctx->retries)
                            .arg(ctx->maxRetries)
                            .arg(url.toString()));
        QTimer::singleShot(delayMs, this, [this, ctx]() {
            if (!ctx->cancelled) {
                startRequest(ctx);
            }
        });
        return;
    }

    NetworkException* exception = nullptr;
    if (statusCode == 401 || statusCode == 403) {
        exception = new CookieException(QStringLiteral("Cookie 无效或已过期，请更新 Cookie"));
    } else {
        const QString msg = QStringLiteral("请求失败: %1, 状态码: %2, 网络错误: %3")
                                .arg(url.toString())
                                .arg(statusCode)
                                .arg(netErr);
        exception = new NetworkException(msg, statusCode);
    }
    emit ctx->wrapper->error(*exception);
    delete exception;
    ctx->wrapper->deleteLater();
    m_contexts.remove(ctx);
}

bool HttpClient::shouldRetry(const std::shared_ptr<RequestContext>& ctx) const {
    if (ctx->retries >= ctx->maxRetries) {
        return false;
    }

    if (!ctx->reply) {
        return false;
    }

    const int statusCode = ctx->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = ctx->reply->error();

    if (statusCode == 401 || statusCode == 403) {
        return false;
    }
    if (statusCode == 429 || statusCode == 500 || statusCode == 502 ||
        statusCode == 503 || statusCode == 504) {
        return true;
    }

    if (netErr == QNetworkReply::TimeoutError ||
        netErr == QNetworkReply::ConnectionRefusedError ||
        netErr == QNetworkReply::RemoteHostClosedError ||
        netErr == QNetworkReply::TemporaryNetworkFailureError ||
        netErr == QNetworkReply::NetworkSessionFailedError ||
        netErr == QNetworkReply::HostNotFoundError) {
        return true;
    }

    return false;
}

int HttpClient::nextDelayMs(int retriesDone) const {
    const double factor = Config::instance().httpBackoffFactor();
    const int base = Config::instance().retryWaitMs();
    int delay = static_cast<int>(base * std::pow(2.0, retriesDone - 1) * factor);
    if (delay < 100) {
        delay = 100;
    }
    return delay;
}

} // namespace bili
