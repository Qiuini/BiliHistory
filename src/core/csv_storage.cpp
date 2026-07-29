#include "csv_storage.h"
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

} // namespace

QStringList CsvStorage::headerColumns() {
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
        // 派生类字段
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

CsvLoadResult CsvStorage::load(const QString& filePath) {
    CsvLoadResult result;

    QFile file(filePath);
    if (!file.exists()) {
        return result;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw StorageException(QStringLiteral("Cannot open CSV: %1").arg(filePath));
    }

    QTextStream stream(&file);

    bool headerRead = false;
    const QStringList expectedHeader = headerColumns();

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty()) continue;

        ++result.rowCount;
        const QStringList fields = splitCsvLine(line);

        if (!headerRead) {
            headerRead = true;
            if (fields.size() >= 1 && fields.first() == expectedHeader.first()) {
                continue; // 跳过表头
            }
            // 无表头，尝试解析这一行
        }

        if (fields.size() != expectedHeader.size()) {
            ++result.errorCount;
            Logger::warning(QStringLiteral("CSV malformed row %1, skipped").arg(result.rowCount));
            continue;
        }

        try {
            RecordPtr record = rowToRecord(fields);
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

    stream << headerColumns().join(',') << "\n";
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

    // 派生类字段
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

RecordPtr CsvStorage::rowToRecord(const QStringList& row) {
    const RecordType type = recordTypeFromString(row[2]);

    RecordPtr record;
    switch (type) {
        case RecordType::Video: {
            auto video = std::make_shared<VideoRecord>();
            video->bvId = row[12];
            video->cid = row[13].toLongLong();
            video->duration = row[14].toLongLong();
            record = video;
            break;
        }
        case RecordType::Live: {
            auto live = std::make_shared<LiveRecord>();
            live->roomId = row[15];
            live->liveId = row[16].toLongLong();
            live->liveStatus = row[17];
            record = live;
            break;
        }
        case RecordType::Article: {
            auto article = std::make_shared<ArticleRecord>();
            article->cvId = row[18];
            article->categoryId = row[19].toLongLong();
            record = article;
            break;
        }
        default:
            return nullptr;
    }

    record->id = row[1].toLongLong();
    record->type = type;
    record->category = row[3];
    record->title = row[4];
    record->authorName = row[5];
    record->authorId = row[6].toLongLong();
    record->viewAt = QDateTime::fromString(row[7], Qt::ISODate);
    record->progress = row[8];
    record->progressPercent = row[9].toInt();
    record->bvid = row[10];
    record->coverUrl = row[11];
    record->rawJson = row[20];

    return record;
}

QString CsvStorage::escapeCsvField(const QString& field) {
    if (field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        QString escaped = field;
        escaped.replace('"', QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return field;
}

QStringList CsvStorage::splitCsvLine(const QString& line) {
    QStringList result;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.append('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            result.append(current);
            current.clear();
        } else {
            current.append(c);
        }
    }
    result.append(current);
    return result;
}

} // namespace bili
