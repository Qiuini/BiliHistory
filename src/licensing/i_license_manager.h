#pragma once

#include <optional>
#include <QString>

namespace bili {

// 授权信息：由激活码解析得到。exp=-1 表示永久。
struct LicenseInfo {
    QString lid;
    QString typ;
    qint64 iat = 0;
    qint64 exp = -1; // -1 表示永久
    QString mid;

    bool isBuyout() const { return typ == QStringLiteral("buyout"); }
    bool isExpired() const;
    QString expiryText() const;
};

// 授权管理抽象接口，用于解耦 GUI 与具体 LicenseManager 实现，
// 便于测试注入 Mock。LicenseManager 继承本接口。
//
// LicenseInfo 定义于此处以避免与 license_manager.h 的循环包含。
class ILicenseManager {
public:
    virtual ~ILicenseManager() = default;

    // 校验激活码（不落盘），返回解析出的授权信息；失败返回 std::nullopt。
    virtual std::optional<LicenseInfo> verifyCode(const QString& code) const = 0;

    // 激活并持久化到 licenseFilePath；成功返回授权信息，失败返回 std::nullopt。
    virtual std::optional<LicenseInfo> activate(const QString& code, const QString& licenseFilePath) const = 0;

    // 读取当前已持久化的授权信息；不存在或无效返回 std::nullopt。
    virtual std::optional<LicenseInfo> currentLicense(const QString& licenseFilePath) const = 0;

    // 当前是否已激活（存在有效授权文件）。
    virtual bool isLicensed(const QString& licenseFilePath) const = 0;
};

} // namespace bili