#include "api_client.h"

#include "config.h"
#include "exceptions.h"
#include "logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace bili {

ApiRequest::ApiRequest(QObject* parent)
    : QObject(parent)
{
}

void ApiRequest::cancel() {
    if (m_reply) {
        m_reply->cancel();
    }
}

ApiClient::ApiClient(QObject* parent)
    : QObject(parent)
    , m_http(new HttpClient(this))
{
}

ApiClient::~ApiClient() = default;

ApiRequest* ApiClient::getHistoryPage(qint64 maxOid,
                                      qint64 viewAt,
                                      const QString& business,
                                      const QString& cookie)
{
    QUrl url = Config::instance().apiUrl(QStringLiteral("history"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("max"), QString::number(maxOid));
    query.addQueryItem(QStringLiteral("view_at"), QString::number(viewAt));
    query.addQueryItem(QStringLiteral("business"), business);
    query.addQueryItem(QStringLiteral("ps"), QString::number(qMin(Config::instance().pageSize(), 30)));
    url.setQuery(query);
    return sendRequest(url, cookie);
}

ApiRequest* ApiClient::getFollowingPage(int pn,
                                        int ps,
                                        const QString& vmid,
                                        const QString& cookie)
{
    QUrl url = Config::instance().apiUrl(QStringLiteral("following"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("vmid"), vmid);
    query.addQueryItem(QStringLiteral("pn"), QString::number(pn));
    query.addQueryItem(QStringLiteral("ps"), QString::number(ps));
    query.addQueryItem(QStringLiteral("order"), QStringLiteral("desc"));
    query.addQueryItem(QStringLiteral("order_type"), QStringLiteral("attention"));
    url.setQuery(query);
    return sendRequest(url, cookie);
}

ApiRequest* ApiClient::getFavoriteFolders(const QString& cookie)
{
    QUrl url = Config::instance().apiUrl(QStringLiteral("favorites_folders"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("up_mid"), QStringLiteral("0"));
    url.setQuery(query);
    return sendRequest(url, cookie);
}

ApiRequest* ApiClient::getFavoriteResources(const QString& folderId,
                                            int pn,
                                            int ps,
                                            const QString& cookie)
{
    QUrl url = Config::instance().apiUrl(QStringLiteral("favorites_resource"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("media_id"), folderId);
    query.addQueryItem(QStringLiteral("pn"), QString::number(pn));
    query.addQueryItem(QStringLiteral("ps"), QString::number(ps));
    query.addQueryItem(QStringLiteral("platform"), QStringLiteral("web"));
    url.setQuery(query);
    return sendRequest(url, cookie);
}

ApiRequest* ApiClient::getUserCard(const QString& mid, const QString& cookie)
{
    QUrl url = Config::instance().apiUrl(QStringLiteral("user_card"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("mid"), mid);
    query.addQueryItem(QStringLiteral("photo"), QStringLiteral("false"));
    url.setQuery(query);
    return sendRequest(url, cookie);
}

ApiRequest* ApiClient::sendRequest(const QUrl& url, const QString& cookie)
{
    auto request = new ApiRequest(this);

    QHash<QString, QString> headers;
    if (!cookie.isEmpty()) {
        headers[QStringLiteral("Cookie")] = cookie;
    }

    NetworkReply* reply = m_http->get(url, headers);
    request->m_reply = reply;

    connect(reply, &NetworkReply::finished, this, [this, request](const NetworkResponse& response) {
        parseSuccess(response, request);
    });
    connect(reply, &NetworkReply::error, this, [request](const NetworkException& error) {
        emit request->error(error);
        request->deleteLater();
    });
    connect(reply, &NetworkReply::cancelled, this, [request]() {
        emit request->cancelled();
        request->deleteLater();
    });

    return request;
}

void ApiClient::parseSuccess(const NetworkResponse& response, ApiRequest* request)
{
    request->m_reply = nullptr;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit request->error(NetworkException(QStringLiteral("API 响应 JSON 解析失败: %1")
                                                 .arg(parseError.errorString())));
        request->deleteLater();
        return;
    }

    const QJsonObject root = doc.object();
    const int apiCode = root.value(QStringLiteral("code")).toInt(0);
    const QString message = root.value(QStringLiteral("message")).toString();

    if (apiCode == -101) {
        emit request->error(CookieException(QStringLiteral("Cookie 无效或已过期 (-101)")));
        request->deleteLater();
        return;
    }

    if (apiCode != 0) {
        emit request->error(ApiException(QStringLiteral("API 返回错误: %1 (code=%2)")
                                             .arg(message)
                                             .arg(apiCode),
                                         apiCode));
        request->deleteLater();
        return;
    }

    emit request->finished(root);
    request->deleteLater();
}

} // namespace bili
