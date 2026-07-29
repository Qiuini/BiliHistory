#include "parser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace bili {

namespace {

qint64 toLongLong(const QJsonValue& value) {
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }
    if (value.isString()) {
        return value.toString().toLongLong();
    }
    return 0;
}

QString toString(const QJsonValue& value) {
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(value.toDouble(), 'f', 0);
    return QString();
}

} // namespace

HistoryParseResult Parser::parseHistory(const QJsonObject& data) {
    HistoryParseResult result;
    const QJsonObject dataObj = data.value(QStringLiteral("data")).toObject();
    const QJsonArray list = dataObj.value(QStringLiteral("list")).toArray();

    for (const QJsonValue& value : list) {
        if (!value.isObject()) continue;
        RecordPtr record = parseHistoryItem(value.toObject());
        if (record) {
            result.records.push_back(record);
        }
    }

    const QJsonObject cursor = dataObj.value(QStringLiteral("cursor")).toObject();
    result.pagination.hasMore = cursor.contains(QStringLiteral("max")) ||
                                cursor.contains(QStringLiteral("view_at"));
    result.pagination.nextCursor = cursor.value(QStringLiteral("max")).toVariant().toString();
    result.pagination.pageSize = cursor.value(QStringLiteral("ps")).toInt(20);
    result.pagination.total = dataObj.value(QStringLiteral("page")).toObject().value(QStringLiteral("total")).toInt();

    return result;
}

RecordPtr Parser::parseHistoryItem(const QJsonObject& item) {
    const QJsonObject history = item.value(QStringLiteral("history")).toObject();
    const QString business = history.value(QStringLiteral("business")).toString()
                                 .toLower();

    if (business == QStringLiteral("archive")) {
        return parseVideo(item, history);
    }
    if (business == QStringLiteral("live")) {
        return parseLive(item, history);
    }
    if (business == QStringLiteral("article")) {
        return parseArticle(item, history);
    }

    // 兼容扁平字段
    const QString flatBusiness = item.value(QStringLiteral("business")).toString().toLower();
    if (flatBusiness == QStringLiteral("archive")) return parseVideo(item, history);
    if (flatBusiness == QStringLiteral("live")) return parseLive(item, history);
    if (flatBusiness == QStringLiteral("article")) return parseArticle(item, history);

    return nullptr;
}

RecordPtr Parser::parseVideo(const QJsonObject& item, const QJsonObject& history) {
    const QString bvid = history.value(QStringLiteral("bvid")).toString().isEmpty()
                             ? item.value(QStringLiteral("bvid")).toString()
                             : history.value(QStringLiteral("bvid")).toString();
    if (bvid.isEmpty()) {
        return nullptr;
    }

    auto record = std::make_shared<VideoRecord>();
    record->type = RecordType::Video;
    record->bvId = bvid;
    record->bvid = bvid;
    record->title = item.value(QStringLiteral("title")).toString();
    record->authorName = extractAuthorName(item);
    record->authorId = toLongLong(item.value(QStringLiteral("mid")));
    record->category = extractVideoCategory(item);
    record->duration = toLongLong(item.value(QStringLiteral("duration")));

    int progress = item.value(QStringLiteral("progress")).toInt(-2);
    if (progress == -1) {
        progress = record->duration;
    }
    record->progressPercent = record->duration > 0
                                  ? static_cast<int>((progress * 100.0) / record->duration)
                                  : 0;
    record->progress = QStringLiteral("%1%").arg(record->progressPercent);
    record->viewAt = parseTimestamp(toLongLong(item.value(QStringLiteral("view_at"))));
    record->coverUrl = item.value(QStringLiteral("cover")).toString();
    record->cid = toLongLong(item.value(QStringLiteral("cid")));

    QJsonDocument doc(item);
    record->rawJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    return record;
}

RecordPtr Parser::parseLive(const QJsonObject& item, const QJsonObject& history) {
    const QString roomId = history.value(QStringLiteral("oid")).toString().isEmpty()
                               ? item.value(QStringLiteral("room_id")).toString()
                               : history.value(QStringLiteral("oid")).toString();
    if (roomId.isEmpty()) {
        return nullptr;
    }

    auto record = std::make_shared<LiveRecord>();
    record->type = RecordType::Live;
    record->roomId = roomId;
    record->bvid = roomId;
    record->title = item.value(QStringLiteral("title")).toString();
    record->authorName = extractAuthorName(item);
    record->authorId = toLongLong(item.value(QStringLiteral("mid")));
    record->category = item.value(QStringLiteral("badge")).toString();
    record->viewAt = parseTimestamp(toLongLong(item.value(QStringLiteral("view_at"))));
    record->coverUrl = item.value(QStringLiteral("cover")).toString();
    record->liveId = toLongLong(item.value(QStringLiteral("live_id")));
    record->liveStatus = item.value(QStringLiteral("live_status")).toString();

    QJsonDocument doc(item);
    record->rawJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    return record;
}

RecordPtr Parser::parseArticle(const QJsonObject& item, const QJsonObject& history) {
    const QString articleId = history.value(QStringLiteral("oid")).toString().isEmpty()
                                  ? item.value(QStringLiteral("id")).toString()
                                  : history.value(QStringLiteral("oid")).toString();
    if (articleId.isEmpty()) {
        return nullptr;
    }

    auto record = std::make_shared<ArticleRecord>();
    record->type = RecordType::Article;
    record->cvId = articleId;
    record->bvid = articleId;
    record->title = item.value(QStringLiteral("title")).toString();
    record->authorName = extractAuthorName(item);
    record->authorId = toLongLong(item.value(QStringLiteral("mid")));
    record->category = item.value(QStringLiteral("category")).toString();
    if (record->category.isEmpty()) {
        record->category = articleCategoryFromTemplate(item.value(QStringLiteral("template_id")).toInt());
    }
    record->viewAt = parseTimestamp(toLongLong(item.value(QStringLiteral("view_at"))));
    record->coverUrl = item.value(QStringLiteral("cover")).toString();
    record->categoryId = toLongLong(item.value(QStringLiteral("category_id")));

    QJsonDocument doc(item);
    record->rawJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    return record;
}

QString Parser::extractVideoCategory(const QJsonObject& item) {
    const QStringList keys = {QStringLiteral("tname"), QStringLiteral("tag_name"), QStringLiteral("typename")};
    for (const QString& key : keys) {
        const QString value = item.value(key).toString();
        if (!value.isEmpty()) return value;
    }

    const QJsonArray tags = item.value(QStringLiteral("tag")).toArray();
    if (!tags.isEmpty()) {
        const QJsonObject first = tags.first().toObject();
        const QString tagName = first.value(QStringLiteral("tag_name")).toString();
        if (!tagName.isEmpty()) return tagName;
        const QString name = first.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) return name;
    }

    const QJsonObject stat = item.value(QStringLiteral("stat")).toObject();
    const QString tname = stat.value(QStringLiteral("tname")).toString();
    if (!tname.isEmpty()) return tname;

    return QString();
}

QString Parser::extractAuthorName(const QJsonObject& item) {
    const QString authorName = item.value(QStringLiteral("author_name")).toString();
    if (!authorName.isEmpty()) return authorName;

    const QJsonObject upper = item.value(QStringLiteral("upper")).toObject();
    if (!upper.isEmpty()) {
        const QString name = upper.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) return name;
        return upper.value(QStringLiteral("uname")).toString();
    }

    const QJsonObject owner = item.value(QStringLiteral("owner")).toObject();
    if (!owner.isEmpty()) {
        const QString name = owner.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) return name;
        return owner.value(QStringLiteral("uname")).toString();
    }

    const QString rootName = item.value(QStringLiteral("name")).toString();
    if (!rootName.isEmpty()) return rootName;

    return QStringLiteral("未知UP主");
}

QString Parser::articleCategoryFromTemplate(int templateId) {
    static const QMap<int, QString> mapping = {
        {4, QStringLiteral("动画")},
        {17, QStringLiteral("游戏")},
        {3, QStringLiteral("音乐")},
        {129, QStringLiteral("舞蹈")},
        {36, QStringLiteral("知识")},
        {188, QStringLiteral("数码")},
        {95, QStringLiteral("科技")},
        {122, QStringLiteral("生活")},
        {160, QStringLiteral("时尚")},
        {211, QStringLiteral("美食")},
        {119, QStringLiteral("鬼畜")},
        {155, QStringLiteral("娱乐")},
        {5, QStringLiteral("影视")},
        {181, QStringLiteral("影视")},
        {202, QStringLiteral("资讯")}
    };
    return mapping.value(templateId, QString());
}

FollowingList Parser::parseFollowing(const QJsonObject& data) {
    FollowingList result;
    const QJsonObject dataObj = data.value(QStringLiteral("data")).toObject();
    const QJsonArray list = dataObj.value(QStringLiteral("list")).toArray();

    for (const QJsonValue& value : list) {
        if (!value.isObject()) continue;
        const QJsonObject item = value.toObject();
        const qint64 mid = toLongLong(item.value(QStringLiteral("mid")));
        if (mid == 0) continue;

        FollowingUser user;
        user.mid = mid;
        user.name = item.value(QStringLiteral("uname")).toString();
        if (user.name.isEmpty()) {
            user.name = item.value(QStringLiteral("username")).toString();
        }
        if (user.name.isEmpty()) {
            user.name = QStringLiteral("未知UP主");
        }
        user.faceUrl = item.value(QStringLiteral("face")).toString();
        user.sign = item.value(QStringLiteral("sign")).toString();
        user.level = item.value(QStringLiteral("level")).toInt();

        const QJsonObject official = item.value(QStringLiteral("official_verify")).toObject();
        user.officialVerify = official.value(QStringLiteral("type")).toInt();

        result.push_back(user);
    }

    return result;
}

FavoriteFolderList Parser::parseFavoriteFolders(const QJsonObject& data) {
    FavoriteFolderList result;
    const QJsonObject dataObj = data.value(QStringLiteral("data")).toObject();
    const QJsonArray list = dataObj.value(QStringLiteral("list")).toArray();

    for (const QJsonValue& value : list) {
        if (!value.isObject()) continue;
        const QJsonObject item = value.toObject();
        const qint64 id = toLongLong(item.value(QStringLiteral("id")));
        if (id == 0) continue;

        FavoriteFolder folder;
        folder.id = id;
        folder.name = item.value(QStringLiteral("title")).toString();
        if (folder.name.isEmpty()) {
            folder.name = QStringLiteral("未命名收藏夹");
        }
        folder.mediaCount = toLongLong(item.value(QStringLiteral("media_count")));
        result.push_back(folder);
    }

    return result;
}

std::vector<FavoriteItem> Parser::parseFavoriteResources(const QJsonObject& data) {
    std::vector<FavoriteItem> result;
    const QJsonObject dataObj = data.value(QStringLiteral("data")).toObject();
    const QJsonArray medias = dataObj.value(QStringLiteral("medias")).toArray();

    for (const QJsonValue& value : medias) {
        if (!value.isObject()) continue;
        const QJsonObject item = value.toObject();

        FavoriteItem fav;
        fav.id = toString(item.value(QStringLiteral("id")));
        fav.title = item.value(QStringLiteral("title")).toString();
        fav.bvid = item.value(QStringLiteral("bvid")).toString();
        fav.coverUrl = item.value(QStringLiteral("cover")).toString();
        fav.type = recordTypeFromString(favoriteTypeName(item.value(QStringLiteral("type")).toInt(2)));

        const QJsonObject upper = item.value(QStringLiteral("upper")).toObject();
        fav.upperName = upper.value(QStringLiteral("name")).toString();
        if (fav.upperName.isEmpty()) {
            fav.upperName = upper.value(QStringLiteral("uname")).toString();
        }
        fav.upperId = toLongLong(upper.value(QStringLiteral("mid")));
        fav.favTime = parseTimestamp(toLongLong(item.value(QStringLiteral("fav_time"))));

        result.push_back(fav);
    }

    return result;
}

QString Parser::favoriteTypeName(int typeId) {
    static const QMap<int, QString> mapping = {
        {2, QStringLiteral("video")},
        {12, QStringLiteral("article")},
        {21, QStringLiteral("video")},
        {22, QStringLiteral("bangumi")}
    };
    return mapping.value(typeId, QStringLiteral("video"));
}

UserInfo Parser::parseUserCard(const QJsonObject& data) {
    UserInfo info;
    const QJsonObject dataObj = data.value(QStringLiteral("data")).toObject();
    const QJsonObject card = dataObj.value(QStringLiteral("card")).toObject();

    info.mid = toLongLong(card.value(QStringLiteral("mid")));
    info.name = card.value(QStringLiteral("name")).toString();
    info.sign = card.value(QStringLiteral("sign")).toString();
    info.faceUrl = card.value(QStringLiteral("face")).toString();
    info.level = card.value(QStringLiteral("level_info")).toObject().value(QStringLiteral("current_level")).toInt();
    info.registrationTime = parseRegistrationTime(data);
    info.registrationTimeText = info.registrationTime.toString(QStringLiteral("yyyy-MM-dd"));

    return info;
}

QDateTime Parser::parseRegistrationTime(const QJsonObject& data) {
    const QJsonObject dataObj = data.value(QStringLiteral("data")).toObject();
    const QJsonObject card = dataObj.value(QStringLiteral("card")).toObject();
    const qint64 regTs = toLongLong(card.value(QStringLiteral("regtime")));
    if (regTs > 0) {
        return parseTimestamp(regTs);
    }
    return QDateTime();
}

QDateTime Parser::parseTimestamp(qint64 ts) {
    if (ts <= 0) {
        return QDateTime::currentDateTime();
    }
    if (ts > 1e12) {
        // 毫秒时间戳
        return QDateTime::fromMSecsSinceEpoch(ts);
    }
    return QDateTime::fromSecsSinceEpoch(ts);
}

} // namespace bili
