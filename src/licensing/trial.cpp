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

QString Trial::formatIso(const QDateTime& dt)
{
    return dt.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
}

QDateTime Trial::parseIso(const QString& value)
{
    return QDateTime::fromString(value, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
}

QByteArray Trial::sign(const QString& start)
{
    const QByteArray key(keys::TrialHmacSecret);
    return Crypto::hmacSha256(key, start.toUtf8()).toHex();
}

void Trial::write(const QString& filePath, const QString& start)
{
    const QFileInfo info(filePath);
    QDir dir(info.path());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QJsonObject obj;
    obj[QStringLiteral("start")] = start;
    obj[QStringLiteral("sig")] = QString::fromUtf8(sign(start));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << QStringLiteral("写入试用记录失败: %1").arg(filePath);
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();
}

Trial::Status Trial::read(const QString& filePath)
{
    Status status;

    QFile file(filePath);
    if (!file.exists()) {
        status.start = QDateTime::currentDateTime();
        status.tampered = false;
        return status;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << QStringLiteral("试用记录无法读取，判定为已过期");
        status.start = QDateTime::currentDateTime();
        status.tampered = true;
        return status;
    }

    const QByteArray content = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << QStringLiteral("试用记录损坏，判定为已过期");
        status.start = QDateTime::currentDateTime();
        status.tampered = true;
        return status;
    }

    const QJsonObject obj = doc.object();
    const QString start = obj.value(QStringLiteral("start")).toString();
    const QString sig = obj.value(QStringLiteral("sig")).toString();

    status.start = parseIso(start);
    if (status.start.isNull()) {
        qWarning() << QStringLiteral("试用记录时间格式异常，判定为已过期");
        status.start = QDateTime::currentDateTime();
        status.tampered = true;
        return status;
    }

    if (QString::fromUtf8(sign(start)).compare(sig, Qt::CaseInsensitive) != 0) {
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

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime expiredAt = st.start.addDays(TrialDays);
    const int left = static_cast<int>(st.start.daysTo(expiredAt) - st.start.daysTo(now));
    return qMax(0, left);
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
    write(filePath, formatIso(st.start));
    return remainingDays(filePath);
}

} // namespace bili
