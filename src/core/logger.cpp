#include "logger.h"
#include "paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtGlobal>

#include <atomic>
#include <mutex>

Q_LOGGING_CATEGORY(biliLog, "bili")

namespace bili {

namespace {

constexpr int MaxLogFiles = 7;
constexpr qint64 MaxLogSizeBytes = 5 * 1024 * 1024; // 5MB

// 注入的 ILogger 实例（由 main 通过 Logger::setInstance 设置）。
// 使用函数局部 static + atomic 保证线程安全初始化与访问。
std::atomic<ILogger*>& overloadInstance() {
    static std::atomic<ILogger*> ptr{nullptr};
    return ptr;
}

// 默认内部 LogWriter，仅在未注入时兜底使用。
LogWriter& defaultWriterInstance() {
    static LogWriter writer;
    return writer;
}

std::once_flag& defaultInitFlag() {
    static std::once_flag flag;
    return flag;
}

} // namespace

// --- LogWriter ---

LogWriter::LogWriter() = default;

LogWriter::LogWriter(QString logPath)
    : m_logPath(std::move(logPath)) {
}

LogWriter::~LogWriter() = default;

void LogWriter::init() {
    ensureLogDir();
    rotateLogs();
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} [%{category}] %{type}: %{message}"));
}

void LogWriter::info(const QString& msg) {
    qCInfo(biliLog) << msg;
    write(QStringLiteral("INFO"), msg);
}

void LogWriter::debug(const QString& msg) {
    qCDebug(biliLog) << msg;
    write(QStringLiteral("DEBUG"), msg);
}

void LogWriter::warning(const QString& msg) {
    qCWarning(biliLog) << msg;
    write(QStringLiteral("WARN"), msg);
}

void LogWriter::error(const QString& msg) {
    qCCritical(biliLog) << msg;
    write(QStringLiteral("ERROR"), msg);
}

QString LogWriter::currentLogPath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPath;
}

void LogWriter::write(const QString& level, const QString& message) {
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

void LogWriter::rotateIfNeeded() {
    if (!m_file.isOpen()) return;
    if (m_file.size() < MaxLogSizeBytes) return;
    m_file.close();
    const QString backup = QStringLiteral("%1.%2")
                               .arg(m_currentPath,
                                    QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    QFile::rename(m_currentPath, backup);
    openCurrent();
}

void LogWriter::openCurrent() {
    m_currentPath = m_logPath.isEmpty()
                        ? (Paths::logsDir() + QStringLiteral("/bilihistory.log"))
                        : m_logPath;
    m_file.setFileName(m_currentPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qCWarning(biliLog) << "Failed to open log file:" << m_currentPath;
    }
}

QString LogWriter::logDir() const {
    if (!m_logPath.isEmpty()) {
        return QFileInfo(m_logPath).absolutePath();
    }
    return Paths::logsDir();
}

void LogWriter::ensureLogDir() {
    QDir dir(logDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

void LogWriter::rotateLogs() {
    QDir dir(logDir());
    const QString prefix = m_logPath.isEmpty()
                               ? QStringLiteral("bilihistory.log.*")
                               : (QFileInfo(m_logPath).fileName() + QStringLiteral(".*"));
    const auto entries = dir.entryInfoList({prefix}, QDir::Files, QDir::Time);
    if (entries.size() > MaxLogFiles) {
        for (int i = MaxLogFiles; i < entries.size(); ++i) {
            QFile::remove(entries.at(i).absoluteFilePath());
        }
    }
}

// --- Logger 静态门面 ---

void Logger::setInstance(ILogger* instance) {
    overloadInstance().store(instance, std::memory_order_release);
}

ILogger* Logger::instance() {
    if (ILogger* p = overloadInstance().load(std::memory_order_acquire)) {
        return p;
    }
    // 未注入：返回默认内部 LogWriter（不强制 init，保持与原“未调用 init”语义一致）。
    return &defaultWriterInstance();
}

void Logger::init() {
    if (overloadInstance().load(std::memory_order_acquire) != nullptr) {
        return; // 已注入实例，不覆盖
    }
    LogWriter& w = defaultWriterInstance();
    std::call_once(defaultInitFlag(), [&w] { w.init(); });
    overloadInstance().store(&w, std::memory_order_release);
}

void Logger::shutdown() {
}

void Logger::debug(const QString& message) {
    instance()->debug(message);
}

void Logger::info(const QString& message) {
    instance()->info(message);
}

void Logger::warning(const QString& message) {
    instance()->warning(message);
}

void Logger::error(const QString& message) {
    instance()->error(message);
}

QString Logger::currentLogPath() {
    return instance()->currentLogPath();
}

} // namespace bili
