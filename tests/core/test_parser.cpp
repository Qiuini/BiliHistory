#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>

#include "parser.h"

using namespace bili;

static QJsonObject jsonFromString(const QByteArray& data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.object();
}

TEST(Parser, ParseVideoRecord) {
    const QByteArray data = R"({
        "data": {
            "list": [{
                "title": "测试视频",
                "view_at": 1700000000,
                "duration": 120,
                "progress": 60,
                "author_name": "UP主",
                "cover": "https://example.com/cover.jpg",
                "history": {
                    "business": "archive",
                    "bvid": "BV1xx411c7mD"
                },
                "tname": "动画"
            }],
            "cursor": {"max": 12345, "ps": 20}
        }
    })";

    const auto result = Parser::parseHistory(jsonFromString(data));
    ASSERT_EQ(result.records.size(), 1u);
    ASSERT_EQ(result.pagination.hasMore, true);

    auto video = std::dynamic_pointer_cast<VideoRecord>(result.records[0]);
    ASSERT_NE(video, nullptr);
    EXPECT_EQ(video->type, RecordType::Video);
    EXPECT_EQ(video->bvId, QStringLiteral("BV1xx411c7mD"));
    EXPECT_EQ(video->title, QStringLiteral("测试视频"));
    EXPECT_EQ(video->authorName, QStringLiteral("UP主"));
    EXPECT_EQ(video->category, QStringLiteral("动画"));
    EXPECT_EQ(video->duration, 120);
    EXPECT_EQ(video->progressPercent, 50);
}

TEST(Parser, ParseLiveRecord) {
    const QByteArray data = R"({
        "data": {
            "list": [{
                "title": "测试直播",
                "view_at": 1700000000,
                "badge": "直播中",
                "upper": {"name": "主播"},
                "history": {
                    "business": "live",
                    "oid": "123456"
                }
            }]
        }
    })";

    const auto result = Parser::parseHistory(jsonFromString(data));
    ASSERT_EQ(result.records.size(), 1u);

    auto live = std::dynamic_pointer_cast<LiveRecord>(result.records[0]);
    ASSERT_NE(live, nullptr);
    EXPECT_EQ(live->type, RecordType::Live);
    EXPECT_EQ(live->roomId, QStringLiteral("123456"));
    EXPECT_EQ(live->category, QStringLiteral("直播中"));
}

TEST(Parser, ParseArticleRecord) {
    const QByteArray data = R"({
        "data": {
            "list": [{
                "title": "测试专栏",
                "view_at": 1700000000,
                "author_name": "作者",
                "template_id": 4,
                "history": {
                    "business": "article",
                    "oid": "987654"
                }
            }]
        }
    })";

    const auto result = Parser::parseHistory(jsonFromString(data));
    ASSERT_EQ(result.records.size(), 1u);

    auto article = std::dynamic_pointer_cast<ArticleRecord>(result.records[0]);
    ASSERT_NE(article, nullptr);
    EXPECT_EQ(article->type, RecordType::Article);
    EXPECT_EQ(article->cvId, QStringLiteral("987654"));
    EXPECT_EQ(article->category, QStringLiteral("动画"));
}

TEST(Parser, ParseFollowingList) {
    const QByteArray data = R"({
        "data": {
            "list": [
                {"mid": 123, "uname": "UP主A", "sign": "签名A", "face": "faceA", "level": 5},
                {"mid": 456, "uname": "UP主B", "sign": "签名B", "face": "faceB", "level": 6}
            ]
        }
    })";

    const auto list = Parser::parseFollowing(jsonFromString(data));
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].mid, 123);
    EXPECT_EQ(list[0].name, QStringLiteral("UP主A"));
    EXPECT_EQ(list[1].level, 6);
}

TEST(Parser, ParseFavoriteFolders) {
    const QByteArray data = R"({
        "data": {
            "list": [
                {"id": 1, "title": "默认收藏夹", "media_count": 10},
                {"id": 2, "title": "稍后观看", "media_count": 5}
            ]
        }
    })";

    const auto folders = Parser::parseFavoriteFolders(jsonFromString(data));
    ASSERT_EQ(folders.size(), 2u);
    EXPECT_EQ(folders[0].name, QStringLiteral("默认收藏夹"));
    EXPECT_EQ(folders[1].mediaCount, 5);
}

TEST(Parser, ParseUserCardRegistrationTime) {
    const QByteArray data = R"({
        "data": {
            "card": {
                "mid": 12345,
                "name": "用户",
                "regtime": 1600000000
            }
        }
    })";

    const auto regTime = Parser::parseRegistrationTime(jsonFromString(data));
    EXPECT_TRUE(regTime.isValid());
    EXPECT_EQ(regTime.toSecsSinceEpoch(), 1600000000);
}
