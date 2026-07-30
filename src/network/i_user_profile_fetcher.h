#pragma once

#include "coro/cancellation_token.h"
#include "core/models.h"

#include <QObject>
#include <QString>

namespace bili {

class NetworkException;

// 用户资料抓取器抽象接口，用于解耦 GUI（ProfilePage）与具体
// UserProfileFetcher 实现，便于测试注入 Mock。
//
// 继承 QObject 以暴露 signals（与 IHistoryFetcher 等接口风格一致）。
// 实现类 UserProfileFetcher 经 IUserProfileFetcher 获得唯一的 QObject 祖先。
class IUserProfileFetcher : public QObject {
    Q_OBJECT
public:
    explicit IUserProfileFetcher(QObject* parent = nullptr)
        : QObject(parent) {}

    // 抓取当前登录用户完整资料（nav + user_card 注册时间）。
    virtual void fetch(const QString& cookie) = 0;
    // 取消正在进行的任务。
    virtual void cancel() = 0;

signals:
    void finished(const bili::UserInfo& info);
    void error(const bili::NetworkException& e);
    void cancelled();

protected:
    ~IUserProfileFetcher() override = default;
};

} // namespace bili