#pragma once

#include "i_license_manager.h"

#include <functional>
#include <QByteArray>
#include <QString>

namespace bili {

// 授权管理：离线激活码验签 + 机器码绑定 + 持久化。
//
// 依赖注入式设计：
//   - publicKeyPem：默认空 → 使用内置公钥；测试可注入临时公钥
//   - machineIdProvider：默认空 → 调用 MachineId::current()；测试可注入固定值
//
// 实例化后即可在 FeatureAccess / ActivationDialog 等处共享，移除全局可变状态。
// 继承 ILicenseManager 以支持 GUI 层依赖抽象接口注入。
class LicenseManager : public ILicenseManager {
public:
    // 默认构造使用内置公钥 + 真实机器码，适用于生产装配
    LicenseManager();
    // 注入式构造：公钥或机器码为空时回退到内置/真实值
    LicenseManager(QByteArray publicKeyPem,
                   std::function<QString()> machineIdProvider = {});

    std::optional<LicenseInfo> verifyCode(const QString& code) const override;
    std::optional<LicenseInfo> activate(const QString& code, const QString& licenseFilePath) const override;
    std::optional<LicenseInfo> currentLicense(const QString& licenseFilePath) const override;
    bool isLicensed(const QString& licenseFilePath) const override;

private:
    bool matchesMachine(const LicenseInfo& info) const;
    QByteArray resolvePublicKey() const;

    QByteArray m_publicKeyPem;             // 空 → 使用内置 keys::PublicKeyPem
    std::function<QString()> m_machineIdProvider; // 空 → MachineId::current()
};

} // namespace bili
