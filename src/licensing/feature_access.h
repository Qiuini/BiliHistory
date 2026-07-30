#pragma once

#include "core/i_feature_access.h"
#include "license_manager.h"

namespace bili {

// 基于 LicenseManager + Trial 的专业版功能访问实现。
// 持有 LicenseManager 引用，避免全局可变状态；licenseFilePath / trialFilePath
// 由调用方注入（生产用 Paths::licensePath()，测试可指向临时目录）。
class FeatureAccess : public IFeatureAccess {
public:
    // 生产装配：路径自动取 Paths
    explicit FeatureAccess(LicenseManager& licenseManager);
    // 测试/自定义装配：显式指定路径
    FeatureAccess(LicenseManager& licenseManager,
                  QString licensePath,
                  QString trialPath);

    bool isProUnlocked() const override;
    int remainingTrialDays() const override;
    QString statusText() const override;

private:
    LicenseManager& m_licenseManager;
    QString m_licensePath;
    QString m_trialPath;
};

} // namespace bili
