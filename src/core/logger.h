#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(biliLog)

namespace bili {

class Logger {
public:
    static void init();
    static void shutdown();

    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);

    static QString currentLogPath();

private:
    static void ensureLogDir();
    static void rotateLogs();
};

} // namespace bili
