#include <gtest/gtest.h>

#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include "business/fetch_worker.h"
#include "config.h"
#include "test_http_server.h"

using namespace bili;
using namespace bili::business;

namespace {

class FetchWorkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().loadDefaults();
        Config::instance().setValue(QStringLiteral("retry_wait_ms"), 50);
        Config::instance().setValue(QStringLiteral("http_backoff_factor"), 0.5);
        Config::instance().setValue(QStringLiteral("http_total_retries"), 1);
        Config::instance().setValue(QStringLiteral("page_size"), 5);

        m_server = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_server->start());

        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(m_server->serverPort());
        Config::instance().setValue(QStringLiteral("base_url"), base);
    }

    void TearDown() override {
        m_worker.reset();
        m_server.reset();
    }

    std::unique_ptr<TestHttpServer> m_server;
    std::unique_ptr<FetchWorker> m_worker;
};

} // namespace

TEST_F(FetchWorkerTest, EmitsStartedFinishedAndForwardsRecords) {
    const QByteArray page = R"({
        "code": 0,
        "data": {
            "list": [{
                "title": "视频1",
                "view_at": 1700000000,
                "duration": 120,
                "progress": 60,
                "author_name": "UP主",
                "cover": "cover.jpg",
                "history": {"business": "archive", "bvid": "BV1xx411c7mD"},
                "tname": "动画"
            }],
            "cursor": {"max": 0, "view_at": 0, "business": ""}
        }
    })";

    m_server->enqueueResponse(200, page);

    m_worker = std::make_unique<FetchWorker>();

    bool started = false;
    bool finished = false;
    int progressTotal = 0;
    int pageNumber = 0;
    RecordList allRecords;

    QEventLoop loop;
    QObject::connect(m_worker.get(), &FetchWorker::started, &loop, [&]() {
        started = true;
    });
    QObject::connect(m_worker.get(), &FetchWorker::progress, &loop, [&](int total) {
        progressTotal = total;
    });
    QObject::connect(m_worker.get(), &FetchWorker::pageFetched, &loop, [&](const RecordList&, int page, int) {
        pageNumber = page;
    });
    QObject::connect(m_worker.get(), &FetchWorker::finished, &loop, [&](const RecordList& records) {
        finished = true;
        allRecords = records;
        loop.quit();
    });
    QObject::connect(m_worker.get(), &FetchWorker::error, &loop, [&](const QString& message) {
        ADD_FAILURE() << message.toStdString();
        loop.quit();
    });

    QMetaObject::invokeMethod(m_worker.get(), "startFetch",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("SESSDATA=test")));

    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(finished);
    EXPECT_TRUE(started);
    EXPECT_EQ(allRecords.size(), 1u);
    EXPECT_EQ(progressTotal, 1);
    EXPECT_EQ(pageNumber, 1);
}

TEST_F(FetchWorkerTest, CancelFetchEmitsCancelled) {
    m_server->enqueueResponse(200, QByteArrayLiteral("{}"), 5000);

    m_worker = std::make_unique<FetchWorker>();

    bool cancelled = false;
    QEventLoop loop;
    QObject::connect(m_worker.get(), &FetchWorker::finished, &loop, &QEventLoop::quit);
    QObject::connect(m_worker.get(), &FetchWorker::error, &loop, &QEventLoop::quit);
    QObject::connect(m_worker.get(), &FetchWorker::cancelled, &loop, [&]() {
        cancelled = true;
        loop.quit();
    });

    QMetaObject::invokeMethod(m_worker.get(), "startFetch",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("SESSDATA=test")));
    QTimer::singleShot(100, m_worker.get(), &FetchWorker::cancelFetch);
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_TRUE(cancelled);
}

TEST_F(FetchWorkerTest, EmptyCookieEmitsError) {
    m_worker = std::make_unique<FetchWorker>();

    bool errored = false;
    QString errorMessage;
    QEventLoop loop;
    QObject::connect(m_worker.get(), &FetchWorker::error, &loop, [&](const QString& message) {
        errored = true;
        errorMessage = message;
        loop.quit();
    });

    QMetaObject::invokeMethod(m_worker.get(), "startFetch",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QString()));
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_TRUE(errored);
    EXPECT_FALSE(errorMessage.isEmpty());
}
