#pragma once

#include <QString>

namespace bili {

// 抽象日志接口，支持依赖注入与测试 mock。
// 具体实现见 LogWriter；静态门面 Logger 通过 setInstance 接受 ILogger 实例。
class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void info(const QString& msg) = 0;
    virtual void debug(const QString& msg) = 0;
    virtual void warning(const QString& msg) = 0;
    virtual void error(const QString& msg) = 0;

    virtual QString currentLogPath() const = 0;
};

} // namespace bili
