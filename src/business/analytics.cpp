#include "analytics.h"

#include <QDate>
#include <QMap>
#include <QSet>

namespace bili::business {

namespace {

struct Aggregate {
    int count = 0;
    qint64 watchSeconds = 0;
    qint64 completionSum = 0;
};

qint64 watchedSecondsForRecord(const RecordPtr& record)
{
    if (!record) {
        return 0;
    }

    if (auto video = std::dynamic_pointer_cast<VideoRecord>(record)) {
        if (video->duration <= 0) {
            return 0;
        }
        // progressPercent 为负时视为完整观看（使用总时长）
        if (video->progressPercent < 0) {
            return video->duration;
        }
        return video->duration * video->progressPercent / 100;
    }

    return 0;
}

int completionForRecord(const RecordPtr& record)
{
    if (!record) {
        return 0;
    }
    if (record->progressPercent < 0) {
        return 100;
    }
    return record->progressPercent;
}

QVariantMap aggregateToMap(const QString& name, const Aggregate& agg)
{
    QVariantMap map;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("count")] = agg.count;
    map[QStringLiteral("watch_seconds")] = static_cast<qlonglong>(agg.watchSeconds);
    map[QStringLiteral("watch_time_text")] = formatDuration(static_cast<int>(agg.watchSeconds));
    const int avgCompletion = agg.count > 0 ? static_cast<int>(agg.completionSum / agg.count) : 0;
    map[QStringLiteral("avg_completion")] = avgCompletion;
    map[QStringLiteral("avg_completion_text")] = QStringLiteral("%1%").arg(avgCompletion);
    return map;
}

} // namespace

QString formatDuration(int totalSeconds)
{
    if (totalSeconds <= 0) {
        return QStringLiteral("0秒");
    }

    const int seconds = totalSeconds % 60;
    const int minutes = (totalSeconds / 60) % 60;
    const int hours = (totalSeconds / 3600) % 24;
    const int days = totalSeconds / 86400;

    if (days > 0) {
        return QStringLiteral("%1天%2小时%3分%4秒")
            .arg(days)
            .arg(hours)
            .arg(minutes)
            .arg(seconds);
    }
    if (hours > 0) {
        return QStringLiteral("%1小时%2分%3秒").arg(hours).arg(minutes).arg(seconds);
    }
    if (minutes > 0) {
        return QStringLiteral("%1分%2秒").arg(minutes).arg(seconds);
    }
    return QStringLiteral("%1秒").arg(seconds);
}

QVariantMap computeBasicStats(const RecordList& records)
{
    int totalVideos = 0;
    int totalLives = 0;
    int totalArticles = 0;
    qint64 totalWatchSeconds = 0;
    qint64 completionSum = 0;
    QSet<QString> authors;

    for (const auto& record : records) {
        if (!record) continue;

        switch (record->type) {
        case RecordType::Video:   ++totalVideos; break;
        case RecordType::Live:    ++totalLives; break;
        case RecordType::Article: ++totalArticles; break;
        default: break;
        }

        totalWatchSeconds += watchedSecondsForRecord(record);
        completionSum += completionForRecord(record);
        authors.insert(record->authorName);
    }

    QVariantMap stats;
    stats[QStringLiteral("total_records")] = static_cast<int>(records.size());
    stats[QStringLiteral("total_videos")] = totalVideos;
    stats[QStringLiteral("total_lives")] = totalLives;
    stats[QStringLiteral("total_articles")] = totalArticles;
    stats[QStringLiteral("total_watch_seconds")] = static_cast<qlonglong>(totalWatchSeconds);
    stats[QStringLiteral("total_watch_time_text")] = formatDuration(static_cast<int>(totalWatchSeconds));
    stats[QStringLiteral("unique_authors")] = static_cast<int>(authors.size());
    const int avgCompletion = records.empty() ? 0 : static_cast<int>(completionSum / static_cast<int>(records.size()));
    stats[QStringLiteral("avg_completion")] = avgCompletion;
    stats[QStringLiteral("avg_completion_text")] = QStringLiteral("%1%").arg(avgCompletion);
    return stats;
}

QVariantList topAuthors(const RecordList& records, int topN)
{
    QMap<QString, Aggregate> map;
    for (const auto& record : records) {
        if (!record || record->authorName.isEmpty()) continue;
        auto& agg = map[record->authorName];
        ++agg.count;
        agg.watchSeconds += watchedSecondsForRecord(record);
        agg.completionSum += completionForRecord(record);
    }

    QList<QPair<QString, Aggregate>> list;
    for (auto it = map.begin(); it != map.end(); ++it) {
        list.append(qMakePair(it.key(), it.value()));
    }

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        if (a.second.count != b.second.count) {
            return a.second.count > b.second.count;
        }
        return a.second.watchSeconds > b.second.watchSeconds;
    });

    QVariantList result;
    const int limit = qMin(topN, list.size());
    for (int i = 0; i < limit; ++i) {
        result.append(aggregateToMap(list[i].first, list[i].second));
    }
    return result;
}

QVariantList topCategories(const RecordList& records, int topN)
{
    QMap<QString, Aggregate> map;
    for (const auto& record : records) {
        if (!record || record->category.isEmpty()) continue;
        auto& agg = map[record->category];
        ++agg.count;
        agg.watchSeconds += watchedSecondsForRecord(record);
        agg.completionSum += completionForRecord(record);
    }

    QList<QPair<QString, Aggregate>> list;
    for (auto it = map.begin(); it != map.end(); ++it) {
        list.append(qMakePair(it.key(), it.value()));
    }

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        if (a.second.count != b.second.count) {
            return a.second.count > b.second.count;
        }
        return a.second.watchSeconds > b.second.watchSeconds;
    });

    QVariantList result;
    const int limit = qMin(topN, list.size());
    for (int i = 0; i < limit; ++i) {
        result.append(aggregateToMap(list[i].first, list[i].second));
    }
    return result;
}

QVariantMap timeOfDayDistribution(const RecordList& records)
{
    int dawn = 0;      // 00:00 - 05:59
    int morning = 0;   // 06:00 - 11:59
    int afternoon = 0; // 12:00 - 17:59
    int evening = 0;   // 18:00 - 23:59

    for (const auto& record : records) {
        if (!record || !record->viewAt.isValid()) continue;
        const int hour = record->viewAt.time().hour();
        if (hour < 6) {
            ++dawn;
        } else if (hour < 12) {
            ++morning;
        } else if (hour < 18) {
            ++afternoon;
        } else {
            ++evening;
        }
    }

    QVariantMap map;
    map[QStringLiteral("凌晨")] = dawn;
    map[QStringLiteral("上午")] = morning;
    map[QStringLiteral("下午")] = afternoon;
    map[QStringLiteral("晚上")] = evening;
    return map;
}

QVariantList topAuthorsByCompletion(const RecordList& records, int minCount, int topN)
{
    QMap<QString, Aggregate> map;
    for (const auto& record : records) {
        if (!record || record->authorName.isEmpty()) continue;
        auto& agg = map[record->authorName];
        ++agg.count;
        agg.watchSeconds += watchedSecondsForRecord(record);
        agg.completionSum += completionForRecord(record);
    }

    QList<QPair<QString, Aggregate>> list;
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (it.value().count >= minCount) {
            list.append(qMakePair(it.key(), it.value()));
        }
    }

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        const int avgA = a.second.count > 0 ? static_cast<int>(a.second.completionSum / a.second.count) : 0;
        const int avgB = b.second.count > 0 ? static_cast<int>(b.second.completionSum / b.second.count) : 0;
        if (avgA != avgB) {
            return avgA > avgB;
        }
        return a.second.count > b.second.count;
    });

    QVariantList result;
    const int limit = qMin(topN, list.size());
    for (int i = 0; i < limit; ++i) {
        result.append(aggregateToMap(list[i].first, list[i].second));
    }
    return result;
}

QVariantList dailyTrend(const RecordList& records, int days)
{
    const QDate today = QDate::currentDate();
    QMap<QDate, int> counts;

    for (const auto& record : records) {
        if (!record || !record->viewAt.isValid()) continue;
        const QDate date = record->viewAt.date();
        if (date.isValid() && date.daysTo(today) < days) {
            ++counts[date];
        }
    }

    QVariantList result;
    for (int i = days - 1; i >= 0; --i) {
        const QDate date = today.addDays(-i);
        QVariantMap map;
        map[QStringLiteral("date")] = date.toString(QStringLiteral("yyyy-MM-dd"));
        map[QStringLiteral("count")] = counts.value(date, 0);
        result.append(map);
    }
    return result;
}

} // namespace bili::business
