#include <gtest/gtest.h>

#include "business/filter.h"
#include "core/models.h"

#include <QDateTime>
#include <QSet>

using namespace bili;
using namespace bili::business;

namespace {

RecordPtr makeVideo(const QString& title,
                    const QString& author,
                    const QString& category,
                    const QString& bvid,
                    int progressPercent,
                    const QDateTime& viewAt)
{
    auto record = std::make_shared<VideoRecord>();
    record->type = RecordType::Video;
    record->title = title;
    record->authorName = author;
    record->category = category;
    record->bvid = bvid;
    record->progressPercent = progressPercent;
    record->viewAt = viewAt;
    return record;
}

RecordPtr makeLive(const QString& title,
                   const QString& author,
                   const QString& bvid,
                   int progressPercent,
                   const QDateTime& viewAt)
{
    auto record = std::make_shared<LiveRecord>();
    record->type = RecordType::Live;
    record->title = title;
    record->authorName = author;
    record->category = QStringLiteral("直播");
    record->bvid = bvid;
    record->progressPercent = progressPercent;
    record->viewAt = viewAt;
    return record;
}

RecordPtr makeArticle(const QString& title,
                      const QString& author,
                      const QString& bvid,
                      int progressPercent,
                      const QDateTime& viewAt)
{
    auto record = std::make_shared<ArticleRecord>();
    record->type = RecordType::Article;
    record->title = title;
    record->authorName = author;
    record->category = QStringLiteral("专栏");
    record->bvid = bvid;
    record->progressPercent = progressPercent;
    record->viewAt = viewAt;
    return record;
}

QDateTime dt(int year, int month, int day, int hour = 12, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute));
}

} // namespace

// ===== 时间范围筛选 =====

TEST(FilterTest, TimeRange_Inside)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, TimeRange_OutsideBefore)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2023, 6, 15));
    EXPECT_FALSE(matchesFilter(r.get(), c));
}

TEST(FilterTest, TimeRange_OutsideAfter)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2025, 6, 15));
    EXPECT_FALSE(matchesFilter(r.get(), c));
}

TEST(FilterTest, TimeRange_BoundaryStartInclusive)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, TimeRange_BoundaryEndInclusive)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 12, 31));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, TimeRange_OnlyStart)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    auto r1 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    auto r2 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 50, dt(2023, 6, 15));
    EXPECT_TRUE(matchesFilter(r1.get(), c));
    EXPECT_FALSE(matchesFilter(r2.get(), c));
}

TEST(FilterTest, TimeRange_OnlyEnd)
{
    FilterCriteria c;
    c.endTime = dt(2024, 12, 31);
    auto r1 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    auto r2 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 50, dt(2025, 6, 15));
    EXPECT_TRUE(matchesFilter(r1.get(), c));
    EXPECT_FALSE(matchesFilter(r2.get(), c));
}

// ===== 类型集合筛选 =====

TEST(FilterTest, Types_SingleMatch)
{
    FilterCriteria c;
    c.types = { RecordType::Video };
    auto v = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto l = makeLive(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("LV1"),
                      50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(v.get(), c));
    EXPECT_FALSE(matchesFilter(l.get(), c));
}

TEST(FilterTest, Types_MultipleMatch)
{
    FilterCriteria c;
    c.types = { RecordType::Video, RecordType::Live };
    auto v = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto l = makeLive(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("LV1"),
                      50, dt(2024, 1, 1));
    auto a = makeArticle(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("CV1"),
                         50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(v.get(), c));
    EXPECT_TRUE(matchesFilter(l.get(), c));
    EXPECT_FALSE(matchesFilter(a.get(), c));
}

TEST(FilterTest, Types_EmptyMatchesAll)
{
    FilterCriteria c;
    // types 留空
    auto v = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto l = makeLive(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("LV1"),
                      50, dt(2024, 1, 1));
    auto a = makeArticle(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("CV1"),
                         50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(v.get(), c));
    EXPECT_TRUE(matchesFilter(l.get(), c));
    EXPECT_TRUE(matchesFilter(a.get(), c));
}

// ===== 分类模糊匹配 =====

TEST(FilterTest, Category_FuzzyMatch)
{
    FilterCriteria c;
    c.category = QStringLiteral("动画");
    auto r1 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("动画"),
                        QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto r2 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("动画-国创"),
                        QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto r3 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("音乐"),
                        QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r1.get(), c));
    EXPECT_TRUE(matchesFilter(r2.get(), c));
    EXPECT_FALSE(matchesFilter(r3.get(), c));
}

TEST(FilterTest, Category_CaseInsensitive)
{
    FilterCriteria c;
    c.category = QStringLiteral("ANIME");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("anime"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

// ===== 作者模糊匹配 =====

TEST(FilterTest, Author_FuzzyMatch)
{
    FilterCriteria c;
    c.author = QStringLiteral("老番茄");
    auto r1 = makeVideo(QStringLiteral("t"), QStringLiteral("老番茄"),
                        QStringLiteral("c"), QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto r2 = makeVideo(QStringLiteral("t"), QStringLiteral("老番茄工作室"),
                        QStringLiteral("c"), QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto r3 = makeVideo(QStringLiteral("t"), QStringLiteral("其他UP"),
                        QStringLiteral("c"), QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r1.get(), c));
    EXPECT_TRUE(matchesFilter(r2.get(), c));
    EXPECT_FALSE(matchesFilter(r3.get(), c));
}

TEST(FilterTest, Author_CaseInsensitive)
{
    FilterCriteria c;
    c.author = QStringLiteral("UP_A");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("up_a"),
                       QStringLiteral("c"), QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

// ===== 进度区间筛选 =====

TEST(FilterTest, Progress_InsideRange)
{
    FilterCriteria c;
    c.minProgress = 30;
    c.maxProgress = 70;
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Progress_BelowMin)
{
    FilterCriteria c;
    c.minProgress = 60;
    c.maxProgress = 100;
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_FALSE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Progress_AboveMax)
{
    FilterCriteria c;
    c.minProgress = 0;
    c.maxProgress = 40;
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_FALSE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Progress_BoundaryMinMaxInclusive)
{
    FilterCriteria c;
    c.minProgress = 50;
    c.maxProgress = 50;
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Progress_NegativeMeansComplete)
{
    // progressPercent < 0 => effectiveCompletionPercent = 100
    FilterCriteria c;
    c.minProgress = 95;
    c.maxProgress = 100;
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), -1, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

// ===== 关键字筛选 =====

TEST(FilterTest, Keyword_MatchesTitle)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("原神");
    auto r = makeVideo(QStringLiteral("原神3.0"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Keyword_MatchesAuthor)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("番茄");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("老番茄"),
                       QStringLiteral("c"), QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Keyword_MatchesBvid)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("BV1xx411c7mD");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1xx411c7mD"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Keyword_MatchesCategory)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("动画");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("动画"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Keyword_NoMatch)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("不存在的关键字XYZ");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_FALSE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Keyword_CaseInsensitive)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("HELLO");
    auto r = makeVideo(QStringLiteral("hello world"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

// ===== 多条件组合 (AND 语义) =====

TEST(FilterTest, Combined_AllConditionsMatch)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    c.types = { RecordType::Video };
    c.category = QStringLiteral("动画");
    c.author = QStringLiteral("番茄");
    c.minProgress = 30;
    c.maxProgress = 80;
    c.keyword = QStringLiteral("原神");

    auto r = makeVideo(QStringLiteral("原神3.0"), QStringLiteral("老番茄"),
                       QStringLiteral("动画"), QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Combined_OneConditionFails)
{
    FilterCriteria c;
    c.startTime = dt(2024, 1, 1);
    c.endTime = dt(2024, 12, 31);
    c.types = { RecordType::Video };
    c.category = QStringLiteral("动画");
    c.author = QStringLiteral("番茄");
    c.minProgress = 30;
    c.maxProgress = 80;
    c.keyword = QStringLiteral("原神");

    // 类型不匹配（Live）
    auto r1 = makeLive(QStringLiteral("原神3.0"), QStringLiteral("老番茄"),
                       QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    EXPECT_FALSE(matchesFilter(r1.get(), c));

    // 时间不匹配
    auto r2 = makeVideo(QStringLiteral("原神3.0"), QStringLiteral("老番茄"),
                        QStringLiteral("动画"), QStringLiteral("BV1"), 50, dt(2023, 6, 15));
    EXPECT_FALSE(matchesFilter(r2.get(), c));

    // 分类不匹配
    auto r3 = makeVideo(QStringLiteral("原神3.0"), QStringLiteral("老番茄"),
                        QStringLiteral("音乐"), QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    EXPECT_FALSE(matchesFilter(r3.get(), c));

    // 作者不匹配
    auto r4 = makeVideo(QStringLiteral("原神3.0"), QStringLiteral("其他人"),
                        QStringLiteral("动画"), QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    EXPECT_FALSE(matchesFilter(r4.get(), c));

    // 进度不匹配
    auto r5 = makeVideo(QStringLiteral("原神3.0"), QStringLiteral("老番茄"),
                        QStringLiteral("动画"), QStringLiteral("BV1"), 10, dt(2024, 6, 15));
    EXPECT_FALSE(matchesFilter(r5.get(), c));

    // 关键字不匹配
    auto r6 = makeVideo(QStringLiteral("其他"), QStringLiteral("老番茄"),
                        QStringLiteral("动画"), QStringLiteral("BV1"), 50, dt(2024, 6, 15));
    EXPECT_FALSE(matchesFilter(r6.get(), c));
}

// ===== 空筛选条件 =====

TEST(FilterTest, EmptyCriteria_MatchesAll)
{
    FilterCriteria c;  // 全部默认值
    auto v = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto l = makeLive(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("LV1"),
                      50, dt(2024, 1, 1));
    auto a = makeArticle(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("CV1"),
                         50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(v.get(), c));
    EXPECT_TRUE(matchesFilter(l.get(), c));
    EXPECT_TRUE(matchesFilter(a.get(), c));
}

// ===== filterRecords 集合行为 =====

TEST(FilterTest, FilterRecords_EmptyList)
{
    FilterCriteria c;
    RecordList records;
    const RecordList result = filterRecords(records, c);
    EXPECT_TRUE(result.empty());
}

TEST(FilterTest, FilterRecords_AllMatch)
{
    FilterCriteria c;
    RecordList records;
    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("a"), QStringLiteral("c"),
                                QStringLiteral("BV1"), 50, dt(2024, 1, 1)));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("a"), QStringLiteral("c"),
                                QStringLiteral("BV2"), 50, dt(2024, 1, 1)));
    const RecordList result = filterRecords(records, c);
    EXPECT_EQ(result.size(), 2u);
}

TEST(FilterTest, FilterRecords_NoneMatch)
{
    FilterCriteria c;
    c.types = { RecordType::Live };
    RecordList records;
    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("a"), QStringLiteral("c"),
                                QStringLiteral("BV1"), 50, dt(2024, 1, 1)));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("a"), QStringLiteral("c"),
                                QStringLiteral("BV2"), 50, dt(2024, 1, 1)));
    const RecordList result = filterRecords(records, c);
    EXPECT_TRUE(result.empty());
}

TEST(FilterTest, FilterRecords_PartialMatch)
{
    FilterCriteria c;
    c.category = QStringLiteral("动画");
    RecordList records;
    records.push_back(makeVideo(QStringLiteral("v1"), QStringLiteral("a"), QStringLiteral("动画"),
                                QStringLiteral("BV1"), 50, dt(2024, 1, 1)));
    records.push_back(makeVideo(QStringLiteral("v2"), QStringLiteral("a"), QStringLiteral("音乐"),
                                QStringLiteral("BV2"), 50, dt(2024, 1, 1)));
    records.push_back(makeVideo(QStringLiteral("v3"), QStringLiteral("a"), QStringLiteral("动画"),
                                QStringLiteral("BV3"), 50, dt(2024, 1, 1)));
    const RecordList result = filterRecords(records, c);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0]->title, QStringLiteral("v1"));
    EXPECT_EQ(result[1]->title, QStringLiteral("v3"));
}

TEST(FilterTest, FilterRecords_PreservesSharedPtrs)
{
    FilterCriteria c;
    RecordList records;
    auto r1 = makeVideo(QStringLiteral("v1"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto raw = r1.get();
    records.push_back(r1);
    const RecordList result = filterRecords(records, c);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].get(), raw);
    EXPECT_EQ(result[0].use_count(), 3); // records, r1, result
}

// ===== 边界 case =====

TEST(FilterTest, NullRecord_ReturnsFalse)
{
    FilterCriteria c;
    EXPECT_FALSE(matchesFilter(nullptr, c));
}

TEST(FilterTest, EmptyStringCriteria_MatchesAll)
{
    FilterCriteria c;
    c.category = QStringLiteral("");
    c.author = QStringLiteral("");
    c.keyword = QStringLiteral("");
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, EmptyFields_KeywordNoMatchOnEmptyTitle)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("hello");
    auto r = makeVideo(QStringLiteral(""), QStringLiteral(""), QStringLiteral(""),
                       QStringLiteral(""), 50, dt(2024, 1, 1));
    EXPECT_FALSE(matchesFilter(r.get(), c));
}

TEST(FilterTest, EmptyFields_EmptyKeywordMatches)
{
    FilterCriteria c;
    c.keyword = QStringLiteral("");
    auto r = makeVideo(QStringLiteral(""), QStringLiteral(""), QStringLiteral(""),
                       QStringLiteral(""), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}

TEST(FilterTest, Progress_ExtremeValuesMin)
{
    FilterCriteria c;
    c.minProgress = 0;
    c.maxProgress = 0;
    auto r0 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 0, dt(2024, 1, 1));
    auto r1 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 1, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r0.get(), c));
    EXPECT_FALSE(matchesFilter(r1.get(), c));
}

TEST(FilterTest, Progress_ExtremeValuesMax)
{
    FilterCriteria c;
    c.minProgress = 100;
    c.maxProgress = 100;
    auto r100 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                          QStringLiteral("BV1"), 100, dt(2024, 1, 1));
    auto r99 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                         QStringLiteral("BV1"), 99, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r100.get(), c));
    EXPECT_FALSE(matchesFilter(r99.get(), c));
}

TEST(FilterTest, Progress_DefaultRangeMatchesAll)
{
    FilterCriteria c; // minProgress=0, maxProgress=100 (默认值)
    auto r0 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                        QStringLiteral("BV1"), 0, dt(2024, 1, 1));
    auto r50 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                         QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    auto r100 = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                          QStringLiteral("BV1"), 100, dt(2024, 1, 1));
    auto rNeg = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                          QStringLiteral("BV1"), -1, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r0.get(), c));
    EXPECT_TRUE(matchesFilter(r50.get(), c));
    EXPECT_TRUE(matchesFilter(r100.get(), c));
    EXPECT_TRUE(matchesFilter(rNeg.get(), c));
}

TEST(FilterTest, InvalidDateTime_DoesNotFilter)
{
    // startTime 无效时不应触发时间过滤
    FilterCriteria c;
    c.startTime = QDateTime(); // invalid
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("a"), QStringLiteral("c"),
                       QStringLiteral("BV1"), 50, dt(2024, 1, 1));
    EXPECT_TRUE(matchesFilter(r.get(), c));
}
