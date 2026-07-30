#pragma once

#include "core/i_config.h"
#include "i_api_client.h"
#include "i_http_client.h"

#include <QJsonObject>
#include <QObject>

namespace bili {

class ApiClient : public QObject, public IApiClient {
    Q_OBJECT
public:
    // 注入式构造：外部传入 IHttpClient（推荐，便于测试注入 Mock）。
    // 所有权由外部管理（典型做法：将 http 的 parent 设为本 ApiClient）。
    explicit ApiClient(IConfig* config, IHttpClient* http, QObject* parent = nullptr);
    ~ApiClient() override;

    coro::Task<QJsonObject> getHistoryPage(qint64 maxOid,
                                           qint64 viewAt,
                                           const QString& business,
                                           const QString& cookie,
                                           coro::CancellationToken::Ptr token = nullptr) override;

    coro::Task<QJsonObject> getFollowingPage(int pn,
                                             int ps,
                                             const QString& vmid,
                                             const QString& cookie,
                                             coro::CancellationToken::Ptr token = nullptr) override;

    coro::Task<QJsonObject> getFavoriteFolders(const QString& cookie,
                                               coro::CancellationToken::Ptr token = nullptr) override;

    coro::Task<QJsonObject> getFavoriteResources(const QString& folderId,
                                                 int pn,
                                                 int ps,
                                                 const QString& cookie,
                                                 coro::CancellationToken::Ptr token = nullptr) override;

    coro::Task<QJsonObject> getNav(const QString& cookie,
                                   coro::CancellationToken::Ptr token = nullptr) override;

    coro::Task<QJsonObject> getUserCard(const QString& mid,
                                        const QString& cookie,
                                        coro::CancellationToken::Ptr token = nullptr) override;

private:
    coro::Task<QJsonObject> sendRequest(const QUrl& url,
                                        const QString& cookie,
                                        coro::CancellationToken::Ptr token);
    QJsonObject parseResponse(const NetworkResponse& response);

    IConfig* m_config = nullptr;
    IHttpClient* m_http = nullptr;
};

} // namespace bili
