#include "trial.h"

#include "crypto.h"
#include "keys.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace bili {

namespace {

// 文件 mtime 作为防回拨的辅助锚点：用户调回系统时间，但文件 mtime 不会随之倒退。
QDateTime fileLastModified(const QString& filePath)
{
    const QFileInfo info(filePath);
    return info.exists() ? info.lastModified() : QDateTime();
}

QDateTime maxOf(const QDateTime& a, const QDateTime& b)
{
    if (!a.isValid()) return b;
    if (!b.isValid()) return a;
    return a > b ? a : b;
}

QDateTime maxOf(const QDateTime& a, const QDateTime& b, const QDateTime& c)
{
    return maxOf(maxOf(a, b), c);
}

} // namespace

QString Trial::formatIso(const QDateTime& dt)
{
    return dt.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
}

QDateTime Trial::parseIso(const QString& value)
{
    return QDateTime::fromString(value, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
}

QByteArray Trial::sign(const QString& start, const QString& maxSeen)
{
    const QByteArray key(keys::TrialHmacSecret);
    const QByteArray payload = start.toUtf8() + '|' + maxSeen.toUtf8();
    return Crypto::hmacSha256(key, payload).toHex();
}

void Trial::write(const QString& filePath, const QString& start, const QString& maxSeen)
{
    const QFileInfo info(filePath);
    QDir dir(info.path());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QJsonObject obj;
    obj[QStringLiteral("start")] = start;
    obj[QStringLiteral("max_seen")] = maxSeen;
    obj[QStringLiteral("sig")] = QString::fromUtf8(sign(start, maxSeen));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << QStringLiteral("写入试用记录失败: %1").arg(filePath);
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();
}

QDateTime Trial::effectiveNow(const QString& filePath, const QDateTime& storedMaxSeen)
{
    // 防回拨三重锚点：系统时间、文件 mtime、已持久化的 max_seen，取最大者
    return maxOf(QDateTime::currentDateTime(),
                 fileLastModified(filePath),
                 storedMaxSeen);
}

Trial::Status Trial::read(const QString& filePath)
{
    Status status;

    QFile file(filePath);
    if (!file.exists()) {
        // 首次启动：start 与 maxSeen 都初始化为当前时间
        const QDateTime now = QDateTime::currentDateTime();
        status.start = now;
        status.maxSeen = now;
        status.tampered = false;
        return status;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << QStringLiteral("试用记录无法读取，判定为已过期");
        const QDateTime now = QDateTime::currentDateTime();
        status.start = now;
        status.maxSeen = now;
        status.tampered = true;
        return status;
    }

    const QByteArray content = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << QStringLiteral("试用记录损坏，判定为已过期");
        const QDateTime now = QDateTime::currentDateTime();
        status.start = now;
        status.maxSeen = now;
        status.tampered = true;
        return status;
    }

    const QJsonObject obj = doc.object();
    const QString start = obj.value(QStringLiteral("start")).toString();
    const QString maxSeenStr = obj.value(QStringLiteral("max_seen")).toString();
    const QString sig = obj.value(QStringLiteral("sig")).toString();

    status.start = parseIso(start);
    if (status.start.isNull()) {
        qWarning() << QStringLiteral("试用记录时间格式异常，判定为已过期");
        const QDateTime now = QDateTime::currentDateTime();
        status.start = now;
        status.maxSeen = now;
        status.tampered = true;
        return status;
    }

    // 兼容旧版本（无 max_seen 字段）：用 start 作为 max_seen
    status.maxSeen = maxSeenStr.isEmpty() ? status.start : parseIso(maxSeenStr);
    if (!status.maxSeen.isValid()) {
        status.maxSeen = status.start;
    }

    if (QString::fromUtf8(sign(start, maxSeenStr)).compare(sig, Qt::CaseInsensitive) != 0) {
        qWarning() << QStringLiteral("试用记录签名不符（疑似篡改），判定为已过期");
        status.tampered = true;
        return status;
    }

    status.tampered = false;
    return status;
}

int Trial::remainingDays(const QString& filePath)
{
    const Status st = read(filePath);
    if (st.tampered) {
        return 0;
    }

    const QDateTime effective = effectiveNow(filePath, st.maxSeen);
    const QDateTime expiredAt = st.start.addDays(TrialDays);
    // 用 effective 而非 currentDateTime：用户调回时间后 effective 仍接近真实时间
    const qint64 leftSecs = st.start.secsTo(effective);
    const qint64 totalSecs = st.start.secsTo(expiredAt);
    if (leftSecs >= totalSecs) {
        return 0;
    }
    const int left = static_cast<int>(qMax<qint64>(0, (totalSecs - leftSecs) / 86400));
    return left;
}

bool Trial::isActive(const QString& filePath)
{
    return remainingDays(filePath) > 0;
}

int Trial::consume(const QString& filePath)
{
    const Status st = read(filePath);
    if (st.tampered) {
        return 0;
    }

    // 每次写入都更新 max_seen = max(stored, effective_now)
    const QDateTime newMaxSeen = maxOf(st.maxSeen, effectiveNow(filePath, st.maxSeen));
    write(filePath, formatIso(st.start), formatIso(newMaxSeen));
    return remainingDays(filePath);
}

} // namespace bili
