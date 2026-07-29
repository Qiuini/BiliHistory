#pragma once

#include "models.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

namespace bili {

struct HistoryParseResult {
    RecordList records;
    PaginationInfo pagination;
};

class Parser {
public:
    // 历史记录
    static HistoryParseResult parseHistory(const QJsonObject& data);

    // 关注列表
    static FollowingList parseFollowing(const QJsonObject& data);

    // 收藏夹文件夹
    static FavoriteFolderList parseFavoriteFolders(const QJsonObject& data);

    // 收藏夹内容
    static std::vector<FavoriteItem> parseFavoriteResources(const QJsonObject& data);

    // 用户卡片
    static UserInfo parseUserCard(const QJsonObject& data);

    // 注册时间
    static QDateTime parseRegistrationTime(const QJsonObject& data);

private:
    static RecordPtr parseHistoryItem(const QJsonObject& item);
    static RecordPtr parseVideo(const QJsonObject& item, const QJsonObject& history);
    static RecordPtr parseLive(const QJsonObject& item, const QJsonObject& history);
    static RecordPtr parseArticle(const QJsonObject& item, const QJsonObject& history);

    static QString extractVideoCategory(const QJsonObject& item);
    static QString extractAuthorName(const QJsonObject& item);
    static QString articleCategoryFromTemplate(int templateId);
    static QString favoriteTypeName(int typeId);
    static QDateTime parseTimestamp(qint64 ts);
};

} // namespace bili
