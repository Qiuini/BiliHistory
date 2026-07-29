#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUrl>

namespace bili {

class Config {
public:
    static Config& instance();

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
    Config() = default;

    QString m_secretsPath;

    QJsonObject m_root;
    QJsonObject m_endpoints;

    void loadDefaultEndpoints();
};

} // namespace bili
