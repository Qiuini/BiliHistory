#include <gtest/gtest.h>

#include "business/exporter.h"
#include "core/models.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace bili;
using namespace bili::business;

namespace {

RecordPtr makeVideo(const QString& title,
                    const QString& author,
                    const QString& category,
                    int duration,
                    int progressPercent)
{
    auto record = std::make_shared<VideoRecord>();
    record->type = RecordType::Video;
    record->id = 1;
    record->title = title;
    record->authorName = author;
    record->category = category;
    record->duration = duration;
    record->progressPercent = progressPercent;
    record->viewAt = QDateTime::currentDateTime();
    record->bvid = QStringLiteral("BV1xx411c7mD");
    return record;
}

RecordList makeRecords()
{
    RecordList records;
    records.push_back(makeVideo(QStringLiteral("测试视频"), QStringLiteral("UP_A"), QStringLiteral("动画"), 120, 100));
    return records;
}

} // namespace

TEST(ExporterTest, ExportCsv)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = QDir(dir.path()).filePath(QStringLiteral("history.csv"));
    const QString result = exportCsv(makeRecords(), path);
    EXPECT_EQ(result, path);

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray data = file.readAll();
    EXPECT_TRUE(data.startsWith("\xEF\xBB\xBF"));
    EXPECT_TRUE(data.contains("schema_version"));
    EXPECT_TRUE(data.contains("测试视频"));
}

TEST(ExporterTest, ExportJson)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = QDir(dir.path()).filePath(QStringLiteral("history.json"));
    const QString result = exportJson(makeRecords(), path);
    EXPECT_EQ(result, path);

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray data = file.readAll();
    EXPECT_TRUE(data.contains("exported_at"));
    EXPECT_TRUE(data.contains("records"));
    EXPECT_TRUE(data.contains("测试视频"));
}

TEST(ExporterTest, ExportHtml)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = QDir(dir.path()).filePath(QStringLiteral("history.html"));
    const QString result = exportHtml(makeRecords(), path);
    EXPECT_EQ(result, path);

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray data = file.readAll();
    EXPECT_TRUE(data.contains("<!DOCTYPE html>"));
    EXPECT_TRUE(data.contains("测试视频"));
}

TEST(ExporterTest, ExportMarkdown)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = QDir(dir.path()).filePath(QStringLiteral("history.md"));
    const QString result = exportMarkdown(makeRecords(), path);
    EXPECT_EQ(result, path);

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray data = file.readAll();
    EXPECT_TRUE(data.contains("# BiliHistory"));
    EXPECT_TRUE(data.contains("测试视频"));
}

TEST(ExporterTest, ExportRecordsDispatchesByExtension)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString csvPath = QDir(dir.path()).filePath(QStringLiteral("history.csv"));
    EXPECT_EQ(exportRecords(makeRecords(), csvPath), csvPath);

    const QString jsonPath = QDir(dir.path()).filePath(QStringLiteral("history.json"));
    EXPECT_EQ(exportRecords(makeRecords(), jsonPath), jsonPath);
}

TEST(ExporterTest, ExportRecordsThrowsOnUnsupportedExtension)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = QDir(dir.path()).filePath(QStringLiteral("history.xyz"));
    EXPECT_THROW(exportRecords(makeRecords(), path), ExportException);
}
