#pragma once

#include "coro/cancellation_token.h"
#include "coro/task.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QUrl>

namespace bili {

// HTTP 响应数据：状态码、响应体、最终 URL、耗时。
struct NetworkResponse {
    int statusCode = 0;
    QByteArray body;
    QUrl url;
    qint64 elapsedMs = 0;
};

// HTTP 方法枚举，用于统一的 request 接口。
enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete
};

// HTTP 客户端抽象接口，用于解耦 ApiClient 与具体 HttpClient 实现，
// 便于测试注入 Mock。所有网络操作均为 C++20 协程。
//
// 注意：IHttpClient 不能继承 QObject（HttpClient 已是 QObject，
// 多继承时仅允许一个 QObject 祖先），因此本接口为纯抽象类，
// 信号相关逻辑不在此暴露。
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // 通用请求接口，支持 Get/Post/Put/Delete，含重试与取消令牌。
    virtual coro::Task<NetworkResponse> request(HttpMethod method,
                                                 const QUrl& url,
                                                 const QHash<QString, QString>& headers = {},
                                                 const QByteArray& body = {},
                                                 const QByteArray& contentType = "application/json",
                                                 coro::CancellationToken::Ptr token = nullptr,
                                                 int maxRetries = -1,
                                                 int timeoutMs = -1) = 0;

    // 便捷方法
    virtual coro::Task<NetworkResponse> get(const QUrl& url,
                                            const QHash<QString, QString>& headers = {},
                                            coro::CancellationToken::Ptr token = nullptr,
                                            int maxRetries = -1,
                                            int timeoutMs = -1) = 0;

    virtual coro::Task<NetworkResponse> post(const QUrl& url,
                                             const QByteArray& body = {},
                                             const QHash<QString, QString>& headers = {},
                                             const QByteArray& contentType = "application/json",
                                             coro::CancellationToken::Ptr token = nullptr,
                                             int maxRetries = -1,
                                             int timeoutMs = -1) = 0;

    virtual coro::Task<NetworkResponse> put(const QUrl& url,
                                            const QByteArray& body = {},
                                            const QHash<QString, QString>& headers = {},
                                            const QByteArray& contentType = "application/json",
                                            coro::CancellationToken::Ptr token = nullptr,
                                            int maxRetries = -1,
                                            int timeoutMs = -1) = 0;

    virtual coro::Task<NetworkResponse> del(const QUrl& url,
                                            const QHash<QString, QString>& headers = {},
                                            coro::CancellationToken::Ptr token = nullptr,
                                            int maxRetries = -1,
                                            int timeoutMs = -1) = 0;
};

} // namespace bili