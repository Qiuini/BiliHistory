#pragma once

#include "cancellation_token.h"
#include "core/exceptions.h"

#include <QByteArray>
#include <QMetaObject>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>

#include <coroutine>
#include <utility>

namespace bili::coro {

// 将 QNetworkReply 包装为 C++20 可等待对象。
// 安全保证：
//   1. 使用 QPointer 感知 reply 被提前销毁；
//   2. 通过 CancellationToken 支持外部取消，取消时 abort reply 并恢复协程；
//   3. await_resume 中立即断开信号连接，避免已恢复协程仍被回调引用。
class NetworkAwaitable {
public:
    explicit NetworkAwaitable(QNetworkReply* reply, CancellationToken::Ptr token = nullptr)
        : m_reply(reply)
        , m_token(std::move(token))
    {
    }

    NetworkAwaitable(const NetworkAwaitable&) = delete;
    NetworkAwaitable& operator=(const NetworkAwaitable&) = delete;

    ~NetworkAwaitable()
    {
        if (m_reply) {
            QObject::disconnect(m_connection);
            m_reply->abort();
        }
    }

    bool await_ready() const noexcept
    {
        return !m_reply || m_reply->isFinished();
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        m_handle = handle;

        // 外部取消时 abort 请求；由 reply->finished 信号恢复协程，避免在回调里直接 resume 已销毁句柄。
        // Guard 确保 NetworkAwaitable 销毁后不再被回调访问。
        if (m_token) {
            m_cancelGuard = m_token->onCancel([this]() {
                if (m_reply) {
                    m_reply->abort();
                }
            });
        }

        m_connection = QObject::connect(m_reply, &QNetworkReply::finished, [this]() {
            if (m_handle) {
                auto h = m_handle;
                m_handle = nullptr;
                h.resume();
            }
        });
    }

    QByteArray await_resume()
    {
        QObject::disconnect(m_connection);

        if (!m_reply) {
            throw NetworkException(QStringLiteral("network reply was destroyed before completion"));
        }

        if (m_token && m_token->isCancelled()) {
            throw NetworkException(QStringLiteral("request cancelled"));
        }

        // HTTP 4xx/5xx 仍属于已完成的响应，保留 body 供上层根据状态码判断。
        // 只有底层网络错误且未拿到 HTTP 状态码时才抛网络异常。
        const int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (m_reply->error() != QNetworkReply::NoError && statusCode == 0) {
            throw NetworkException(m_reply->errorString());
        }

        return m_reply->readAll();
    }

private:
    QPointer<QNetworkReply> m_reply;
    QMetaObject::Connection m_connection;
    CancellationToken::Ptr m_token;
    CancellationToken::Guard m_cancelGuard;
    std::coroutine_handle<> m_handle = nullptr;
};

} // namespace bili::coro
