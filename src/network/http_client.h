#pragma once

#include "exceptions.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QUrl>

#include <functional>
#include <memory>

namespace bili {

struct NetworkResponse {
    int statusCode = 0;
    QByteArray body;
    QUrl url;
    qint64 elapsedMs = 0;
};

class NetworkReply : public QObject {
    Q_OBJECT
public:
    explicit NetworkReply(QObject* parent = nullptr);

    void cancel();
    void setCancelFunc(std::function<void()> func);

signals:
    void finished(const bili::NetworkResponse& response);
    void error(const bili::NetworkException& error);
    void cancelled();

private:
    std::function<void()> m_cancelFunc;
};

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject* parent = nullptr);
    ~HttpClient() override;

    NetworkReply* get(const QUrl& url,
                      const QHash<QString, QString>& headers = {},
                      int maxRetries = -1,
                      int timeoutMs = -1);

    void cancelAll();

private:
    struct RequestContext;

    void startRequest(std::shared_ptr<RequestContext> ctx);
    void onFinished(std::shared_ptr<RequestContext> ctx);
    bool shouldRetry(const std::shared_ptr<RequestContext>& ctx) const;
    int nextDelayMs(int retriesDone) const;

    QNetworkAccessManager* m_manager = nullptr;
    QSet<std::shared_ptr<RequestContext>> m_contexts;
};

} // namespace bili
