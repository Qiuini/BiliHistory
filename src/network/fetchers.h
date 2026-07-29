#pragma once

#include "api_client.h"
#include "models.h"
#include "parser.h"

#include <QDateTime>
#include <QObject>

namespace bili {

class HistoryFetcher : public QObject {
    Q_OBJECT
public:
    explicit HistoryFetcher(ApiClient* client, QObject* parent = nullptr);

    void fetchAll(const QString& cookie);
    void cancel();

signals:
    void pageFetched(const bili::RecordList& records, int page, int totalSoFar);
    void finished(const bili::RecordList& allRecords);
    void progress(int total);
    void error(const bili::NetworkException& e);
    void cancelled();

private:
    void fetchPage(qint64 maxOid, qint64 viewAt, const QString& business, int page);
    void onPageFinished(const QJsonObject& root,
                        qint64 maxOid,
                        qint64 viewAt,
                        const QString& business,
                        int page);
    void finishWithError(const NetworkException& e);

    ApiClient* m_client = nullptr;
    QString m_cookie;
    RecordList m_records;
    ApiRequest* m_current = nullptr;
    bool m_cancelled = false;
};

class FollowingFetcher : public QObject {
    Q_OBJECT
public:
    explicit FollowingFetcher(ApiClient* client, QObject* parent = nullptr);

    void fetchAll(const QString& vmid, const QString& cookie);
    void cancel();

signals:
    void pageFetched(const bili::FollowingList& users, int page, int totalSoFar);
    void finished(const bili::FollowingList& allUsers);
    void error(const bili::NetworkException& e);
    void cancelled();

private:
    void fetchPage(int pn, int ps, const QString& vmid);
    void onPageFinished(const QJsonObject& root, int pn, int ps, const QString& vmid);

    ApiClient* m_client = nullptr;
    QString m_cookie;
    FollowingList m_users;
    ApiRequest* m_current = nullptr;
    bool m_cancelled = false;
};

class FavoritesFetcher : public QObject {
    Q_OBJECT
public:
    explicit FavoritesFetcher(ApiClient* client, QObject* parent = nullptr);

    void fetchFolders(const QString& cookie);
    void fetchResources(const QString& folderId, int pageSize, const QString& cookie);
    void cancel();

signals:
    void foldersFinished(const bili::FavoriteFolderList& folders);
    void resourcesFinished(const QString& folderId, const std::vector<bili::FavoriteItem>& items);
    void error(const bili::NetworkException& e);
    void cancelled();

private:
    void onFoldersFinished(const QJsonObject& root);
    void fetchResourcesPage(const QString& folderId, int pn, int ps);
    void onResourcesPageFinished(const QJsonObject& root, const QString& folderId, int pn, int ps);

    ApiClient* m_client = nullptr;
    QString m_cookie;
    ApiRequest* m_current = nullptr;
    bool m_cancelled = false;
    std::vector<FavoriteItem> m_currentResources;
};

class UserInfoFetcher : public QObject {
    Q_OBJECT
public:
    explicit UserInfoFetcher(ApiClient* client, QObject* parent = nullptr);

    void fetchRegistrationTime(const QString& mid, const QString& cookie);
    void cancel();

signals:
    void finished(const QDateTime& registrationTime);
    void error(const bili::NetworkException& e);
    void cancelled();

private:
    ApiClient* m_client = nullptr;
    ApiRequest* m_current = nullptr;
};

} // namespace bili
