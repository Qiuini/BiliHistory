#pragma once

#include <QList>
#include <QString>
#include <functional>

namespace bili::business {

struct UpdateInfo {
    bool hasUpdate = false;
    QString currentVersion;
    QString latestVersion;
    QString releaseUrl;
    QString releaseNotes;
    QString error;
};

QList<int> parseVersion(const QString& version);
bool isNewer(const QString& current, const QString& latest);
void checkUpdate(const QString& currentVersion,
                 const QString& owner = QStringLiteral("Qiuini"),
                 const QString& repo = QStringLiteral("BiliHistory"),
                 std::function<void(UpdateInfo)> callback = nullptr);

} // namespace bili::business
