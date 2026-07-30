#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace bili::coro {

// 协作式取消令牌。线程安全，支持注册一次性回调。
class CancellationToken : public std::enable_shared_from_this<CancellationToken> {
public:
    using Ptr = std::shared_ptr<CancellationToken>;

    // RAII 句柄，析构时自动从令牌注销对应回调，防止回调访问已销毁对象。
    class Guard {
    public:
        Guard() = default;
        explicit Guard(std::weak_ptr<CancellationToken> token, std::size_t id)
            : m_token(std::move(token))
            , m_id(id)
        {
        }
        ~Guard() { remove(); }

        Guard(Guard&& other) noexcept
            : m_token(std::move(other.m_token))
            , m_id(other.m_id)
        {
            other.m_id = 0;
        }
        Guard& operator=(Guard&& other) noexcept
        {
            if (this != &other) {
                remove();
                m_token = std::move(other.m_token);
                m_id = other.m_id;
                other.m_id = 0;
            }
            return *this;
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        void remove()
        {
            if (m_id == 0) {
                return;
            }
            if (auto token = m_token.lock()) {
                std::lock_guard lock(token->m_mutex);
                token->m_callbacks.erase(m_id);
            }
            m_id = 0;
            m_token.reset();
        }

    private:
        std::weak_ptr<CancellationToken> m_token;
        std::size_t m_id = 0;
    };

    CancellationToken() = default;
    ~CancellationToken() = default;

    CancellationToken(const CancellationToken&) = delete;
    CancellationToken& operator=(const CancellationToken&) = delete;

    // 发起取消，所有已注册回调会被同步调用一次。
    void cancel()
    {
        bool expected = false;
        if (!m_cancelled.compare_exchange_strong(expected, true,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed)) {
            return;
        }

        std::unordered_map<std::size_t, std::function<void()>> callbacks;
        {
            std::lock_guard lock(m_mutex);
            callbacks = std::move(m_callbacks);
        }
        for (auto& [id, cb] : callbacks) {
            if (cb) cb();
        }
    }

    bool isCancelled() const
    {
        return m_cancelled.load(std::memory_order_acquire);
    }

    // 注册取消回调。若已取消则立即调用；否则返回 Guard， lifetime 结束时自动注销。
    Guard onCancel(std::function<void()> callback)
    {
        {
            std::lock_guard lock(m_mutex);
            if (!m_cancelled.load(std::memory_order_relaxed)) {
                const std::size_t id = ++m_nextId;
                m_callbacks.emplace(id, std::move(callback));
                return Guard(weak_from_this(), id);
            }
        }
        if (callback) callback();
        return Guard();
    }

private:
    std::atomic<bool> m_cancelled{false};
    std::mutex m_mutex;
    std::unordered_map<std::size_t, std::function<void()>> m_callbacks;
    std::size_t m_nextId = 0;
};

} // namespace bili::coro
