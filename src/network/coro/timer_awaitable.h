#pragma once

#include <QObject>
#include <QTimer>

#include <coroutine>
#include <memory>

namespace bili::coro {

// 可等待的延时，底层使用 QTimer::singleShot，不阻塞事件循环。
// 使用 shared_ptr 保护协程句柄，避免 SleepAwaitable 销毁后 timer 回调访问悬挂指针。
class SleepAwaitable {
public:
    explicit SleepAwaitable(int ms)
        : m_ms(ms)
        , m_state(std::make_shared<State>())
    {
    }

    bool await_ready() const noexcept
    {
        return m_ms <= 0;
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        m_state->handle = handle;
        QTimer::singleShot(m_ms, nullptr, [state = m_state]() {
            if (!state || !state->handle) {
                return;
            }
            auto h = state->handle;
            state->handle = nullptr;
            h.resume();
        });
    }

    void await_resume() const noexcept
    {
        if (m_state) {
            m_state->handle = nullptr;
        }
    }

private:
    struct State {
        std::coroutine_handle<> handle = nullptr;
    };

    int m_ms = 0;
    std::shared_ptr<State> m_state;
};

inline SleepAwaitable sleepFor(int ms)
{
    return SleepAwaitable(ms);
}

} // namespace bili::coro
