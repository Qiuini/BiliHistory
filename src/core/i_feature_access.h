#pragma once

#include <QString>

namespace bili {

// 专业版功能访问抽象接口，便于 UI 层解耦授权/试用判断逻辑。
class IFeatureAccess {
public:
    virtual ~IFeatureAccess() = default;

    // 当前是否已解锁专业版功能（已授权或试用有效期内）。
    virtual bool isProUnlocked() const = 0;

    // 试用期剩余天数；已授权返回 0。
    virtual int remainingTrialDays() const = 0;

    // 返回用于状态栏展示的简短状态文本。
    virtual QString statusText() const = 0;
};

} // namespace bili
