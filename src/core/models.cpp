#include "models.h"

namespace bili {

QString recordTypeToString(RecordType type) {
    switch (type) {
        case RecordType::Video: return QStringLiteral("video");
        case RecordType::Live: return QStringLiteral("live");
        case RecordType::Article: return QStringLiteral("article");
        default: return QStringLiteral("unknown");
    }
}

RecordType recordTypeFromString(const QString& str) {
    const auto lower = str.toLower();
    if (lower == QStringLiteral("video")) return RecordType::Video;
    if (lower == QStringLiteral("live")) return RecordType::Live;
    if (lower == QStringLiteral("article")) return RecordType::Article;
    return RecordType::Unknown;
}

QVariantMap BaseRecord::toVariantMap() const {
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("type")] = recordTypeToString(type);
    map[QStringLiteral("category")] = category;
    map[QStringLiteral("title")] = title;
    map[QStringLiteral("author_name")] = authorName;
    map[QStringLiteral("author_id")] = authorId;
    map[QStringLiteral("view_at")] = viewAt.toString(Qt::ISODate);
    map[QStringLiteral("progress")] = progress;
    map[QStringLiteral("progress_percent")] = progressPercent;
    map[QStringLiteral("bvid")] = bvid;
    map[QStringLiteral("cover_url")] = coverUrl;
    return map;
}

QString VideoRecord::uniqueKey() const {
    return QStringLiteral("video:%1").arg(bvId.isEmpty() ? bvid : bvId);
}

qint64 VideoRecord::watchedSeconds() const {
    if (duration <= 0) {
        return 0;
    }
    if (progressPercent < 0) {
        return duration;
    }
    return duration * progressPercent / 100;
}

QStringList VideoRecord::derivedCsvFields() const {
    return {
        bvId,
        QString::number(cid),
        QString::number(duration),
        QString(),
        QStringLiteral("0"),
        QString(),
        QString(),
        QStringLiteral("0")
    };
}

void VideoRecord::applyDerivedCsvFields(const QStringList& fields) {
    if (fields.size() >= 3) {
        bvId = fields[0];
        cid = fields[1].toLongLong();
        duration = fields[2].toLongLong();
    }
}

QVariantMap VideoRecord::derivedToVariantMap() const {
    QVariantMap map;
    map[QStringLiteral("bv_id")] = bvId;
    map[QStringLiteral("cid")] = cid;
    map[QStringLiteral("duration")] = static_cast<qlonglong>(duration);
    return map;
}

QString LiveRecord::uniqueKey() const {
    return QStringLiteral("live:%1").arg(roomId.isEmpty() ? bvid : roomId);
}

QStringList LiveRecord::derivedCsvFields() const {
    return {
        QString(),
        QStringLiteral("0"),
        QStringLiteral("0"),
        roomId,
        QString::number(liveId),
        liveStatus,
        QString(),
        QStringLiteral("0")
    };
}

void LiveRecord::applyDerivedCsvFields(const QStringList& fields) {
    if (fields.size() >= 6) {
        roomId = fields[3];
        liveId = fields[4].toLongLong();
        liveStatus = fields[5];
    }
}

QVariantMap LiveRecord::derivedToVariantMap() const {
    QVariantMap map;
    map[QStringLiteral("room_id")] = roomId;
    map[QStringLiteral("live_id")] = liveId;
    map[QStringLiteral("live_status")] = liveStatus;
    return map;
}

QString ArticleRecord::uniqueKey() const {
    return QStringLiteral("article:%1").arg(cvId.isEmpty() ? bvid : cvId);
}

QStringList ArticleRecord::derivedCsvFields() const {
    return {
        QString(),
        QStringLiteral("0"),
        QStringLiteral("0"),
        QString(),
        QStringLiteral("0"),
        QString(),
        cvId,
        QString::number(categoryId)
    };
}

void ArticleRecord::applyDerivedCsvFields(const QStringList& fields) {
    if (fields.size() >= 8) {
        cvId = fields[6];
        categoryId = fields[7].toLongLong();
    }
}

QVariantMap ArticleRecord::derivedToVariantMap() const {
    QVariantMap map;
    map[QStringLiteral("cv_id")] = cvId;
    map[QStringLiteral("category_id")] = static_cast<qlonglong>(categoryId);
    return map;
}

} // namespace bili
