#include "xlsx_writer.h"

#include "exporter.h"
#include "miniz.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace bili::business {

namespace {

constexpr const char* ContentTypesXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
  <Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
  <Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>
</Types>
)";

constexpr const char* RelsXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>
)";

constexpr const char* WorkbookRelsXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
</Relationships>
)";

constexpr const char* StylesXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
  <fonts count="2">
    <font><sz val="11"/><name val="Calibri"/></font>
    <font><b/><sz val="11"/><name val="Calibri"/></font>
  </fonts>
  <fills count="2">
    <fill><patternFill patternType="none"/></fill>
    <fill><patternFill patternType="gray125"/></fill>
  </fills>
  <borders count="1">
    <border><left/><right/><top/><bottom/><diagonal/></border>
  </borders>
  <cellXfs count="2">
    <xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>
    <xf numFmtId="0" fontId="1" fillId="0" borderId="0" xfId="0" applyFont="1"/>
  </cellXfs>
</styleSheet>
)";

bool addZipFile(mz_zip_archive& zip, const char* path, const QByteArray& content)
{
    return mz_zip_writer_add_mem(&zip, path, content.constData(), content.size(), MZ_BEST_COMPRESSION) == MZ_TRUE;
}

QVariant recordFieldToVariant(const QVariant& value)
{
    if (value.userType() == QMetaType::QDateTime) {
        return value.toDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    }
    if (value.userType() == QMetaType::QDate) {
        return value.toDate().toString(QStringLiteral("yyyy-MM-dd"));
    }
    return value;
}

} // namespace

XlsxWriter::XlsxWriter(const QString& sheetName)
    : m_sheetName(sheetName)
{
}

void XlsxWriter::setHeaders(const QStringList& headers)
{
    m_headers = headers;
}

void XlsxWriter::addRow(const QVariantList& row)
{
    m_rows.push_back(row);
}

bool XlsxWriter::save(const QString& filePath) const
{
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    mz_zip_archive zip{};
    mz_zip_zero_struct(&zip);
    if (mz_zip_writer_init_file(&zip, filePath.toUtf8().constData(), 0) != MZ_TRUE) {
        return false;
    }

    bool ok = true;
    ok &= addZipFile(zip, "[Content_Types].xml", buildContentTypes());
    ok &= addZipFile(zip, "_rels/.rels", buildRels());
    ok &= addZipFile(zip, "xl/_rels/workbook.xml.rels", buildWorkbookRels());
    ok &= addZipFile(zip, "xl/workbook.xml", buildWorkbook());
    ok &= addZipFile(zip, "xl/styles.xml", QByteArray(StylesXml));
    ok &= addZipFile(zip, "xl/worksheets/sheet1.xml", buildWorksheet());

    ok &= (mz_zip_writer_finalize_archive(&zip) == MZ_TRUE);
    ok &= (mz_zip_writer_end(&zip) == MZ_TRUE);
    return ok;
}

QString XlsxWriter::escapeXml(const QString& text) const
{
    QString escaped = text;
    escaped.replace('&', QStringLiteral("&amp;"));
    escaped.replace('<', QStringLiteral("&lt;"));
    escaped.replace('>', QStringLiteral("&gt;"));
    escaped.replace('"', QStringLiteral("&quot;"));
    escaped.replace('\'', QStringLiteral("&apos;"));
    return escaped;
}

QString XlsxWriter::cellReference(int row, int col) const
{
    QString colRef;
    int c = col;
    while (c >= 0) {
        colRef.prepend(QChar('A' + (c % 26)));
        c = c / 26 - 1;
    }
    return colRef + QString::number(row + 1);
}

QByteArray XlsxWriter::buildContentTypes() const
{
    return QByteArray(ContentTypesXml);
}

QByteArray XlsxWriter::buildRels() const
{
    return QByteArray(RelsXml);
}

QByteArray XlsxWriter::buildWorkbookRels() const
{
    return QByteArray(WorkbookRelsXml);
}

QByteArray XlsxWriter::buildWorkbook() const
{
    QString xml = QStringLiteral(R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <sheets>
    <sheet name="%1" sheetId="1" r:id="rId1"/>
  </sheets>
</workbook>
)").arg(escapeXml(m_sheetName));
    return xml.toUtf8();
}

QByteArray XlsxWriter::buildStyles() const
{
    return QByteArray(StylesXml);
}

QByteArray XlsxWriter::buildWorksheet() const
{
    QString xml = QStringLiteral(R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
  <sheetData>
)");

    auto writeRow = [&](int rowIndex, const QVariantList& values, bool header) {
        xml += QStringLiteral("    <row r=\"%1\">\n").arg(rowIndex + 1);
        for (int col = 0; col < values.size(); ++col) {
            const QVariant v = recordFieldToVariant(values.at(col));
            const QString ref = cellReference(rowIndex, col);
            const QString text = v.isNull() ? QString() : escapeXml(v.toString());

            if (v.userType() == QMetaType::Int || v.userType() == QMetaType::LongLong
                || v.userType() == QMetaType::Double || v.userType() == QMetaType::UInt
                || v.userType() == QMetaType::ULongLong) {
                xml += QStringLiteral("      <c r=\"%1\" s=\"%2\"><v>%3</v></c>\n")
                           .arg(ref)
                           .arg(header ? 1 : 0)
                           .arg(text);
            } else {
                xml += QStringLiteral("      <c r=\"%1\" t=\"inlineStr\" s=\"%2\"><is><t>%3</t></is></c>\n")
                           .arg(ref)
                           .arg(header ? 1 : 0)
                           .arg(text);
            }
        }
        xml += QStringLiteral("    </row>\n");
    };

    if (!m_headers.isEmpty()) {
        QVariantList headerValues;
        for (const QString& h : m_headers) {
            headerValues.append(h);
        }
        writeRow(0, headerValues, true);
    }

    for (size_t i = 0; i < m_rows.size(); ++i) {
        writeRow(static_cast<int>(i) + (m_headers.isEmpty() ? 0 : 1), m_rows[i], false);
    }

    xml += QStringLiteral(R"(  </sheetData>
</worksheet>
)");
    return xml.toUtf8();
}

QString exportXlsx(const RecordList& records, const QString& filePath)
{
    XlsxWriter writer(QStringLiteral("历史记录"));

    const QStringList headers = {
        QStringLiteral("类型"),
        QStringLiteral("分类"),
        QStringLiteral("标题"),
        QStringLiteral("UP 主"),
        QStringLiteral("UP 主 ID"),
        QStringLiteral("观看时间"),
        QStringLiteral("进度"),
        QStringLiteral("完成度 %"),
        QStringLiteral("BV 号"),
        QStringLiteral("封面链接"),
        QStringLiteral("视频时长(秒)"),
        QStringLiteral("原始 JSON"),
    };
    writer.setHeaders(headers);

    for (const auto& record : records) {
        if (!record) continue;

        QVariantList row;
        row.append(recordTypeToString(record->type));
        row.append(record->category);
        row.append(record->title);
        row.append(record->authorName);
        row.append(static_cast<qint64>(record->authorId));
        row.append(record->viewAt);
        row.append(record->progress);
        row.append(record->progressPercent);
        row.append(record->bvid);
        row.append(record->coverUrl);
        row.append(record->watchedSeconds());
        row.append(record->rawJson);
        writer.addRow(row);
    }

    if (!writer.save(filePath)) {
        throw ExportException(QStringLiteral("无法写入 Excel 文件: %1").arg(filePath));
    }
    return filePath;
}

} // namespace bili::business
