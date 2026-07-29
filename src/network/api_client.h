#pragma once

#include "http_client.h"

#include <QJsonObject>
#include <QObject>

namespace bili {

class ApiRequest : public QObject {
    Q_OBJECT
public:
    explicit ApiRequest(QObject* parent = nullptr);
    void cancel();

signals:
    void finished(const QJsonObject& data);
    void error(const bili::NetworkException& error);
    void cancelled();

private:
    NetworkReply* m_reply = nullptr;
    friend class ApiClient;
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(QObject* parent = nullptr);
    ~ApiClient() override;

    ApiRequest* getHistoryPage(qint64 maxOid,
                               qint64 viewAt,
                               const QString& business,
                               const QString& cookie);

    ApiRequest* getFollowingPage(int pn,
                                 int ps,
                                 const QString& vmid,
                                 const QString& cookie);

    ApiRequest* getFavoriteFolders(const QString& cookie);

    ApiRequest* getFavoriteResources(const QString& folderId,
                                     int pn,
                                     int ps,
                                     const QString& cookie);

    ApiRequest* getUserCard(const QString& mid, const QString& cookie);

private:
    ApiRequest* sendRequest(const QUrl& url, const QString& cookie);
    void parseSuccess(const NetworkResponse& response, ApiRequest* request);

    HttpClient* m_http = nullptr;
};

} // namespace bili
