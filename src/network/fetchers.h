#pragma once

#include "api_client.h"
#include "i_fetcher.h"
#include "i_user_profile_fetcher.h"
#include "models.h"
#include "parser.h"

#include <QDateTime>
#include <QObject>

#include <memory>

namespace bili {

// 抓取器公共基类（非 QObject）：统一持有 API 客户端、取消令牌与协程任务，
// 提供 cancelTask() 与 isCancelled() 助手，消除各 Fetcher 的重复样板代码。
//
// 设计要点：
// - 不继承 QObject，避免与各 Fetcher 已有的 QObject 派生接口形成多继承冲突；
// - 信号由子类各自 emit（cancelled/error 在各接口或子类中声明），
//   本基类仅提取「判断是否已取消」的公共逻辑（方案 A）。
class BaseFetcher {
public:
    // 取消正在进行的任务：触发令牌取消并释放协程句柄。
    void cancelTask()
    {
        if (m_token) {
            m_token->cancel();
            m_token.reset();
        }
        m_task.reset();
    }

    // 判断当前是否处于已取消状态（令牌存在且已标记取消）。
    // 子类 catch 块据此决定 emit cancelled() 还是 emit error(e)。
    bool isCancelled() const noexcept { return m_token && m_token->isCancelled(); }

protected:
    explicit BaseFetcher(IApiClient* client) noexcept
        : m_client(client) {}
    ~BaseFetcher() = default;

    IApiClient* m_client = nullptr;
    coro::CancellationToken::Ptr m_token;
    std::unique_ptr<coro::Task<void>> m_task;
};

class HistoryFetcher : public IHistoryFetcher, public BaseFetcher {
    Q_OBJECT
public:
    explicit HistoryFetcher(IApiClient* client,
                            IConfig* config,
                            QObject* parent = nullptr);

    void fetchAll(const QString& cookie) override;
    void cancel() override;

private:
    coro::Task<void> runFetch(const QString& cookie);

    IConfig* m_config = nullptr;
};

class FollowingFetcher : public IFollowingFetcher, public BaseFetcher {
    Q_OBJECT
public:
    explicit FollowingFetcher(IApiClient* client, IConfig* config, QObject* parent = nullptr);

    void fetchAll(const QString& vmid, const QString& cookie) override;
    void cancel() override;

private:
    coro::Task<void> runFetch(const QString& vmid, const QString& cookie);

    IConfig* m_config = nullptr;
};

class FavoritesFetcher : public IFavoritesFetcher, public BaseFetcher {
    Q_OBJECT
public:
    explicit FavoritesFetcher(IApiClient* client, IConfig* config, QObject* parent = nullptr);

    void fetchFolders(const QString& cookie) override;
    void fetchResources(const QString& folderId, int pageSize, const QString& cookie) override;
    void cancel() override;

private:
    coro::Task<void> runFetchFolders(const QString& cookie);
    coro::Task<void> runFetchResources(const QString& folderId, int pageSize, const QString& cookie);

    IConfig* m_config = nullptr;
};

class UserInfoFetcher : public QObject, public BaseFetcher {
    Q_OBJECT
public:
    explicit UserInfoFetcher(IApiClient* client, QObject* parent = nullptr);

    void fetchRegistrationTime(const QString& mid, const QString& cookie);
    void cancel();

private:
    coro::Task<void> runFetch(const QString& mid, const QString& cookie);

signals:
    void finished(const QDateTime& registrationTime);
    void error(const bili::NetworkException& e);
    void cancelled();
};

// 抓取当前登录用户完整资料（nav + user_card 注册时间）。
// 继承 IUserProfileFetcher 以支持 GUI 层依赖抽象接口注入；
// 经 IUserProfileFetcher 获得唯一的 QObject 祖先（不直接继承 QObject）。
class UserProfileFetcher : public IUserProfileFetcher, public BaseFetcher {
    Q_OBJECT
public:
    explicit UserProfileFetcher(IApiClient* client, QObject* parent = nullptr);

    void fetch(const QString& cookie) override;
    void cancel() override;

private:
    coro::Task<void> runFetch(const QString& cookie);
};

} // namespace bili
