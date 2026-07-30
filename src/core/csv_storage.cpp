#include "csv_storage.h"
#include "csv_parser.h"
#include "exceptions.h"
#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace bili {

namespace {

constexpr int CurrentSchemaVersion = 1;

// 列名常量，避免拼写错误扩散到多处
const QString kColSchemaVersion = QStringLiteral("schema_version");
const QString kColId            = QStringLiteral("id");
const QString kColType          = QStringLiteral("type");
const QString kColCategory      = QStringLiteral("category");
const QString kColTitle         = QStringLiteral("title");
const QString kColAuthorName    = QStringLiteral("author_name");
const QString kColAuthorId      = QStringLiteral("author_id");
const QString kColViewAt        = QStringLiteral("view_at");
const QString kColProgress      = QStringLiteral("progress");
const QString kColProgressPct   = QStringLiteral("progress_percent");
const QString kColBvid          = QStringLiteral("bvid");
const QString kColCoverUrl      = QStringLiteral("cover_url");
const QString kColRawJson       = QStringLiteral("raw_json");

} // namespace

QStringList CsvStorage::headerColumns() {
    return {
        kColSchemaVersion,
        kColId,
        kColType,
        kColCategory,
        kColTitle,
        kColAuthorName,
        kColAuthorId,
        kColViewAt,
        kColProgress,
        kColProgressPct,
        kColBvid,
        kColCoverUrl,
        // 派生类字段
        QStringLiteral("bv_id"),
        QStringLiteral("cid"),
        QStringLiteral("duration"),
        QStringLiteral("room_id"),
        QStringLiteral("live_id"),
        QStringLiteral("live_status"),
        QStringLiteral("cv_id"),
        QStringLiteral("category_id"),
        kColRawJson
    };
}

QStringList CsvStorage::derivedColumnNames() {
    return {
        QStringLiteral("bv_id"),
        QStringLiteral("cid"),
        QStringLiteral("duration"),
        QStringLiteral("room_id"),
        QStringLiteral("live_id"),
        QStringLiteral("live_status"),
        QStringLiteral("cv_id"),
        QStringLiteral("category_id")
    };
}

CsvLoadResult CsvStorage::load(const QString& filePath) {
    CsvLoadResult result;
    const CsvParser parser;
    const QStringList expectedHeader = headerColumns();

    QFile file(filePath);
    if (!file.exists()) {
        return result;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw StorageException(QStringLiteral("Cannot open CSV: %1").arg(filePath));
    }

    QTextStream stream(&file);

    // 列名 → 下标 索引：默认按 headerColumns() 顺序，遇到真实表头则用真实表头覆盖
    QHash<QString, int> index;
    const int expectedSize = expectedHeader.size();
    index.reserve(expectedSize);
    for (int i = 0; i < expectedSize; ++i) {
        index.insert(expectedHeader[i], i);
    }

    bool headerRead = false;
    bool hasHeader = false;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty()) continue;

        ++result.rowCount;
        const QStringList fields = parser.parseLine(line);

        if (!headerRead) {
            headerRead = true;
            if (parser.looksLikeHeader(fields, expectedHeader)) {
                hasHeader = true;
                // 用真实表头重建索引：列顺序变更/新增列也能正确解析
                index.clear();
                for (int i = 0; i < fields.size(); ++i) {
                    index.insert(fields[i], i);
                }
                // 表头里的 schema_version
                const auto it = index.find(kColSchemaVersion);
                if (it != index.end()) {
                    bool ok = false;
                    const int v = fields.value(it.value()).toInt(&ok);
                    if (ok && v > 0) result.schemaVersion = v;
                }
                continue;
            }
            // 无表头，使用默认索引解析这一行
        }

        // 字段数过少直接跳过；字段数多于表头也允许（多余列忽略）
        const int minRequired = hasHeader ? index.size() : expectedSize;
        if (fields.size() < minRequired) {
            ++result.errorCount;
            Logger::warning(QStringLiteral("CSV malformed row %1, skipped").arg(result.rowCount));
            continue;
        }

        try {
            RecordPtr record = rowToRecord(fields, index);
            if (record) {
                result.records.push_back(record);
            }
        } catch (...) {
            ++result.errorCount;
            Logger::warning(QStringLiteral("CSV parse row %1 failed, skipped").arg(result.rowCount));
        }
    }

    return result;
}

bool CsvStorage::save(const QString& filePath, const RecordList& records) {
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        Logger::error(QStringLiteral("Cannot write CSV: %1").arg(filePath));
        return false;
    }

    QTextStream stream(&file);

    stream << CsvParser::joinFields(headerColumns()) << "\n";
    for (const auto& record : records) {
        stream << recordToRow(record) << "\n";
    }

    stream.flush();
    return true;
}

bool CsvStorage::backup(const QString& sourcePath, const QString& backupDir) {
    QFileInfo info(sourcePath);
    if (!info.exists()) return true;

    QDir dir(backupDir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));

    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    const QString dest = dir.filePath(QStringLiteral("%1_%2.csv").arg(info.completeBaseName(), ts));
    return QFile::copy(sourcePath, dest);
}

QString CsvStorage::recordToRow(const RecordPtr& record) {
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

    // 派生类字段通过虚函数获取，避免 dynamic_pointer_cast
    const QStringList derived = record->derivedCsvFields();
    for (const QString& field : derived) {
        fields << CsvParser::escapeField(field);
    }
    fields << CsvParser::escapeField(record->rawJson);

    return CsvParser::joinFields(fields);
}

namespace {

// 从 fields 中按列名安全取值，列缺失返回空字符串
QString columnValue(const QStringList& fields, const QHash<QString, int>& index, const QString& name) {
    const auto it = index.find(name);
    if (it == index.end()) return QString();
    const int pos = it.value();
    if (pos < 0 || pos >= fields.size()) return QString();
    return fields.at(pos);
}

qint64 columnLongLong(const QStringList& fields, const QHash<QString, int>& index, const QString& name) {
    return columnValue(fields, index, name).toLongLong();
}

int columnInt(const QStringList& fields, const QHash<QString, int>& index, const QString& name, int defaultValue = 0) {
    const QString v = columnValue(fields, index, name);
    bool ok = false;
    const int n = v.toInt(&ok);
    return ok ? n : defaultValue;
}

} // namespace

RecordPtr CsvStorage::rowToRecord(const QStringList& row, const QHash<QString, int>& index) {
    const QString typeStr = columnValue(row, index, kColType);
    const RecordType type = recordTypeFromString(typeStr);

    RecordPtr record;
    switch (type) {
        case RecordType::Video:   record = std::make_shared<VideoRecord>(); break;
        case RecordType::Live:    record = std::make_shared<LiveRecord>(); break;
        case RecordType::Article: record = std::make_shared<ArticleRecord>(); break;
        default: return nullptr;
    }

    record->id            = columnLongLong(row, index, kColId);
    record->type          = type;
    record->category      = columnValue(row, index, kColCategory);
    record->title         = columnValue(row, index, kColTitle);
    record->authorName    = columnValue(row, index, kColAuthorName);
    record->authorId      = columnLongLong(row, index, kColAuthorId);
    record->viewAt        = QDateTime::fromString(columnValue(row, index, kColViewAt), Qt::ISODate);
    record->progress      = columnValue(row, index, kColProgress);
    record->progressPercent = columnInt(row, index, kColProgressPct);
    record->bvid          = columnValue(row, index, kColBvid);
    record->coverUrl      = columnValue(row, index, kColCoverUrl);
    record->rawJson       = columnValue(row, index, kColRawJson);

    // 按派生列名顺序提取子列表，传给 BaseRecord::applyDerivedCsvFields
    const QStringList derivedNames = derivedColumnNames();
    QStringList derivedFields;
    derivedFields.reserve(derivedNames.size());
    for (const QString& name : derivedNames) {
        derivedFields.append(columnValue(row, index, name));
    }
    record->applyDerivedCsvFields(derivedFields);

    return record;
}

} // namespace bili
