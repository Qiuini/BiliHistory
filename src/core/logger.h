#pragma once

#include "i_logger.h"

#include <QFile>
#include <QLoggingCategory>
#include <QString>

#include <mutex>

Q_DECLARE_LOGGING_CATEGORY(biliLog)

namespace bili {

// 具体日志写入器，实现 ILogger 接口。
// 不再是 Meyer's 单例：可由 main 构造并注入到 Logger 门面，也可在测试中替换为 mock。
class LogWriter : public ILogger {
public:
    LogWriter();
    explicit LogWriter(QString logPath);
    ~LogWriter() override;

    LogWriter(const LogWriter&) = delete;
    LogWriter& operator=(const LogWriter&) = delete;

    // 实例方法：确保日志目录、轮转旧文件、设置 Qt 消息格式。
    void init();

    void info(const QString& msg) override;
    void debug(const QString& msg) override;
    void warning(const QString& msg) override;
    void error(const QString& msg) override;

    QString currentLogPath() const override;

private:
    void write(const QString& level, const QString& message);
    void rotateIfNeeded();
    void openCurrent();
    void ensureLogDir();
    void rotateLogs();
    QString logDir() const;

    mutable std::mutex m_mutex;
    QFile m_file;
    QString m_currentPath;
    QString m_logPath; // 为空时使用默认路径 Paths::logsDir()/bilihistory.log
};

// 静态门面：保持现有调用点不变，内部委托给可设置的 ILogger 实例。
// main 中通过 setInstance 注入；未注入时返回一个默认内部 LogWriter。
class Logger {
public:
    static void init();
    static void shutdown();

    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);

    static QString currentLogPath();

    // 依赖注入入口：main 装配时调用，传入 LogWriter* 或 mock。
    static void setInstance(ILogger* instance);
    // 返回当前实例；未设置时返回默认内部 LogWriter（兜底，兼容未注入场景）。
    static ILogger* instance();
};

} // namespace bili
