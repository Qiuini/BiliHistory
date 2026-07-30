#pragma once

#include "coro/cancellation_token.h"
#include "coro/task.h"

#include <QJsonObject>
#include <QString>

namespace bili {

// 基于 C++20 协程的 API 客户端接口，用于解耦 Fetcher 与具体 ApiClient 实现。
class IApiClient {
public:
    virtual ~IApiClient() = default;

    virtual coro::Task<QJsonObject> getHistoryPage(qint64 maxOid,
                                                   qint64 viewAt,
                                                   const QString& business,
                                                   const QString& cookie,
                                                   coro::CancellationToken::Ptr token = nullptr) = 0;

    virtual coro::Task<QJsonObject> getFollowingPage(int pn,
                                                     int ps,
                                                     const QString& vmid,
                                                     const QString& cookie,
                                                     coro::CancellationToken::Ptr token = nullptr) = 0;

    virtual coro::Task<QJsonObject> getFavoriteFolders(const QString& cookie,
                                                       coro::CancellationToken::Ptr token = nullptr) = 0;

    virtual coro::Task<QJsonObject> getFavoriteResources(const QString& folderId,
                                                         int pn,
                                                         int ps,
                                                         const QString& cookie,
                                                         coro::CancellationToken::Ptr token = nullptr) = 0;

    virtual coro::Task<QJsonObject> getNav(const QString& cookie,
                                           coro::CancellationToken::Ptr token = nullptr) = 0;

    virtual coro::Task<QJsonObject> getUserCard(const QString& mid,
                                                const QString& cookie,
                                                coro::CancellationToken::Ptr token = nullptr) = 0;
};

} // namespace bili
