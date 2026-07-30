#pragma once

#include "models.h"

#include <QObject>
#include <QString>
#include <vector>

namespace bili {

class NetworkException;

// 历史记录抓取器接口
class IHistoryFetcher : public QObject {
    Q_OBJECT
public:
    explicit IHistoryFetcher(QObject* parent = nullptr)
        : QObject(parent) {}

    virtual void fetchAll(const QString& cookie) = 0;
    virtual void cancel() = 0;

signals:
    void pageFetched(const bili::RecordList& records, int page, int totalSoFar);
    void finished(const bili::RecordList& allRecords);
    void progress(int total);
    void error(const bili::NetworkException& e);
    void cancelled();

protected:
    ~IHistoryFetcher() override = default;
};

// 关注列表抓取器接口
class IFollowingFetcher : public QObject {
    Q_OBJECT
public:
    explicit IFollowingFetcher(QObject* parent = nullptr)
        : QObject(parent) {}

    virtual void fetchAll(const QString& vmid, const QString& cookie) = 0;
    virtual void cancel() = 0;

signals:
    void pageFetched(const bili::FollowingList& users, int page, int totalSoFar);
    void finished(const bili::FollowingList& allUsers);
    void error(const bili::NetworkException& e);
    void cancelled();

protected:
    ~IFollowingFetcher() override = default;
};

// 收藏夹抓取器接口
class IFavoritesFetcher : public QObject {
    Q_OBJECT
public:
    explicit IFavoritesFetcher(QObject* parent = nullptr)
        : QObject(parent) {}

    virtual void fetchFolders(const QString& cookie) = 0;
    virtual void fetchResources(const QString& folderId, int pageSize, const QString& cookie) = 0;
    virtual void cancel() = 0;

signals:
    void foldersFinished(const bili::FavoriteFolderList& folders);
    void resourcesFinished(const QString& folderId, const std::vector<bili::FavoriteItem>& items);
    void error(const bili::NetworkException& e);
    void cancelled();

protected:
    ~IFavoritesFetcher() override = default;
};

} // namespace bili
