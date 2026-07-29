#include "fetch_worker.h"

#include "core/logger.h"

namespace bili::business {

FetchWorker::FetchWorker(QObject* parent)
    : QObject(parent)
{
    m_thread = new QThread(this);
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
        Logger::warning(QStringLiteral("FetchWorker 已在运行中"));
        return;
    }

    if (cookie.isEmpty()) {
        emit error(QStringLiteral("未设置 Cookie，无法抓取历史记录"));
        return;
    }

    m_running.store(true);
    m_client = new ApiClient(this);
    m_fetcher = new HistoryFetcher(m_client, this);

    connect(m_fetcher, &HistoryFetcher::pageFetched,
            this, [this](const RecordList& records, int page, int totalSoFar) {
        emit pageFetched(records, page, totalSoFar);
    });

    connect(m_fetcher, &HistoryFetcher::progress,
            this, [this](int total) {
        emit progress(total);
    });

    connect(m_fetcher, &HistoryFetcher::finished,
            this, [this](const RecordList& records) {
        m_running.store(false);
        emit finished(records);
        cleanup();
    });

    connect(m_fetcher, &HistoryFetcher::error,
            this, [this](const NetworkException& e) {
        m_running.store(false);
        emit error(e.message());
        cleanup();
    });

    connect(m_fetcher, &HistoryFetcher::cancelled,
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
    if (m_fetcher && m_running.load()) {
        m_fetcher->cancel();
    }
}

void FetchWorker::cleanup()
{
    if (m_fetcher) {
        m_fetcher->deleteLater();
        m_fetcher = nullptr;
    }
    if (m_client) {
        m_client->deleteLater();
        m_client = nullptr;
    }
}

} // namespace bili::business
