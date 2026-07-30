#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace bili {

// 配置接口，用于解耦 HttpClient / ApiClient 与 Config 单例，便于测试注入。
class IConfig {
public:
    virtual ~IConfig() = default;

    virtual int requestTimeoutMs() const = 0;
    virtual int requestSlowWarningMs() const = 0;
    virtual int httpTotalRetries() const = 0;
    virtual int retryWaitMs() const = 0;
    virtual double httpBackoffFactor() const = 0;
    virtual QStringList userAgents() const = 0;

    virtual QUrl apiUrl(const QString& name) const = 0;
    virtual int pageSize() const = 0;
    virtual bool fetchAll() const = 0;
    virtual void setFetchAll(bool fetchAll) = 0;

    // 抓取延迟（基数 + 抖动），用于 fetchers 在分页请求间节流
    virtual int fetchHistoryDelayBaseMs() const = 0;
    virtual int fetchHistoryDelayJitterMs() const = 0;
    virtual int fetchListDelayBaseMs() const = 0;
    virtual int fetchListDelayJitterMs() const = 0;

    // 专用页大小：关注列表 / 收藏夹资源（pageSize() 留作历史等通用场景）
    virtual int followingPageSize() const = 0;
    virtual int favoritesPageSize() const = 0;

    // 敏感信息（Cookie）读写，用于解耦 GUI 与 Config 单例
    virtual QString cookie() const = 0;
    virtual void saveCookie(const QString& cookie) const = 0;
};

} // namespace bili
