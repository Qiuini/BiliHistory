#include "paths.h"

#include <QDateTime>
#include <QStandardPaths>

namespace bili {

QString Paths::appName() {
    return QStringLiteral("BiliHistory");
}

QString Paths::organization() {
    return QStringLiteral("BiliHistory");
}

QString Paths::appDataDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return base;
}

QString Paths::configDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return base;
}

QString Paths::historyCsvPath() {
    return QDir(appDataDir()).filePath(QStringLiteral("bilibili_history.csv"));
}

QString Paths::backupsDir() {
    QDir dir(appDataDir());
    dir.mkpath(QStringLiteral("backups"));
    return dir.filePath(QStringLiteral("backups"));
}

QString Paths::logsDir() {
    QDir dir(appDataDir());
    dir.mkpath(QStringLiteral("logs"));
    return dir.filePath(QStringLiteral("logs"));
}

QString Paths::imageCacheDir() {
    QDir dir(appDataDir());
    dir.mkpath(QStringLiteral("image_cache"));
    return dir.filePath(QStringLiteral("image_cache"));
}

QString Paths::secretsPath() {
    return QDir(configDir()).filePath(QStringLiteral(".secrets.json"));
}

QString Paths::licensePath() {
    return QDir(configDir()).filePath(QStringLiteral("license.dat"));
}

QString Paths::tempDir() {
    const QString path = QDir::temp().filePath(appName());
    QDir dir(path);
    dir.mkpath(QStringLiteral("."));
    return path;
}

QString Paths::backupFileName(const QString& baseName) {
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    return QStringLiteral("%1_%2.csv").arg(baseName, ts);
}

} // namespace bili
