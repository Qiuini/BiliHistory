#include "exporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

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

QString escapeCsvField(const QString& field)
{
    if (field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        QString escaped = field;
        escaped.replace('"', QStringLiteral(""""));
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return field;
}

QString recordToCsvRow(const RecordPtr& record)
{
    if (!record) return QString();

    QStringList fields;
    fields << QString::number(CurrentSchemaVersion);
    fields << QString::number(record->id);
    fields << escapeCsvField(recordTypeToString(record->type));
    fields << escapeCsvField(record->category);
    fields << escapeCsvField(record->title);
    fields << escapeCsvField(record->authorName);
    fields << QString::number(record->authorId);
    fields << escapeCsvField(record->viewAt.toString(Qt::ISODate));
    fields << escapeCsvField(record->progress);
    fields << QString::number(record->progressPercent);
    fields << escapeCsvField(record->bvid);
    fields << escapeCsvField(record->coverUrl);

    QString bvId, roomId, cvId;
    qint64 cid = 0, duration = 0, liveId = 0, categoryId = 0;
    QString liveStatus;

    if (auto video = std::dynamic_pointer_cast<VideoRecord>(record)) {
        bvId = video->bvId;
        cid = video->cid;
        duration = video->duration;
    } else if (auto live = std::dynamic_pointer_cast<LiveRecord>(record)) {
        roomId = live->roomId;
        liveId = live->liveId;
        liveStatus = live->liveStatus;
    } else if (auto article = std::dynamic_pointer_cast<ArticleRecord>(record)) {
        cvId = article->cvId;
        categoryId = article->categoryId;
    }

    fields << escapeCsvField(bvId);
    fields << QString::number(cid);
    fields << QString::number(duration);
    fields << escapeCsvField(roomId);
    fields << QString::number(liveId);
    fields << escapeCsvField(liveStatus);
    fields << escapeCsvField(cvId);
    fields << QString::number(categoryId);
    fields << escapeCsvField(record->rawJson);

    return fields.join(',');
}

QVariantMap recordToExportMap(const RecordPtr& record)
{
    if (!record) return {};

    QVariantMap map = record->toVariantMap();

    if (auto video = std::dynamic_pointer_cast<VideoRecord>(record)) {
        map[QStringLiteral("bv_id")] = video->bvId;
        map[QStringLiteral("cid")] = video->cid;
        map[QStringLiteral("duration")] = static_cast<qlonglong>(video->duration);
    } else if (auto live = std::dynamic_pointer_cast<LiveRecord>(record)) {
        map[QStringLiteral("room_id")] = live->roomId;
        map[QStringLiteral("live_id")] = live->liveId;
        map[QStringLiteral("live_status")] = live->liveStatus;
    } else if (auto article = std::dynamic_pointer_cast<ArticleRecord>(record)) {
        map[QStringLiteral("cv_id")] = article->cvId;
        map[QStringLiteral("category_id")] = static_cast<qlonglong>(article->categoryId);
    }

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

} // namespace

QString exportCsv(const RecordList& records, const QString& filePath)
{
    ensureParentDir(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw ExportException(QStringLiteral("无法写入 CSV 文件: %1").arg(filePath));
    }

    // UTF-8 BOM，提升 Excel 兼容性
    file.write("\xEF\xBB\xBF");

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << csvHeaderColumns().join(',') << "\n";

    for (const auto& record : records) {
        stream << recordToCsvRow(record) << "\n";
    }

    stream.flush();
    return filePath;
}

QString exportJson(const RecordList& records, const QString& filePath)
{
    ensureParentDir(filePath);

    QJsonArray array;
    for (const auto& record : records) {
        array.append(QJsonObject::fromVariantMap(recordToExportMap(record)));
    }

    QJsonObject root;
    root[QStringLiteral("exported_at")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QStringLiteral("count")] = static_cast<int>(records.size());
    root[QStringLiteral("records")] = array;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw ExportException(QStringLiteral("无法写入 JSON 文件: %1").arg(filePath));
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return filePath;
}

QString exportHtml(const RecordList& records, const QString& filePath)
{
    ensureParentDir(filePath);

    QStringList rows;
    rows.reserve(static_cast<int>(records.size()));
    for (const auto& record : records) {
        if (!record) continue;
        rows.append(QStringLiteral(
            "<tr>"
            "<td>%1</td>"
            "<td>%2</td>"
            "<td>%3</td>"
            "<td>%4</td>"
            "<td>%5</td>"
            "<td>%6</td>"
            "</tr>"
        )
            .arg(escapeHtml(recordTypeToString(record->type)))
            .arg(escapeHtml(record->title))
            .arg(escapeHtml(record->authorName))
            .arg(escapeHtml(record->category))
            .arg(escapeHtml(record->viewAt.toString(QStringLiteral("yyyy-MM-dd hh:mm"))))
            .arg(record->progressPercent));
    }

    const QString html = QStringLiteral(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<title>BiliHistory 导出</title>"
        "<style>"
        "body { font-family: system-ui, -apple-system, sans-serif; margin: 24px; background: #F7F6F9; }"
        "table { width: 100%%; border-collapse: collapse; background: #FFFFFF; }"
        "th, td { padding: 10px 12px; text-align: left; border-bottom: 1px solid #EBE8EF; }"
        "th { background: #FAFAFD; font-weight: 600; }"
        "</style>"
        "</head>"
        "<body>"
        "<h2>BiliHistory 历史记录导出</h2>"
        "<p>共 %1 条记录</p>"
        "<table>"
        "<tr><th>类型</th><th>标题</th><th>UP 主</th><th>分类</th><th>观看时间</th><th>进度</th></tr>"
        "%2"
        "</table>"
        "</body>"
        "</html>"
    )
        .arg(records.size())
        .arg(rows.join(QString()));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw ExportException(QStringLiteral("无法写入 HTML 文件: %1").arg(filePath));
    }

    file.write(html.toUtf8());
    return filePath;
}

QString exportMarkdown(const RecordList& records, const QString& filePath)
{
    ensureParentDir(filePath);

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

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw ExportException(QStringLiteral("无法写入 Markdown 文件: %1").arg(filePath));
    }

    file.write(content.toUtf8());
    return filePath;
}

QString exportRecords(const RecordList& records, const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("csv")) {
        return exportCsv(records, filePath);
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
    throw ExportException(QStringLiteral("不支持的导出格式: %1").arg(suffix));
}

QString supportedFilters()
{
    return QStringLiteral(
        "CSV 表格 (*.csv);;"
        "JSON 数据 (*.json);;"
        "HTML 网页 (*.html);;"
        "Markdown 文档 (*.md);;"
        "所有文件 (*)"
    );
}

} // namespace bili::business
