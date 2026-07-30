#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <memory>
#include <optional>
#include <vector>

namespace bili {

enum class RecordType {
    Video,
    Live,
    Article,
    Unknown
};

QString recordTypeToString(RecordType type);
RecordType recordTypeFromString(const QString& str);

// 通用历史记录基类
struct BaseRecord {
    virtual ~BaseRecord() = default;

    qint64 id = 0;                    // 数据库/内部唯一ID
    RecordType type = RecordType::Unknown;
    QString category;                 // 分类字段（项目强制要求）
    QString title;
    QString authorName;
    qint64 authorId = 0;
    QDateTime viewAt;                 // 观看时间
    QString progress;                 // 观看进度文本
    int progressPercent = 0;          // 观看进度 0-100（负数表示完整观看）
    QString bvid;                     // 视频 BV 号 / 直播间号 / 文章 CV 号统一业务键
    QString coverUrl;
    QString rawJson;                  // 原始 JSON 快照

    // 派生类唯一键，避免 video BV、live room、article id 冲突
    virtual QString uniqueKey() const = 0;
    virtual QVariantMap toVariantMap() const;

    // 用于统计的通用指标，消除 dynamic_pointer_cast
    virtual qint64 watchedSeconds() const { return 0; }
    virtual int effectiveCompletionPercent() const {
        return progressPercent < 0 ? 100 : progressPercent;
    }

    // 用于 CSV 序列化的派生字段；顺序：bv_id, cid, duration, room_id, live_id, live_status, cv_id, category_id
    virtual QStringList derivedCsvFields() const {
        return { QString(), QStringLiteral("0"), QStringLiteral("0"),
                 QString(), QStringLiteral("0"), QString(),
                 QString(), QStringLiteral("0") };
    }
    virtual void applyDerivedCsvFields(const QStringList& /*fields*/) {}

    // 用于 JSON 导出的派生字段
    virtual QVariantMap derivedToVariantMap() const { return {}; }
};

using RecordPtr = std::shared_ptr<BaseRecord>;
using RecordList = std::vector<RecordPtr>;

struct VideoRecord : BaseRecord {
    QString bvId;
    qint64 cid = 0;
    qint64 duration = 0;              // 秒
    bool isComplete() const { return progressPercent >= 95; }
    QString uniqueKey() const override;
    qint64 watchedSeconds() const override;
    QStringList derivedCsvFields() const override;
    void applyDerivedCsvFields(const QStringList& fields) override;
    QVariantMap derivedToVariantMap() const override;
};

struct LiveRecord : BaseRecord {
    QString roomId;
    qint64 liveId = 0;
    QString liveStatus;
    QString uniqueKey() const override;
    QStringList derivedCsvFields() const override;
    void applyDerivedCsvFields(const QStringList& fields) override;
    QVariantMap derivedToVariantMap() const override;
};

struct ArticleRecord : BaseRecord {
    QString cvId;
    qint64 categoryId = 0;
    QString uniqueKey() const override;
    QStringList derivedCsvFields() const override;
    void applyDerivedCsvFields(const QStringList& fields) override;
    QVariantMap derivedToVariantMap() const override;
};

// 用户信息
struct UserInfo {
    qint64 mid = 0;
    QString name;
    QString sign;
    QString faceUrl;
    int level = 0;
    QDateTime registrationTime;
    QString registrationTimeText;
    int officialVerify = 0;           // 0: 无, 1: 个人认证, 2: 机构认证
};

// 关注列表项
struct FollowingUser {
    qint64 mid = 0;
    QString name;
    QString sign;
    QString faceUrl;
    int officialVerify = 0;
    int level = 0;
};

using FollowingList = std::vector<FollowingUser>;

// 收藏夹
struct FavoriteItem {
    QString id;
    QString title;
    QString bvid;
    QString cvId;
    RecordType type = RecordType::Unknown;
    QString coverUrl;
    QString upperName;
    qint64 upperId = 0;
    QDateTime favTime;
};

struct FavoriteFolder {
    qint64 id = 0;
    QString name;
    qint64 mediaCount = 0;
    std::vector<FavoriteItem> items;
};

using FavoriteFolderList = std::vector<FavoriteFolder>;

// 分页元数据
struct PaginationInfo {
    int pageNumber = 1;
    int pageSize = 20;
    bool hasMore = false;
    QString nextCursor;
    int total = 0;
};

} // namespace bili
