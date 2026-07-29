#include "fetchers.h"

#include "config.h"
#include "exceptions.h"
#include "logger.h"

#include <QJsonObject>
#include <QRandomGenerator>
#include <QTimer>

namespace bili {

namespace {

qint64 cursorValue(const QJsonObject& cursor, const QString& key) {
    return cursor.value(key).toVariant().toLongLong();
}

} // namespace

// ---------------- HistoryFetcher ----------------

HistoryFetcher::HistoryFetcher(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void HistoryFetcher::fetchAll(const QString& cookie) {
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取历史记录")));
        return;
    }
    m_cookie = cookie;
    m_records.clear();
    m_cancelled = false;
    fetchPage(0, 0, QString(), 1);
}

void HistoryFetcher::cancel() {
    m_cancelled = true;
    if (m_current) {
        m_current->cancel();
        m_current = nullptr;
    }
}

void HistoryFetcher::fetchPage(qint64 maxOid, qint64 viewAt, const QString& business, int page) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }

    Logger::info(QStringLiteral("获取历史记录游标 max=%1 view_at=%2（第 %3 页）...")
                     .arg(maxOid)
                     .arg(viewAt)
                     .arg(page));

    m_current = m_client->getHistoryPage(maxOid, viewAt, business, m_cookie);
    m_current->setParent(this);

    connect(m_current, &ApiRequest::finished, this,
            [this, maxOid, viewAt, business, page](const QJsonObject& root) {
                m_current = nullptr;
                onPageFinished(root, maxOid, viewAt, business, page);
            });
    connect(m_current, &ApiRequest::error, this, [this](const NetworkException& e) {
        m_current = nullptr;
        finishWithError(e);
    });
    connect(m_current, &ApiRequest::cancelled, this, [this]() {
        m_current = nullptr;
        emit cancelled();
    });
}

void HistoryFetcher::onPageFinished(const QJsonObject& root,
                                    qint64 /*maxOid*/,
                                    qint64 /*viewAt*/,
                                    const QString& /*business*/,
                                    int page) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }

    const HistoryParseResult result = Parser::parseHistory(root);
    const int count = static_cast<int>(result.records.size());
    if (count == 0) {
        emit finished(m_records);
        return;
    }

    m_records.insert(m_records.end(), result.records.begin(), result.records.end());
    emit pageFetched(result.records, page, static_cast<int>(m_records.size()));
    emit progress(static_cast<int>(m_records.size()));
    Logger::info(QStringLiteral("历史记录第 %1 页获取 %2 条（累计 %3 条）")
                     .arg(page)
                     .arg(count)
                     .arg(m_records.size()));

    if (!Config::instance().fetchAll()) {
        emit finished(m_records);
        return;
    }

    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject cursor = data.value(QStringLiteral("cursor")).toObject();
    const qint64 nextMax = cursorValue(cursor, QStringLiteral("max"));
    const qint64 nextViewAt = cursorValue(cursor, QStringLiteral("view_at"));
    const QString nextBusiness = cursor.value(QStringLiteral("business")).toString();

    if (nextMax == 0 && nextViewAt == 0) {
        emit finished(m_records);
        return;
    }

    const int delay = 1500 + QRandomGenerator::global()->bounded(1500);
    QTimer::singleShot(delay, this, [this, nextMax, nextViewAt, nextBusiness, page]() {
        fetchPage(nextMax, nextViewAt, nextBusiness, page + 1);
    });
}

void HistoryFetcher::finishWithError(const NetworkException& e) {
    Logger::error(QStringLiteral("历史记录抓取失败: %1").arg(e.message()));
    emit error(e);
}

// ---------------- FollowingFetcher ----------------

FollowingFetcher::FollowingFetcher(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void FollowingFetcher::fetchAll(const QString& vmid, const QString& cookie) {
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取关注列表")));
        return;
    }
    m_cookie = cookie;
    m_users.clear();
    m_cancelled = false;
    fetchPage(1, 50, vmid);
}

void FollowingFetcher::cancel() {
    m_cancelled = true;
    if (m_current) {
        m_current->cancel();
        m_current = nullptr;
    }
}

void FollowingFetcher::fetchPage(int pn, int ps, const QString& vmid) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }

    m_current = m_client->getFollowingPage(pn, ps, vmid, m_cookie);
    m_current->setParent(this);

    connect(m_current, &ApiRequest::finished, this,
            [this, pn, ps, vmid](const QJsonObject& root) {
                m_current = nullptr;
                onPageFinished(root, pn, ps, vmid);
            });
    connect(m_current, &ApiRequest::error, this, [this](const NetworkException& e) {
        m_current = nullptr;
        emit error(e);
    });
    connect(m_current, &ApiRequest::cancelled, this, [this]() {
        m_current = nullptr;
        emit cancelled();
    });
}

void FollowingFetcher::onPageFinished(const QJsonObject& root, int pn, int ps, const QString& vmid) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }

    const FollowingList pageUsers = Parser::parseFollowing(root);
    if (pageUsers.empty()) {
        emit finished(m_users);
        return;
    }

    m_users.insert(m_users.end(), pageUsers.begin(), pageUsers.end());
    emit pageFetched(pageUsers, pn, static_cast<int>(m_users.size()));
    Logger::info(QStringLiteral("关注列表第 %1 页获取 %2 位（累计 %3 位）")
                     .arg(pn)
                     .arg(pageUsers.size())
                     .arg(m_users.size()));

    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const int total = data.value(QStringLiteral("total")).toInt();
    if (pn * ps >= total) {
        emit finished(m_users);
        return;
    }

    const int delay = 500 + QRandomGenerator::global()->bounded(500);
    QTimer::singleShot(delay, this, [this, pn, ps, vmid]() {
        fetchPage(pn + 1, ps, vmid);
    });
}

// ---------------- FavoritesFetcher ----------------

FavoritesFetcher::FavoritesFetcher(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void FavoritesFetcher::fetchFolders(const QString& cookie) {
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取收藏夹")));
        return;
    }
    m_cookie = cookie;
    m_cancelled = false;

    m_current = m_client->getFavoriteFolders(cookie);
    m_current->setParent(this);

    connect(m_current, &ApiRequest::finished, this, [this](const QJsonObject& root) {
        m_current = nullptr;
        onFoldersFinished(root);
    });
    connect(m_current, &ApiRequest::error, this, [this](const NetworkException& e) {
        m_current = nullptr;
        emit error(e);
    });
    connect(m_current, &ApiRequest::cancelled, this, [this]() {
        m_current = nullptr;
        emit cancelled();
    });
}

void FavoritesFetcher::fetchResources(const QString& folderId, int pageSize, const QString& cookie) {
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取收藏内容")));
        return;
    }
    m_cookie = cookie;
    m_currentResources.clear();
    m_cancelled = false;
    fetchResourcesPage(folderId, 1, pageSize);
}

void FavoritesFetcher::cancel() {
    m_cancelled = true;
    if (m_current) {
        m_current->cancel();
        m_current = nullptr;
    }
}

void FavoritesFetcher::onFoldersFinished(const QJsonObject& root) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }
    emit foldersFinished(Parser::parseFavoriteFolders(root));
}

void FavoritesFetcher::fetchResourcesPage(const QString& folderId, int pn, int ps) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }

    m_current = m_client->getFavoriteResources(folderId, pn, ps, m_cookie);
    m_current->setParent(this);

    connect(m_current, &ApiRequest::finished, this,
            [this, folderId, pn, ps](const QJsonObject& root) {
                m_current = nullptr;
                onResourcesPageFinished(root, folderId, pn, ps);
            });
    connect(m_current, &ApiRequest::error, this, [this](const NetworkException& e) {
        m_current = nullptr;
        emit error(e);
    });
    connect(m_current, &ApiRequest::cancelled, this, [this]() {
        m_current = nullptr;
        emit cancelled();
    });
}

void FavoritesFetcher::onResourcesPageFinished(const QJsonObject& root,
                                               const QString& folderId,
                                               int pn,
                                               int ps) {
    if (m_cancelled) {
        emit cancelled();
        return;
    }

    const std::vector<FavoriteItem> items = Parser::parseFavoriteResources(root);
    if (items.empty()) {
        emit resourcesFinished(folderId, m_currentResources);
        return;
    }

    m_currentResources.insert(m_currentResources.end(), items.begin(), items.end());
    Logger::info(QStringLiteral("收藏夹 %1 第 %2 页获取 %3 条").arg(folderId).arg(pn).arg(items.size()));

    const QJsonObject info = root.value(QStringLiteral("data"))
                                 .toObject()
                                 .value(QStringLiteral("info"))
                                 .toObject();
    const int total = info.value(QStringLiteral("media_count")).toInt();
    if (pn * ps >= total) {
        emit resourcesFinished(folderId, m_currentResources);
        return;
    }

    const int delay = 500 + QRandomGenerator::global()->bounded(500);
    QTimer::singleShot(delay, this, [this, folderId, pn, ps]() {
        fetchResourcesPage(folderId, pn + 1, ps);
    });
}

// ---------------- UserInfoFetcher ----------------

UserInfoFetcher::UserInfoFetcher(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void UserInfoFetcher::fetchRegistrationTime(const QString& mid, const QString& cookie) {
    if (mid.isEmpty()) {
        emit finished(QDateTime());
        return;
    }

    m_current = m_client->getUserCard(mid, cookie);
    m_current->setParent(this);

    connect(m_current, &ApiRequest::finished, this, [this](const QJsonObject& root) {
        m_current = nullptr;
        emit finished(Parser::parseRegistrationTime(root));
    });
    connect(m_current, &ApiRequest::error, this, [this](const NetworkException& e) {
        m_current = nullptr;
        emit error(e);
    });
    connect(m_current, &ApiRequest::cancelled, this, [this]() {
        m_current = nullptr;
        emit cancelled();
    });
}

void UserInfoFetcher::cancel() {
    if (m_current) {
        m_current->cancel();
        m_current = nullptr;
    }
}

} // namespace bili
