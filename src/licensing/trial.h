#pragma once

#include <QString>
#include <QDateTime>

namespace bili {

class Trial {
public:
    static constexpr int TrialDays = 30;

    struct Status {
        QDateTime start;
        bool tampered = false;
    };

    static Status read(const QString& filePath);
    static int remainingDays(const QString& filePath);
    static bool isActive(const QString& filePath);
    static int consume(const QString& filePath);

private:
    Trial() = delete;

    static QString formatIso(const QDateTime& dt);
    static QDateTime parseIso(const QString& value);
    static QByteArray sign(const QString& start);
    static void write(const QString& filePath, const QString& start);
};

} // namespace bili
