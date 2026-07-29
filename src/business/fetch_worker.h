#pragma once

#include "core/models.h"
#include "network/api_client.h"
#include "network/fetchers.h"

#include <QObject>
#include <QThread>
#include <atomic>

namespace bili::business {

class FetchWorker : public QObject {
    Q_OBJECT
public:
    explicit FetchWorker(QObject* parent = nullptr);
    ~FetchWorker() override;

public slots:
    void startFetch(const QString& cookie);
    void cancelFetch();

signals:
    void started();
    void progress(int total);
    void pageFetched(const bili::RecordList& records, int page, int totalSoFar);
    void finished(const bili::RecordList& records);
    void error(const QString& message);
    void cancelled();

private:
    void cleanup();

    QThread* m_thread = nullptr;
    std::atomic<bool> m_running{false};
    ApiClient* m_client = nullptr;
    HistoryFetcher* m_fetcher = nullptr;
};

} // namespace bili::business
