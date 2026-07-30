#pragma once

#include "core/i_config.h"
#include "i_fetch_worker.h"
#include "network/i_fetcher.h"

#include <QObject>
#include <QThread>
#include <atomic>
#include <functional>

namespace bili::business {

// HistoryFetcher 工厂签名：在 worker 线程中创建 IHistoryFetcher 实例。
using HistoryFetcherFactory = std::function<bili::IHistoryFetcher*(QObject* parent)>;

class FetchWorker : public IFetchWorker {
    Q_OBJECT
public:
    explicit FetchWorker(IConfig* config,
                         HistoryFetcherFactory factory,
                         QObject* parent = nullptr);
    ~FetchWorker() override;

public slots:
    void startFetch(const QString& cookie) override;
    void cancelFetch() override;

private:
    void cleanup();
    // 同步等待 m_running 变为 false，最多等待 timeoutMs 毫秒。
    // 在 worker 线程中用 QEventLoop 处理 cancelled/finished/error 信号。
    void waitUntilIdle(int timeoutMs);

    IConfig* m_config = nullptr;
    HistoryFetcherFactory m_factory;
    QThread* m_thread = nullptr;
    std::atomic<bool> m_running{false};
    bili::IHistoryFetcher* m_fetcher = nullptr;
};

} // namespace bili::business
