#pragma once

#include "i_config.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUrl>

namespace bili {

// 配置类：可独立构造，由调用方（main / 测试）持有所有权并通过 IConfig* 注入。
// 不提供全局单例，避免可变全局状态污染测试与依赖图。
class Config : public IConfig {
public:
    Config() = default;

    bool loadDefaults();
    bool loadFromFile(const QString& path);
    bool saveToFile(const QString& path) const;

    QJsonValue value(const QString& key, const QJsonValue& defaultValue = QJsonValue()) const;
    void setValue(const QString& key, const QJsonValue& value);

    QUrl apiUrl(const QString& name) const;
    QString apiEndpoint(const QString& name) const;

    int requestTimeoutMs() const;
    int requestRetryCount() const;
    int requestSlowWarningMs() const;
    int retryWaitMs() const;

    int pageSize() const;
    bool fetchAll() const;
    void setFetchAll(bool fetchAll);

    int fetchHistoryDelayBaseMs() const;
    int fetchHistoryDelayJitterMs() const;
    int fetchListDelayBaseMs() const;
    int fetchListDelayJitterMs() const;
    int followingPageSize() const;
    int favoritesPageSize() const;

    int httpTotalRetries() const;
    double httpBackoffFactor() const;

    QString userAgent() const;
    QStringList userAgents() const;

    // 敏感信息（Cookie）读写，优先级：环境变量 BILI_COOKIE > .secrets.json
    QString cookie() const;
    void saveCookie(const QString& cookie) const;

    QString secretsPath() const;
    void setSecretsPath(const QString& path);

private:
    QString m_secretsPath;

    QJsonObject m_root;
    QJsonObject m_endpoints;

    void loadDefaultEndpoints();
};

} // namespace bili