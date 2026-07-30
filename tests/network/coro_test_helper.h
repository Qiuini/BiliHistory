#pragma once

#include "coro/task.h"

#include <QEventLoop>
#include <QTimer>

#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace bili::test {

// 在事件循环中执行协程任务并返回结果，支持超时。
// 用于在 gtest 测试里同步等待协程完成。
template <typename T>
T awaitTask(coro::Task<T>&& task, int timeoutMs = 10000)
{
    std::optional<T> result;
    std::exception_ptr exception;
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);

    auto runner = [](coro::Task<T> t, std::optional<T>& out, std::exception_ptr& err, QEventLoop* loop) -> coro::Task<void> {
        try {
            out = co_await t;
        } catch (...) {
            err = std::current_exception();
        }
        if (loop) {
            loop->quit();
        }
    };

    auto holder = runner(std::move(task), result, exception, &loop);
    loop.exec();

    if (exception) {
        std::rethrow_exception(exception);
    }
    if (!result) {
        throw std::runtime_error("awaitTask timed out");
    }
    return std::move(*result);
}

inline void awaitTask(coro::Task<void>&& task, int timeoutMs = 10000)
{
    bool done = false;
    std::exception_ptr exception;
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);

    auto runner = [](coro::Task<void> t, bool& out, std::exception_ptr& err, QEventLoop* loop) -> coro::Task<void> {
        try {
            co_await t;
            out = true;
        } catch (...) {
            err = std::current_exception();
        }
        if (loop) {
            loop->quit();
        }
    };

    auto holder = runner(std::move(task), done, exception, &loop);
    loop.exec();

    if (exception) {
        std::rethrow_exception(exception);
    }
    if (!done) {
        throw std::runtime_error("awaitTask timed out");
    }
}

} // namespace bili::test
