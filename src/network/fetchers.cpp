#include "fetchers.h"

#include "coro/timer_awaitable.h"
#include "core/i_config.h"
#include "exceptions.h"
#include "logger.h"

#include <QJsonObject>
#include <QRandomGenerator>

#include <QtGlobal>

namespace bili {

namespace {

qint64 cursorValue(const QJsonObject& cursor, const QString& key)
{
    return cursor.value(key).toVariant().toLongLong();
}

} // namespace

// ---------------- HistoryFetcher ----------------

HistoryFetcher::HistoryFetcher(IApiClient* client, IConfig* config, QObject* parent)
    : IHistoryFetcher(parent)
    , BaseFetcher(client)
    , m_config(config)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_config != nullptr);
}

void HistoryFetcher::fetchAll(const QString& cookie)
{
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取历史记录")));
        return;
    }

    cancelTask();
    m_token = std::make_shared<coro::CancellationToken>();
    m_task = std::make_unique<coro::Task<void>>(runFetch(cookie));
}

void HistoryFetcher::cancel()
{
    cancelTask();
}

coro::Task<void> HistoryFetcher::runFetch(const QString& cookie)
{
    try {
        RecordList records;
        int page = 1;
        qint64 maxOid = 0;
        qint64 viewAt = 0;
        QString business;

        while (true) {
            if (isCancelled()) {
                emit cancelled();
                co_return;
            }

            Logger::info(QStringLiteral("获取历史记录游标 max=%1 view_at=%2（第 %3 页）...")
                             .arg(maxOid)
                             .arg(viewAt)
                             .arg(page));

            const QJsonObject root = co_await m_client->getHistoryPage(
                maxOid, viewAt, business, cookie, m_token);
            const HistoryParseResult result = Parser::parseHistory(root);
            const int count = static_cast<int>(result.records.size());

            if (count == 0) {
                emit finished(records);
                co_return;
            }

            records.insert(records.end(), result.records.begin(), result.records.end());
            emit pageFetched(result.records, page, static_cast<int>(records.size()));
            emit progress(static_cast<int>(records.size()));
            Logger::info(QStringLiteral("历史记录第 %1 页获取 %2 条（累计 %3 条）")
                             .arg(page)
                             .arg(count)
                             .arg(records.size()));

            if (!m_config->fetchAll()) {
                emit finished(records);
                co_return;
            }

            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            const QJsonObject cursor = data.value(QStringLiteral("cursor")).toObject();
            const qint64 nextMax = cursorValue(cursor, QStringLiteral("max"));
            const qint64 nextViewAt = cursorValue(cursor, QStringLiteral("view_at"));
            const QString nextBusiness = cursor.value(QStringLiteral("business")).toString();

            if (nextMax == 0 && nextViewAt == 0) {
                emit finished(records);
                co_return;
            }

            const int delay = m_config->fetchHistoryDelayBaseMs()
                + QRandomGenerator::global()->bounded(m_config->fetchHistoryDelayJitterMs());
            co_await coro::sleepFor(delay);

            maxOid = nextMax;
            viewAt = nextViewAt;
            business = nextBusiness;
            ++page;
        }
    } catch (const NetworkException& e) {
        if (isCancelled()) {
            emit cancelled();
        } else {
            Logger::error(QStringLiteral("历史记录抓取失败: %1").arg(e.message()));
            emit error(e);
        }
    }
}

// ---------------- FollowingFetcher ----------------

FollowingFetcher::FollowingFetcher(IApiClient* client, IConfig* config, QObject* parent)
    : IFollowingFetcher(parent)
    , BaseFetcher(client)
    , m_config(config)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_config != nullptr);
}

void FollowingFetcher::fetchAll(const QString& vmid, const QString& cookie)
{
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取关注列表")));
        return;
    }

    cancelTask();
    m_token = std::make_shared<coro::CancellationToken>();
    m_task = std::make_unique<coro::Task<void>>(runFetch(vmid, cookie));
}

void FollowingFetcher::cancel()
{
    cancelTask();
}

coro::Task<void> FollowingFetcher::runFetch(const QString& vmid, const QString& cookie)
{
    try {
        FollowingList users;
        int pn = 1;
        const int ps = m_config->followingPageSize();

        while (true) {
            if (isCancelled()) {
                emit cancelled();
                co_return;
            }

            const QJsonObject root = co_await m_client->getFollowingPage(
                pn, ps, vmid, cookie, m_token);
            const FollowingList pageUsers = Parser::parseFollowing(root);

            if (pageUsers.empty()) {
                emit finished(users);
                co_return;
            }

            users.insert(users.end(), pageUsers.begin(), pageUsers.end());
            emit pageFetched(pageUsers, pn, static_cast<int>(users.size()));
            Logger::info(QStringLiteral("关注列表第 %1 页获取 %2 位（累计 %3 位）")
                             .arg(pn)
                             .arg(pageUsers.size())
                             .arg(users.size()));

            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            const int total = data.value(QStringLiteral("total")).toInt();
            if (pn * ps >= total) {
                emit finished(users);
                co_return;
            }

            const int delay = m_config->fetchListDelayBaseMs()
                + QRandomGenerator::global()->bounded(m_config->fetchListDelayJitterMs());
            co_await coro::sleepFor(delay);
            ++pn;
        }
    } catch (const NetworkException& e) {
        if (isCancelled()) {
            emit cancelled();
        } else {
            emit error(e);
        }
    }
}

// ---------------- FavoritesFetcher ----------------

FavoritesFetcher::FavoritesFetcher(IApiClient* client, IConfig* config, QObject* parent)
    : IFavoritesFetcher(parent)
    , BaseFetcher(client)
    , m_config(config)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_config != nullptr);
}

void FavoritesFetcher::fetchFolders(const QString& cookie)
{
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取收藏夹")));
        return;
    }

    cancelTask();
    m_token = std::make_shared<coro::CancellationToken>();
    m_task = std::make_unique<coro::Task<void>>(runFetchFolders(cookie));
}

void FavoritesFetcher::fetchResources(const QString& folderId, int pageSize, const QString& cookie)
{
    if (cookie.isEmpty()) {
        emit error(CookieException(QStringLiteral("未设置 Cookie，无法抓取收藏内容")));
        return;
    }

    cancelTask();
    m_token = std::make_shared<coro::CancellationToken>();
    m_task = std::make_unique<coro::Task<void>>(runFetchResources(folderId, pageSize, cookie));
}

void FavoritesFetcher::cancel()
{
    cancelTask();
}

coro::Task<void> FavoritesFetcher::runFetchFolders(const QString& cookie)
{
    try {
        const QJsonObject root = co_await m_client->getFavoriteFolders(cookie, m_token);
        if (isCancelled()) {
            emit cancelled();
            co_return;
        }
        emit foldersFinished(Parser::parseFavoriteFolders(root));
    } catch (const NetworkException& e) {
        if (isCancelled()) {
            emit cancelled();
        } else {
            emit error(e);
        }
    }
}

coro::Task<void> FavoritesFetcher::runFetchResources(const QString& folderId,
                                                     int pageSize,
                                                     const QString& cookie)
{
    try {
        std::vector<FavoriteItem> allItems;
        int pn = 1;

        while (true) {
            if (isCancelled()) {
                emit cancelled();
                co_return;
            }

            const QJsonObject root = co_await m_client->getFavoriteResources(
                folderId, pn, pageSize, cookie, m_token);
            const std::vector<FavoriteItem> items = Parser::parseFavoriteResources(root);

            if (items.empty()) {
                emit resourcesFinished(folderId, allItems);
                co_return;
            }

            allItems.insert(allItems.end(), items.begin(), items.end());
            Logger::info(QStringLiteral("收藏夹 %1 第 %2 页获取 %3 条")
                             .arg(folderId)
                             .arg(pn)
                             .arg(items.size()));

            const QJsonObject info = root.value(QStringLiteral("data"))
                                         .toObject()
                                         .value(QStringLiteral("info"))
                                         .toObject();
            const int total = info.value(QStringLiteral("media_count")).toInt();
            if (pn * pageSize >= total) {
                emit resourcesFinished(folderId, allItems);
                co_return;
            }

            const int delay = m_config->fetchListDelayBaseMs()
                + QRandomGenerator::global()->bounded(m_config->fetchListDelayJitterMs());
            co_await coro::sleepFor(delay);
            ++pn;
        }
    } catch (const NetworkException& e) {
        if (isCancelled()) {
            emit cancelled();
        } else {
            emit error(e);
        }
    }
}

// ---------------- UserInfoFetcher ----------------

UserInfoFetcher::UserInfoFetcher(IApiClient* client, QObject* parent)
    : QObject(parent)
    , BaseFetcher(client)
{
    Q_ASSERT(m_client != nullptr);
}

void UserInfoFetcher::fetchRegistrationTime(const QString& mid, const QString& cookie)
{
    cancelTask();
    m_token = std::make_shared<coro::CancellationToken>();
    m_task = std::make_unique<coro::Task<void>>(runFetch(mid, cookie));
}

void UserInfoFetcher::cancel()
{
    cancelTask();
}

coro::Task<void> UserInfoFetcher::runFetch(const QString& mid, const QString& cookie)
{
    try {
        if (mid.isEmpty()) {
            emit finished(QDateTime());
            co_return;
        }

        const QJsonObject root = co_await m_client->getUserCard(mid, cookie, m_token);
        if (isCancelled()) {
            emit cancelled();
            co_return;
        }
        emit finished(Parser::parseRegistrationTime(root));
    } catch (const NetworkException& e) {
        if (isCancelled()) {
            emit cancelled();
        } else {
            emit error(e);
        }
    }
}

// ---------------- UserProfileFetcher ----------------

UserProfileFetcher::UserProfileFetcher(IApiClient* client, QObject* parent)
    : IUserProfileFetcher(parent)
    , BaseFetcher(client)
{
    Q_ASSERT(m_client != nullptr);
}

void UserProfileFetcher::fetch(const QString& cookie)
{
    cancelTask();
    m_token = std::make_shared<coro::CancellationToken>();
    m_task = std::make_unique<coro::Task<void>>(runFetch(cookie));
}

void UserProfileFetcher::cancel()
{
    cancelTask();
}

coro::Task<void> UserProfileFetcher::runFetch(const QString& cookie)
{
    try {
        if (cookie.isEmpty()) {
            emit finished(bili::UserInfo());
            co_return;
        }

        const QJsonObject navRoot = co_await m_client->getNav(cookie, m_token);
        if (isCancelled()) {
            emit cancelled();
            co_return;
        }

        bili::UserInfo info = Parser::parseNavUserInfo(navRoot);
        if (info.mid != 0) {
            const QJsonObject cardRoot = co_await m_client->getUserCard(
                QString::number(info.mid), cookie, m_token);
            if (isCancelled()) {
                emit cancelled();
                co_return;
            }
            info.registrationTime = Parser::parseRegistrationTime(cardRoot);
            info.registrationTimeText = info.registrationTime.toString(QStringLiteral("yyyy-MM-dd"));
        }

        emit finished(info);
    } catch (const NetworkException& e) {
        if (isCancelled()) {
            emit cancelled();
        } else {
            emit error(e);
        }
    }
}

} // namespace bili
