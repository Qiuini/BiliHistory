#include <gtest/gtest.h>

#include "business/exporter.h"
#include "business/xlsx_writer.h"
#include "core/models.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariantList>

#include "miniz.h"

#include <vector>

using namespace bili;
using namespace bili::business;

namespace {

// 读取 ZIP 中指定文件名为 QByteArray；文件不存在或读取失败返回空 QByteArray（并设置 *ok=false）。
QByteArray readZipEntry(const QString& zipPath, const std::string& entryName, bool* ok = nullptr)
{
    if (ok) *ok = false;
    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    if (mz_zip_reader_init_file(&zip, zipPath.toUtf8().constData(), 0) != MZ_TRUE) {
        return {};
    }

    const int index = mz_zip_reader_locate_file(&zip, entryName.c_str(), nullptr, 0);
    if (index < 0) {
        mz_zip_reader_end(&zip);
        return {};
    }

    mz_zip_archive_file_stat stat{};
    if (mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(index), &stat) != MZ_TRUE) {
        mz_zip_reader_end(&zip);
        return {};
    }

    std::vector<char> buf(stat.m_uncomp_size);
    if (mz_zip_reader_extract_to_mem(&zip, static_cast<mz_uint>(index), buf.data(), buf.size(), 0) != MZ_TRUE) {
        mz_zip_reader_end(&zip);
        return {};
    }

    mz_zip_reader_end(&zip);
    if (ok) *ok = true;
    return QByteArray(buf.data(), static_cast<int>(buf.size()));
}

// 列出 ZIP 中所有文件名。
QStringList listZipEntries(const QString& zipPath)
{
    QStringList result;
    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    if (mz_zip_reader_init_file(&zip, zipPath.toUtf8().constData(), 0) != MZ_TRUE) {
        return result;
    }
    const mz_uint num = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < num; ++i) {
        mz_zip_archive_file_stat stat{};
        if (mz_zip_reader_file_stat(&zip, i, &stat) == MZ_TRUE) {
            result.append(QString::fromUtf8(stat.m_filename));
        }
    }
    mz_zip_reader_end(&zip);
    return result;
}

// 校验给定 xlsx 文件可被 miniz 打开且包含指定必备条目。
bool zipContainsEntries(const QString& zipPath, const std::vector<std::string>& required)
{
    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    if (mz_zip_reader_init_file(&zip, zipPath.toUtf8().constData(), 0) != MZ_TRUE) {
        return false;
    }
    bool allFound = true;
    for (const auto& name : required) {
        if (mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, 0) < 0) {
            allFound = false;
            break;
        }
    }
    mz_zip_reader_end(&zip);
    return allFound;
}

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
    record->duration = 120;
    return record;
}

} // namespace

// ===== 基本写入 =====

TEST(XlsxWriterTest, SaveBasicFile)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("basic.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("标题"), QStringLiteral("UP 主") });
    writer.addRow({ QVariant(QStringLiteral("测试视频")), QVariant(QStringLiteral("UP_A")) });

    EXPECT_TRUE(writer.save(path));

    QFile file(path);
    ASSERT_TRUE(file.exists());
    EXPECT_GT(file.size(), 0);
}

TEST(XlsxWriterTest, SaveReturnsTrueOnValidPath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("valid.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    EXPECT_TRUE(writer.save(path));
}

TEST(XlsxWriterTest, SaveCreatesParentDirectory)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("sub/dir/nested.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    EXPECT_TRUE(writer.save(path));
    EXPECT_TRUE(QFile::exists(path));
}

// ===== 文件格式验证 (ZIP 魔数 + miniz 可打开) =====

TEST(XlsxWriterTest, FileStartsWithZipMagic)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("magic.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    writer.addRow({ QVariant(QStringLiteral("v")) });
    ASSERT_TRUE(writer.save(path));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray head = file.read(4);
    ASSERT_EQ(head.size(), 4);
    // PK\x03\x04
    EXPECT_EQ(static_cast<quint8>(head[0]), 0x50u); // 'P'
    EXPECT_EQ(static_cast<quint8>(head[1]), 0x4Bu); // 'K'
    EXPECT_EQ(static_cast<quint8>(head[2]), 0x03u);
    EXPECT_EQ(static_cast<quint8>(head[3]), 0x04u);
}

TEST(XlsxWriterTest, ZipArchiveContainsRequiredEntries)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("entries.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    writer.addRow({ QVariant(QStringLiteral("v")) });
    ASSERT_TRUE(writer.save(path));

    const std::vector<std::string> required = {
        "[Content_Types].xml",
        "_rels/.rels",
        "xl/workbook.xml",
        "xl/worksheets/sheet1.xml",
        "xl/styles.xml",
        "xl/_rels/workbook.xml.rels"
    };
    EXPECT_TRUE(zipContainsEntries(path, required));
}

TEST(XlsxWriterTest, ZipArchiveIsOpenable)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("openable.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    ASSERT_TRUE(writer.save(path));

    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    EXPECT_EQ(mz_zip_reader_init_file(&zip, path.toUtf8().constData(), 0), MZ_TRUE);
    EXPECT_GT(mz_zip_reader_get_num_files(&zip), 0u);
    mz_zip_reader_end(&zip);
}

// ===== 内容验证 =====

TEST(XlsxWriterTest, HeadersPresentInWorksheet)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("headers.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("标题"), QStringLiteral("UP 主"), QStringLiteral("BV 号") });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    ASSERT_FALSE(sheet.isEmpty());

    EXPECT_TRUE(sheet.contains("标题"));
    EXPECT_TRUE(sheet.contains("UP 主"));
    EXPECT_TRUE(sheet.contains("BV 号"));
}

TEST(XlsxWriterTest, DataRowsPresentInWorksheet)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("data.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("标题"), QStringLiteral("UP 主") });
    writer.addRow({ QVariant(QStringLiteral("原神3.0")), QVariant(QStringLiteral("老番茄")) });
    writer.addRow({ QVariant(QStringLiteral("星穹铁道")), QVariant(QStringLiteral("米哈游")) });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("原神3.0"));
    EXPECT_TRUE(sheet.contains("老番茄"));
    EXPECT_TRUE(sheet.contains("星穹铁道"));
    EXPECT_TRUE(sheet.contains("米哈游"));
}

TEST(XlsxWriterTest, WorkbookContainsSheetName)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("wb.xlsx"));

    XlsxWriter writer(QStringLiteral("历史记录"));
    writer.setHeaders({ QStringLiteral("A") });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray wb = readZipEntry(path, "xl/workbook.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(wb.contains("历史记录"));
}

TEST(XlsxWriterTest, ContentTypesDeclaresWorksheet)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("ct.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray ct = readZipEntry(path, "[Content_Types].xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(ct.contains("spreadsheetml.sheet.main+xml"));
    EXPECT_TRUE(ct.contains("spreadsheetml.worksheet+xml"));
}

// ===== 空数据 =====

TEST(XlsxWriterTest, EmptyRowsStillProducesValidXlsx)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("empty_rows.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("标题"), QStringLiteral("UP 主") });
    // 不 addRow
    ASSERT_TRUE(writer.save(path));

    // 仍是合法 ZIP
    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    EXPECT_EQ(mz_zip_reader_init_file(&zip, path.toUtf8().constData(), 0), MZ_TRUE);
    EXPECT_GT(mz_zip_reader_get_num_files(&zip), 0u);
    mz_zip_reader_end(&zip);

    // worksheet 仍包含表头但不包含数据行
    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("标题"));
}

TEST(XlsxWriterTest, NoHeadersStillProducesValidXlsx)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("no_headers.xlsx"));

    XlsxWriter writer;
    // 不 setHeaders
    writer.addRow({ QVariant(QStringLiteral("v1")) });
    EXPECT_TRUE(writer.save(path));
    EXPECT_TRUE(QFile::exists(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("v1"));
}

TEST(XlsxWriterTest, ExportXlsxWithEmptyRecords)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("empty_records.xlsx"));

    RecordList records;
    const QString result = exportXlsx(records, path);
    EXPECT_EQ(result, path);

    // 仍生成合法 xlsx（含表头行）
    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("类型"));
    EXPECT_TRUE(sheet.contains("BV 号"));
}

// ===== 特殊字符转义 =====

TEST(XlsxWriterTest, SpecialCharsAreXmlEscaped)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("escape.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("字段") });
    // 包含 < > & " '
    writer.addRow({ QVariant(QStringLiteral("<tag> & \"quote\" 'apos'")) });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);

    // 应该被转义为 &lt; &gt; &amp; &quot; &apos;
    EXPECT_TRUE(sheet.contains("&lt;tag&gt;"));
    EXPECT_TRUE(sheet.contains("&amp;"));
    EXPECT_TRUE(sheet.contains("&quot;quote&quot;"));
    EXPECT_TRUE(sheet.contains("&apos;apos&apos;"));

    // 不应该出现未转义的裸字符（注意 XML 声明里的 ? 不算）
    // 检查单元格内不应有裸 < （<tag> 已被转义）
    // 取出 <t>...</t> 部分校验
    int tPos = sheet.indexOf("<t>");
    ASSERT_GE(tPos, 0);
    int tEnd = sheet.indexOf("</t>", tPos);
    ASSERT_GT(tEnd, tPos);
    const QByteArray inner = sheet.mid(tPos + 3, tEnd - tPos - 3);
    EXPECT_FALSE(inner.contains("<"));
    EXPECT_FALSE(inner.contains(">"));
    EXPECT_FALSE(inner.contains("&amp;amp")); // 双重转义检查
}

TEST(XlsxWriterTest, ExportXlsxHandlesSpecialCharsInRecords)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("escape2.xlsx"));

    RecordList records;
    records.push_back(makeVideo(QStringLiteral("<原神> & \"3.0\""),
                                QStringLiteral("UP&UP"),
                                QStringLiteral("动画&音乐"),
                                QStringLiteral("BV1<x>"),
                                50,
                                QDateTime(QDate(2024, 1, 1), QTime(12, 0))));
    EXPECT_NO_THROW(exportXlsx(records, path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("&lt;原神&gt;"));
    EXPECT_TRUE(sheet.contains("&quot;3.0&quot;"));
    EXPECT_TRUE(sheet.contains("UP&amp;UP"));
    EXPECT_TRUE(sheet.contains("动画&amp;音乐"));
}

// ===== 数字类型 =====

TEST(XlsxWriterTest, NumericTypesWrittenAsValues)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("nums.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("整型"), QStringLiteral("浮点") });
    writer.addRow({ QVariant(static_cast<int>(42)), QVariant(static_cast<double>(3.14)) });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);

    // 数字以 <v>...</v> 形式写入
    EXPECT_TRUE(sheet.contains("<v>42</v>"));
    EXPECT_TRUE(sheet.contains("3.14"));
}

TEST(XlsxWriterTest, ExportXlsxNumericFieldsUseValueTag)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("nums2.xlsx"));

    RecordList records;
    auto r = makeVideo(QStringLiteral("t"), QStringLiteral("UP"),
                       QStringLiteral("动画"), QStringLiteral("BV1xx411c7mD"),
                       75, QDateTime(QDate(2024, 6, 15), QTime(12, 0)));
    r->authorId = 12345;
    records.push_back(r);
    EXPECT_NO_THROW(exportXlsx(records, path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    // authorId 是 qint64 → 数字单元格
    EXPECT_TRUE(sheet.contains("<v>12345</v>"));
    // progressPercent 是 int → 数字单元格
    EXPECT_TRUE(sheet.contains("<v>75</v>"));
}

// ===== 多列：列数与表头一致 =====

TEST(XlsxWriterTest, MultipleColumnsMatchHeaders)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("cols.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("C1"), QStringLiteral("C2"), QStringLiteral("C3"), QStringLiteral("C4") });
    writer.addRow({ QVariant(QStringLiteral("a")), QVariant(QStringLiteral("b")),
                    QVariant(QStringLiteral("c")), QVariant(QStringLiteral("d")) });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);

    // 验证表头行 (row 1) 包含 A1, B1, C1, D1 单元格引用
    EXPECT_TRUE(sheet.contains("r=\"A1\""));
    EXPECT_TRUE(sheet.contains("r=\"B1\""));
    EXPECT_TRUE(sheet.contains("r=\"C1\""));
    EXPECT_TRUE(sheet.contains("r=\"D1\""));

    // 验证数据行 (row 2) 包含 A2, B2, C2, D2 单元格引用
    EXPECT_TRUE(sheet.contains("r=\"A2\""));
    EXPECT_TRUE(sheet.contains("r=\"B2\""));
    EXPECT_TRUE(sheet.contains("r=\"C2\""));
    EXPECT_TRUE(sheet.contains("r=\"D2\""));
}

TEST(XlsxWriterTest, CellReferenceForManyColumns)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("cells.xlsx"));

    XlsxWriter writer;
    QStringList headers;
    for (int i = 0; i < 5; ++i) {
        headers << QStringLiteral("H%1").arg(i + 1);
    }
    writer.setHeaders(headers);
    QVariantList row;
    for (int i = 0; i < 5; ++i) {
        row << QVariant(QStringLiteral("v%1").arg(i + 1));
    }
    writer.addRow(row);
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    // 第 5 列 = E
    EXPECT_TRUE(sheet.contains("r=\"E1\""));
    EXPECT_TRUE(sheet.contains("r=\"E2\""));
    EXPECT_TRUE(sheet.contains("v5"));
}

// ===== exportXlsx 端到端 =====

TEST(XlsxWriterTest, ExportXlsxEndToEnd)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("e2e.xlsx"));

    RecordList records;
    records.push_back(makeVideo(QStringLiteral("原神3.0"), QStringLiteral("老番茄"),
                                QStringLiteral("动画"), QStringLiteral("BV1xx411c7mD"),
                                100, QDateTime(QDate(2024, 6, 15), QTime(12, 0))));
    records.push_back(makeVideo(QStringLiteral("星穹铁道"), QStringLiteral("米哈游"),
                                QStringLiteral("游戏"), QStringLiteral("BV2yy422c8nE"),
                                50, QDateTime(QDate(2024, 7, 20), QTime(18, 30))));

    QString result;
    EXPECT_NO_THROW(result = exportXlsx(records, path));
    EXPECT_EQ(result, path);
    EXPECT_TRUE(QFile::exists(path));

    // ZIP 完整性
    EXPECT_TRUE(zipContainsEntries(path, {
        "[Content_Types].xml",
        "xl/workbook.xml",
        "xl/worksheets/sheet1.xml"
    }));

    // 内容
    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("原神3.0"));
    EXPECT_TRUE(sheet.contains("星穹铁道"));
    EXPECT_TRUE(sheet.contains("老番茄"));
    EXPECT_TRUE(sheet.contains("BV1xx411c7mD"));
}

TEST(XlsxWriterTest, ExportXlsxThrowsOnInvalidPath)
{
    // Windows 非法路径（含 NUL 等会失败）
    // 这里用一个不可能存在的路径触发失败
    RecordList records;
    records.push_back(makeVideo(QStringLiteral("t"), QStringLiteral("a"),
                                QStringLiteral("c"), QStringLiteral("BV1"),
                                50, QDateTime::currentDateTime()));
    // 路径包含非法字符 < > 等，save 会失败 → 抛 ExportException
    EXPECT_THROW(exportXlsx(records, QStringLiteral("Z:/nonexistent_dir_<x>/out.xlsx")), ExportException);
}

// ===== DateTime / Date 转换 =====

TEST(XlsxWriterTest, DateTimeFieldsFormattedAsString)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("dt.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("时间") });
    writer.addRow({ QVariant(QDateTime(QDate(2024, 6, 15), QTime(12, 30, 45))) });
    ASSERT_TRUE(writer.save(path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("2024-06-15 12:30:45"));
}

TEST(XlsxWriterTest, ExportXlsxViewAtFormatted)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("vt.xlsx"));

    RecordList records;
    records.push_back(makeVideo(QStringLiteral("t"), QStringLiteral("a"),
                                QStringLiteral("c"), QStringLiteral("BV1"),
                                50, QDateTime(QDate(2024, 1, 2), QTime(3, 4, 5))));
    EXPECT_NO_THROW(exportXlsx(records, path));

    bool ok = false;
    const QByteArray sheet = readZipEntry(path, "xl/worksheets/sheet1.xml", &ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(sheet.contains("2024-01-02 03:04:05"));
}

// ===== 多条目 ZIP 完整性 =====

TEST(XlsxWriterTest, ZipHasConsistentEntries)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("cons.xlsx"));

    XlsxWriter writer;
    writer.setHeaders({ QStringLiteral("A") });
    writer.addRow({ QVariant(QStringLiteral("v1")) });
    writer.addRow({ QVariant(QStringLiteral("v2")) });
    ASSERT_TRUE(writer.save(path));

    const QStringList entries = listZipEntries(path);
    EXPECT_GE(entries.size(), 6);
    EXPECT_TRUE(entries.contains(QStringLiteral("[Content_Types].xml")));
    EXPECT_TRUE(entries.contains(QStringLiteral("_rels/.rels")));
    EXPECT_TRUE(entries.contains(QStringLiteral("xl/workbook.xml")));
    EXPECT_TRUE(entries.contains(QStringLiteral("xl/worksheets/sheet1.xml")));
    EXPECT_TRUE(entries.contains(QStringLiteral("xl/styles.xml")));
    EXPECT_TRUE(entries.contains(QStringLiteral("xl/_rels/workbook.xml.rels")));
}
