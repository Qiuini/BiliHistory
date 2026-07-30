#pragma once

#include "core/models.h"

#include <QObject>
#include <QString>

namespace bili::business {

// 后台抓取工作线程接口，便于 UI 层依赖抽象而非具体实现。
class IFetchWorker : public QObject {
    Q_OBJECT
public:
    explicit IFetchWorker(QObject* parent = nullptr)
        : QObject(parent) {}

    virtual void startFetch(const QString& cookie) = 0;
    virtual void cancelFetch() = 0;

signals:
    void started();
    void pageFetched(const bili::RecordList& records, int page, int totalSoFar);
    void progress(int total);
    void finished(const bili::RecordList& records);
    void error(const QString& message);
    void cancelled();

protected:
    ~IFetchWorker() override = default;
};

} // namespace bili::business
