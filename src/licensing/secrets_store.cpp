#include "secrets_store.h"

#include "crypto.h"
#include "core/exceptions.h"
#include "licensing/exceptions.h"
#include "core/logger.h"
#include "machine_id.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace bili {

SecretsStore::SecretsStore(const QString& path, std::function<QString()> machineIdProvider)
    : m_path(path)
    , m_machineIdProvider(std::move(machineIdProvider))
{
}

QString SecretsStore::machineId() const
{
    if (m_machineIdProvider) {
        return m_machineIdProvider();
    }
    return MachineId::current();
}

QJsonObject SecretsStore::load() const
{
    QFile file(m_path);
    if (!file.exists()) {
        return QJsonObject();
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QJsonObject();
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        Logger::warning(QStringLiteral("敏感信息文件格式异常，按空处理"));
        return QJsonObject();
    }

    const QJsonObject content = doc.object();
    if (!content.contains(QStringLiteral("_enc"))) {
        // 旧版明文：直接读取，下次保存时自动迁移
        return content;
    }

    const QString tag = content.value(QStringLiteral("_enc")).toString();
    if (tag != QString::fromLatin1(FormatTag)) {
        Logger::warning(QStringLiteral("敏感信息文件使用了不支持的加密格式: %1").arg(tag));
        return QJsonObject();
    }

    try {
        const QByteArray raw = Crypto::base64UrlDecode(content.value(QStringLiteral("data")).toString());
        if (raw.size() < Crypto::SaltLength + Crypto::GcmIvLength + Crypto::GcmTagLength) {
            Logger::warning(QStringLiteral("敏感信息密文过短"));
            return QJsonObject();
        }

        Crypto::GcmEnvelope env;
        int offset = 0;
        env.salt = raw.mid(offset, Crypto::SaltLength);
        offset += Crypto::SaltLength;
        env.iv = raw.mid(offset, Crypto::GcmIvLength);
        offset += Crypto::GcmIvLength;
        env.tag = raw.mid(offset, Crypto::GcmTagLength);
        offset += Crypto::GcmTagLength;
        env.ciphertext = raw.mid(offset);

        const QByteArray key = Crypto::pbkdf2(machineId().toUtf8(), env.salt);
        const QByteArray payload = Crypto::decryptGcm(env, key);

        const QJsonDocument plainDoc = QJsonDocument::fromJson(payload);
        if (!plainDoc.isObject()) {
            Logger::warning(QStringLiteral("解密后的敏感信息不是 JSON 对象"));
            return QJsonObject();
        }
        return plainDoc.object();
    } catch (const LicensingException& e) {
        Logger::warning(QStringLiteral("无法解密敏感信息文件（机器码可能已变更）: %1").arg(e.message()));
        return QJsonObject();
    } catch (const BiliException& e) {
        Logger::warning(QStringLiteral("无法解密敏感信息文件（机器码可能已变更）: %1").arg(e.message()));
        return QJsonObject();
    }
}

void SecretsStore::save(const QJsonObject& data) const
{
    const QFileInfo info(m_path);
    QDir dir(info.path());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const QByteArray payload = QJsonDocument(data).toJson(QJsonDocument::Compact);
    const QByteArray salt = Crypto::randomBytes(Crypto::SaltLength);
    const QByteArray key = Crypto::pbkdf2(machineId().toUtf8(), salt);

    Crypto::GcmEnvelope env = Crypto::encryptGcm(payload, key);
    env.salt = salt;

    QByteArray blob;
    blob.append(env.salt);
    blob.append(env.iv);
    blob.append(env.tag);
    blob.append(env.ciphertext);

    QJsonObject encrypted;
    encrypted[QStringLiteral("_enc")] = QString::fromLatin1(FormatTag);
    encrypted[QStringLiteral("data")] = Crypto::base64UrlEncode(blob);

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        throw StorageException(QStringLiteral("无法写入敏感信息文件: %1").arg(m_path));
    }
    file.write(QJsonDocument(encrypted).toJson(QJsonDocument::Compact));
    file.close();
}

QJsonValue SecretsStore::get(const QString& key, const QJsonValue& defaultValue) const
{
    const QJsonObject obj = load();
    return obj.contains(key) ? obj.value(key) : defaultValue;
}

void SecretsStore::set(const QString& key, const QJsonValue& value) const
{
    QJsonObject data = load();
    data[key] = value;
    save(data);
}

} // namespace bili
