#include "license_manager.h"

#include "crypto.h"
#include "keys.h"
#include "machine_id.h"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

namespace bili {

bool LicenseInfo::isExpired() const
{
    if (exp < 0) {
        return false;
    }
    return QDateTime::currentDateTimeUtc().toSecsSinceEpoch() > exp;
}

QString LicenseInfo::expiryText() const
{
    if (exp < 0) {
        return QStringLiteral("永久");
    }
    return QDateTime::fromSecsSinceEpoch(exp, QTimeZone::UTC)
        .toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}

LicenseManager::LicenseManager() = default;

LicenseManager::LicenseManager(QByteArray publicKeyPem,
                               std::function<QString()> machineIdProvider)
    : m_publicKeyPem(std::move(publicKeyPem))
    , m_machineIdProvider(std::move(machineIdProvider))
{
}

QByteArray LicenseManager::resolvePublicKey() const
{
    if (!m_publicKeyPem.isEmpty()) {
        return m_publicKeyPem;
    }

    const QByteArray pem(keys::PublicKeyPem);
    if (pem.isEmpty() || pem.contains("00000000")) {
        qCritical() << QStringLiteral("未回填内置公钥，无法校验激活码");
        return QByteArray();
    }
    return pem;
}

bool LicenseManager::matchesMachine(const LicenseInfo& info) const
{
    if (info.mid.isEmpty()) {
        return true;
    }
    const QString currentMid = m_machineIdProvider ? m_machineIdProvider() : MachineId::current();
    return info.mid == currentMid;
}

std::optional<LicenseInfo> LicenseManager::verifyCode(const QString& code) const
{
    if (code.isEmpty() || !code.contains('.')) {
        return std::nullopt;
    }

    const QByteArray pem = resolvePublicKey();
    if (pem.isEmpty()) {
        return std::nullopt;
    }

    const QStringList parts = code.split('.');
    if (parts.size() != 2) {
        return std::nullopt;
    }

    const QByteArray payloadBytes = Crypto::base64UrlDecode(parts.at(0));
    const QByteArray signature = Crypto::base64UrlDecode(parts.at(1));
    if (payloadBytes.isEmpty() || signature.isEmpty()) {
        return std::nullopt;
    }

    BIO* bio = BIO_new_mem_buf(pem.constData(), pem.size());
    if (!bio) {
        return std::nullopt;
    }
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        qCritical() << QStringLiteral("内置公钥无效");
        return std::nullopt;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return std::nullopt;
    }

    bool verified = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1) {
        if (EVP_DigestVerifyUpdate(ctx, payloadBytes.constData(), payloadBytes.size()) == 1) {
            verified = EVP_DigestVerifyFinal(ctx,
                                             reinterpret_cast<const unsigned char*>(signature.constData()),
                                             static_cast<size_t>(signature.size())) == 1;
        }
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    if (!verified) {
        qWarning() << QStringLiteral("激活码验签失败");
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payloadBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << QStringLiteral("激活码内容异常");
        return std::nullopt;
    }

    const QJsonObject obj = doc.object();
    LicenseInfo info;
    info.lid = obj.value(QStringLiteral("lid")).toString();
    info.typ = obj.value(QStringLiteral("typ")).toString();
    info.iat = static_cast<qint64>(obj.value(QStringLiteral("iat")).toDouble());
    info.mid = obj.value(QStringLiteral("mid")).toString();

    if (obj.contains(QStringLiteral("exp")) && !obj.value(QStringLiteral("exp")).isNull()) {
        info.exp = static_cast<qint64>(obj.value(QStringLiteral("exp")).toDouble());
    } else {
        info.exp = -1;
    }

    if (info.lid.isEmpty() || (info.typ != QStringLiteral("buyout") && info.typ != QStringLiteral("month"))) {
        qWarning() << QStringLiteral("激活码字段缺失或类型无效");
        return std::nullopt;
    }

    if (info.isExpired()) {
        qWarning() << QStringLiteral("激活码已过期");
        return std::nullopt;
    }

    return info;
}

std::optional<LicenseInfo> LicenseManager::activate(const QString& code, const QString& licenseFilePath) const
{
    auto info = verifyCode(code);
    if (!info) {
        return std::nullopt;
    }

    if (!matchesMachine(*info)) {
        qWarning() << QStringLiteral("激活码绑定的机器码与本机不符");
        return std::nullopt;
    }

    QFile file(licenseFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << QStringLiteral("写入授权文件失败: %1").arg(licenseFilePath);
        return std::nullopt;
    }
    file.write(code.trimmed().toUtf8());
    file.close();

    qInfo() << QStringLiteral("激活成功：%1，到期 %2").arg(info->typ, info->expiryText());
    return info;
}

std::optional<LicenseInfo> LicenseManager::currentLicense(const QString& licenseFilePath) const
{
    QFile file(licenseFilePath);
    if (!file.exists()) {
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    const QString code = QString::fromUtf8(file.readAll()).trimmed();
    file.close();

    auto info = verifyCode(code);
    if (!info) {
        return std::nullopt;
    }

    if (!matchesMachine(*info)) {
        qWarning() << QStringLiteral("本地授权的机器码与本机不符，视为无效");
        return std::nullopt;
    }

    return info;
}

bool LicenseManager::isLicensed(const QString& licenseFilePath) const
{
    return currentLicense(licenseFilePath).has_value();
}

} // namespace bili
