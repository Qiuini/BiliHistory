#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace bili::coro {

// 单值异步协程任务。不可拷贝，只可移动；异常会通过 await_resume 抛出。
// 设计约束：Task 必须被 co_await，不允许 fire-and-forget，以确保协程帧生命周期安全。
template <typename T>
class Task {
public:
    struct promise_type {
        std::optional<T> result;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object()
        {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }

        auto final_suspend() noexcept
        {
            struct FinalAwaiter {
                promise_type* promise;

                bool await_ready() const noexcept { return false; }

                void await_suspend(std::coroutine_handle<> h) const noexcept
                {
                    // 不在这里销毁协程帧，由持有 Task 的对象负责销毁。
                    // 仅恢复调用方（如果存在）。
                    if (promise->continuation) {
                        promise->continuation.resume();
                    }
                }

                void await_resume() const noexcept {}
            };
            return FinalAwaiter{this};
        }

        void return_value(T value)
        {
            result = std::move(value);
        }

        void unhandled_exception()
        {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit Task(handle_type h)
        : m_handle(h)
    {
    }

    ~Task()
    {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    Task(Task&& other) noexcept
        : m_handle(other.m_handle)
    {
        other.m_handle = nullptr;
    }

    Task& operator=(Task&& other) noexcept
    {
        if (this != &other) {
            if (m_handle) {
                m_handle.destroy();
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool await_ready() const noexcept
    {
        return m_handle.done();
    }

    void await_suspend(std::coroutine_handle<> h) noexcept
    {
        m_handle.promise().continuation = h;
    }

    T await_resume()
    {
        if (m_handle.promise().exception) {
            std::rethrow_exception(m_handle.promise().exception);
        }
        return std::move(*m_handle.promise().result);
    }

    handle_type handle() const noexcept { return m_handle; }

private:
    handle_type m_handle;
};

// Task<void> 特化
template <>
class Task<void> {
public:
    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object()
        {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept
        {
            struct FinalAwaiter {
                promise_type* promise;
                bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<>) const noexcept
                {
                    if (promise->continuation) {
                        promise->continuation.resume();
                    }
                }
                void await_resume() const noexcept {}
            };
            return FinalAwaiter{this};
        }

        void return_void() {}

        void unhandled_exception()
        {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit Task(handle_type h) : m_handle(h) {}

    ~Task()
    {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    Task(Task&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }

    Task& operator=(Task&& other) noexcept
    {
        if (this != &other) {
            if (m_handle) m_handle.destroy();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool await_ready() const noexcept { return m_handle.done(); }
    void await_suspend(std::coroutine_handle<> h) noexcept { m_handle.promise().continuation = h; }

    void await_resume()
    {
        if (m_handle.promise().exception) {
            std::rethrow_exception(m_handle.promise().exception);
        }
    }

private:
    handle_type m_handle;
};

} // namespace bili::coro
