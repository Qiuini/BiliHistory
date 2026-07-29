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

QString LiveRecord::uniqueKey() const {
    return QStringLiteral("live:%1").arg(roomId.isEmpty() ? bvid : roomId);
}

QString ArticleRecord::uniqueKey() const {
    return QStringLiteral("article:%1").arg(cvId.isEmpty() ? bvid : cvId);
}

} // namespace bili
