#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "csv_storage.h"
#include "models.h"

using namespace bili;

TEST(CsvStorage, SaveAndLoadMixedRecords) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = dir.filePath("history.csv");

    RecordList records;
    auto video = std::make_shared<VideoRecord>();
    video->type = RecordType::Video;
    video->id = 1;
    video->bvId = QStringLiteral("BV1xx411c7mD");
    video->bvid = video->bvId;
    video->title = QStringLiteral("视频标题");
    video->authorName = QStringLiteral("UP主");
    video->category = QStringLiteral("动画");
    video->duration = 120;
    video->progressPercent = 100;
    records.push_back(video);

    auto live = std::make_shared<LiveRecord>();
    live->type = RecordType::Live;
    live->id = 2;
    live->roomId = QStringLiteral("123456");
    live->bvid = live->roomId;
    live->title = QStringLiteral("直播标题");
    live->category = QStringLiteral("直播中");
    records.push_back(live);

    auto article = std::make_shared<ArticleRecord>();
    article->type = RecordType::Article;
    article->id = 3;
    article->cvId = QStringLiteral("987654");
    article->bvid = article->cvId;
    article->title = QStringLiteral("专栏标题");
    article->category = QStringLiteral("知识");
    records.push_back(article);

    EXPECT_TRUE(CsvStorage::save(path, records));

    const auto result = CsvStorage::load(path);
    EXPECT_EQ(result.records.size(), 3u);
    EXPECT_EQ(result.errorCount, 0);

    auto loadedVideo = std::dynamic_pointer_cast<VideoRecord>(result.records[0]);
    ASSERT_NE(loadedVideo, nullptr);
    EXPECT_EQ(loadedVideo->bvId, QStringLiteral("BV1xx411c7mD"));
    EXPECT_EQ(loadedVideo->category, QStringLiteral("动画"));

    auto loadedLive = std::dynamic_pointer_cast<LiveRecord>(result.records[1]);
    ASSERT_NE(loadedLive, nullptr);
    EXPECT_EQ(loadedLive->roomId, QStringLiteral("123456"));

    auto loadedArticle = std::dynamic_pointer_cast<ArticleRecord>(result.records[2]);
    ASSERT_NE(loadedArticle, nullptr);
    EXPECT_EQ(loadedArticle->cvId, QStringLiteral("987654"));
}

TEST(CsvStorage, SkipMalformedRows) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = dir.filePath("history.csv");
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("schema_version,id,type,category,title,author_name,author_id,view_at,progress,progress_percent,bvid,cover_url,bv_id,cid,duration,room_id,live_id,live_status,cv_id,category_id,raw_json\n");
    file.write("1,1,video,动画,标题,UP主,0,2024-01-01T00:00:00,100%,100,BV1xx,cover.jpg,BV1xx,0,120,,0,,,0,\n");
    file.write("malformed row without enough fields\n");
    file.close();

    const auto result = CsvStorage::load(path);
    EXPECT_EQ(result.records.size(), 1u);
    EXPECT_EQ(result.errorCount, 1);
}
