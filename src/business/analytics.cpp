#include "analytics.h"

#include <QDate>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <functional>
#include <vector>

namespace bili::business {

namespace {

struct Aggregate {
    int count = 0;
    qint64 watchSeconds = 0;
    qint64 completionSum = 0;
};

qint64 watchedSecondsForRecord(const RecordPtr& record)
{
    return record ? record->watchedSeconds() : 0;
}

int completionForRecord(const RecordPtr& record)
{
    return record ? record->effectiveCompletionPercent() : 0;
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

template <typename KeyFunc>
QHash<QString, Aggregate> aggregateBy(const RecordList& records, KeyFunc keyFunc)
{
    QHash<QString, Aggregate> map;
    map.reserve(static_cast<int>(records.size()));
    for (const auto& record : records) {
        if (!record) continue;
        const QString key = keyFunc(record.get());
        if (key.isEmpty()) continue;
        Aggregate& agg = map[key];
        ++agg.count;
        agg.watchSeconds += watchedSecondsForRecord(record);
        agg.completionSum += completionForRecord(record);
    }
    return map;
}

using RankedItem = std::pair<QString, Aggregate>;

// 默认过滤谓词：接受所有条目。仅作为 topNFromMap 的模板默认参数使用。
struct AcceptAllAggregate {
    bool operator()(const Aggregate&) const noexcept { return true; }
};

template <typename Cmp, typename Pred = AcceptAllAggregate>
QVariantList topNFromMap(QHash<QString, Aggregate> map,
                         int topN,
                         Cmp cmp,
                         Pred pred = Pred{})
{
    std::vector<RankedItem> list;
    list.reserve(static_cast<size_t>(map.size()));
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (pred(it.value())) {
            list.emplace_back(it.key(), it.value());
        }
    }

    const auto less = [&cmp](const RankedItem& a, const RankedItem& b) {
        return cmp(a.second, b.second); // true if a should come before b
    };

    if (topN > 0 && static_cast<size_t>(topN) < list.size()) {
        std::partial_sort(list.begin(), list.begin() + topN, list.end(), less);
        list.resize(static_cast<size_t>(topN));
    } else {
        std::sort(list.begin(), list.end(), less);
    }

    QVariantList result;
    result.reserve(static_cast<int>(list.size()));
    for (const auto& item : list) {
        result.append(aggregateToMap(item.first, item.second));
    }
    return result;
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
    return topNFromMap(
        aggregateBy(records, [](const BaseRecord* r) { return r->authorName; }),
        topN,
        [](const Aggregate& a, const Aggregate& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.watchSeconds > b.watchSeconds;
        });
}

QVariantList topCategories(const RecordList& records, int topN)
{
    return topNFromMap(
        aggregateBy(records, [](const BaseRecord* r) { return r->category; }),
        topN,
        [](const Aggregate& a, const Aggregate& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.watchSeconds > b.watchSeconds;
        });
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
    auto map = aggregateBy(records, [](const BaseRecord* r) { return r->authorName; });

    const auto cmp = [](const Aggregate& a, const Aggregate& b) {
        const int avgA = a.count > 0 ? static_cast<int>(a.completionSum / a.count) : 0;
        const int avgB = b.count > 0 ? static_cast<int>(b.completionSum / b.count) : 0;
        if (avgA != avgB) return avgA > avgB;
        return a.count > b.count;
    };

    const auto pred = [minCount](const Aggregate& a) {
        return a.count >= minCount;
    };

    return topNFromMap(std::move(map), topN, cmp, pred);
}

QVariantList dailyTrend(const RecordList& records, int days)
{
    const QDate today = QDate::currentDate();
    const QDate windowStart = today.addDays(-(days - 1));
    QHash<QDate, int> counts;
    counts.reserve(static_cast<int>(records.size()));

    for (const auto& record : records) {
        if (!record || !record->viewAt.isValid()) continue;
        const QDate date = record->viewAt.date();
        // 仅聚合 [windowStart, ∞) 区间：未来日期（时钟漂移/数据预录）也计入，
        // 不再用 date.daysTo(today) < days，避免未来记录被错误排除
        if (date.isValid() && date >= windowStart) {
            ++counts[date];
        }
    }

    QVariantList result;
    result.reserve(days);
    for (int i = days - 1; i >= 0; --i) {
        const QDate date = today.addDays(-i);
        QVariantMap map;
        map[QStringLiteral("date")] = date.toString(QStringLiteral("yyyy-MM-dd"));
        map[QStringLiteral("count")] = counts.value(date, 0);
        result.append(map);
    }
    return result;
}

QVariantList monthlyTrend(const RecordList& records, int months)
{
    const QDate today = QDate::currentDate();
    QHash<QString, int> counts;
    counts.reserve(static_cast<int>(records.size()));

    for (const auto& record : records) {
        if (!record || !record->viewAt.isValid()) continue;
        const QDate date = record->viewAt.date();
        if (date.isValid()) {
            const QString key = date.toString(QStringLiteral("yyyy-MM"));
            ++counts[key];
        }
    }

    QVariantList result;
    result.reserve(months);
    for (int i = months - 1; i >= 0; --i) {
        const QDate month = today.addMonths(-i);
        const QString key = month.toString(QStringLiteral("yyyy-MM"));
        QVariantMap map;
        map[QStringLiteral("date")] = key;
        map[QStringLiteral("count")] = counts.value(key, 0);
        result.append(map);
    }
    return result;
}

QVariantList yearlyTrend(const RecordList& records)
{
    QHash<int, int> counts;
    counts.reserve(static_cast<int>(records.size()));

    for (const auto& record : records) {
        if (!record || !record->viewAt.isValid()) continue;
        const QDate date = record->viewAt.date();
        if (date.isValid()) {
            ++counts[date.year()];
        }
    }

    const QList<int> years = counts.keys();
    const int minYear = years.isEmpty() ? QDate::currentDate().year() : *std::min_element(years.begin(), years.end());
    const int maxYear = years.isEmpty() ? QDate::currentDate().year() : *std::max_element(years.begin(), years.end());

    QVariantList result;
    for (int year = minYear; year <= maxYear; ++year) {
        QVariantMap map;
        map[QStringLiteral("date")] = QString::number(year);
        map[QStringLiteral("count")] = counts.value(year, 0);
        result.append(map);
    }
    return result;
}

} // namespace bili::business
