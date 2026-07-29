#pragma once

#include <functional>
#include <QString>
#include <QJsonObject>
#include <QJsonValue>

namespace bili {

class SecretsStore {
public:
    explicit SecretsStore(const QString& path,
                          std::function<QString()> machineIdProvider = nullptr);

    QJsonObject load() const;
    void save(const QJsonObject& data) const;

    QJsonValue get(const QString& key, const QJsonValue& defaultValue = QJsonValue()) const;
    void set(const QString& key, const QJsonValue& value) const;

    QString path() const { return m_path; }

private:
    static constexpr char FormatTag[] = "aes-256-gcm-v1";

    QString machineId() const;

    QString m_path;
    std::function<QString()> m_machineIdProvider;
};

} // namespace bili
