#include "config.h"
#include "exceptions.h"
#include "logger.h"
#include "paths.h"
#include "licensing/secrets_store.h"
#include "version.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>

namespace bili {

bool Config::loadDefaults() {
    m_root = QJsonObject{
        {QStringLiteral("base_url"), QStringLiteral("https://api.bilibili.com")},
        {QStringLiteral("request_timeout_ms"), 30000},
        {QStringLiteral("request_retry_count"), 3},
        {QStringLiteral("request_slow_warning_ms"), 3000},
        {QStringLiteral("retry_wait_ms"), 2000},
        {QStringLiteral("page_size"), 50},
        {QStringLiteral("fetch_all"), true},
        {QStringLiteral("fetch_history_delay_base_ms"), 1500},
        {QStringLiteral("fetch_history_delay_jitter_ms"), 1500},
        {QStringLiteral("fetch_list_delay_base_ms"), 500},
        {QStringLiteral("fetch_list_delay_jitter_ms"), 500},
        {QStringLiteral("following_page_size"), 50},
        {QStringLiteral("favorites_page_size"), 20},
        {QStringLiteral("http_total_retries"), 3},
        {QStringLiteral("http_backoff_factor"), 1.0},
        {QStringLiteral("user_agent"), QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36")},
        {QStringLiteral("user_agents"), QJsonArray{
            QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"),
            QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"),
            QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36")
        }}
    };
    loadDefaultEndpoints();
    return true;
}

bool Config::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray data = file.readAll();
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        throw ConfigException(QStringLiteral("config.json parse error: %1").arg(error.errorString()));
    }
    m_root = doc.object();
    loadDefaultEndpoints();
    return true;
}

bool Config::saveToFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    return true;
}

QJsonValue Config::value(const QString& key, const QJsonValue& defaultValue) const {
    return m_root.value(key).isUndefined() ? defaultValue : m_root.value(key);
}

void Config::setValue(const QString& key, const QJsonValue& value) {
    m_root[key] = value;
}

QUrl Config::apiUrl(const QString& name) const {
    const QString endpoint = apiEndpoint(name);
    if (endpoint.isEmpty()) {
        return {};
    }
    const QString base = m_root.value(QStringLiteral("base_url")).toString();
    return QUrl(base + endpoint);
}

QString Config::apiEndpoint(const QString& name) const {
    return m_endpoints.value(name).toString();
}

int Config::requestTimeoutMs() const {
    return value(QStringLiteral("request_timeout_ms"), 30000).toInt();
}

int Config::requestRetryCount() const {
    return value(QStringLiteral("request_retry_count"), 3).toInt();
}

int Config::requestSlowWarningMs() const {
    return value(QStringLiteral("request_slow_warning_ms"), 3000).toInt();
}

QString Config::userAgent() const {
    return value(QStringLiteral("user_agent")).toString();
}

QStringList Config::userAgents() const {
    const QJsonArray arr = value(QStringLiteral("user_agents")).toArray();
    QStringList list;
    for (const QJsonValue& v : arr) {
        list.append(v.toString());
    }
    if (list.isEmpty()) {
        list.append(userAgent());
    }
    return list;
}

int Config::retryWaitMs() const {
    return value(QStringLiteral("retry_wait_ms"), 2000).toInt();
}

int Config::pageSize() const {
    return value(QStringLiteral("page_size"), 50).toInt();
}

int Config::fetchHistoryDelayBaseMs() const {
    return value(QStringLiteral("fetch_history_delay_base_ms"), 1500).toInt();
}

int Config::fetchHistoryDelayJitterMs() const {
    return value(QStringLiteral("fetch_history_delay_jitter_ms"), 1500).toInt();
}

int Config::fetchListDelayBaseMs() const {
    return value(QStringLiteral("fetch_list_delay_base_ms"), 500).toInt();
}

int Config::fetchListDelayJitterMs() const {
    return value(QStringLiteral("fetch_list_delay_jitter_ms"), 500).toInt();
}

int Config::followingPageSize() const {
    return value(QStringLiteral("following_page_size"), 50).toInt();
}

int Config::favoritesPageSize() const {
    return value(QStringLiteral("favorites_page_size"), 20).toInt();
}

bool Config::fetchAll() const {
    return value(QStringLiteral("fetch_all"), true).toBool();
}

void Config::setFetchAll(bool fetchAll) {
    setValue(QStringLiteral("fetch_all"), fetchAll);
}

int Config::httpTotalRetries() const {
    return value(QStringLiteral("http_total_retries"), 3).toInt();
}

double Config::httpBackoffFactor() const {
    return value(QStringLiteral("http_backoff_factor"), 1.0).toDouble();
}

QString Config::secretsPath() const
{
    return m_secretsPath.isEmpty() ? Paths::secretsPath() : m_secretsPath;
}

void Config::setSecretsPath(const QString& path)
{
    m_secretsPath = path;
}

QString Config::cookie() const
{
    const QString envCookie = QProcessEnvironment::systemEnvironment().value(QStringLiteral("BILI_COOKIE"));
    if (!envCookie.isEmpty()) {
        return envCookie;
    }

    const SecretsStore store(secretsPath());
    return store.get(QStringLiteral("cookie")).toString();
}

void Config::saveCookie(const QString& cookie) const
{
    SecretsStore store(secretsPath());
    QJsonObject data = store.load();
    data[QStringLiteral("cookie")] = cookie.trimmed();
    store.save(data);
    Logger::info(QStringLiteral("Cookie 已加密保存到 .secrets.json"));
}

void Config::loadDefaultEndpoints() {
    m_endpoints = QJsonObject{
        {QStringLiteral("history"), QStringLiteral("/x/web-interface/history/cursor")},
        {QStringLiteral("following"), QStringLiteral("/x/relation/followings")},
        {QStringLiteral("favorites_folders"), QStringLiteral("/x/v3/fav/folder/list4main")},
        {QStringLiteral("favorites_resource"), QStringLiteral("/x/v3/fav/resource/list")},
        {QStringLiteral("user_card"), QStringLiteral("/x/web-interface/card")},
        {QStringLiteral("nav"), QStringLiteral("/x/web-interface/nav")}
    };
    if (m_root.contains(QStringLiteral("endpoints"))) {
        const QJsonObject overrides = m_root.value(QStringLiteral("endpoints")).toObject();
        for (const QString& key : overrides.keys()) {
            m_endpoints[key] = overrides.value(key);
        }
    }
}

} // namespace bili
