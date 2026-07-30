#include "fetch_worker.h"

#include "core/exceptions.h"
#include "core/logger.h"

#include <QEventLoop>
#include <QTimer>

namespace bili::business {

namespace {

constexpr int kCancelWaitMs = 5000;

} // namespace

FetchWorker::FetchWorker(IConfig* config,
                         HistoryFetcherFactory factory,
                         QObject* /*parent*/)
    : IFetchWorker(nullptr)
    , m_config(config)
    , m_factory(std::move(factory))
    , m_thread(new QThread(this))
{
    Q_ASSERT(m_config != nullptr);
    // worker 对象自身需要移到 worker 线程，因此不能有 QObject parent。
    moveToThread(m_thread);
    m_thread->start();
}

FetchWorker::~FetchWorker()
{
    cancelFetch();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
}

void FetchWorker::startFetch(const QString& cookie)
{
    // 该槽函数在 worker 线程中执行，因为对象已 moveToThread
    if (m_running.load()) {
        Logger::warning(QStringLiteral("FetchWorker 已在运行中，忽略新的 startFetch 请求"));
        return;
    }

    if (cookie.isEmpty()) {
        emit error(QStringLiteral("未设置 Cookie，无法抓取历史记录"));
        return;
    }

    m_running.store(true);
    m_fetcher = m_factory(this);

    connect(m_fetcher, &bili::IHistoryFetcher::pageFetched,
            this, [this](const RecordList& records, int page, int totalSoFar) {
        emit pageFetched(records, page, totalSoFar);
    });

    connect(m_fetcher, &bili::IHistoryFetcher::progress,
            this, [this](int total) {
        emit progress(total);
    });

    connect(m_fetcher, &bili::IHistoryFetcher::finished,
            this, [this](const RecordList& records) {
        m_running.store(false);
        emit finished(records);
        cleanup();
    });

    connect(m_fetcher, &bili::IHistoryFetcher::error,
            this, [this](const NetworkException& e) {
        m_running.store(false);
        emit error(e.message());
        cleanup();
    });

    connect(m_fetcher, &bili::IHistoryFetcher::cancelled,
            this, [this]() {
        m_running.store(false);
        emit cancelled();
        cleanup();
    });

    emit started();
    m_fetcher->fetchAll(cookie);
}

void FetchWorker::cancelFetch()
{
    if (!m_fetcher || !m_running.load()) {
        return;
    }

    m_fetcher->cancel();

    // 同步等待 terminated 信号之一到来，避免连续 startFetch/cancelFetch 出现悬挂。
    // cancelFetch 在 worker 线程执行，QEventLoop 能处理同线程的 DirectConnection 信号。
    waitUntilIdle(kCancelWaitMs);

    if (m_running.load()) {
        // 超时兜底：强制标记结束，防止状态卡死
        Logger::error(QStringLiteral("FetchWorker cancel 超时未收到终止信号，强制结束"));
        m_running.store(false);
        cleanup();
        emit cancelled();
    }
}

void FetchWorker::waitUntilIdle(int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeoutMs);

    // 任一终止信号到达即退出循环
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(this, &IFetchWorker::cancelled, &loop, &QEventLoop::quit);
    QObject::connect(this, &IFetchWorker::finished, &loop, &QEventLoop::quit);
    QObject::connect(this, &IFetchWorker::error, &loop, &QEventLoop::quit);

    timer.start();
    loop.exec();
    timer.stop();
}

void FetchWorker::cleanup()
{
    if (m_fetcher) {
        m_fetcher->deleteLater();
        m_fetcher = nullptr;
    }
}

} // namespace bili::business
