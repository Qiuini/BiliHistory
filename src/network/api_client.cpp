#include "api_client.h"

#include "exceptions.h"
#include "logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace bili {

ApiClient::ApiClient(IConfig* config, IHttpClient* http, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_http(http)
{
    Q_ASSERT(m_config != nullptr);
    Q_ASSERT(m_http != nullptr);
}

ApiClient::~ApiClient() = default;

coro::Task<QJsonObject> ApiClient::getHistoryPage(qint64 maxOid,
                                                  qint64 viewAt,
                                                  const QString& business,
                                                  const QString& cookie,
                                                  coro::CancellationToken::Ptr token)
{
    QUrl url = m_config->apiUrl(QStringLiteral("history"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("max"), QString::number(maxOid));
    query.addQueryItem(QStringLiteral("view_at"), QString::number(viewAt));
    query.addQueryItem(QStringLiteral("business"), business);
    query.addQueryItem(QStringLiteral("ps"), QString::number(qMin(m_config->pageSize(), 100)));
    url.setQuery(query);
    co_return co_await sendRequest(url, cookie, token);
}

coro::Task<QJsonObject> ApiClient::getFollowingPage(int pn,
                                                    int ps,
                                                    const QString& vmid,
                                                    const QString& cookie,
                                                    coro::CancellationToken::Ptr token)
{
    QUrl url = m_config->apiUrl(QStringLiteral("following"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("vmid"), vmid);
    query.addQueryItem(QStringLiteral("pn"), QString::number(pn));
    query.addQueryItem(QStringLiteral("ps"), QString::number(ps));
    query.addQueryItem(QStringLiteral("order"), QStringLiteral("desc"));
    query.addQueryItem(QStringLiteral("order_type"), QStringLiteral("attention"));
    url.setQuery(query);
    co_return co_await sendRequest(url, cookie, token);
}

coro::Task<QJsonObject> ApiClient::getFavoriteFolders(const QString& cookie,
                                                      coro::CancellationToken::Ptr token)
{
    QUrl url = m_config->apiUrl(QStringLiteral("favorites_folders"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("up_mid"), QStringLiteral("0"));
    url.setQuery(query);
    co_return co_await sendRequest(url, cookie, token);
}

coro::Task<QJsonObject> ApiClient::getFavoriteResources(const QString& folderId,
                                                        int pn,
                                                        int ps,
                                                        const QString& cookie,
                                                        coro::CancellationToken::Ptr token)
{
    QUrl url = m_config->apiUrl(QStringLiteral("favorites_resource"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("media_id"), folderId);
    query.addQueryItem(QStringLiteral("pn"), QString::number(pn));
    query.addQueryItem(QStringLiteral("ps"), QString::number(ps));
    query.addQueryItem(QStringLiteral("platform"), QStringLiteral("web"));
    url.setQuery(query);
    co_return co_await sendRequest(url, cookie, token);
}

coro::Task<QJsonObject> ApiClient::getNav(const QString& cookie,
                                          coro::CancellationToken::Ptr token)
{
    const QUrl url = m_config->apiUrl(QStringLiteral("nav"));
    co_return co_await sendRequest(url, cookie, token);
}

coro::Task<QJsonObject> ApiClient::getUserCard(const QString& mid,
                                               const QString& cookie,
                                               coro::CancellationToken::Ptr token)
{
    QUrl url = m_config->apiUrl(QStringLiteral("user_card"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("mid"), mid);
    query.addQueryItem(QStringLiteral("photo"), QStringLiteral("false"));
    url.setQuery(query);
    co_return co_await sendRequest(url, cookie, token);
}

coro::Task<QJsonObject> ApiClient::sendRequest(const QUrl& url,
                                               const QString& cookie,
                                               coro::CancellationToken::Ptr token)
{
    QHash<QString, QString> headers;
    if (!cookie.isEmpty()) {
        headers[QStringLiteral("Cookie")] = cookie;
    }

    Logger::info(QStringLiteral("[API] %1").arg(url.toString()));
    const NetworkResponse response = co_await m_http->get(url, headers, token);
    co_return parseResponse(response);
}

QJsonObject ApiClient::parseResponse(const NetworkResponse& response)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        throw NetworkException(QStringLiteral("API 响应 JSON 解析失败: %1")
                                   .arg(parseError.errorString()));
    }

    const QJsonObject root = doc.object();
    const int apiCode = root.value(QStringLiteral("code")).toInt(0);
    const QString message = root.value(QStringLiteral("message")).toString();

    if (apiCode == -101) {
        throw CookieException(QStringLiteral("Cookie 无效或已过期 (-101)"));
    }

    if (apiCode != 0) {
        throw ApiException(QStringLiteral("API 返回错误: %1 (code=%2)")
                               .arg(message)
                               .arg(apiCode),
                           apiCode);
    }

    return root;
}

} // namespace bili
