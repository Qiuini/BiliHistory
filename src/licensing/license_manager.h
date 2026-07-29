#pragma once

#include <optional>
#include <QString>

namespace bili {

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

class LicenseManager {
public:
    static std::optional<LicenseInfo> verifyCode(const QString& code);
    static std::optional<LicenseInfo> activate(const QString& code, const QString& licenseFilePath);
    static std::optional<LicenseInfo> currentLicense(const QString& licenseFilePath);
    static bool isLicensed(const QString& licenseFilePath);

    // 仅用于测试：注入临时公钥，传空则恢复内置公钥。
    static void setPublicKeyForTest(const QByteArray& pem);

private:
    static bool matchesMachine(const LicenseInfo& info);
    static QByteArray publicKeyPem();
};

} // namespace bili
