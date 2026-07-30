#pragma once

#include <QString>
#include <QDateTime>

namespace bili {

// 试用期记录：30 天免费试用，HMAC 防篡改 + 时间防回拨。
//
// 防回拨机制：
//   - 持久化 max_seen（已见过的最大时刻）
//   - 每次读取时 effective_now = max(系统当前时间, 文件 mtime, stored max_seen)
//   - 用户把系统时间调回过去，文件 mtime 与 stored max_seen 仍会暴露真实时间
class Trial {
public:
    static constexpr int TrialDays = 30;

    struct Status {
        QDateTime start;
        QDateTime maxSeen;   // 已见过的最大时刻（防回拨）
        bool tampered = false;
    };

    static Status read(const QString& filePath);
    static int remainingDays(const QString& filePath);
    static bool isActive(const QString& filePath);
    static int consume(const QString& filePath);

private:
    Trial() = delete;

    static QString formatIso(const QDateTime& dt);
    static QDateTime parseIso(const QString& value);
    static QByteArray sign(const QString& start, const QString& maxSeen);
    static void write(const QString& filePath, const QString& start, const QString& maxSeen);
    static QDateTime effectiveNow(const QString& filePath, const QDateTime& storedMaxSeen);
};

} // namespace bili
