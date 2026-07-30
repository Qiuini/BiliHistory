#include "updater.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace bili::business {

namespace {
// 更新检查总超时：覆盖 DNS/连接/传输全程，避免弱网下 helper 永久挂起泄漏。
constexpr int kUpdateCheckTimeoutMs = 15000;
} // namespace

QList<int> parseVersion(const QString& version)
{
    QList<int> result;
    QString trimmed = version;
    if (trimmed.startsWith('v', Qt::CaseInsensitive)) {
        trimmed = trimmed.mid(1);
    }

    const QStringList parts = trimmed.split('.');
    for (const QString& part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        result.append(ok ? value : 0);
    }

    while (result.size() < 3) {
        result.append(0);
    }
    return result;
}

bool isNewer(const QString& current, const QString& latest)
{
    const QList<int> currentParts = parseVersion(current);
    const QList<int> latestParts = parseVersion(latest);

    const int maxLen = qMax(currentParts.size(), latestParts.size());
    for (int i = 0; i < maxLen; ++i) {
        const int c = i < currentParts.size() ? currentParts[i] : 0;
        const int l = i < latestParts.size() ? latestParts[i] : 0;
        if (l != c) {
            return l > c;
        }
    }
    return false;
}

void checkUpdate(const QString& currentVersion,
                 const QString& owner,
                 const QString& repo,
                 std::function<void(UpdateInfo)> callback)
{
    if (!callback) {
        return;
    }

    auto* helper = new QObject();
    auto* manager = new QNetworkAccessManager(helper);

    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                       .arg(owner, repo));
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent",
                         QStringLiteral("BiliHistory/%1").arg(currentVersion).toUtf8());
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(kUpdateCheckTimeoutMs);

    QNetworkReply* reply = manager->get(request);
    // 看门狗：transferTimeout 仅覆盖传输阶段，DNS/连接挂起时需兜底强制中止并清理。
    QTimer::singleShot(kUpdateCheckTimeoutMs, helper, [helper, reply]() {
        if (reply) reply->abort();
        helper->deleteLater();
    });
    QObject::connect(reply, &QNetworkReply::finished, helper,
                     [helper, reply, currentVersion, callback]() {
        UpdateInfo info;
        info.currentVersion = currentVersion;

        if (reply->error() != QNetworkReply::NoError) {
            info.error = QStringLiteral("网络请求失败: %1").arg(reply->errorString());
            callback(info);
            helper->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            info.error = QStringLiteral("解析响应失败: %1").arg(parseError.errorString());
            callback(info);
            helper->deleteLater();
            return;
        }

        const QJsonObject obj = doc.object();
        info.latestVersion = obj.value(QStringLiteral("tag_name")).toString();
        info.releaseUrl = obj.value(QStringLiteral("html_url")).toString();
        info.releaseNotes = obj.value(QStringLiteral("body")).toString();

        if (info.latestVersion.isEmpty()) {
            info.error = QStringLiteral("响应中未找到版本号");
            callback(info);
            helper->deleteLater();
            return;
        }

        info.hasUpdate = isNewer(currentVersion, info.latestVersion);
        callback(info);
        helper->deleteLater();
    });
}

} // namespace bili::business
