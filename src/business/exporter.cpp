#include "exporter.h"

#include "core/csv_parser.h"
#include "xlsx_writer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>
#include <QTextStream>

#include <memory>

namespace bili::business {

ExportException::ExportException(const QString& message)
    : std::runtime_error(message.toStdString())
    , m_message(message)
{
}

ExportException::ExportException(const std::string& message)
    : std::runtime_error(message)
    , m_message(QString::fromStdString(message))
{
}

QString ExportException::message() const
{
    return m_message;
}

namespace {

constexpr int CurrentSchemaVersion = 1;

QStringList csvHeaderColumns()
{
    return {
        QStringLiteral("schema_version"),
        QStringLiteral("id"),
        QStringLiteral("type"),
        QStringLiteral("category"),
        QStringLiteral("title"),
        QStringLiteral("author_name"),
        QStringLiteral("author_id"),
        QStringLiteral("view_at"),
        QStringLiteral("progress"),
        QStringLiteral("progress_percent"),
        QStringLiteral("bvid"),
        QStringLiteral("cover_url"),
        QStringLiteral("bv_id"),
        QStringLiteral("cid"),
        QStringLiteral("duration"),
        QStringLiteral("room_id"),
        QStringLiteral("live_id"),
        QStringLiteral("live_status"),
        QStringLiteral("cv_id"),
        QStringLiteral("category_id"),
        QStringLiteral("raw_json")
    };
}

QString recordToCsvRow(const RecordPtr& record)
{
    if (!record) return QString();

    QStringList fields;
    fields << QString::number(CurrentSchemaVersion);
    fields << QString::number(record->id);
    fields << CsvParser::escapeField(recordTypeToString(record->type));
    fields << CsvParser::escapeField(record->category);
    fields << CsvParser::escapeField(record->title);
    fields << CsvParser::escapeField(record->authorName);
    fields << QString::number(record->authorId);
    fields << CsvParser::escapeField(record->viewAt.toString(Qt::ISODate));
    fields << CsvParser::escapeField(record->progress);
    fields << QString::number(record->progressPercent);
    fields << CsvParser::escapeField(record->bvid);
    fields << CsvParser::escapeField(record->coverUrl);

    const QStringList derived = record->derivedCsvFields();
    for (const QString& field : derived) {
        fields << CsvParser::escapeField(field);
    }
    fields << CsvParser::escapeField(record->rawJson);

    return CsvParser::joinFields(fields);
}

QVariantMap recordToExportMap(const RecordPtr& record)
{
    if (!record) return {};

    QVariantMap map = record->toVariantMap();
    map.insert(record->derivedToVariantMap());
    map[QStringLiteral("raw_json")] = record->rawJson;
    return map;
}

QString escapeHtml(const QString& text)
{
    QString escaped = text;
    escaped.replace('&', QStringLiteral("&amp;"));
    escaped.replace('<', QStringLiteral("&lt;"));
    escaped.replace('>', QStringLiteral("&gt;"));
    escaped.replace('"', QStringLiteral("&quot;"));
    return escaped;
}

void ensureParentDir(const QString& filePath)
{
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

QString recordHtmlCell(const RecordPtr& record, const QString& header)
{
    if (header == QStringLiteral("类型")) return escapeHtml(recordTypeToString(record->type));
    if (header == QStringLiteral("标题")) return escapeHtml(record->title);
    if (header == QStringLiteral("UP 主")) return escapeHtml(record->authorName);
    if (header == QStringLiteral("分类")) return escapeHtml(record->category);
    if (header == QStringLiteral("观看时间")) return escapeHtml(record->viewAt.toString(QStringLiteral("yyyy-MM-dd hh:mm")));
    if (header == QStringLiteral("进度")) return QString::number(record->progressPercent);
    if (header == QStringLiteral("BV 号")) return escapeHtml(record->bvid);
    return QString();
}

QString buildHtmlTable(const RecordList& records, const QStringList& columns, const QString& style)
{
    QStringList rows;
    rows.reserve(static_cast<int>(records.size()));
    for (const auto& record : records) {
        if (!record) continue;
        QStringList cells;
        for (const auto& header : columns) {
            cells << QStringLiteral("<td>%1</td>").arg(recordHtmlCell(record, header));
        }
        rows << QStringLiteral("<tr>%1</tr>").arg(cells.join(QString()));
    }

    QStringList headerCells;
    for (const auto& header : columns) {
        headerCells << QStringLiteral("<th>%1</th>").arg(header);
    }

    return QStringLiteral(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<title>BiliHistory 导出</title>"
        "<style>"
        "%2"
        "</style>"
        "</head>"
        "<body>"
        "<h2>BiliHistory 历史记录导出</h2>"
        "<p>共 %1 条记录</p>"
        "<table>"
        "<tr>%3</tr>"
        "%4"
        "</table>"
        "</body>"
        "</html>"
    )
        .arg(records.size())
        .arg(style)
        .arg(headerCells.join(QString()))
        .arg(rows.join(QString()));
}

std::unique_ptr<QFile> openOutput(const QString& filePath, QIODevice::OpenMode mode, const QString& formatLabel)
{
    ensureParentDir(filePath);
    auto file = std::make_unique<QFile>(filePath);
    if (!file->open(mode)) {
        throw ExportException(QStringLiteral("无法写入 %1 文件: %2").arg(formatLabel, filePath));
    }
    return file;
}

} // namespace

QString exportCsv(const RecordList& records, const QString& filePath)
{
    auto file = openOutput(filePath, QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate, QStringLiteral("CSV"));

    // UTF-8 BOM，提升 Excel 兼容性
    file->write("\xEF\xBB\xBF");

    QTextStream stream(file.get());
    stream.setEncoding(QStringConverter::Utf8);
    stream << CsvParser::joinFields(csvHeaderColumns()) << "\n";

    for (const auto& record : records) {
        stream << recordToCsvRow(record) << "\n";
    }

    stream.flush();
    return filePath;
}

QString exportJson(const RecordList& records, const QString& filePath)
{
    QJsonArray array;
    for (const auto& record : records) {
        array.append(QJsonObject::fromVariantMap(recordToExportMap(record)));
    }

    QJsonObject root;
    root[QStringLiteral("exported_at")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QStringLiteral("count")] = static_cast<int>(records.size());
    root[QStringLiteral("records")] = array;

    auto file = openOutput(filePath, QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate, QStringLiteral("JSON"));
    file->write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return filePath;
}

QString exportHtml(const RecordList& records, const QString& filePath)
{
    const QString style = QStringLiteral(
        "body { font-family: system-ui, -apple-system, sans-serif; margin: 24px; background: #F7F6F9; }"
        "table { width: 100%; border-collapse: collapse; background: #FFFFFF; }"
        "th, td { padding: 10px 12px; text-align: left; border-bottom: 1px solid #EBE8EF; }"
        "th { background: #FAFAFD; font-weight: 600; }"
    );
    const QStringList columns = {
        QStringLiteral("类型"),
        QStringLiteral("标题"),
        QStringLiteral("UP 主"),
        QStringLiteral("分类"),
        QStringLiteral("观看时间"),
        QStringLiteral("进度"),
    };
    const QString html = buildHtmlTable(records, columns, style);

    auto file = openOutput(filePath, QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate, QStringLiteral("HTML"));
    file->write(html.toUtf8());
    return filePath;
}

QString exportMarkdown(const RecordList& records, const QString& filePath)
{
    QString content;
    content.append(QStringLiteral("# BiliHistory 历史记录导出\n\n"));
    content.append(QStringLiteral("共 %1 条记录\n\n").arg(records.size()));

    int index = 1;
    for (const auto& record : records) {
        if (!record) continue;
        content.append(QStringLiteral("%1. [%2] **%3** — %4 (%5，进度 %6%%)\n")
                           .arg(index++)
                           .arg(recordTypeToString(record->type))
                           .arg(record->title)
                           .arg(record->authorName)
                           .arg(record->category)
                           .arg(record->progressPercent));
    }

    auto file = openOutput(filePath, QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate, QStringLiteral("Markdown"));
    file->write(content.toUtf8());
    return filePath;
}

QString exportPdf(const RecordList& records, const QString& filePath)
{
    ensureParentDir(filePath);

    const QString style = QStringLiteral(
        "body { font-family: system-ui, -apple-system, sans-serif; margin: 24px; background: #FFFFFF; }"
        "table { width: 100%; border-collapse: collapse; }"
        "th, td { padding: 8px 10px; text-align: left; border-bottom: 1px solid #EBE8EF; font-size: 10px; }"
        "th { background: #FAFAFD; font-weight: 600; }"
    );
    const QStringList columns = {
        QStringLiteral("类型"),
        QStringLiteral("分类"),
        QStringLiteral("标题"),
        QStringLiteral("UP 主"),
        QStringLiteral("观看时间"),
        QStringLiteral("进度"),
        QStringLiteral("BV 号"),
    };
    const QString html = buildHtmlTable(records, columns, style);

    QPdfWriter writer(filePath);
    writer.setPageSize(QPageSize::A4);
    writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);
    return filePath;
}

QString exportRecords(const RecordList& records, const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("csv")) {
        return exportCsv(records, filePath);
    }
    if (suffix == QStringLiteral("xlsx")) {
        return exportXlsx(records, filePath);
    }
    if (suffix == QStringLiteral("json")) {
        return exportJson(records, filePath);
    }
    if (suffix == QStringLiteral("html")) {
        return exportHtml(records, filePath);
    }
    if (suffix == QStringLiteral("md")) {
        return exportMarkdown(records, filePath);
    }
    if (suffix == QStringLiteral("pdf")) {
        return exportPdf(records, filePath);
    }
    throw ExportException(QStringLiteral("不支持的导出格式: %1").arg(suffix));
}

QString supportedFilters()
{
    return QStringLiteral(
        "Excel 工作簿 (*.xlsx);;"
        "CSV 表格 (*.csv);;"
        "PDF 文档 (*.pdf);;"
        "JSON 数据 (*.json);;"
        "HTML 网页 (*.html);;"
        "Markdown 文档 (*.md);;"
        "所有文件 (*)"
    );
}

} // namespace bili::business
