#include "logger.h"
#include "paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtGlobal>

#include <mutex>

Q_LOGGING_CATEGORY(biliLog, "bili")

namespace bili {

namespace {

constexpr int MaxLogFiles = 7;
constexpr qint64 MaxLogSizeBytes = 5 * 1024 * 1024; // 5MB

class LogWriter {
public:
    static LogWriter& instance() {
        static LogWriter instance;
        return instance;
    }

    void write(const QString& level, const QString& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        rotateIfNeeded();
        if (!m_file.isOpen()) {
            openCurrent();
        }
        const QString line = QStringLiteral("[%1] [%2] %3\n")
                                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                      level,
                                      message);
        QTextStream stream(&m_file);
        stream << line;
        stream.flush();
    }

    void rotateIfNeeded() {
        if (!m_file.isOpen()) return;
        if (m_file.size() < MaxLogSizeBytes) return;
        m_file.close();
        const QString backup = QStringLiteral("%1.%2")
                                   .arg(m_currentPath,
                                        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
        QFile::rename(m_currentPath, backup);
        openCurrent();
    }

    void openCurrent() {
        m_currentPath = Paths::logsDir() + QStringLiteral("/bilihistory.log");
        m_file.setFileName(m_currentPath);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qCWarning(biliLog) << "Failed to open log file:" << m_currentPath;
        }
    }

    QString currentPath() const { return m_currentPath; }

private:
    LogWriter() = default;
    mutable std::mutex m_mutex;
    QFile m_file;
    QString m_currentPath;
};

} // namespace

void Logger::init() {
    ensureLogDir();
    rotateLogs();
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} [%{category}] %{type}: %{message}"));
}

void Logger::shutdown() {
}

void Logger::debug(const QString& message) {
    qCDebug(biliLog) << message;
    LogWriter::instance().write(QStringLiteral("DEBUG"), message);
}

void Logger::info(const QString& message) {
    qCInfo(biliLog) << message;
    LogWriter::instance().write(QStringLiteral("INFO"), message);
}

void Logger::warning(const QString& message) {
    qCWarning(biliLog) << message;
    LogWriter::instance().write(QStringLiteral("WARN"), message);
}

void Logger::error(const QString& message) {
    qCCritical(biliLog) << message;
    LogWriter::instance().write(QStringLiteral("ERROR"), message);
}

QString Logger::currentLogPath() {
    return LogWriter::instance().currentPath();
}

void Logger::ensureLogDir() {
    QDir dir(Paths::logsDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

void Logger::rotateLogs() {
    QDir dir(Paths::logsDir());
    const auto entries = dir.entryInfoList({QStringLiteral("bilihistory.log.*")},
                                            QDir::Files,
                                            QDir::Time);
    if (entries.size() > MaxLogFiles) {
        for (int i = MaxLogFiles; i < entries.size(); ++i) {
            QFile::remove(entries.at(i).absoluteFilePath());
        }
    }
}

} // namespace bili
