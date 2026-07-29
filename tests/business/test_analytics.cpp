#include <gtest/gtest.h>

#include "business/analytics.h"
#include "core/models.h"

#include <QDateTime>

using namespace bili;
using namespace bili::business;

namespace {

RecordPtr makeVideo(const QString& title,
                    const QString& author,
                    const QString& category,
                    int duration,
                    int progressPercent,
                    const QDateTime& viewAt)
{
    auto record = std::make_shared<VideoRecord>();
    record->type = RecordType::Video;
    record->title = title;
    record->authorName = author;
    record->category = category;
    record->duration = duration;
    record->progressPercent = progressPercent;
    record->viewAt = viewAt;
    return record;
}

RecordPtr makeLive(const QString& title,
                   const QString& author,
                   const QDateTime& viewAt)
{
    auto record = std::make_shared<LiveRecord>();
    record->type = RecordType::Live;
    record->title = title;
    record->authorName = author;
    record->category = QStringLiteral("直播");
    record->viewAt = viewAt;
    return record;
}

} // namespace

TEST(AnalyticsTest, ComputeBasicStats)
{
    RecordList records;
    const QDateTime now = QDateTime::currentDateTime();
    records.push_back(makeVideo(QStringLiteral("视频1"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 50, now));
    records.push_back(makeVideo(QStringLiteral("视频2"), QStringLiteral("UP_A"), QStringLiteral("动画"), 200, 100, now));
    records.push_back(makeLive(QStringLiteral("直播1"), QStringLiteral("UP_B"), now));

    const QVariantMap stats = computeBasicStats(records);

    EXPECT_EQ(stats[QStringLiteral("total_records")].toInt(), 3);
    EXPECT_EQ(stats[QStringLiteral("total_videos")].toInt(), 2);
    EXPECT_EQ(stats[QStringLiteral("total_lives")].toInt(), 1);
    EXPECT_EQ(stats[QStringLiteral("total_articles")].toInt(), 0);
    EXPECT_EQ(stats[QStringLiteral("unique_authors")].toInt(), 2);
    EXPECT_EQ(stats[QStringLiteral("total_watch_seconds")].toLongLong(), 250);
    EXPECT_EQ(stats[QStringLiteral("avg_completion")].toInt(), 50);
}

TEST(AnalyticsTest, TopAuthors)
{
    RecordList records;
    const QDateTime now = QDateTime::currentDateTime();
    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v3"), QStringLiteral("UP_B"), QStringLiteral("动画"), 100, 50, now));

    const QVariantList authors = topAuthors(records, 10);
    ASSERT_EQ(authors.size(), 2);

    EXPECT_EQ(authors[0].toMap()[QStringLiteral("name")].toString(), QStringLiteral("UP_A"));
    EXPECT_EQ(authors[0].toMap()[QStringLiteral("count")].toInt(), 2);
    EXPECT_EQ(authors[1].toMap()[QStringLiteral("name")].toString(), QStringLiteral("UP_B"));
}

TEST(AnalyticsTest, TopCategories)
{
    RecordList records;
    const QDateTime now = QDateTime::currentDateTime();
    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("UP_B"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v3"), QStringLiteral("UP_C"), QStringLiteral("音乐"), 100, 100, now));

    const QVariantList categories = topCategories(records, 10);
    ASSERT_EQ(categories.size(), 2);

    EXPECT_EQ(categories[0].toMap()[QStringLiteral("name")].toString(), QStringLiteral("动画"));
    EXPECT_EQ(categories[0].toMap()[QStringLiteral("count")].toInt(), 2);
}

TEST(AnalyticsTest, TimeOfDayDistribution)
{
    RecordList records;
    QDateTime morning = QDateTime::currentDateTime();
    morning.setTime(QTime(9, 0));
    QDateTime evening = QDateTime::currentDateTime();
    evening.setTime(QTime(21, 0));

    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, morning));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, morning));
    records.push_back(makeVideo(QStringLiteral("v3"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, evening));

    const QVariantMap dist = timeOfDayDistribution(records);
    EXPECT_EQ(dist[QStringLiteral("上午")].toInt(), 2);
    EXPECT_EQ(dist[QStringLiteral("晚上")].toInt(), 1);
    EXPECT_EQ(dist[QStringLiteral("下午")].toInt(), 0);
}

TEST(AnalyticsTest, TopAuthorsByCompletion)
{
    RecordList records;
    const QDateTime now = QDateTime::currentDateTime();
    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v3"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, now));
    records.push_back(makeVideo(QStringLiteral("v4"), QStringLiteral("UP_B"), QStringLiteral("动画"), 100, 50, now));

    const QVariantList authors = topAuthorsByCompletion(records, 3, 10);
    ASSERT_EQ(authors.size(), 1);
    EXPECT_EQ(authors[0].toMap()[QStringLiteral("name")].toString(), QStringLiteral("UP_A"));
    EXPECT_EQ(authors[0].toMap()[QStringLiteral("avg_completion")].toInt(), 100);
}

TEST(AnalyticsTest, DailyTrend)
{
    RecordList records;
    QDateTime today = QDateTime::currentDateTime();
    QDateTime yesterday = today.addDays(-1);

    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, today));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, today));
    records.push_back(makeVideo(QStringLiteral("v3"), QStringLiteral("UP_A"), QStringLiteral("动画"), 100, 100, yesterday));

    const QVariantList trend = dailyTrend(records, 7);
    ASSERT_EQ(trend.size(), 7);

    const QVariantMap last = trend.last().toMap();
    EXPECT_EQ(last[QStringLiteral("count")].toInt(), 2);
}

TEST(AnalyticsTest, FormatDuration)
{
    EXPECT_EQ(formatDuration(0), QStringLiteral("0秒"));
    EXPECT_EQ(formatDuration(45), QStringLiteral("45秒"));
    EXPECT_EQ(formatDuration(125), QStringLiteral("2分5秒"));
    EXPECT_EQ(formatDuration(3665), QStringLiteral("1小时1分5秒"));
    EXPECT_TRUE(formatDuration(90061).contains(QStringLiteral("天")));
}
