#pragma once

#include "core/i_config.h"
#include "i_http_client.h"

#include <QNetworkAccessManager>
#include <QObject>

namespace bili {

// 基于 C++20 协程的 HTTP 客户端。
// 所有网络操作都在调用线程的事件循环中执行，建议配合独立工作线程使用。
//
// 继承 IHttpClient 以支持依赖注入（ApiClient 接收 IHttpClient*）。
// HttpClient 是 QObject，IHttpClient 不是 QObject，多继承合法。
class HttpClient : public QObject, public IHttpClient {
    Q_OBJECT
public:
    explicit HttpClient(IConfig* config, QObject* parent = nullptr);
    ~HttpClient() override;

    // 通用请求接口，支持 Get/Post/Put/Delete，含重试与取消令牌。
    coro::Task<NetworkResponse> request(HttpMethod method,
                                        const QUrl& url,
                                        const QHash<QString, QString>& headers = {},
                                        const QByteArray& body = {},
                                        const QByteArray& contentType = "application/json",
                                        coro::CancellationToken::Ptr token = nullptr,
                                        int maxRetries = -1,
                                        int timeoutMs = -1) override;

    // 便捷方法
    coro::Task<NetworkResponse> get(const QUrl& url,
                                    const QHash<QString, QString>& headers = {},
                                    coro::CancellationToken::Ptr token = nullptr,
                                    int maxRetries = -1,
                                    int timeoutMs = -1) override;

    coro::Task<NetworkResponse> post(const QUrl& url,
                                     const QByteArray& body = {},
                                     const QHash<QString, QString>& headers = {},
                                     const QByteArray& contentType = "application/json",
                                     coro::CancellationToken::Ptr token = nullptr,
                                     int maxRetries = -1,
                                     int timeoutMs = -1) override;

    coro::Task<NetworkResponse> put(const QUrl& url,
                                    const QByteArray& body = {},
                                    const QHash<QString, QString>& headers = {},
                                    const QByteArray& contentType = "application/json",
                                    coro::CancellationToken::Ptr token = nullptr,
                                    int maxRetries = -1,
                                    int timeoutMs = -1) override;

    coro::Task<NetworkResponse> del(const QUrl& url,
                                    const QHash<QString, QString>& headers = {},
                                    coro::CancellationToken::Ptr token = nullptr,
                                    int maxRetries = -1,
                                    int timeoutMs = -1) override;

private:
    coro::Task<NetworkResponse> requestOnce(HttpMethod method,
                                            const QUrl& url,
                                            const QHash<QString, QString>& headers,
                                            const QByteArray& body,
                                            const QByteArray& contentType,
                                            coro::CancellationToken::Ptr token,
                                            int timeoutMs);

    int nextDelayMs(int retriesDone) const;

    IConfig* m_config = nullptr;
    QNetworkAccessManager* m_manager = nullptr;
};

} // namespace bili
